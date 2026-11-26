# Firmware

## Setup

1. Clone the repository
2. Open **nRF Connect for Desktop** → **Open an existing application**
3. Go to **Applications** → **Add build configuration**
4. Go to **Actions** → **Flash the device**.
   Do not use **Erase and flash**.
   A plain flash keeps the bonds and the stored records.
   An erase removes both, and then every bonded laptop must pair again (see [Bonds](#bonds)).
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

## Bonds

The board pairs only while the pairing window is open (long press, blue SLOW).
Pairing uses Just Works, so there is no PIN.

After pairing, the board and the laptop each keep a bond.
The board stores its bonds in the settings partition at the end of flash.
A plain flash and a reset keep that partition.

If one side loses its bond, the two sides no longer share a key:

| Side that lost the bond                 | Symptom                                                                        | Recovery                                                                                                      |
| --------------------------------------- | ------------------------------------------------------------------------------ | ------------------------------------------------------------------------------------------------------------- |
| Board (erase, `hs ble unpair`)          | The decoder reports `connect failed (Could not start notify ... Unreachable)`. | The decoder removes the laptop bond. Long-press again, because the failed connection closed the first window. |
| Laptop ("Hydration" removed in Windows) | The board logs `Pairing refused: window closed`.                               | Long-press. The board replaces its old bond for that laptop (`CONFIG_BT_SMP_ALLOW_UNAUTH_OVERWRITE`).         |

The record CCC is the setting that turns on notifications for the record characteristic.
The board sends records only after the central writes the record CCC on the current link.
A CCC that the board restores from the bond does not count.
Thus a central must subscribe again on every connection.

Notifications have no acknowledgement.
The board marks a record as sent when the BLE stack sends it.
If no app on the central listens at that time, the record is lost.

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

### Live test with a laptop as the central

The laptop connects, pairs, subscribes, and prints each record that the board sends.

1. Wake the board and open the pairing window: `hs press short`, then `hs press long` (or a long press on the button).
   The board logs `Advertising open for pairing`.
   The window stays open for `CONFIG_PAIRING_TIMEOUT_S` (default 60 s).
2. In that window, start the decoder from `firmware/tools`:

   ```bash
   uv run ble_decode.py --count 5
   ```

   The decoder prints `connected, MTU <n>`, and the board logs `Pairing complete, new bond stored` and `Notifications on`.
3. Send `hs press short` once for each record.
   The decoder prints one `#<n>` line per record.
   `hs store list` on the board shows the same hex, with `sent=1`.

Notes:

- When the board is not pairing or measuring, it goes to sleep after `SLEEP_GUARD_MS` (10 s) and drops the link.
  Send the next press before that.
- Do not send `hs press short` while the pairing window is open and nothing is connected.
  It closes the window.
- If the decoder reports `connect failed (Could not start notify ... Unreachable)`, the board lost its bond (see [Bonds](#bonds)).
  The decoder removes the laptop bond and tries again for 60 s.
  Long-press again in that time, because the failed connection closed the first window.
  If the board connects but does not pair, long-press again.
  If that also fails, remove "Hydration" in the Windows Bluetooth settings, run `hs ble unpair`, and start again.
- The board keeps the bond.
  Run `hs ble unpair` to remove it.
- At the default MTU of 23 a record takes 2 fragments.
  Windows usually negotiates a larger MTU (65 in testing), so the 18-byte record goes in 1 fragment.
