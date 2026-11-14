// TFT_eSPI User_Setup.h for ESP32-Dino-Game
// ESP32 (WROOM) + 2.8" ILI9341 SPI TFT with XPT2046 resistive touch
//
// Copy this file over TFT_eSPI/User_Setup.h in your Arduino libraries folder
// (back up the original first). Updating the TFT_eSPI library overwrites it.

#define USER_SETUP_ID 9001

// ---------- Driver ----------
#define ILI9341_DRIVER

// If colors look wrong, try one of these:
//#define TFT_RGB_ORDER TFT_BGR
//#define TFT_INVERSION_ON
//#define TFT_INVERSION_OFF

#define TFT_WIDTH  240
#define TFT_HEIGHT 320

// ---------- ESP32 pins (see docs/WIRING.md) ----------
#define TFT_MISO 19
#define TFT_MOSI 23
#define TFT_SCLK 18
#define TFT_CS   15   // display chip select
#define TFT_DC    2   // data / command (RS)
#define TFT_RST   4   // reset

#define TOUCH_CS 21   // touch chip select (T_CS)

// ---------- Fonts ----------
// The game uses fonts 2 and 4; the others are loaded for convenience.
#define LOAD_GLCD   // Font 1, original Adafruit 8 pixel font
#define LOAD_FONT2  // Font 2, small 16 pixel high font
#define LOAD_FONT4  // Font 4, medium 26 pixel high font
#define LOAD_FONT6  // Font 6, large 48 pixel font, digits and a few symbols only
#define LOAD_FONT7  // Font 7, 7 segment 48 pixel font, digits only
#define LOAD_FONT8  // Font 8, large 75 pixel font, digits only
#define LOAD_GFXFF  // FreeFonts

#define SMOOTH_FONT

// ---------- SPI speed ----------
#define SPI_FREQUENCY       40000000   // lower to 27000000 if you see glitches
#define SPI_READ_FREQUENCY  20000000
#define SPI_TOUCH_FREQUENCY  2500000   // XPT2046 needs a slow clock
