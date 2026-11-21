#include "ble/ble_service.h"
#include "ble/packet.h"
#include "fsm/state_machine.h"
#include <errno.h>
#include <zephyr/bluetooth/bluetooth.h>
#include <zephyr/bluetooth/conn.h>
#include <zephyr/bluetooth/gatt.h>
#include <zephyr/bluetooth/hci.h>
#include <zephyr/bluetooth/uuid.h>
#include <zephyr/kernel.h>
#include <zephyr/logging/log.h>
#include <zephyr/settings/settings.h>
#include <zephyr/sys/util.h>

LOG_MODULE_REGISTER(ble, LOG_LEVEL_INF);

/** Largest notification payload that the stack can send: ATT MTU - 3. */
#define FRAG_MAX (CONFIG_BT_L2CAP_TX_MTU - 3)

/** Set static UUIDs TODO: should this be dynamic? */
#define SVC_UUID_VAL    BT_UUID_128_ENCODE(0x3f6c0001, 0x8a4e, 0x4b5d, 0x9c2a, 0x6e1f0b7d4a21)
#define RECORD_UUID_VAL BT_UUID_128_ENCODE(0x3f6c0002, 0x8a4e, 0x4b5d, 0x9c2a, 0x6e1f0b7d4a21)

static void ccc_changed(const struct bt_gatt_attr *attr, uint16_t value);

/* A write to the CCC needs an encrypted link. Thus a central must pair before it subscribes. */
BT_GATT_SERVICE_DEFINE(hydration_svc, BT_GATT_PRIMARY_SERVICE(BT_UUID_DECLARE_128(SVC_UUID_VAL)),
                       BT_GATT_CHARACTERISTIC(BT_UUID_DECLARE_128(RECORD_UUID_VAL),
                                              BT_GATT_CHRC_NOTIFY, BT_GATT_PERM_NONE, NULL, NULL,
                                              NULL),
                       BT_GATT_CCC(ccc_changed,
                                   BT_GATT_PERM_READ_ENCRYPT | BT_GATT_PERM_WRITE_ENCRYPT));

/** Characteristic declaration of the record. The notify and subscribe calls use it. */
#define RECORD_ATTR (&hydration_svc.attrs[1])

static const struct bt_data ad[] = {
    BT_DATA_BYTES(BT_DATA_FLAGS, (BT_LE_AD_GENERAL | BT_LE_AD_NO_BREDR)),
    BT_DATA(BT_DATA_NAME_COMPLETE, CONFIG_BT_DEVICE_NAME, sizeof(CONFIG_BT_DEVICE_NAME) - 1),
};

static const struct bt_data sd[] = {
    BT_DATA_BYTES(BT_DATA_UUID128_ALL, SVC_UUID_VAL),
};

/**
 * @brief Defines the advertising that the FSM asked for.
 *
 * recycled() starts this advertising again when the stack frees a connection object.
 */
typedef enum {
    ADV_OFF,
    ADV_BONDED,
    ADV_PAIRING,
} adv_mode_e;

static struct bt_conn *current_conn;
static volatile adv_mode_e adv_mode;
static volatile bool pairing_open;
static bool ready;

/**
 * @brief Start the advertising that adv_mode selects.
 *
 * For ADV_BONDED, the filter accept list must already hold the bonded peers. For ADV_OFF, the
 * function does nothing.
 */
static void adv_start(void)
{
    int err;

    switch (adv_mode) {
        case ADV_PAIRING:
            err = bt_le_adv_start(BT_LE_ADV_CONN_FAST_1, ad, ARRAY_SIZE(ad), sd, ARRAY_SIZE(sd));
            break;
        case ADV_BONDED:
            err = bt_le_adv_start(BT_LE_ADV_PARAM(BT_LE_ADV_OPT_CONN | BT_LE_ADV_OPT_FILTER_CONN |
                                                      BT_LE_ADV_OPT_FILTER_SCAN_REQ,
                                                  BT_GAP_ADV_FAST_INT_MIN_2,
                                                  BT_GAP_ADV_FAST_INT_MAX_2, NULL),
                                  ad, ARRAY_SIZE(ad), sd, ARRAY_SIZE(sd));
            break;
        default:
            return;
    }
    if (err == -ENOMEM) {
        // The object of the last link is not back in the pool yet. recycled() tries again.
        LOG_INF("Advertising deferred until the link is released");
    } else if (err && err != -EALREADY) {
        LOG_ERR("Advertising start failed (err %d)", err);
    } else {
        LOG_INF("Advertising %s", adv_mode == ADV_PAIRING ? "open for pairing" : "to bonded peers");
    }
}

/**
 * @brief Stop advertising and close the pairing window.
 */
static void stop_adv(void)
{
    adv_mode = ADV_OFF;
    pairing_open = false;
    int err = bt_le_adv_stop();
    if (err) { LOG_ERR("Advertising stop failed (err %d)", err); }
}

/**
 * @brief Add 1 to the bond counter. bt_foreach_bond() calls it for each bond.
 *
 * @param[in]     info       Bond. The function does not use this value.
 * @param[in,out] user_data  Counter, a size_t.
 */
static void count_bond(const struct bt_bond_info *info, void *user_data)
{
    ARG_UNUSED(info);
    (*(size_t *)user_data)++;
}

/**
 * @brief Add a bonded peer to the filter accept list. bt_foreach_bond() calls it for each bond.
 *
 * @param[in] info       Bond to add.
 * @param[in] user_data  The function does not use this value.
 */
static void accept_bond(const struct bt_bond_info *info, void *user_data)
{
    ARG_UNUSED(user_data);
    int err = bt_le_filter_accept_list_add(&info->addr);
    if (err) { LOG_ERR("Accept list add failed (err %d)", err); }
}

/**
 * @brief Count the stored bonds.
 *
 * @return Number of bonds on the default identity.
 */
static size_t bond_count(void)
{
    size_t n = 0;

    bt_foreach_bond(BT_ID_DEFAULT, count_bond, &n);
    return n;
}

/**
 * @brief Post BLE_SUBSCRIBED when the record CCC turns on notifications.
 *
 * @param[in] attr   CCC attribute. The function does not use this value.
 * @param[in] value  New CCC value.
 */
static void ccc_changed(const struct bt_gatt_attr *attr, uint16_t value)
{
    ARG_UNUSED(attr);
    LOG_INF("Notifications %s", (value & BT_GATT_CCC_NOTIFY) ? "on" : "off");
    if (value & BT_GATT_CCC_NOTIFY) { fsm_post(STATE_EVENT_BLE_SUBSCRIBED); }
}

/**
 * @brief Accept a new link and post CONNECTION_COMPLETE.
 *
 * When the pairing window is closed and the peer has no bond, the function disconnects the peer
 * and posts nothing.
 *
 * @param[in] conn  New link.
 * @param[in] err   HCI error code. 0 when the connection succeeded.
 */
static void connected(struct bt_conn *conn, uint8_t err)
{
    if (err) {
        LOG_WRN("Connection failed (err 0x%02x)", err);
        return;
    }
    // A central can connect at the instant that the pairing window closes. This check stops it.
    if (!pairing_open && !bt_le_bond_exists(BT_ID_DEFAULT, bt_conn_get_dst(conn))) {
        LOG_WRN("Unbonded peer outside the pairing window, disconnecting");
        (void)bt_conn_disconnect(conn, BT_HCI_ERR_REMOTE_USER_TERM_CONN);
        return;
    }
    // The stack stopped connectable advertising when the central connected.
    adv_mode = ADV_OFF;
    current_conn = bt_conn_ref(conn);
    LOG_INF("Connected, ATT MTU %u", bt_gatt_get_mtu(conn));
    fsm_post(STATE_EVENT_CONNECTION_COMPLETE);
}

/**
 * @brief Release the current link and post LINK_LOST.
 *
 * For a link that connected() refused, the function posts nothing.
 *
 * @param[in] conn    Link that went down.
 * @param[in] reason  HCI reason code.
 */
static void disconnected(struct bt_conn *conn, uint8_t reason)
{
    LOG_INF("Disconnected (reason 0x%02x)", reason);
    if (conn != current_conn) {
        return;  // The FSM did not get CONNECTION_COMPLETE for this link.
    }
    bt_conn_unref(current_conn);
    current_conn = NULL;
    fsm_post(STATE_EVENT_LINK_LOST);
}

/**
 * @brief Start the advertising again when the stack frees a connection object.
 *
 * A connectable advertising start fails with -ENOMEM until the object of the old link is free.
 */
static void recycled(void)
{
    if (adv_mode != ADV_OFF && !current_conn) { adv_start(); }
}

BT_CONN_CB_DEFINE(conn_cb) = {
    .connected = connected,
    .disconnected = disconnected,
    .recycled = recycled,
};

/**
 * @brief Refuse pairing when the pairing window is closed.
 *
 * @param[in] conn  Link that asks to pair. The function does not use this value.
 * @param[in] feat  Pairing features of the peer. The function does not use this value.
 *
 * @retval BT_SECURITY_ERR_SUCCESS           The pairing window is open.
 * @retval BT_SECURITY_ERR_PAIR_NOT_ALLOWED  The pairing window is closed.
 */
static enum bt_security_err pairing_accept(struct bt_conn *conn,
                                           const struct bt_conn_pairing_feat *const feat)
{
    ARG_UNUSED(conn);
    ARG_UNUSED(feat);
    if (!pairing_open) {
        LOG_WRN("Pairing refused: window closed");
        return BT_SECURITY_ERR_PAIR_NOT_ALLOWED;
    }
    return BT_SECURITY_ERR_SUCCESS;
}

/* No display and no keyboard callbacks: NoInputNoOutput, so LE Secure Connections Just Works. */
static const struct bt_conn_auth_cb auth_cb = {
    .pairing_accept = pairing_accept,
};

/**
 * @brief Log the end of a pairing.
 *
 * @param[in] conn    Link that paired. The function does not use this value.
 * @param[in] bonded  true when the stack stored a new bond.
 */
static void pairing_complete(struct bt_conn *conn, bool bonded)
{
    ARG_UNUSED(conn);
    LOG_INF("Pairing complete, %s", bonded ? "new bond stored" : "not bonded");
}

/**
 * @brief Log a failed pairing.
 *
 * @param[in] conn    Link that failed to pair. The function does not use this value.
 * @param[in] reason  Security error from the stack.
 */
static void pairing_failed(struct bt_conn *conn, enum bt_security_err reason)
{
    ARG_UNUSED(conn);
    LOG_WRN("Pairing failed (%s)", bt_security_err_to_str(reason));
}

static struct bt_conn_auth_info_cb auth_info_cb = {
    .pairing_complete = pairing_complete,
    .pairing_failed = pairing_failed,
};

int ble_init(void)
{
    if (ready) { return 0; }
    int err = bt_enable(NULL);
    if (err && err != -EALREADY) {
        LOG_ERR("bt_enable failed (err %d)", err);
        return err;
    }
    // The record store owns the "hs" subtree. Load only the bonds and the CCC state of the stack.
    err = settings_subsys_init();
    err = err ? err : settings_load_subtree("bt");
    if (err) {
        LOG_ERR("Bond load failed (err %d)", err);
        return err;
    }
    err = bt_conn_auth_cb_register(&auth_cb);
    err = err ? err : bt_conn_auth_info_cb_register(&auth_info_cb);
    if (err) {
        LOG_ERR("Auth callback register failed (err %d)", err);
        return err;
    }
    ready = true;
    LOG_INF("Bluetooth ready: packet v%u, %u B, %zu bonds", PACKET_VERSION, PACKET_SIZE,
            bond_count());
    return 0;
}

void ble_adv_bonded(void)
{
    if (!ready) { return; }
    stop_adv();
    if (bond_count() == 0) {
        return;  // BLE_IDLE without a bond does not advertise.
    }
    int err = bt_le_filter_accept_list_clear();
    if (err) {
        LOG_ERR("Accept list clear failed (err %d)", err);
        return;
    }
    bt_foreach_bond(BT_ID_DEFAULT, accept_bond, NULL);
    adv_mode = ADV_BONDED;
    adv_start();
}

void ble_adv_pairing(void)
{
    if (!ready) { return; }
    stop_adv();
    pairing_open = true;
    adv_mode = ADV_PAIRING;
    adv_start();
}

void ble_adv_stop(void)
{
    if (!ready) { return; }
    stop_adv();
}

void ble_disconnect(void)
{
    if (!current_conn) { return; }
    int err = bt_conn_disconnect(current_conn, BT_HCI_ERR_REMOTE_USER_TERM_CONN);
    if (err) { LOG_ERR("Disconnect failed (err %d)", err); }
}

/**
 * @brief Check if a link can receive packets.
 *
 * The stored CCC of a bonded peer can read as subscribed before the link is encrypted again.
 * Thus the function also checks the security level. Packets go out on an encrypted link only.
 *
 * @param[in] conn  Link to check. Can be NULL.
 *
 * @return true when @p conn is encrypted and has notifications on, and false when it does not.
 */
static bool subscribed(struct bt_conn *conn)
{
    return conn && bt_conn_get_security(conn) >= BT_SECURITY_L2 &&
           bt_gatt_is_subscribed(conn, RECORD_ATTR, BT_GATT_CCC_NOTIFY);
}

bool ble_subscribed(void) { return subscribed(current_conn); }

/**
 * @brief Post TX_DONE when the stack sent the last fragment of a packet.
 *
 * The BT host calls this function. It only posts the event.
 *
 * @param[in] conn       Link. The function does not use this value.
 * @param[in] user_data  The function does not use this value.
 */
static void record_sent(struct bt_conn *conn, void *user_data)
{
    ARG_UNUSED(conn);
    ARG_UNUSED(user_data);
    fsm_post(STATE_EVENT_TX_DONE);
}

int ble_send(const uint8_t *rec, size_t len, uint16_t session_id, uint16_t sequence, size_t *frag)
{
    struct bt_conn *conn = current_conn;
    uint8_t buf[FRAG_MAX];

    if (!conn) { return -ENOTCONN; }
    if (!subscribed(conn)) { return -EACCES; }
    const uint16_t mtu = bt_gatt_get_mtu(conn);
    const size_t payload = MIN((size_t)mtu - 3, sizeof(buf));
    const size_t n = packet_frag_count(len, payload);
    if (n == 0) { return -EMSGSIZE; }

    for (; *frag < n; (*frag)++) {
        struct bt_gatt_notify_params params = {
            .attr = RECORD_ATTR,
            .data = buf,
            .len = packet_frag_build(rec, len, session_id, sequence, (uint8_t)*frag, payload, buf),
            .func = *frag + 1 == n ? record_sent : NULL,
        };
        const int err = bt_gatt_notify_cb(conn, &params);

        if (err == -ENOMEM) { return err; }
        if (err) {
            LOG_ERR("Packet seq %u fragment %zu/%zu failed (err %d)", sequence, *frag + 1, n, err);
            return err;
        }
    }
    LOG_INF("Packet seq %u queued: %zu B in %zu fragments, MTU %u", sequence, len, n, mtu);
    return 0;
}

#ifdef CONFIG_SHELL
#include <zephyr/shell/shell.h>

/**
 * @brief Shell command "hs ble stat". Print the link and bond status.
 *
 * @param[in] sh    Shell that received the command.
 * @param[in] argc  Number of arguments. The function does not use this value.
 * @param[in] argv  Arguments. The function does not use these values.
 *
 * @retval 0  The function printed the status.
 */
static int cmd_stat(const struct shell *sh, size_t argc, char **argv)
{
    ARG_UNUSED(argc);
    ARG_UNUSED(argv);
    struct bt_conn *conn = current_conn;

    shell_print(sh, "connected=%d subscribed=%d bonds=%zu pairing_open=%d mtu=%u", conn != NULL,
                ble_subscribed(), bond_count(), pairing_open, conn ? bt_gatt_get_mtu(conn) : 0);
    return 0;
}

/**
 * @brief Shell command "hs ble unpair". Remove all bonds.
 *
 * When the device advertises to bonded peers, the command also stops the advertising.
 *
 * @param[in] sh    Shell that received the command.
 * @param[in] argc  Number of arguments. The function does not use this value.
 * @param[in] argv  Arguments. The function does not use these values.
 *
 * @retval 0  The function printed the result of bt_unpair().
 */
static int cmd_unpair(const struct shell *sh, size_t argc, char **argv)
{
    ARG_UNUSED(argc);
    ARG_UNUSED(argv);
    int err = bt_unpair(BT_ID_DEFAULT, BT_ADDR_LE_ANY);

    if (adv_mode == ADV_BONDED) { ble_adv_stop(); }
    shell_print(sh, "unpair rc=%d bonds=%zu", err, bond_count());
    return 0;
}

SHELL_STATIC_SUBCMD_SET_CREATE(sub, SHELL_CMD(stat, NULL, "Link and bond status", cmd_stat),
                               SHELL_CMD(unpair, NULL, "Remove all bonds", cmd_unpair),
                               SHELL_SUBCMD_SET_END);
SHELL_SUBCMD_ADD((hs), ble, &sub, "BLE", NULL, 1, 0);
#endif
