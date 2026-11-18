#ifndef BLE_SERVICE_H
#define BLE_SERVICE_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

/**
 * @brief Enable Bluetooth, load the bonds and register the callbacks.
 *
 * The function does this work one time. Later calls return 0 and do nothing.
 *
 * @return 0 on success, or the negative error code from bt_enable(), the settings load or a
 *         callback registration.
 */
int ble_init(void);

/**
 * @brief Advertise to bonded peers only (BLE_IDLE).
 *
 * The function stops the current advertising and closes the pairing window. Then it fills the
 * filter accept list with the bonded peers and starts advertising. When no bond is stored, the
 * function does not advertise. Before ble_init() succeeds, the function does nothing.
 */
void ble_adv_bonded(void);

/**
 * @brief Open the pairing window and advertise to all peers (PAIRING).
 *
 * The function stops the current advertising first. A new peer can connect and bond until the
 * next call to ble_adv_bonded() or ble_adv_stop(). Before ble_init() succeeds, the function does
 * nothing.
 */
void ble_adv_pairing(void);

/**
 * @brief Stop advertising and close the pairing window.
 *
 * Before ble_init() succeeds, the function does nothing.
 */
void ble_adv_stop(void);

/**
 * @brief Disconnect the current link.
 *
 * When there is no link, the function does nothing. The module posts LINK_LOST when the link is
 * down.
 */
void ble_disconnect(void);

/**
 * @brief Check if a central is connected.
 *
 * @return true from CONNECTION_COMPLETE until LINK_LOST, and false at other times.
 */
bool ble_is_connected(void);

/**
 * @brief Check if the connected peer can receive packets.
 *
 * @return true when the link is encrypted and the peer turned on notifications, and false when
 *         it is not.
 */
bool ble_subscribed(void);

/**
 * @brief Check if a bond is stored.
 *
 * @return true when at least one bond is stored, and false when no bond is stored.
 */
bool ble_has_bond(void);

/**
 * @brief Queue one serialized packet as notifications.
 *
 * The function cuts the packet into fragments that fit the current ATT MTU.
 * It queues the fragments, starting at fragment @p *frag. When the stack
 * reports the last fragment as sent, the module posts TRANSMIT_DONE. When the link goes down
 * first, the module posts nothing. At the default ATT MTU of 23 bytes, a PACKET_SIZE packet is
 * one fragment.
 *
 * On the system work queue, the stack does not wait for a free TX buffer, and the function
 * returns -ENOMEM. On other threads, such as the FSM thread, the stack waits for a free buffer.
 *
 * @param[in]     rec         Serialized packet.
 * @param[in]     len         Size of @p rec in bytes.
 * @param[in]     session_id  Session ID for the fragment header. Use the value in the packet.
 * @param[in]     sequence    Sequence for the fragment header. Use the value in the packet.
 * @param[in,out] frag        In: first fragment to queue. Use 0 for a new packet. Out: next
 *                            fragment to queue.
 *
 * @retval 0          All fragments are queued.
 * @retval -ENOTCONN  There is no link.
 * @retval -EACCES    The link is not encrypted, or notifications are off.
 * @retval -EMSGSIZE  The current ATT MTU cannot carry the packet in 255 fragments or fewer.
 * @retval -ENOMEM    No TX buffer is free. Call again later with the same @p frag.
 * @retval <0         Other error from bt_gatt_notify_cb().
 */
int ble_send(const uint8_t *rec, size_t len, uint16_t session_id, uint16_t sequence, size_t *frag);

#endif  // BLE_SERVICE_H
