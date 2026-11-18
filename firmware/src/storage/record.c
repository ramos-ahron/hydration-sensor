#include "storage/record.h"
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <zephyr/kernel.h>
#include <zephyr/logging/log.h>
#include <zephyr/settings/settings.h>
#include <zephyr/shell/shell.h>

LOG_MODULE_REGISTER(record, LOG_LEVEL_INF);

#define SLOTS CONFIG_RECORD_SLOTS
#define HDR   2

struct meta {
    uint16_t next_id;
    uint16_t count;
    uint32_t lost;
    uint16_t boot;
};

struct slot {
    uint8_t sent;
    uint8_t len;
    uint8_t valid;
};

static struct meta m;
static struct slot mirror[SLOTS];
static bool inited;
static bool fault;
static K_MUTEX_DEFINE(lock);

/**
 * @brief Copies one saved key from flash into RAM.
 *
 * Zephyr calls this function one time for each key under "hs". The calls occur
 * while record_init() runs settings_load_subtree("hs"). Zephyr removes the
 * "hs/" prefix first, so @p key is "meta" or "r/<slot>".
 *
 *   Key          Action                         RAM result (example values)
 *   ---------    ---------------------------    ------------------------------------
 *   "meta"       copy all 8 bytes          -->  m = { next_id 70, count 64,
 *                                                     lost 3, boot 5 }
 *   "r/0"        copy the 2-byte header    -->  mirror[0] = { sent 1, len 39, valid 1 }
 *   "r/6"        copy the 2-byte header    -->  mirror[6] = { sent 0, len 39, valid 1 }
 *   other key    ignore                         no change
 *
 * A slot value in flash has this layout. This function reads only bytes 0 and 1.
 *
 *   byte 0     byte 1     bytes 2 to (len + 1)
 *   [ sent ]   [ len ]    [ record, len bytes ]
 *   '--- header (HDR) ---'
 *
 * The mirror is a RAM table with one summary for each slot. The record bytes stay
 * in flash.
 *
 * This function ignores a slot key in these conditions:
 *   - The slot number is SLOTS or more.
 *   - The saved value is shorter than HDR bytes.
 *   - The read of the header fails.
 *
 * @param[in] key      Key name, without the "hs/" prefix.
 * @param[in] len      Size of the saved value in bytes.
 * @param[in] read_cb  Function that copies bytes of the saved value into a buffer.
 * @param[in] arg      Value to give to @p read_cb. Do not change it.
 *
 * @retval 0     The key loaded, the key is not one of ours, or the function
 *               ignored a slot key.
 * @retval -EIO  The read of "meta" failed.
 */
static int set_cb(const char *key, size_t len, settings_read_cb read_cb, void *arg)
{
    const char *next;

    // Read metadata. An exact match sets next to NULL.
    if (settings_name_steq(key, "meta", &next) && !next) {
        return read_cb(arg, &m, sizeof(m)) < 0 ? -EIO : 0;
    }

    // Read keys
    if (settings_name_steq(key, "r", &next) && next) {
        unsigned long s = strtoul(next, NULL, 10);
        uint8_t hdr[HDR];

        if (s < SLOTS && len >= HDR && read_cb(arg, hdr, HDR) == HDR) {
            mirror[s] = (struct slot){ hdr[0], hdr[1], 1 };
        }
    }
    return 0;
}
SETTINGS_STATIC_HANDLER_DEFINE(hs, "hs", NULL, set_cb, NULL, NULL);

/**
 * @brief Writes the settings key for the slot that holds record @p id.
 *
 * The slot number is @p id modulo SLOTS. For example, with 64 slots, ID 70 gives
 * "hs/r/6".
 *
 * @param[out] key  Receives the key. Must hold 16 bytes.
 * @param[in]  id   Record ID.
 */
static void slot_key(char *key, uint16_t id) { snprintf(key, 16, "hs/r/%u", id % SLOTS); }

/**
 * @brief Copies one slot value into a buffer during settings_load_subtree_direct().
 *
 * rd_slot() gives this function to Zephyr. Zephyr calls it for the slot key.
 *
 * @param[in]  key      Key name. This function does not use it.
 * @param[in]  len      Size of the saved value in bytes. This function does not use it.
 * @param[in]  read_cb  Function that copies bytes of the saved value into a buffer.
 * @param[in]  arg      Value to give to @p read_cb. Do not change it.
 * @param[out] param    Buffer that receives the value. Must hold HDR + RECORD_MAX_LEN bytes.
 *
 * @retval 1     The value is copied. The load stops.
 * @retval -EIO  The read failed.
 */
static int rd_cb(const char *key, size_t len, settings_read_cb read_cb, void *arg, void *param)
{ return read_cb(arg, param, HDR + RECORD_MAX_LEN) < 0 ? -EIO : 1; }

/**
 * @brief Reads the slot value of record @p id from flash.
 *
 * The value is the 2-byte header and then the record bytes. See set_cb() for the layout.
 *
 * @param[in]  id   Record ID.
 * @param[out] buf  Receives the slot value. Must hold HDR + RECORD_MAX_LEN bytes.
 *
 * @retval 0     The load completed. If the key is not in flash, @p buf does not change.
 * @retval -EIO  The load failed.
 */
static int rd_slot(uint16_t id, uint8_t *buf)
{
    char key[16];

    slot_key(key, id);
    return settings_load_subtree_direct(key, rd_cb, buf) ? -EIO : 0;
}

/**
 * @brief Writes record @p id and its header to its slot in flash.
 *
 * The new value replaces the old value in the slot. This function does not change
 * the mirror or the metadata.
 *
 * @param[in] id    Record ID.
 * @param[in] sent  true if the record is already sent.
 * @param[in] rec   Record bytes.
 * @param[in] len   Size of @p rec in bytes. Must range from 1 to RECORD_MAX_LEN.
 *
 * @retval 0     The write completed.
 * @retval -EIO  The write failed.
 */
static int wr_slot(uint16_t id, bool sent, const uint8_t *rec, size_t len)
{
    uint8_t buf[HDR + RECORD_MAX_LEN] = { sent, len };
    char key[16];

    memcpy(buf + HDR, rec, len);
    slot_key(key, id);
    return settings_save_one(key, buf, HDR + len) ? -EIO : 0;
}

/**
 * @brief Writes the metadata to flash.
 *
 * This function does not change @c m. The caller copies @p nm into @c m after the
 * write completes.
 *
 * @param[in] nm  New metadata.
 *
 * @retval 0     The write completed.
 * @retval -EIO  The write failed.
 */
static int wr_meta(const struct meta *nm)
{ return settings_save_one("hs/meta", nm, sizeof(*nm)) ? -EIO : 0; }

/**
 * @brief Gives the ID of the oldest record in the log.
 *
 * The result is correct after the ID counter wraps at 65536. If the log is empty,
 * the result is equal to the next ID.
 *
 * @return ID of the oldest record.
 */
static uint16_t oldest_id(void) { return (uint16_t)(m.next_id - m.count); }

int record_init(void)
{
    int rc = 0;

    k_mutex_lock(&lock, K_FOREVER);
    if (!inited) {
        struct meta nm;

        rc = settings_subsys_init();
        if (!rc) { rc = settings_load_subtree("hs"); }
        nm = m;
        nm.boot++;
        if (!rc && wr_meta(&nm) == 0) {
            m = nm;
            inited = true;
        } else if (!rc) {
            rc = -EIO;
        }
    }
    k_mutex_unlock(&lock);
    return rc;
}

int record_put(const uint8_t *rec, size_t len, bool sent, uint16_t *id)
{
    struct meta nm;
    uint16_t slot;
    int rc;

    if (len < 1 || len > RECORD_MAX_LEN) { return -EMSGSIZE; }
    k_mutex_lock(&lock, K_FOREVER);
    nm = m;
    slot = m.next_id % SLOTS;
    if (fault) {
        rc = -EIO;
    } else {
        if (m.count == SLOTS && mirror[slot].valid && !mirror[slot].sent) { nm.lost++; }
        nm.next_id++;
        if (nm.count < SLOTS) { nm.count++; }
        // record first, meta second so a power cut never leaves meta pointing at nothing
        rc = wr_slot(m.next_id, sent, rec, len);
        if (!rc) { rc = wr_meta(&nm); }
    }
    if (!rc) {
        if (id) { *id = m.next_id; }
        mirror[slot] = (struct slot){ sent, len, 1 };
        m = nm;
    }
    k_mutex_unlock(&lock);
    return rc;
}

/**
 * @brief Reads record @p id from flash and copies it to the caller.
 *
 * The caller must hold @c lock.
 *
 * @param[in]  id    Record ID.
 * @param[out] rec   Receives the record. Must hold RECORD_MAX_LEN bytes.
 * @param[out] len   Receives the size of the record in bytes.
 * @param[out] sent  Receives the sent flag. Can be NULL.
 *
 * @retval 0     The record is copied.
 * @retval -EIO  The read failed. The outputs do not change.
 */
static int out(uint16_t id, uint8_t *rec, size_t *len, bool *sent)
{
    uint8_t buf[HDR + RECORD_MAX_LEN];
    int rc = rd_slot(id, buf);

    if (!rc) {
        memcpy(rec, buf + HDR, buf[1]);
        *len = buf[1];
        if (sent) { *sent = buf[0]; }
    }
    return rc;
}

int record_oldest_unsent(uint8_t *rec, size_t *len, uint16_t *id)
{
    int rc = -ENOENT;

    k_mutex_lock(&lock, K_FOREVER);
    for (uint16_t i = 0; i < m.count; i++) {
        uint16_t cur = oldest_id() + i;

        if (mirror[cur % SLOTS].valid && !mirror[cur % SLOTS].sent) {
            rc = out(cur, rec, len, NULL);
            if (!rc && id) { *id = cur; }
            break;
        }
    }
    k_mutex_unlock(&lock);
    return rc;
}

int record_mark_sent(uint16_t id)
{
    uint8_t buf[HDR + RECORD_MAX_LEN];
    int rc = -ENOENT;

    k_mutex_lock(&lock, K_FOREVER);
    if ((uint16_t)(id - oldest_id()) < m.count) {
        rc = rd_slot(id, buf);
        if (!rc && !buf[0]) {
            rc = wr_slot(id, true, buf + HDR, buf[1]);
            if (!rc) { mirror[id % SLOTS].sent = 1; }
        }
    }
    k_mutex_unlock(&lock);
    return rc;
}

int record_store_peek(uint16_t back, uint8_t *rec, size_t *len, uint16_t *id, bool *sent)
{
    int rc = -ENOENT;

    k_mutex_lock(&lock, K_FOREVER);
    if (back < m.count) {
        uint16_t cur = m.next_id - 1 - back;

        rc = out(cur, rec, len, sent);
        if (!rc && id) { *id = cur; }
    }
    k_mutex_unlock(&lock);
    return rc;
}

uint16_t record_count(void) { return m.count; }

uint16_t record_unsent(void)
{
    uint16_t n = 0;

    k_mutex_lock(&lock, K_FOREVER);
    for (uint16_t i = 0; i < SLOTS; i++) {
        n += mirror[i].valid && !mirror[i].sent;
    }
    k_mutex_unlock(&lock);
    return n;
}

uint32_t record_lost(void) { return m.lost; }

uint16_t record_session_id(void) { return m.boot; }

void record_inject_fault(bool on) { fault = on; }

int record_clear(void)
{
    struct meta nm;
    char key[16];
    int rc = 0;

    k_mutex_lock(&lock, K_FOREVER);
    for (uint16_t i = 0; i < SLOTS && !rc; i++) {
        if (mirror[i].valid) {
            snprintf(key, sizeof(key), "hs/r/%u", i);
            rc = settings_delete(key) ? -EIO : 0;
            mirror[i].valid = 0;
        }
    }
    nm = m;
    nm.count = 0;
    nm.lost = 0;
    if (!rc) { rc = wr_meta(&nm); }
    if (!rc) { m = nm; }
    k_mutex_unlock(&lock);
    return rc;
}

#ifdef CONFIG_SHELL
/**
 * @brief "hs store list [n]": prints the newest @p n records, newest first.
 *
 * The default for @p n is 5. Each line shows the ID, the sent flag, the size, and the
 * record bytes in hexadecimal. The list stops early if the log holds fewer records.
 *
 * @retval 0  Always.
 */
static int cmd_list(const struct shell *sh, size_t argc, char **argv)
{
    uint16_t n = argc > 1 ? strtoul(argv[1], NULL, 10) : 5;

    for (uint16_t b = 0; b < n; b++) {
        uint8_t rec[RECORD_MAX_LEN];
        char hex[2 * RECORD_MAX_LEN + 1];
        size_t len;
        uint16_t id;
        bool sent;

        if (record_store_peek(b, rec, &len, &id, &sent)) { break; }
        for (size_t i = 0; i < len; i++) {
            snprintf(hex + 2 * i, 3, "%02x", rec[i]);
        }
        shell_print(sh, "id=%u sent=%d len=%u %s", id, sent, (unsigned)len, hex);
    }
    return 0;
}

/**
 * @brief "hs store stat": prints the log counters.
 *
 * The counters are the record count, the unsent count, the lost count, the session ID,
 * and the number of slots.
 *
 * @retval 0  Always.
 */
static int cmd_stat(const struct shell *sh, size_t argc, char **argv)
{
    shell_print(sh, "count=%u unsent=%u lost=%u session=%u slots=%d", record_count(),
                record_unsent(), (unsigned)record_lost(), record_session_id(), SLOTS);
    return 0;
}

/**
 * @brief "hs store clear": deletes all records. See record_clear().
 *
 * @retval 0     All records are deleted.
 * @retval -EIO  A delete or write failed.
 */
static int cmd_clear(const struct shell *sh, size_t argc, char **argv) { return record_clear(); }

/**
 * @brief "hs store fault <0|1>": sets the test fault. See record_inject_fault().
 *
 * A number other than 0 makes record_put() fail with -EIO. 0 stops the fault. A value
 * that is not a number counts as 0.
 *
 * @retval 0  Always.
 */
static int cmd_fault(const struct shell *sh, size_t argc, char **argv)
{
    record_inject_fault(atoi(argv[1]) != 0);
    return 0;
}

SHELL_STATIC_SUBCMD_SET_CREATE(sub, SHELL_CMD_ARG(list, NULL, "[n]", cmd_list, 1, 1),
                               SHELL_CMD(stat, NULL, "Counters", cmd_stat),
                               SHELL_CMD(clear, NULL, "Erase all records", cmd_clear),
                               SHELL_CMD_ARG(fault, NULL, "<0|1>", cmd_fault, 2, 0),
                               SHELL_SUBCMD_SET_END);
SHELL_SUBCMD_ADD((hs), store, &sub, "Record store", NULL, 1, 0);
#endif
