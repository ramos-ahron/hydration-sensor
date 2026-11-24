#include "ble/packet.h"
#include <errno.h>
#include <string.h>
#include <zephyr/sys/byteorder.h>
#include <zephyr/sys/crc.h>
#include <zephyr/sys/util.h>

BUILD_ASSERT(PACKET_SIZE == 17, "BLE packet version 2 is 17 bytes");

/* Byte offset of the CRC. It is the last field, so the CRC covers bytes 0 to PACKET_CRC_OFF - 1. */
#define PACKET_CRC_OFF (PACKET_SIZE - PACKET_WIRE_SIZE_u16)

/* Write and read one field. The ##type in each X picks the macro by wire type. */
#define PUT_u8(p, v)  (*(p) = (v))
#define PUT_u16(p, v) sys_put_le16((v), (p))
#define PUT_i16(p, v) sys_put_le16((uint16_t)(v), (p))
#define PUT_u32(p, v) sys_put_le32((v), (p))

#define GET_u8(p)  (*(p))
#define GET_u16(p) sys_get_le16(p)
#define GET_i16(p) ((int16_t)sys_get_le16(p))
#define GET_u32(p) sys_get_le32(p)

/*
 * One step of packet_serialize() for each row of PACKET_FIELDS. For the session_id row it becomes:
 *
 *   sys_put_le16((pkt->session_id), (p));
 *   p += 2;
 */
#define SERIALIZE_FIELD(name, type, scale, unit)                                                   \
    PUT_##type(p, pkt->name);                                                                      \
    p += PACKET_WIRE_SIZE_##type;

/* One step of packet_parse() for each row of PACKET_FIELDS. The reverse of SERIALIZE_FIELD. */
#define PARSE_FIELD(name, type, scale, unit)                                                       \
    pkt->name = GET_##type(p);                                                                     \
    p += PACKET_WIRE_SIZE_##type;

void packet_init(struct packet *pkt, uint16_t session_id, uint32_t timestamp_s, uint16_t sequence)
{
    memset(pkt, 0, sizeof(*pkt));
    pkt->version = PACKET_VERSION;
    pkt->session_id = session_id;
    pkt->timestamp_s = timestamp_s;
    pkt->sequence = sequence;
}

size_t packet_serialize(const struct packet *pkt, uint8_t out[PACKET_SIZE])
{
    uint8_t *p = out;

    PACKET_FIELDS(SERIALIZE_FIELD)

    // The table wrote pkt->record_crc16 into the last 2 bytes. Write the real CRC over it.
    sys_put_le16(crc16_itu_t(PACKET_CRC_INIT, out, PACKET_CRC_OFF), &out[PACKET_CRC_OFF]);
    return PACKET_SIZE;
}

int packet_parse(const uint8_t *buf, size_t len, struct packet *pkt)
{
    const uint8_t *p = buf;

    if (len != PACKET_SIZE) {
        return -EMSGSIZE;
    }
    if (crc16_itu_t(PACKET_CRC_INIT, buf, PACKET_CRC_OFF) != sys_get_le16(&buf[PACKET_CRC_OFF])) {
        return -EBADMSG;
    }

    PACKET_FIELDS(PARSE_FIELD)
    return 0;
}

size_t packet_frag_count(size_t pkt_len, size_t att_payload)
{
    if (att_payload <= PACKET_FRAG_HDR_LEN) {
        return 0;
    }

    const size_t chunk = att_payload - PACKET_FRAG_HDR_LEN;
    const size_t n = DIV_ROUND_UP(pkt_len, chunk);

    // The header stores the count in one byte.
    return n > UINT8_MAX ? 0 : n;
}

size_t packet_frag_build(const uint8_t *pkt, size_t pkt_len, uint16_t session_id, uint16_t sequence,
                         uint8_t index, size_t att_payload, uint8_t *out)
{
    const size_t count = packet_frag_count(pkt_len, att_payload);

    if (index >= count) {
        return 0;
    }

    const size_t chunk = att_payload - PACKET_FRAG_HDR_LEN;
    const size_t off = (size_t)index * chunk;
    const size_t n = MIN(chunk, pkt_len - off);

    sys_put_le16(session_id, &out[0]);
    sys_put_le16(sequence, &out[2]);
    out[4] = index;
    out[5] = (uint8_t)count;
    memcpy(&out[PACKET_FRAG_HDR_LEN], &pkt[off], n);
    return PACKET_FRAG_HDR_LEN + n;
}
