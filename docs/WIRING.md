# Wiring

ESP32 (WROOM) to a 2.8" ILI9341 TFT with XPT2046 resistive touch, all over one shared SPI bus (VSPI).

## Connection table

| Display pin | ESP32 pin | Note |
|-------------|-----------|------|
| VCC | 3.3V | |
| GND | GND | |
| CS | GPIO 15 | display chip select |
| RESET | GPIO 4 | |
| DC / RS | GPIO 2 | |
| SDI (MOSI) | GPIO 23 | shared with touch |
| SCK | GPIO 18 | shared with touch |
| LED | 3.3V | backlight |
| SDO (MISO) | GPIO 19 | shared with touch |
| T_CLK | GPIO 18 | same as SCK |
| T_DIN | GPIO 23 | same as MOSI |
| T_DO | GPIO 19 | same as MISO |
| T_CS | GPIO 21 | its own pin, so the two chips don't clash |
| T_IRQ | not connected | not needed here |

## Diagram

```
        ESP32 WROOM                     2.8" TFT + touch
      ┌─────────────┐                 ┌─────────────────┐
      │         3V3 ├──────┬──────────┤ VCC             │
      │             │      └──────────┤ LED (backlight) │
      │         GND ├─────────────────┤ GND             │
      │             │                 │                 │
      │     GPIO 15 ├─────────────────┤ CS              │
      │      GPIO 4 ├─────────────────┤ RESET           │
      │      GPIO 2 ├─────────────────┤ DC / RS         │
      │             │                 │                 │
      │     GPIO 23 ├──────┬──────────┤ SDI (MOSI)      │
      │             │      └──────────┤ T_DIN           │
      │     GPIO 18 ├──────┬──────────┤ SCK             │
      │             │      └──────────┤ T_CLK           │
      │     GPIO 19 ├──────┬──────────┤ SDO (MISO)      │
      │             │      └──────────┤ T_DO            │
      │             │                 │                 │
      │     GPIO 21 ├─────────────────┤ T_CS            │
      │             │                 │ T_IRQ  (unused) │
      └─────────────┘                 └─────────────────┘
```

Each shared line (MOSI, SCK, MISO) goes from one ESP32 pin to two display pins, so you need a way to split it: a breadboard rail, a Y-jumper, or two wires into the same header pin.

## Notes

- **BOOT button:** the sketch also reads GPIO 0, the BOOT button already on the dev board. No extra wiring is needed.
- **Power:** the display runs from the 3.3V pin. If your module has a 5V/3.3V regulator jumper, 3.3V is the safe choice with ESP32 logic.
- **Backlight:** LED is tied straight to 3.3V, so it is always on. Some modules need a series resistor if the backlight is too bright or draws too much; most have one on the board.
- **Random seed:** the sketch reads GPIO 34 (floating input) for random-number seeding. Leave it unconnected.
- **Strapping pins:** GPIO 2 and GPIO 15 are ESP32 boot-strapping pins. They work fine for DC and CS on most boards, but if the board fails to boot with the display attached, unplug the DC and CS wires while uploading or powering on.
- **Keep wires short:** long jumper wires on the touch lines make the touch reading noisy.

## Checklist before powering on

- [ ] VCC and LED both on 3.3V
- [ ] GND connected
- [ ] CS 15, RESET 4, DC 2
- [ ] MOSI 23, SCK 18, MISO 19 each connected to both the display and touch pins
- [ ] T_CS on 21
- [ ] T_IRQ left unconnected
- [ ] `User_Setup.h` copied into the TFT_eSPI library
