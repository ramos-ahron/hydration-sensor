# Firmware

## Setup

1. Clone the repository
2. Open **nRF Connect for Desktop** → **Open an existing application**
3. Go to **Applications** → **Add build configuration**
4. Go to **Actions** → **Flash the device**
5. Go to **Connected devices** → **Connect to serial port** → set baud rate to `115200`

## Shell

The UART console has a shell. Every command starts with `hs`.

| Command                                              | What it does                                                                               |
| ---------------------------------------------------- | ------------------------------------------------------------------------------------------ |
| `hs press <short\|long>`                             | Injects a button press.                                                                    |
| `hs led`                                             | Prints the color the LED is driving right now.                                             |
| `hs batt [<mv>\|auto]`                               | Shows the battery reading, or overrides it.                                                |
| `hs vbus [0\|1\|auto]`                               | Shows charger detection, or overrides it.                                                  |
| `hs store list [n]`, `stat`, `clear`, `fault <0\|1>` | Shows records as hex, shows counters, erases the log, or fails all writes until `fault 0`. |
| `hs ble stat`, `hs ble unpair`                       | Shows the link state, or removes every bond.                                               |
| `hs ctx`                                             | Reads the accelerometer and the temperature sensor.                                        |

## BLE packet

Version 3, 18 bytes, little-endian. `src/ble/packet.h` is the source of truth.

| Offset | Field          | Type | Notes                                       |
| -----: | -------------- | ---- | ------------------------------------------- |
|      0 | `version`      | u8   | 3                                           |
|      1 | `session_id`   | u16  | Boot counter                                |
|      3 | `timestamp_s`  | u32  | Seconds since boot                          |
|      7 | `sequence`     | u16  | Record ID from the store. Wraps at 65536    |
|      9 | `temp_centi_c` | i16  | Fixture temperature in 0.01 C               |
|     11 | `a_rms_mm_s2`  | i16  | Motion in mm/s^2                            |
|     13 | `battery_mv`   | i16  | Battery voltage in mV                       |
|     15 | `reject`       | u8   | Reject reason. 0 means accepted             |
|     16 | `record_crc16` | u16  | CRC-16/CCITT-FALSE over bytes 0 to 15       |

INT16_MIN in an i16 field means that the value is not available.

### Fragments

Each notification carries one fragment: a 6-byte header, then packet bytes. At the default ATT
MTU of 23 bytes, a notification holds 20 bytes. Thus the 18-byte packet goes in 2 fragments: 20 bytes
(14 packet bytes) and 10 bytes (4 packet bytes).

```text
byte   0      1      2      3      4        5        6 ...
     +------+------+------+------+--------+--------+--------------+
     |  session_id |   sequence  | index  | count  | packet bytes |
     +------+------+------+------+--------+--------+--------------+
```

### Decoder

`tools/ble_decode.py` reads the field table and version from `src/ble/packet.h`, so it matches the firmware in the same checkout. Only the live mode needs bleak.

```bash
python firmware/tools/ble_decode.py --selftest
```

```bash
python firmware/tools/ble_decode.py --hex <record hex from "hs store list">
```

```bash
python firmware/tools/ble_decode.py --count 20
```
