# Bring-up Notes

A running log of hardware testing. Add new entries at the bottom.

## Bench setup

**Power (interface board J1):** a USB cable with the plug end cut off. The red (+5 V) and black (GND) wires are twisted onto the J1 lead and taped; the white data wire is unused. It plugs into a USB-A phone charger.

**Debug (interface board J4):** a USB-to-TTL adapter, 3 wires only:

| Adapter | J4 |
|---|---|
| GND | pin 1 GND |
| TXD | pin 4 UART_RX |
| RXD | pin 3 UART_TX |

J4 pin 2 (+3V3) is left unconnected, because the board makes its own 3.3 V from J1. Connecting the adapter's 3V3 there would put two supplies on the same rail.

**Relay module:** ST-Link V2 on the SWD header.

**Tools:** Fluke 101 multimeter, F-F jumpers, 868 MHz SMA antenna.

## Relay module (STM32F103)

- ST-Link connects. Device ID 0x410 (STM32F10x medium density), as expected for the F103C8T6.
- Firmware flashed with STM32CubeProgrammer straight from the `.elf` (18.5 KB). Download verified.
- Seen once: "No STM32 target found" after a power-cycle attempt, followed by CubeProgrammer hanging. Recovery: reseat the SWD wires, check the module is powered, lower the SWD frequency if needed. A PC reboot cleared the hung CubeProgrammer.

## Interface board (ESP32-S3)

| Check | Result |
|---|---|
| J1 input | 5 V present |
| 3.3 V rail (J4 pin 2 to pin 1) | 3.29 V, so the AMS1117 stage is working |
| Power LED | **Not lighting.** It's wired directly to the 3.3 V rail (the other LEDs are GPIO-driven). With power on, 3.29 V sits across the LED itself, so no current is flowing. Most likely soldered in reverse, or damaged. To do: compare the cathode mark with the silkscreen, then rotate or replace it. This doesn't affect the rest of the board. |
| ESP32 boot over J4 | To do: serial monitor at 115200, power-cycle, look for `ESP-ROM:esp32s3` |
| Firmware flash | To do: needs GPIO0 held low at power-up (no DTR/RTS on J4) |

## Open items

- [ ] Fix or replace the interface board power LED.
- [ ] Confirm the ESP32 boot log over J4.
- [ ] Flash the interface board firmware.
- [ ] Check the EEPROM programmer sketch targets the right chip. The sketch is named for an AT24C32; some project docs say AT24C02.
- [ ] Add the Raspberry Pi code to `raspberry-pi/`.
