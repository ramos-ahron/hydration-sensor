#include "ble/packet.h"
#include <errno.h>
#include <string.h>
#include <zephyr/sys/byteorder.h>
#include <zephyr/sys/crc.h>
#include <zephyr/ztest.h>

ZTEST_SUITE(packet, NULL, NULL, NULL, NULL, NULL);

/**
 * @brief Fills a packet with fixed test values.
 *
 * @param[out] pkt  Receives the packet. The session ID is 0x0506, the timestamp is 0x090A0B0C,
 *                  the sequence is 0x0708, the temperature is -1234, a_rms is INT16_MIN, and the
 *                  battery voltage is 3000.
 */
static void mk_pkt(struct packet *pkt)
{
    packet_init(pkt, 0x0506, 0x090A0B0C, 0x0708);
    pkt->temp_centi_c = -1234;
    pkt->a_rms_mm_s2 = INT16_MIN;
    pkt->battery_mv = 3000;
}

ZTEST(packet, test_size_and_crc_algo)
{
    zassert_equal(PACKET_SIZE, 17);
    // Standard check value for CRC-16/CCITT-FALSE.
    zassert_equal(crc16_itu_t(PACKET_CRC_INIT, (const uint8_t *)"123456789", 9), 0x29B1);
}

ZTEST(packet, test_init)
{
    struct packet a;

    mk_pkt(&a);
    zassert_equal(a.version, PACKET_VERSION);
    zassert_equal(a.session_id, 0x0506);
    zassert_equal(a.timestamp_s, 0x090A0B0C);
    zassert_equal(a.sequence, 0x0708);
    zassert_equal(a.temp_centi_c, -1234);
    zassert_equal(a.a_rms_mm_s2, INT16_MIN);
    zassert_equal(a.battery_mv, 3000);
    zassert_equal(a.record_crc16, 0);
}

ZTEST(packet, test_round_trip)
{
    struct packet a, b;
    uint8_t buf[PACKET_SIZE];

    mk_pkt(&a);
    zassert_equal(packet_serialize(&a, buf), PACKET_SIZE);
    zassert_ok(packet_parse(buf, sizeof(buf), &b));
    a.record_crc16 = b.record_crc16;
#define CHECK(name, type, scale, unit) zassert_equal(a.name, b.name, #name);
    PACKET_FIELDS(CHECK)
#undef CHECK
    zassert_equal(b.record_crc16, crc16_itu_t(PACKET_CRC_INIT, buf, PACKET_SIZE - 2));
}

ZTEST(packet, test_golden_offsets)
{
    const struct packet pkt = {
        .version = 2,
        .session_id = 0x0708,
        .timestamp_s = 0x090A0B0C,
        .sequence = 0x0D0E,
        .temp_centi_c = 0x1112,
        .a_rms_mm_s2 = 0x1314,
        .battery_mv = 0x1516,
    };
    const uint8_t want[PACKET_SIZE - 2] = { 0x02, 0x08, 0x07, 0x0C, 0x0B, 0x0A, 0x09, 0x0E,
                                            0x0D, 0x12, 0x11, 0x14, 0x13, 0x16, 0x15 };
    uint8_t b[PACKET_SIZE];

    packet_serialize(&pkt, b);
    zassert_mem_equal(b, want, sizeof(want));
    zassert_equal(sys_get_le16(&b[15]), crc16_itu_t(PACKET_CRC_INIT, b, 15));
}

ZTEST(packet, test_parse_rejects)
{
    struct packet a, b;
    uint8_t buf[PACKET_SIZE];

    mk_pkt(&a);
    packet_serialize(&a, buf);
    memset(&b, 0xA5, sizeof(b));
    zassert_equal(packet_parse(buf, PACKET_SIZE - 1, &b), -EMSGSIZE);
    zassert_equal(packet_parse(buf, PACKET_SIZE + 1, &b), -EMSGSIZE);
    buf[4] ^= 0x04;
    zassert_equal(packet_parse(buf, PACKET_SIZE, &b), -EBADMSG);
    zassert_equal(b.version, 0xA5, "pkt changed on a failed parse");
}

/**
 * @brief Builds every fragment, checks each header, and joins the payloads.
 *
 * @param[in]  pkt  Serialized packet.
 * @param[in]  len  Size of @p pkt in bytes.
 * @param[in]  att  ATT payload size for each fragment, in bytes.
 * @param[out] out  Receives the joined payloads. Must hold @p len bytes.
 *
 * @return Number of payload bytes in @p out.
 */
static size_t reassemble(const uint8_t *pkt, size_t len, size_t att, uint8_t *out)
{
    uint8_t f[244];
    size_t n = packet_frag_count(len, att), total = 0;

    for (size_t i = 0; i < n; i++) {
        size_t w = packet_frag_build(pkt, len, 0xBEEF, 0x1234, i, att, f);

        zassert_equal(sys_get_le16(&f[0]), 0xBEEF);
        zassert_equal(sys_get_le16(&f[2]), 0x1234);
        zassert_equal(f[4], i);
        zassert_equal(f[5], n);
        zassert_true(w <= att);
        if (i + 1 < n) { zassert_equal(w, att); }
        memcpy(&out[total], &f[PACKET_FRAG_HDR_LEN], w - PACKET_FRAG_HDR_LEN);
        total += w - PACKET_FRAG_HDR_LEN;
    }
    return total;
}

ZTEST(packet, test_fragments)
{
    struct packet a;
    uint8_t pkt[PACKET_SIZE], out[PACKET_SIZE + 20], f[244];

    mk_pkt(&a);
    packet_serialize(&a, pkt);

    // Default ATT MTU: 2 fragments of 14 and 3 packet bytes.
    zassert_equal(packet_frag_count(PACKET_SIZE, 20), 2);
    zassert_equal(packet_frag_build(pkt, PACKET_SIZE, 1, 2, 0, 20, f), 20);
    zassert_equal(packet_frag_build(pkt, PACKET_SIZE, 1, 2, 1, 20, f), 9);
    zassert_equal(packet_frag_build(pkt, PACKET_SIZE, 1, 2, 2, 20, f), 0);
    zassert_equal(reassemble(pkt, PACKET_SIZE, 20, out), PACKET_SIZE);
    zassert_mem_equal(out, pkt, PACKET_SIZE);

    // 6 packet bytes per fragment: 3 fragments of 6, 6 and 5 bytes.
    zassert_equal(packet_frag_count(PACKET_SIZE, 12), 3);
    zassert_equal(packet_frag_build(pkt, PACKET_SIZE, 1, 2, 0, 12, f), 12);
    zassert_equal(packet_frag_build(pkt, PACKET_SIZE, 1, 2, 1, 12, f), 12);
    zassert_equal(packet_frag_build(pkt, PACKET_SIZE, 1, 2, 2, 12, f), 11);
    zassert_equal(packet_frag_build(pkt, PACKET_SIZE, 1, 2, 3, 12, f), 0);
    zassert_equal(reassemble(pkt, PACKET_SIZE, 12, out), PACKET_SIZE);
    zassert_mem_equal(out, pkt, PACKET_SIZE);

    zassert_equal(packet_frag_count(PACKET_SIZE, PACKET_FRAG_HDR_LEN), 0);
    zassert_equal(packet_frag_count(PACKET_SIZE, 0), 0);
    zassert_equal(packet_frag_build(pkt, PACKET_SIZE, 1, 2, 0, PACKET_FRAG_HDR_LEN, f), 0);
    zassert_equal(packet_frag_count(300, 7), 0);  // 300 fragments > 255
}
