#include <Arduino.h>
#include <TFT_eSPI.h>

TFT_eSPI tft = TFT_eSPI();
TFT_eSprite spr = TFT_eSprite(&tft);   // Off-screen buffer (no flicker)

// ---------- Display & Sprite Layout ----------
#define SCREEN_W   320
#define SCREEN_H   240
#define SPR_W      320
#define SPR_H      160
#define SPR_Y      40                  // Vertical position of game strip on 320x240 screen
#define GROUND_Y   132                 // Ground line inside sprite

// ---------- Touch & Anti-Ghosting Configuration ----------
// Raise TOUCH_Z_MIN (e.g. 800-1200) if still too sensitive; lower (e.g. 400) if hard presses don't register.
#define TOUCH_Z_MIN          200       // Minimum sustained pressure required across ALL samples
#define TOUCH_Z_MAX          3900      // Reject railed/floating SPI reads (4095)
#define TOUCH_Z_MAX_JITTER   280       // Reject 50/60Hz hand-proximity hum (unstable Z)
#define TOUCH_XY_MIN         120       // Reject open-circuit X/Y plate reads near 0
#define TOUCH_XY_MAX         3950      // Reject open-circuit X/Y plate reads near 4095
#define TOUCH_XY_MAX_JITTER  45        // Max allowed X/Y drift between consecutive reads
#define TOUCH_DEBOUNCE_COUNT 2         // Require N consecutive valid 6ms polls to confirm press
#define TOUCH_RELEASE_MS     35        // Brief hold after valid press to bridge micro-dropouts
#define TOUCH_COOLDOWN_MS    90        // Minimum time between separate tap edges (stops multi-trigger)

#define JUMP_BUFFER_MS       120       // Remembers early tap right before landing
#define COYOTE_TIME_MS       55        // Allows jump briefly after landing
#define BOOT_BTN_PIN         0         // Built-in BOOT button on ESP32 works alongside touch

// ---------- Physics Constants ----------
#define DINO_X             32
#define DINO_W             22
#define DINO_H             28
#define JUMP_VELOCITY      11.5f       // Initial upward velocity (px/frame)
#define MIN_TAP_VELOCITY   7.5f        // Minimum upward velocity on a short tap (clears ~48px)
#define GRAVITY            0.78f       // Downward acceleration per frame
#define FAST_FALL_GRAVITY  2.1f        // Extra gravity if player taps again mid-air
#define START_SPEED        4.6f
#define MAX_SPEED          10.5f
#define SPEED_INC          0.0022f

// ---------- Color Palettes ----------
uint16_t colBg     = TFT_WHITE;
uint16_t colFg     = 0x4208;           // Dark slate grey
uint16_t colCloud  = 0xCE79;           // Soft grey
uint16_t colGround = 0x8410;           // Mid grey

// ---------- Game Objects ----------
enum ObsType : uint8_t { OBS_CACTUS = 0, OBS_BIRD = 1 };

struct Obstacle {
  float x;
  int16_t y;       // Top Y position inside sprite
  uint8_t w;       // Total width in pixels
  uint8_t h;       // Height in pixels
  uint8_t count;   // Number of cactus stems (1..3)
  ObsType type;
  bool active;
};

struct Cloud {
  float x;
  int16_t y;
  float speed;
};

struct Star {
  float x;
  int16_t y;
};

Obstacle obs[4];
Cloud clouds[4];
Star stars[8];

enum State { WAITING, RUNNING, OVER };
State state = WAITING;

// ---------- Game State Variables ----------
float dinoY = 0.0f;                    // Height above ground (0 = on ground)
float vy = 0.0f;                       // Vertical velocity
bool fastFalling = false;              // True when player taps mid-air to drop quickly
float speed = START_SPEED;
float score = 0.0f;
int hiScore = 0;
float nextGap = 150.0f;
float groundScroll = 0.0f;

uint32_t overTime = 0;
uint32_t frameCount = 0;
uint32_t lastValidTouchTime = 0;
uint32_t lastTapTime = 0;
uint32_t lastEdgeTime = 0;
uint32_t lastGroundedTime = 0;
uint16_t debugRawZ = 0;                // Shown in top-left corner so you can see live Z values
uint16_t baselineZ = 0;                // Ambient idle Z measured at startup
uint8_t validStreak = 0;
bool wasTouched = false;
bool jumpConsumed = false;
bool nightMode = false;

// ---------- 3-Stage Anti-Ghosting Touch Validator ----------
// Why hand proximity false-triggers raw getTouchRawZ():
// When the screen is NOT physically pressed, the X and Y resistive sheets are floating
// and act like an antenna that picks up 50/60Hz capacitive hum from a nearby hand.
// We block phantom proximity triggers by requiring:
//   1) ALL 4 Z readings (minZ, not maxZ!) to exceed threshold with low jitter
//   2) Both X and Y plates to form a real, stable contact point (getTouchRaw)
//   3) Multiple consecutive valid polls (temporal debounce)
bool checkPhysicalTouch() {
  if (digitalRead(BOOT_BTN_PIN) == LOW) {
    debugRawZ = 999;
    return true;
  }

  // Stage 1: Read Z pressure 4 times and require MINIMUM sample to pass
  uint16_t minZ = 0xFFFF;
  uint16_t maxZ = 0;
  for (uint8_t i = 0; i < 4; i++) {
    uint16_t z = tft.getTouchRawZ();
    if (z < minZ) minZ = z;
    if (z > maxZ) maxZ = z;
  }
  debugRawZ = minZ;

  uint16_t dynamicMinZ = TOUCH_Z_MIN;
  if (baselineZ + 350 > dynamicMinZ) {
    dynamicMinZ = baselineZ + 350;
  }

  // Reject if pressure drops below threshold, rails to 4095, or oscillates wildly (hand proximity EMI)
  if (minZ < dynamicMinZ || maxZ > TOUCH_Z_MAX || (maxZ - minZ) > TOUCH_Z_MAX_JITTER) {
    return false;
  }

  // Stage 2: Verify X and Y resistive plates are physically shorted together at a stable point
  uint16_t x1 = 0, y1 = 0, x2 = 0, y2 = 0;
  tft.getTouchRaw(&x1, &y1);
  tft.getTouchRaw(&x2, &y2);

  // When a hand only hovers nearby without touching, X/Y plates are open-circuit (near 0/4095 or jumping wildly)
  if (x1 < TOUCH_XY_MIN || x1 > TOUCH_XY_MAX || y1 < TOUCH_XY_MIN || y1 > TOUCH_XY_MAX) {
    return false;
  }
  if (x2 < TOUCH_XY_MIN || x2 > TOUCH_XY_MAX || y2 < TOUCH_XY_MIN || y2 > TOUCH_XY_MAX) {
    return false;
  }
  int dx = (int)x1 - (int)x2;
  int dy = (int)y1 - (int)y2;
  if (dx < 0) dx = -dx;
  if (dy < 0) dy = -dy;
  if (dx > TOUCH_XY_MAX_JITTER || dy > TOUCH_XY_MAX_JITTER) {
    return false;
  }

  // Re-verify Z is still held after X/Y read
  uint16_t zAfter = tft.getTouchRawZ();
  if (zAfter < dynamicMinZ || zAfter > TOUCH_Z_MAX) {
    return false;
  }

  return true;
}

bool pollTouchDebounced(uint32_t now) {
  static uint32_t lastPoll = 0;
  static bool filteredState = false;

  // Poll at a controlled 6ms interval (avoids charging floating ADC caps at 20kHz)
  if (now - lastPoll < 6) {
    return filteredState;
  }
  lastPoll = now;

  if (checkPhysicalTouch()) {
    if (validStreak < 10) validStreak++;
    if (validStreak >= TOUCH_DEBOUNCE_COUNT) {
      lastValidTouchTime = now;
      filteredState = true;
    }
  } else {
    validStreak = 0;
    if (filteredState && (now - lastValidTouchTime) >= TOUCH_RELEASE_MS) {
      filteredState = false;
    }
  }

  return filteredState;
}

void calibrateIdleTouchBaseline() {
  uint32_t sum = 0;
  uint16_t peak = 0;
  for (int i = 0; i < 32; i++) {
    uint16_t z = tft.getTouchRawZ();
    if (z < TOUCH_Z_MAX) {
      sum += z;
      if (z > peak) peak = z;
    }
    delay(2);
  }
  baselineZ = sum / 32;
  // Cap baseline so a pressed screen at boot doesn't lock out touch
  if (baselineZ > 500) baselineZ = 200;
}

// ---------- Drawing Helpers ----------
void updatePalette(bool night) {
  if (night) {
    colBg     = 0x1084; // Deep night blue/grey
    colFg     = 0xE71C; // Crisp off-white
    colCloud  = 0x3186; // Dim night cloud
    colGround = 0x632C; // Muted ground detail
  } else {
    colBg     = TFT_WHITE;
    colFg     = 0x4208;
    colCloud  = 0xCE79;
    colGround = 0x8410;
  }
}

void drawDino(int x, int top, int legFrame, bool dead) {
  // Tail
  spr.fillRect(x - 6, top + 11, 6, 4, colFg);
  spr.fillRect(x - 4, top + 9, 4, 2, colFg);
  // Body
  spr.fillRect(x, top + 8, 18, 14, colFg);
  // Neck & Head
  spr.fillRect(x + 9, top, 15, 11, colFg);
  spr.fillRect(x + 24, top + 2, 2, 6, colFg); // Snout tip
  // Arm
  spr.fillRect(x + 17, top + 14, 5, 3, colFg);
  spr.fillRect(x + 20, top + 16, 2, 2, colFg);
  // Eye
  if (dead) {
    spr.fillRect(x + 13, top + 2, 4, 4, colBg);
    spr.drawPixel(x + 14, top + 3, colFg);
  } else {
    spr.fillRect(x + 14, top + 2, 3, 3, colBg);
  }
  // Mouth line
  spr.fillRect(x + 18, top + 8, 6, 2, colBg);

  // Legs (0 = left down, 1 = right down, 2 = both down / jumping)
  if (legFrame == 0) {
    spr.fillRect(x + 3, top + 22, 4, 6, colFg);
    spr.fillRect(x + 11, top + 22, 4, 3, colFg);
  } else if (legFrame == 1) {
    spr.fillRect(x + 3, top + 22, 4, 3, colFg);
    spr.fillRect(x + 11, top + 22, 4, 6, colFg);
  } else {
    spr.fillRect(x + 3, top + 22, 4, 6, colFg);
    spr.fillRect(x + 11, top + 22, 4, 6, colFg);
  }
}

void drawCactus(const Obstacle &o) {
  for (uint8_t i = 0; i < o.count; i++) {
    int tx = (int)o.x + i * 11;
    int stemH = o.h - (i % 2) * 3;
    int top = GROUND_Y - stemH;

    // Main trunk
    spr.fillRect(tx + 3, top, 5, stemH, colFg);
    // Left branch
    int armL = top + stemH / 3;
    spr.fillRect(tx, armL, 3, 3, colFg);
    spr.fillRect(tx, armL - 5, 2, 5, colFg);
    // Right branch
    int armR = top + stemH / 2;
    spr.fillRect(tx + 8, armR, 3, 3, colFg);
    spr.fillRect(tx + 9, armR - 5, 2, 5, colFg);
  }
}

void drawBird(const Obstacle &o) {
  int bx = (int)o.x;
  int by = o.y;
  bool wingUp = ((frameCount / 6) % 2) == 0;

  // Beak & head
  spr.fillRect(bx, by + 6, 6, 3, colFg);
  spr.fillRect(bx + 4, by + 4, 6, 5, colFg);
  // Body & tail
  spr.fillRect(bx + 9, by + 6, 11, 5, colFg);
  spr.fillRect(bx + 20, by + 7, 4, 2, colFg);
  // Flapping wing
  if (wingUp) {
    spr.fillRect(bx + 9, by, 6, 6, colFg);
  } else {
    spr.fillRect(bx + 9, by + 11, 6, 5, colFg);
  }
}

void drawCloud(int x, int y) {
  spr.fillRoundRect(x, y, 34, 8, 4, colCloud);
  spr.fillRoundRect(x + 8, y - 5, 18, 8, 4, colCloud);
}

void drawScoreAndHUD() {
  char buf[32];
  spr.setTextColor(colFg, colBg);
  spr.setTextDatum(TR_DATUM);
  snprintf(buf, sizeof(buf), "HI %05d  %05d", hiScore, (int)score);
  spr.drawString(buf, SPR_W - 6, 4, 2);
}

void drawScene(bool dead, bool touched) {
  spr.fillSprite(colBg);

  // Night stars & crescent moon
  if (nightMode) {
    //spr.fillCircle(45, 28, 7, colCloud);
    //spr.fillCircle(48, 26, 6, colBg);
    for (auto &s : stars) {
      spr.drawPixel((int)s.x, s.y, colFg);
    }
  }

  // Clouds
  for (auto &c : clouds) {
    drawCloud((int)c.x, c.y);
  }

  // Ground line & scrolling pebbles
  spr.drawFastHLine(0, GROUND_Y, SPR_W, colFg);
  for (int i = 0; i < 14; i++) {
    int gx = (int)(i * 31 - groundScroll) % (SPR_W + 40);
    if (gx < 0) gx += SPR_W + 40;
    spr.drawFastHLine(gx, GROUND_Y + 4 + (i % 3) * 4, 3 + (i % 3) * 2, colGround);
  }

  // Obstacles
  for (auto &o : obs) {
    if (!o.active) continue;
    if (o.type == OBS_CACTUS) drawCactus(o);
    else drawBird(o);
  }

  // Dino
  int top = GROUND_Y - DINO_H - (int)dinoY;
  int leg = (dinoY > 1.0f) ? 2 : ((frameCount / 4) % 2);
  if (state == WAITING) leg = 2;
  drawDino(DINO_X, top, leg, dead);

  drawScoreAndHUD();
}

// ---------- Game Logic ----------
void resetGame() {
  for (auto &o : obs) o.active = false;
  dinoY = 0.0f;
  vy = 0.0f;
  fastFalling = false;
  speed = START_SPEED;
  score = 0.0f;
  nextGap = 160.0f;
  frameCount = 0;
  nightMode = false;
  updatePalette(false);

  clouds[0] = {50.0f,  24, 0.5f};
  clouds[1] = {145.0f, 42, 0.7f};
  clouds[2] = {230.0f, 18, 0.4f};
  clouds[3] = {305.0f, 34, 0.6f};

  for (int i = 0; i < 8; i++) {
    stars[i] = {(float)random(10, SPR_W - 10), (int16_t)random(14, 65)};
  }
}

void spawnObstacle() {
  for (auto &o : obs) {
    if (!o.active) {
      o.active = true;
      o.x = SPR_W + 12;

      // Introduce flying birds once score reaches 150
      if (score > 150 && random(0, 100) < 28) {
        o.type = OBS_BIRD;
        o.count = 1;
        o.w = 24;
        o.h = 16;
        // Two bird altitudes: low (jump over) or high (run safely underneath)
        bool highBird = random(0, 2) == 0;
        o.y = highBird ? (GROUND_Y - DINO_H - 18) : (GROUND_Y - 22);
      } else {
        o.type = OBS_CACTUS;
        // Start with single/double cacti early on; allow triple cacti once faster
        int maxCount = (score < 80) ? 2 : ((score < 220) ? 3 : 4);
        o.count = random(1, maxCount);
        o.h = random(20, (score < 100) ? 29 : 34);
        o.w = o.count * 11;
        o.y = GROUND_Y - o.h;
      }

      // Guarantee generous landing distance between obstacles
      nextGap = speed * 25.0f + random(55, 165);
      return;
    }
  }
}

bool checkCollision() {
  // Forgiving Dino hitbox (inset away from tail tip & snout)
  int dx0 = DINO_X + 4;
  int dx1 = DINO_X + DINO_W - 3;
  int dy0 = GROUND_Y - DINO_H - (int)dinoY + 4;
  int dy1 = GROUND_Y - (int)dinoY - 3;

  for (auto &o : obs) {
    if (!o.active) continue;
    int ox0 = (int)o.x + 3;
    int ox1 = (int)o.x + o.w - 3;
    int oy0 = o.y + 2;
    int oy1 = o.y + o.h - 2;

    if (dx1 > ox0 && dx0 < ox1 && dy1 > oy0 && dy0 < oy1) {
      return true;
    }
  }
  return false;
}

void updateGame(bool touched, bool pressEdge, uint32_t now) {
  frameCount++;

  if (dinoY <= 0.0f) {
    lastGroundedTime = now;
  }

  // Record tap timestamp for jump buffering
  if (pressEdge) {
    lastTapTime = now;
    jumpConsumed = false;
  }

  // Check if a buffered or fresh tap should trigger a jump
  bool bufferedJumpReady = !jumpConsumed && ((now - lastTapTime) <= JUMP_BUFFER_MS);
  bool canJumpNow = (dinoY <= 0.0f) || ((now - lastGroundedTime) <= COYOTE_TIME_MS && vy <= 0.0f);

  if ((bufferedJumpReady || (touched && !jumpConsumed)) && canJumpNow && vy <= 0.0f) {
    vy = JUMP_VELOCITY;
    dinoY = 0.1f;
    fastFalling = false;
    jumpConsumed = true;
  } else if (pressEdge && dinoY > 14.0f) {
    // Tapping a second time while in the air triggers a fast-fall slam
    fastFalling = true;
    if (vy > 0.0f) vy = 0.0f;
  }

  // Variable jump height: releasing early softens the peak, but still clears 1-3 cacti
  if (!touched && vy > MIN_TAP_VELOCITY) {
    vy = MIN_TAP_VELOCITY;
  }

  // Vertical physics integration
  if (dinoY > 0.0f || vy > 0.0f) {
    dinoY += vy;
    vy -= (fastFalling ? FAST_FALL_GRAVITY : GRAVITY);
    if (dinoY <= 0.0f) {
      dinoY = 0.0f;
      vy = 0.0f;
      fastFalling = false;
    }
  }

  // Speed & score progression
  if (speed < MAX_SPEED) speed += SPEED_INC;
  score += speed * 0.08f;

  // Toggle Day/Night every 500 points
  bool shouldBeNight = (((int)score / 500) % 2) == 1;
  if (shouldBeNight != nightMode) {
    nightMode = shouldBeNight;
    updatePalette(nightMode);
  }

  // Scroll ground
  groundScroll += speed;
  if (groundScroll >= SPR_W + 40) groundScroll -= (SPR_W + 40);

  // Scroll clouds & stars
  for (auto &c : clouds) {
    c.x -= c.speed;
    if (c.x < -40) {
      c.x = SPR_W + random(5, 70);
      c.y = random(15, 55);
    }
  }
  if (nightMode) {
    for (auto &s : stars) {
      s.x -= 0.25f;
      if (s.x < 0) s.x = SPR_W;
    }
  }

  // Update obstacles
  float rightmost = -1000.0f;
  for (auto &o : obs) {
    if (!o.active) continue;
    o.x -= speed;
    if (o.x + o.w < 0) {
      o.active = false;
    } else if (o.x + o.w > rightmost) {
      rightmost = o.x + o.w;
    }
  }
  if (SPR_W - rightmost > nextGap) {
    spawnObstacle();
  }

  // Collision check
  if (checkCollision()) {
    state = OVER;
    overTime = now;
    if ((int)score > hiScore) hiScore = (int)score;
  }
}

// ---------- Setup & Main Loop ----------
void setup() {
  Serial.begin(115200);
  pinMode(BOOT_BTN_PIN, INPUT_PULLUP);
  randomSeed(analogRead(34) ^ micros());

  tft.init();
  tft.setRotation(3);                  // Landscape 320x240
  tft.fillScreen(TFT_WHITE);

  calibrateIdleTouchBaseline();

  spr.setColorDepth(8);                // 8-bit color uses ~51KB RAM and pushes fast
  spr.createSprite(SPR_W, SPR_H);

  resetGame();
}

void loop() {
  uint32_t now = millis();

  // Poll debounced, X/Y-verified touch state
  bool touched = pollTouchDebounced(now);

  static uint32_t lastFrame = 0;
  if (now - lastFrame < 22) return;    // ~45 FPS update rate
  lastFrame = now;

  // Enforce a minimum cooldown between rising edges so proximity hum cannot rapid-fire
  bool pressEdge = false;
  if (touched && !wasTouched && (now - lastEdgeTime >= TOUCH_COOLDOWN_MS)) {
    pressEdge = true;
    lastEdgeTime = now;
  }
  wasTouched = touched;

  switch (state) {
    case WAITING:
      drawScene(false, touched);
      spr.setTextColor(colFg, colBg);
      spr.setTextDatum(MC_DATUM);
      spr.drawString("TAP TO START", SPR_W / 2, 65, 4);
      if (pressEdge) {
        resetGame();
        state = RUNNING;
        lastTapTime = now;
        jumpConsumed = false;
      }
      break;

    case RUNNING:
      updateGame(touched, pressEdge, now);
      drawScene(false, touched);
      break;

    case OVER:
      drawScene(true, touched);
      spr.setTextColor(colFg, colBg);
      spr.setTextDatum(MC_DATUM);
      spr.drawString("GAME OVER", SPR_W / 2, 56, 4);
      spr.drawString("Tap screen or BOOT button", SPR_W / 2, 86, 2);
      if (pressEdge && (now - overTime > 500)) {
        resetGame();
        state = RUNNING;
        lastTapTime = now;
        jumpConsumed = false;
      }
      break;
  }

  spr.pushSprite(0, SPR_Y);
}