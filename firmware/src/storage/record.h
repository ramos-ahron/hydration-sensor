#ifndef RECORD_H
#define RECORD_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

/** Maximum size of one record, in bytes. */
#define RECORD_MAX_LEN 64

// TODO: add uptime time to record and ble

/**
 * Starts the log. Reads the saved state and adds 1 to the boot counter.
 *
 * @retval 0 on success
 * @retval -EIO the saved state cannot be written
 * @return Another negative error code if the settings subsystem fails.
 */
int record_init(void);

/**
 * Adds a record at the newest position.
 *
 * @param[in]  rec  Record bytes.
 * @param[in]  len  Size of @p rec in bytes. Must range from 1 to RECORD_MAX_LEN.
 * @param[in]  sent true if the record is already sent.
 * @param[out] id   Receives the record ID. The ID increases by 1 for each record and wraps
 *                  at 65536.
 *
 * @retval 0 on success
 * @retval -EIO a write failed, or the test fault is on. The log does not change.
 * @retval -EMSGSIZE @p len is 0 or more than RECORD_MAX_LEN
 */
int record_put(const uint8_t *rec, size_t len, bool sent, uint16_t *id);

/**
 * Sets the sent flag on a record.
 *
 * @param id ID that record_put() returned.
 *
 * @retval 0 on success, or the record is already marked sent
 * @retval -ENOENT the log no longer holds this ID
 * @retval -EIO a read or write failed
 */
int record_mark_sent(uint16_t id);

/**
 * Copies the oldest record that is not marked sent. The record stays in the log.
 *
 * @param[out] rec  Receives the record. Must hold RECORD_MAX_LEN bytes.
 * @param[out] len  Receives the size of the record in bytes.
 * @param[out] id   Receives the record ID. Can be NULL.
 *
 * @retval 0 on success
 * @retval -ENOENT all records are marked sent, or the log is empty
 * @retval -EIO a read failed
 */
int record_oldest_unsent(uint8_t *rec, size_t *len, uint16_t *id);

/**
 * Copies a record out of the log. The record stays in the log.
 *
 * @param[in]  back 0 is the newest record. 1 is the record before it, and so on.
 * @param[out] rec  Receives the record. Must hold RECORD_MAX_LEN bytes.
 * @param[out] len  Receives the size of the record in bytes.
 * @param[out] id   Receives the record ID. Can be NULL.
 * @param[out] sent Receives the sent flag. Can be NULL.
 *
 * @retval 0 on success
 * @retval -ENOENT @p back is not less than record_count()
 * @retval -EIO a read failed
 */
int record_store_peek(uint16_t back, uint8_t *rec, size_t *len, uint16_t *id, bool *sent);

/** @return Number of records in the log. The maximum is CONFIG_HS_RECORD_STORE_SLOTS. */
uint16_t record_count(void);

/** @return Number of records in the log that are not marked sent. */
uint16_t record_unsent(void);

/**
 * @return Number of unsent records that a new record replaced. The value is saved across
 *         resets. record_clear() sets it to 0.
 */
uint32_t record_lost(void);

/** @return Boot counter. record_init() adds 1 to it at each start. Use it as the session ID. */
uint16_t record_session_id(void);

/**
 * Deletes all records and sets the lost counter to 0.
 * The record ID counter and the boot counter do not change.
 *
 * @retval 0 on success
 * @retval -EIO a delete or write failed
 */
int record_clear(void);

/**
 * Test function. Set @p on to true to make record_put() fail with -EIO.
 * Set @p on to false to go back to normal.
 */
void record_inject_fault(bool on);

#endif  // RECORD_H
