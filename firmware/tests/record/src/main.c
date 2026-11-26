#include "storage/record.h"

#include <errno.h>
#include <stdio.h>
#include <string.h>

#include <zephyr/settings/settings.h>
#include <zephyr/ztest.h>

#define SLOTS CONFIG_RECORD_SLOTS

/**
 * @brief Starts the log, deletes all records, and turns the test fault off before each test.
 *
 * @param[in] f  Test fixture. The function does not use this value.
 */
static void before(void *f)
{
    zassert_ok(record_init());
    zassert_ok(record_clear());
    record_inject_fault(false);
}
ZTEST_SUITE(record, NULL, NULL, before, NULL, NULL);

/**
 * @brief Adds a 3-byte record to the log.
 *
 * The record bytes are @p tag, @p tag + 1 and @p tag + 2.
 *
 * @param[in] tag   First byte of the record.
 * @param[in] sent  true if the record is already sent.
 *
 * @return ID of the new record.
 */
static uint16_t put_n(uint8_t tag, bool sent)
{
    uint8_t r[3] = { tag, tag + 1, tag + 2 };
    uint16_t id;

    zassert_ok(record_put(r, sizeof(r), sent, &id));
    return id;
}

ZTEST(record, test_roundtrip)
{
    uint8_t in[5] = { 1, 2, 3, 4, 5 }, out[RECORD_MAX_LEN];
    size_t len;
    uint16_t id, id2;
    bool sent;

    zassert_ok(record_put(in, 5, true, &id));
    zassert_ok(record_store_peek(0, out, &len, &id2, &sent));
    zassert_equal(len, 5);
    zassert_mem_equal(in, out, 5);
    zassert_equal(id, id2);
    zassert_true(sent);
    zassert_equal(put_n(9, false), (uint16_t)(id + 1));
    zassert_equal(record_count(), 2);
    zassert_equal(record_unsent(), 1);
    zassert_not_equal(record_session_id(), 0);
}

ZTEST(record, test_unsent_order)
{
    uint8_t out[RECORD_MAX_LEN];
    size_t len;
    uint16_t id, a, b, c;

    zassert_equal(record_oldest_unsent(out, &len, &id), -ENOENT);
    a = put_n(10, false);
    b = put_n(20, true);
    c = put_n(30, false);
    zassert_ok(record_oldest_unsent(out, &len, &id));
    zassert_equal(id, a);
    zassert_equal(out[0], 10);
    zassert_ok(record_mark_sent(a));
    zassert_ok(record_oldest_unsent(out, &len, &id));
    zassert_equal(id, c);
    zassert_ok(record_mark_sent(c));
    zassert_equal(record_oldest_unsent(out, &len, &id), -ENOENT);
    zassert_equal(record_mark_sent((uint16_t)(c + 1)), -ENOENT);
    zassert_equal(record_mark_sent((uint16_t)(a - 1)), -ENOENT);
    zassert_equal(record_store_peek(3, out, &len, &id, NULL), -ENOENT);
    zassert_equal(record_unsent(), 0);
    (void)b;
}

ZTEST(record, test_next_id)
{
    const uint16_t next = record_next_id();

    zassert_equal(put_n(1, false), next);
    zassert_equal(record_next_id(), (uint16_t)(next + 1));

    record_inject_fault(true);
    zassert_equal(record_put((const uint8_t[]){ 1 }, 1, false, NULL), -EIO);
    zassert_equal(record_next_id(), (uint16_t)(next + 1));
}

ZTEST(record, test_bad_len)
{
    uint8_t r[RECORD_MAX_LEN + 1] = { 0 };

    zassert_equal(record_put(r, 0, false, NULL), -EMSGSIZE);
    zassert_equal(record_put(r, RECORD_MAX_LEN + 1, false, NULL), -EMSGSIZE);
    zassert_ok(record_put(r, RECORD_MAX_LEN, false, NULL));
}

ZTEST(record, test_wraparound)
{
    uint8_t out[RECORD_MAX_LEN];
    size_t len;
    uint16_t id, ids[SLOTS + 3];

    for (int i = 0; i < SLOTS + 3; i++) {
        ids[i] = put_n(i, false);
    }
    zassert_equal(record_count(), SLOTS);
    zassert_equal(record_lost(), 3);
    zassert_ok(record_oldest_unsent(out, &len, &id));
    zassert_equal(id, ids[3]);
    zassert_ok(record_store_peek(0, out, &len, &id, NULL));
    zassert_equal(id, ids[SLOTS + 2]);
    zassert_equal(out[0], SLOTS + 2);
}

ZTEST(record, test_overwrite_sent)
{
    for (int i = 0; i < SLOTS + 3; i++) {
        put_n(i, true);
    }
    zassert_equal(record_lost(), 0);
    zassert_equal(record_unsent(), 0);
}

ZTEST(record, test_fault)
{
    uint8_t r[2] = { 1, 2 };

    put_n(1, false);
    record_inject_fault(true);
    zassert_equal(record_put(r, 2, false, NULL), -EIO);
    zassert_equal(record_count(), 1);
    zassert_equal(record_lost(), 0);
    record_inject_fault(false);
    zassert_ok(record_put(r, 2, false, NULL));
    zassert_equal(record_count(), 2);
}

ZTEST(record, test_reload)
{
    uint8_t out[RECORD_MAX_LEN];
    size_t len;
    uint16_t id, last = 0;
    bool sent;

    for (int i = 0; i < SLOTS + 2; i++) {
        last = put_n(i, i % 2);
    }
    zassert_ok(settings_load_subtree("hs"));
    zassert_equal(record_count(), SLOTS);
    zassert_equal(record_lost(), 1);
    zassert_ok(record_store_peek(0, out, &len, &id, &sent));
    zassert_equal(id, last);
    zassert_equal(out[0], SLOTS + 1);
    zassert_equal(sent, (SLOTS + 1) % 2);
    zassert_equal(record_unsent(), SLOTS / 2);
}

ZTEST(record, test_missing_slot)
{
    uint8_t out[RECORD_MAX_LEN];
    char key[16];
    size_t len;
    uint16_t id;

    put_n(1, false);
    id = put_n(2, false);
    snprintf(key, sizeof(key), "hs/r/%u", id % SLOTS);
    zassert_ok(settings_delete(key));
    zassert_equal(record_store_peek(0, out, &len, NULL, NULL), -EIO);
    zassert_equal(record_mark_sent(id), -EIO);
    zassert_ok(record_store_peek(1, out, &len, NULL, NULL));
    zassert_equal(len, 3);
}
