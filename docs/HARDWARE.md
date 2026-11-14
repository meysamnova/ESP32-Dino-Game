# Hardware

## Parts list

| Qty | Part | Notes |
|-----|------|-------|
| 1 | ESP32 WROOM development board | ESP32-WROOM-32 DevKit-style, 30 or 38 pin |
| 1 | 2.8" ILI9341 SPI TFT, 320x240, with resistive touch | Touch controller is XPT2046 (or compatible). Often sold with an SD card slot, which this project doesn't use |
| 14 | Jumper wires | Dupont female-to-female if you plug straight into the display header |
| 1 | USB cable | Data-capable, not charge-only |
| 1 | Breadboard (optional) | Makes splitting the shared SPI lines easier |

## Display module

- Resolution: 320 x 240 pixels (used in landscape, `setRotation(3)`)
- Driver: ILI9341
- Interface: 4-wire SPI
- Touch: resistive, read over the same SPI bus using its own chip select
- Logic level: 3.3V

## Why the touch handling in the sketch is elaborate

On many of these modules, the touch plates are left floating when nobody is pressing. The floating plates can pick up mains hum (50/60 Hz) from a nearby hand and look like a light press. The sketch handles that in three stages:

1. Read pressure (Z) four times; the lowest reading must be above a threshold, with low spread between readings
2. Read X and Y twice; both must be inside a valid range and stable
3. Require several consecutive good polls before accepting a press

If you see phantom jumps, see the tuning section of the README.

## Memory use

The game strip is drawn into an 8-bit sprite of 320 x 160 pixels, about 51 KB of RAM. A standard ESP32 WROOM has enough heap for that. A 16-bit sprite would need roughly 102 KB and may not fit comfortably alongside the rest of the firmware.

## Tested with

- ESP32-WROOM-32 (CH340 USB-serial)
- 2.8" ILI9341 TFT with resistive touch
- Arduino IDE with the Espressif ESP32 board package and TFT_eSPI by Bodmer
