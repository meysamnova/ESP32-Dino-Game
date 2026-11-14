# ESP32 Dino Game

A Chrome-style dinosaur runner for the **ESP32 (WROOM)** and a **2.8" ILI9341 SPI TFT with resistive touch**. Tap the screen to jump, dodge cacti and birds, and watch the game switch between day and night as your score climbs.

Built with the Arduino IDE and the [TFT_eSPI](https://github.com/Bodmer/TFT_eSPI) library.

## Features

- Flicker-free rendering: the game strip is drawn into an off-screen sprite, then pushed to the display in one go
- Touch controls with a 3-stage anti-ghosting filter, so a hand hovering near the screen doesn't trigger phantom jumps
- Variable jump height: a short tap gives a short hop, holding gives a full jump
- Fast-fall: tap again in mid-air to slam down
- Jump buffering and coyote time, so taps right before landing or just after leaving the ground still count
- Cacti (1 to 3 stems) from the start, flying birds (high and low) once the score passes 150
- Speed ramps up over time; day/night palette switches every 500 points
- High score kept until power-off
- The built-in **BOOT** button (GPIO0) also works as a jump button, which helps when tuning or debugging touch

## Hardware

| Part | Notes |
|------|-------|
| ESP32 WROOM dev board | Any ESP32 DevKit-style board with the standard pinout |
| 2.8" ILI9341 TFT, 320x240, SPI, with resistive touch (XPT2046) | The common red "2.8 inch SPI module" boards |
| Jumper wires | 14 connections, see below |
| USB cable | For power and flashing |

See [docs/HARDWARE.md](docs/HARDWARE.md) for details.

## Wiring

The display and the touch controller share the same SPI bus (SCK, MOSI, MISO). Only the chip-select lines are separate.

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
| T_CS | GPIO 21 | touch chip select, its own pin so the two chips don't clash |
| T_IRQ | not connected | not needed |

Full diagram and troubleshooting: [docs/WIRING.md](docs/WIRING.md).

## Software setup

1. Install the [Arduino IDE](https://www.arduino.cc/en/software) (2.x recommended).
2. Add ESP32 support: **Boards Manager**, search for **esp32** by Espressif Systems, install.
3. Install the **TFT_eSPI** library by Bodmer via **Library Manager**.
4. Configure TFT_eSPI for this wiring. Copy [`TFT_eSPI_Setup/User_Setup.h`](TFT_eSPI_Setup/User_Setup.h) over the library's own `User_Setup.h` (usually in `Documents/Arduino/libraries/TFT_eSPI/`). Back up the original first. Updating the library later will overwrite this file, so keep your copy.
5. Open `ESP32_Dino_Game/ESP32_Dino_Game.ino`.
6. Select **ESP32 Dev Module** under Tools, Board, and pick your COM port.
7. Upload. If the upload hangs on "Connecting...", hold the **BOOT** button until it starts.

## How to play

| Action | Control |
|--------|---------|
| Start / restart | Tap the screen (or press BOOT) |
| Jump | Tap; hold for a higher jump, release early for a short hop |
| Fast fall | Tap again while in the air |

## Tuning touch

All touch settings are `#define`s at the top of the sketch.

- Touch fires by itself or reacts when your hand is near: raise `TOUCH_Z_MIN` (try 800 to 1200)
- Hard presses don't register: lower `TOUCH_Z_MIN` (try 400)
- Taps feel laggy: lower `TOUCH_RELEASE_MS` or `TOUCH_DEBOUNCE_COUNT`
- Double jumps on one tap: raise `TOUCH_COOLDOWN_MS`

Gameplay feel is set by the physics constants just below: `JUMP_VELOCITY`, `GRAVITY`, `START_SPEED`, `MAX_SPEED`, `SPEED_INC`.

## Project layout

```
ESP32-Dino-Game/
├── ESP32_Dino_Game/
│   └── ESP32_Dino_Game.ino    # the game
├── TFT_eSPI_Setup/
│   └── User_Setup.h           # TFT_eSPI config matching the wiring
├── docs/
│   ├── HARDWARE.md
│   └── WIRING.md
├── LICENSE
└── README.md
```

## Troubleshooting

| Problem | Likely cause and fix |
|---------|----------------------|
| White or blank screen | TFT_eSPI is not using the right `User_Setup.h`; re-check step 4. Also check the LED pin is on 3.3V |
| Wrong colors (swapped red/blue) | Add `#define TFT_RGB_ORDER TFT_BGR` or try `TFT_INVERSION_ON` in `User_Setup.h` |
| Image is mirrored or upside down | Change `tft.setRotation(3)` in `setup()` to 1 |
| Touch does nothing | Check T_CS is on GPIO 21 and `TOUCH_CS` is defined in `User_Setup.h` |
| Touch triggers on its own | Raise `TOUCH_Z_MIN`, keep touch wires short |
| Upload fails | Hold BOOT while "Connecting..." shows |

## License

MIT, see [LICENSE](LICENSE).
