# Firmware

## Setup

1. Clone the repository
2. Open **nRF Connect for Desktop** → **Open an existing application**
3. Go to **Applications** → **Add build configuration**
4. Go to **Actions** → **Flash the device**
5. Go to **Connected devices** → **Connect to serial port** → set baud rate to `115200`

## Shell

The UART console has a shell. Every command starts with `hs`.

| Command                  | What it does                                   |
| ------------------------ | ---------------------------------------------- |
| `hs press <short\|long>` | Injects a button press.                        |
| `hs led`                 | Prints the color the LED is driving right now. |
| `hs batt [<mv>\|auto]`   | Shows the battery reading, or overrides it.    |
| `hs vbus [0\|1\|auto]`   | Shows charger detection, or overrides it.      |
