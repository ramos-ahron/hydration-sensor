#ifndef PACKET_H
#define PACKET_H

/**
 * @file
 * @brief BLE packet: field table, serializer, parser, CRC and fragments.
 *
 * Data path:
 *
 *   sensor                                                        central
 *   ------                                                    ---------------
 *   packet_init()           fill struct packet
 *         |
 *   packet_serialize()     struct -> 17 bytes + CRC
 *         |
 *   packet_frag_build()    17 bytes -> 1..n fragments  ==BLE==>  join the fragments
 *                                                                  |
 *                                                              packet_parse()
 *                                                              check length and CRC,
 *                                                              bytes -> struct
 *
 * Packet layout (version 2, 17 bytes, little-endian):
 *
 *   byte   0      1      2      3      4      5      6      7      8
 *        +------+------+------+------+------+------+------+------+------+
 *        | ver  |  session_id |        timestamp_s        |  sequence   |
 *        +------+------+------+------+------+------+------+------+------+
 *         u8     u16 LSB first u32 LSB first               u16
 *
 *   byte   9      10     11     12     13     14     15     16
 *        +------+------+------+------+------+------+------+------+
 *        | temp_centi_c| a_rms_mm_s2 | battery_mv  | record_crc16|
 *        +------+------+------+------+------+------+------+------+
 *         i16           i16           i16           u16
 *
 *   The CRC covers bytes 0 to 14.
 *
 *   Field          Source
 *   ------------   --------------------------------------------------------------
 *   version        PACKET_VERSION
 *   session_id     record_session_id(). The boot counter.
 *   timestamp_s    Seconds since boot. There is no wall clock.
 *   sequence       Record ID from record_put(). It adds 1 per record and wraps at 65536.
 *   temp_centi_c   get_temp() in 0.01 C.
 *   a_rms_mm_s2    get_motion() in mm/s^2, limited to INT16_MAX.
 *   battery_mv     battery_mv() in mV.
 *   record_crc16   packet_serialize() calculates it.
 *
 * INT16_MIN in an i16 field means that the value is not available.
 *
 * (session_id, timestamp_s) puts packets in order across boots.
 *
 * Example: version 2, session 0x0708, timestamp 0x090A0B0C, sequence 0x0D0E, temp 0x1112,
 * a_rms 0x1314, battery 0x1516
 *
 *   02  08 07  0C 0B 0A 09  0E 0D  12 11  14 13  16 15  <crc LSB> <crc MSB>
 *
 * Fields go on the wire one at a time, in table order. The firmware never sends
 * struct packet as is, because the compiler can add padding between its members.
 */

#include <stddef.h>
#include <stdint.h>

// Packet format version. Increase it each time PACKET_FIELDS changes.
#define PACKET_VERSION 2

/**
 * @brief Packet field table.
 *
 * Each row is X(name, wire type, scale, unit). The order of the rows is the order on the wire.
 *
 *   Column      Meaning
 *   ---------   -----------------------------------------------------------------
 *   name        Member name in struct packet.
 *   wire type   u8, u16, i16 or u32. Sets the size in bytes and the C type.
 *   scale       Real value = raw integer * scale. A host decoder uses it.
 *   unit        Unit of the real value. A host decoder uses it.
 *
 * Each use of the table gives its own X. The macros below use it to make the struct members and
 * PACKET_SIZE. packet.c uses it to make the serializer and the parser.
 *
 * To change the packet, edit this table, keep one row on each line, and increase
 * PACKET_VERSION. Keep record_crc16 as the last row.
 */
#define PACKET_FIELDS(X)                                                                           \
    X(version, u8, 1, "")                                                                          \
    X(session_id, u16, 1, "")                                                                      \
    X(timestamp_s, u32, 1, "s")                                                                    \
    X(sequence, u16, 1, "")                                                                        \
    X(temp_centi_c, i16, 0.01, "C")                                                                \
    X(a_rms_mm_s2, i16, 0.001, "m/s^2")                                                            \
    X(battery_mv, i16, 1, "mV")                                                                    \
    X(record_crc16, u16, 1, "")

#define PACKET_WIRE_SIZE_u8  1
#define PACKET_WIRE_SIZE_u16 2
#define PACKET_WIRE_SIZE_i16 2
#define PACKET_WIRE_SIZE_u32 4
#define PACKET_CTYPE_u8      uint8_t
#define PACKET_CTYPE_u16     uint16_t
#define PACKET_CTYPE_i16     int16_t
#define PACKET_CTYPE_u32     uint32_t

#define PACKET_FIELD_SIZE(name, type, scale, unit)   +PACKET_WIRE_SIZE_##type
#define PACKET_FIELD_MEMBER(name, type, scale, unit) PACKET_CTYPE_##type name;

#define PACKET_SIZE     (0 PACKET_FIELDS(PACKET_FIELD_SIZE))
#define PACKET_CRC_INIT 0xFFFFu

struct packet {
    PACKET_FIELDS(PACKET_FIELD_MEMBER)
};

/**
 * @brief Fills a packet.
 *
 * The function sets version to PACKET_VERSION and the other fields to 0. packet_serialize()
 * calculates the CRC. The caller sets the sensor fields.
 *
 * @param[out] pkt          Packet to fill.
 * @param[in]  session_id   Boot counter from record_session_id().
 * @param[in]  timestamp_s  Seconds since boot.
 * @param[in]  sequence     Record ID from record_put().
 */
void packet_init(struct packet *pkt, uint16_t session_id, uint32_t timestamp_s, uint16_t sequence);

/**
 * @brief Writes a packet to a byte buffer.
 *
 * The function writes the fields in table order, little-endian. Then it calculates the CRC over
 * bytes 0 to PACKET_SIZE - 3 and writes it into the last 2 bytes. The function ignores the
 * record_crc16 value in @p pkt.
 *
 * @param[in]  pkt  Packet to write.
 * @param[out] out  Receives the bytes. Must hold PACKET_SIZE bytes.
 *
 * @return PACKET_SIZE.
 */
size_t packet_serialize(const struct packet *pkt, uint8_t out[PACKET_SIZE]);

/**
 * @brief Reads a packet from a byte buffer.
 *
 * The function checks the length, then the CRC, then reads the fields. If a check fails,
 * @p pkt does not change.
 *
 * @param[in]  buf  Packet bytes, after the fragments are joined.
 * @param[in]  len  Size of @p buf in bytes.
 * @param[out] pkt  Receives the fields.
 *
 * @retval 0          on success
 * @retval -EMSGSIZE  @p len is not PACKET_SIZE
 * @retval -EBADMSG   the CRC does not match
 */
int packet_parse(const uint8_t *buf, size_t len, struct packet *pkt);

/**
 * @brief Size of the fragment header in bytes.
 *
 * Each BLE notification carries one fragment. Each fragment has this header, also when the
 * packet fits in one fragment.
 *
 * The receiver joins the fragments with the same (session_id, sequence) in index order.
 */
#define PACKET_FRAG_HDR_LEN 6

/**
 * @brief Calculates the number of fragments for a packet.
 *
 * Each fragment carries att_payload - PACKET_FRAG_HDR_LEN packet bytes.
 *
 * @param[in] pkt_len      Size of the serialized packet in bytes.
 * @param[in] att_payload  Largest notification size in bytes. This is ATT MTU - 3.
 *
 * @return Number of fragments.
 * @retval 0  @p att_payload cannot hold the header and 1 packet byte, or the packet needs more
 *            than 255 fragments.
 */
size_t packet_frag_count(size_t pkt_len, size_t att_payload);

/**
 * @brief Builds one fragment.
 *
 * The function writes the header and then the packet bytes for fragment @p index.
 *
 * @param[in]  pkt          Serialized packet.
 * @param[in]  pkt_len      Size of @p pkt in bytes.
 * @param[in]  session_id   Session ID for the header. Use the value in the packet.
 * @param[in]  sequence     Sequence for the header. Use the value in the packet.
 * @param[in]  index        Fragment number. 0 is the first fragment.
 * @param[in]  att_payload  Largest notification size in bytes. This is ATT MTU - 3.
 * @param[out] out          Receives the fragment. Must hold @p att_payload bytes.
 *
 * @return Number of bytes written: the header and the packet bytes.
 * @retval 0  @p index is not less than packet_frag_count(). @p out does not change.
 */
size_t packet_frag_build(const uint8_t *pkt, size_t pkt_len, uint16_t session_id, uint16_t sequence,
                         uint8_t index, size_t att_payload, uint8_t *out);

#endif  // PACKET_H
