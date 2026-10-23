# Drone Payload Interface

A plug-and-play USB-C interface for hot-swapping payload modules on a drone. The drone carries an **interface board**. Each payload is a self-contained **module** that plugs into it over USB-C. When a module is connected, the interface board identifies it, checks it's a known and valid module, and only then switches power on to it.

The first payload module is a **LoRa communications relay**.

## How module identification works

Each module is identified in two independent ways, and power is enabled only when they agree:

1. **Analog ID (CC1 line):** the interface board has a fixed pull-up resistor on CC1. Each module type has its own pull-down resistor, so each type produces a distinct CC1 voltage that the ESP32 reads with its ADC.
2. **Digital ID (EEPROM):** each module carries an I2C EEPROM holding a module profile record. The interface board reads it over the USB-C SBU1/SBU2 pins (used as I2C SDA/SCL).

If the two channels disagree, or either one is unreadable, the module is not powered.

## Repository layout

```
drone-payload-interface/
├── firmware/
│   ├── interface-board-esp32/   ESP32-S3 firmware (PlatformIO)
│   └── relay-module-stm32/      STM32F103 relay firmware (STM32CubeIDE)
├── hardware/
│   └── interface_board/         KiCad: interface board
│       ├── relay_module/        KiCad: LoRa relay module
│       ├── test_module/         KiCad: test module
│       └── libs/                Bundled custom footprints (shared)
├── tools/
│   └── eeprom-programmer/       Arduino sketch for writing module EEPROMs
├── raspberry-pi/                Companion computer code (to be added)
└── docs/                        Bring-up notes and project documentation
```

## Hardware

### Interface board

| Part | Role |
|---|---|
| ESP32-S3-WROOM-1 | Main controller: detection, ID validation, power control, comms |
| GCT USB4085-GF-A | USB-C receptacle that modules plug into |
| TPS2116 | Power mux |
| INA219 | Current/voltage monitoring of module power |
| AMS1117-3.3 | 3.3 V regulator for the board logic |

**Headers**

| Header | Pins | Use |
|---|---|---|
| J1 | 2-pin | Power input (5 V) |
| J4 | 4-pin: GND / +3V3 / UART_TX / UART_RX | Debug and flashing (ESP32 UART) |
| J3 | 4-pin, same pinout as J4 | Link to the Raspberry Pi |

### Relay module

| Part | Role |
|---|---|
| STM32F103C8T6 | Module controller |
| EBYTE E22-900M30S | SX1262-based LoRa radio, 868 MHz, up to 30 dBm |
| TLV62568 + SRP4020TA-2R2M | Buck converter for the module supply |
| AT24Cxx EEPROM | Module ID profile, read by the interface board |
| SMA connector | Antenna |

> ⚠️ **Never power the relay module without an antenna attached.** The E22-900M30S transmits at up to 30 dBm, and transmitting into an open connector can damage its power amplifier.

## Firmware

### Interface board (ESP32-S3, PlatformIO)

Source is in `firmware/interface-board-esp32/src/`:

| File | Purpose |
|---|---|
| `main.cpp` | Startup and main loop |
| `detection.cpp/.h` | Module detection and CC1 / EEPROM ID checks |
| `power.cpp/.h` | Module power switching and monitoring |
| `comms.cpp/.h` | Communication with the Raspberry Pi |

**Build:** open `firmware/interface-board-esp32` as a folder in VS Code with the PlatformIO extension, then click **Build (✓)**.

**Flash over J4** using a USB-to-TTL adapter (CP2102 or similar):

| Adapter | J4 |
|---|---|
| GND | GND (pin 1) |
| TXD | UART_RX (pin 4) |
| RXD | UART_TX (pin 3) |
| 3V3 / 5V | not connected (the board is powered from J1) |

J4 has no DTR/RTS lines, so the ESP32 can't be put into bootloader mode automatically. Hold **GPIO0 low while powering up** the board, release it, then click **Upload (→)** in PlatformIO.

Serial monitor: 115200 baud.

### Relay module (STM32F103, STM32CubeIDE)

The project is in `firmware/relay-module-stm32/`. It uses the STM32 HAL with USART1, I2C1, SPI1 (to the radio) and TIM2 (1 Hz heartbeat). Received LoRa packets are signalled to the MCU through an EXTI interrupt on the radio's DIO line. The SX126x driver is in `Core/` and `sx126x_driver/`.

**Open:** in STM32CubeIDE, use **File → Import → Existing Projects into Workspace**, select `firmware/relay-module-stm32`, and leave "Copy projects into workspace" unticked.

**Flash:** connect an ST-Link V2 to the module's SWD header (SWDIO, SWCLK, GND, 3V3), then use Run/Debug in CubeIDE, or load `Debug/Relay_module.elf` in STM32CubeProgrammer.

### EEPROM programmer

`tools/eeprom-programmer/` contains an Arduino sketch for writing module profile records to the module EEPROMs. Open the `.ino` in the Arduino IDE. Check that the EEPROM type set in the sketch matches the chip fitted on the module before writing.

## Hardware design files

KiCad projects are in `hardware/interface_board/`. Custom footprints are bundled in the repo:

- `libs/Custom.pretty`: USB4085 USB-C connector, SRP4020 inductor, logo
- `relay_module/libs/E22-900M30S`: E22 radio footprint, symbol and 3D model

These are referenced through each project's `fp-lib-table` with `${KIPRJMOD}` paths, so the projects open on any machine without extra library setup. Gerbers and BOM/placement files for manufacturing are in each board's folder.

## Working with this repo

KiCad's local history (`.history/`), build output (`Debug/`, `.pio/`) and backup folders are excluded by `.gitignore`.

```
git add .
git commit -m "Describe the change"
git push
```

## Status

- Relay module firmware builds and flashes over ST-Link.
- Interface board powers up from J1, and the 3.3 V rail is confirmed.
- Next: confirm the ESP32 boots over the J4 serial link, then flash the interface board firmware.

See `docs/bring-up-notes.md` for details.
