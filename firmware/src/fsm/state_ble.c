#include "fsm/state_ble.h"
#include "ble/ble_service.h"
#include "ble/packet.h"
#include "common/enum_to_string.h"
#include "fsm/state_machine.h"
#include "storage/record.h"
#include "ui/led.h"
#include <zephyr/kernel.h>
#include <zephyr/logging/log.h>
#include <zephyr/sys/__assert.h>

LOG_MODULE_DECLARE(fsm, LOG_LEVEL_INF);

struct ble_transition {
    ble_state_e from;
    state_event_e event;
    ble_state_e to;
};

static const struct ble_transition ble_transitions[] = {
    { BLE_STATE_BLE_IDLE, STATE_EVENT_BUTTON_LONG, BLE_STATE_PAIRING },
    { BLE_STATE_BLE_IDLE, STATE_EVENT_CONNECTION_COMPLETE, BLE_STATE_CONNECTED },
    { BLE_STATE_PAIRING, STATE_EVENT_BUTTON_SHORT, BLE_STATE_BLE_IDLE },
    { BLE_STATE_PAIRING, STATE_EVENT_CONNECTION_COMPLETE, BLE_STATE_CONNECTED },
    { BLE_STATE_PAIRING, STATE_EVENT_PAIRING_TIMEOUT, BLE_STATE_BLE_IDLE },
    { BLE_STATE_CONNECTED, STATE_EVENT_BUTTON_LONG, BLE_STATE_BLE_IDLE },
    // TODO: Is disconnected needed
    { BLE_STATE_CONNECTED, STATE_EVENT_LINK_LOST, BLE_STATE_DISCONNECTED },
    { BLE_STATE_DISCONNECTED, STATE_EVENT_BLE_IDLE, BLE_STATE_BLE_IDLE },
};

static ble_state_e ble_state;

#define TX_RETRY_MS 10

static bool sync_sending;
static bool tx_stalled;
static uint8_t tx_buf[RECORD_MAX_LEN];
static size_t tx_len;
static size_t tx_frag;
static uint16_t tx_id;
static struct ble_record tx_rec;

/**
 * @brief Post TX_KICK to the state machine when the tx_retry delay expires.
 *
 * The system work queue calls this function. It only posts the event.
 *
 * @param[in] work  Work item, UNUSED.
 */
static void tx_retry_fn(struct k_work *work)
{
    ARG_UNUSED(work);
    fsm_post(STATE_EVENT_TX_KICK);
}
static K_WORK_DELAYABLE_DEFINE(tx_retry, tx_retry_fn);

/**
 * @brief Clear the send state and cancel a pending retry.
 *
 * The function sets sync_sending and tx_stalled to false and cancels tx_retry.
 * If tx_retry_fn() is already running, it still posts TX_KICK.
 */
static void sync_reset(void)
{
    sync_sending = false;
    tx_stalled = false;
    (void)k_work_cancel_delayable(&tx_retry);
}

/**
 * @brief Enter a BLE substate.
 *
 * The function asserts that the transition is legal. Then it stores @p to as the current BLE
 * state.
 *
 * @param[in] from  State that the BLE region leaves. Only the assertions use this value.
 * @param[in] to    State that the BLE region enters.
 */
static void ble_enter(ble_state_e from, ble_state_e to)
{
    ARG_UNUSED(from);

    fsm_timer_stop(FSM_TIMER_PAIRING);
    led_clear(LED_PRIO_BLE);
    sync_reset();

    switch (to) {
        case BLE_STATE_BLE_IDLE:
            __ASSERT(from == BLE_STATE_BLE_IDLE || from == BLE_STATE_PAIRING ||
                         from == BLE_STATE_CONNECTED || from == BLE_STATE_DISCONNECTED,
                     "BLE_IDLE entered from %d", from);
            ble_disconnect();
            ble_adv_bonded();
            break;
        case BLE_STATE_PAIRING:
            __ASSERT(from == BLE_STATE_BLE_IDLE, "PAIRING entered from %d", from);
            ble_adv_pairing();
            led_request(LED_PRIO_BLE, LED_BLUE, LED_PAT_SLOW, 0, 0);
            fsm_timer_start(FSM_TIMER_PAIRING, STATE_EVENT_PAIRING_TIMEOUT,
                            CONFIG_PAIRING_TIMEOUT_S * MSEC_PER_SEC);
            break;
        case BLE_STATE_CONNECTED:
            __ASSERT(from == BLE_STATE_PAIRING || from == BLE_STATE_BLE_IDLE,
                     "CONNECTED entered from %d", from);
            led_request(LED_PRIO_BLE, LED_BLUE, LED_PAT_TRIPLE, 0, 0);
            fsm_post(STATE_EVENT_TX_KICK);
            break;
        case BLE_STATE_DISCONNECTED:
            __ASSERT(from == BLE_STATE_CONNECTED, "DISCONNECTED entered from %d", from);
            fsm_post(STATE_EVENT_BLE_IDLE);
            break;
    }
    ble_state = to;
}

/**
 * @brief Queue the fragments of the record in flight.
 *
 * The function sends from fragment tx_frag. If ble_send() returns -ENOMEM, the function sets
 * tx_stalled and posts TX_KICK again after TX_RETRY_MS. If ble_send() returns a different error,
 * the function clears sync_sending. The next TX_KICK then starts again from the oldest unsent
 * record.
 */
static void send_frags(void)
{
    const int err = ble_send(tx_buf, tx_len, tx_rec.session_id, tx_rec.sequence, &tx_frag);

    tx_stalled = err == -ENOMEM;
    if (tx_stalled) {
        (void)k_work_reschedule(&tx_retry, K_MSEC(TX_RETRY_MS));
    } else if (err) {
        sync_sending =
            false;  // No link, not subscribed, or a send error. The next kick tries again.
    }
}

/**
 * @brief Send the oldest unsent record (capstone 4.5.1 "Local copy").
 *
 * Only one record is in flight at a time. When the log has no unsent record, or the record does
 * not parse, the function clears sync_sending and sends nothing. The record stays unsent in the
 * log until the TRANSMIT_DONE handler marks it sent. Thus if the link goes down, the record is
 * sent again after the next connection.
 */
static void send_next_unsent(void)
{
    sync_sending = record_oldest_unsent(tx_buf, &tx_len, &tx_id) == 0 &&
                   packet_parse(tx_buf, tx_len, &tx_rec) == 0;
    if (sync_sending) {
        tx_frag = 0;
        send_frags();
    }
}

bool state_ble_connected(void) { return ble_state == BLE_STATE_CONNECTED; }

bool state_ble_pairing(void) { return ble_state == BLE_STATE_PAIRING; }

bool state_ble_process_event(state_event_e event, bool measurement_busy)
{
    // Ensure measurement is not busy before transitioning
    if (event == STATE_EVENT_BUTTON_LONG && measurement_busy) { return false; }

    if (ble_state == BLE_STATE_CONNECTED) {
        if ((event == STATE_EVENT_TX_KICK || event == STATE_EVENT_BLE_SUBSCRIBED) &&
            !sync_sending) {
            send_next_unsent();
            return true;
        }
        if (event == STATE_EVENT_TX_KICK && tx_stalled) {
            send_frags();
            return true;
        }
        if (event == STATE_EVENT_TX_DONE && sync_sending && !tx_stalled) {
            if (record_mark_sent(tx_id) != 0) { LOG_WRN("record %u gone before marked", tx_id); }
            send_next_unsent();
            return true;
        }
    }

    for (size_t i = 0; i < ARRAY_SIZE(ble_transitions); i++) {
        const struct ble_transition *t = &ble_transitions[i];
        if (t->from == ble_state && t->event == event) {
            LOG_INF("%s -> %s (%s)", ble_state_to_string(t->from), ble_state_to_string(t->to),
                    state_event_to_string(event));
            ble_enter(t->from, t->to);
            return true;
        }
    }
    return false;
}

void state_ble_run(void)
{
    ble_state = BLE_STATE_BLE_IDLE;
    sync_reset();
    ble_disconnect();
    ble_adv_bonded();
    LOG_INF("BLE start -> %s", ble_state_to_string(ble_state));
}
