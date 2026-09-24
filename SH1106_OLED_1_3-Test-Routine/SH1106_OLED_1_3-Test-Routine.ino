/*
  SH1106 OLED Test  -  ESP32 (Arduino core, U8g2)
  1.3" OLED module, 128x64 pixels, monochrome, 7-pin (4-wire SPI or I2C)

  Author:       Stefan Graf
  Contact:      shop@tinkerberg.com
  Where to buy: www.tinkerberg.com
  github:       https://github.com/graflichttechnik
  Version:      v1.0.0
  Date:         2026-09-24
  License:      MIT

  Library (Library Manager): U8g2
  Board:    "ESP32 Dev Module" (or any other ESP32 board)
  Serial:   115200 baud

  Every run starts with the logo, followed by all tests in an endless loop; each
  step is also reported on the serial console. OLED_INTERFACE selects SPI
  (default) or I2C. The module has an SH1106 controller (not an SSD1306), which
  U8g2 drives with its own SH1106 driver.

  Pin labels on the module:  GND  VCC  SCL  DIN  RES  A0  CS  (A0 = DC, DIN = MOSI/SDA)

  --- 4-wire SPI (OLED_INTERFACE_SPI) ---     --- I2C (OLED_INTERFACE_I2C) ---
  Module  ESP32                               Module  ESP32
  GND     GND                                 GND     GND
  VCC     5V       (see notes)                VCC     3V3      (see notes)
  SCL     GPIO18   (VSPI CLK)                 SCL     GPIO22   (= SCL)
  DIN     GPIO23   (VSPI MOSI)                DIN     GPIO21   (= SDA)
  RES     GPIO16                              RES     GPIO16   (hardware reset)
  A0      GPIO17                              A0      GND  -> address 0x3C
  CS      GPIO5                                       3V3  -> address 0x3D
                                              CS      GND

  Notes:
  - The interface is set by the IM1/IM0 jumpers on the back of the module (table
    printed on the PCB). 4-wire SPI (IM1 = 0, IM0 = 0) is the factory setting;
    I2C needs IM1 = 1, IM0 = 0. If the I2C scan finds nothing, the module is
    most likely still in SPI mode.
  - RES must not float. RES and A0 (SPI) can be any free output GPIO. SCL
    (GPIO18) and DIN (GPIO23) are the default VSPI pins used by U8g2.
  - VCC accepts 2.5-5.5 V. The tested module pulsed slightly at 3.3 V and was
    stable at 5 V. Before using 5 V, connect only VCC and GND and measure the
    signal pins against GND: none may show about 5 V (pull-up to VCC). The
    ESP32 pins are not 5 V tolerant. In I2C mode this is critical: the ESP32
    releases SDA and SCL, so any pull-up to 5 V ends up on the pins.
*/

#include <Arduino.h>
#include <Wire.h>
#include <U8g2lib.h>
#include "tinkerberg_logo.h"   // start-up logo (XBM bitmap), replace with your own

#define OLED_INTERFACE_I2C 1
#define OLED_INTERFACE_SPI 2

#ifndef OLED_INTERFACE
#define OLED_INTERFACE OLED_INTERFACE_SPI   // set to OLED_INTERFACE_I2C for an I2C module
#endif

constexpr uint8_t  SCREEN_WIDTH  = 128;
constexpr uint8_t  SCREEN_HEIGHT = 64;
constexpr uint16_t LOGO_MS       = 5000;

constexpr uint8_t PIN_OLED_RST = 16;   // any free GPIO; U8X8_PIN_NONE = module has no RES pin

#if OLED_INTERFACE == OLED_INTERFACE_I2C
constexpr uint8_t PIN_SDA = 21;   // module DIN (= SDA)
constexpr uint8_t PIN_SCL = 22;   // module SCL
U8G2_SH1106_128X64_NONAME_F_HW_I2C u8g2(U8G2_R0, PIN_OLED_RST, PIN_SCL, PIN_SDA);
#else
constexpr uint8_t PIN_OLED_CS = 5;
constexpr uint8_t PIN_OLED_DC = 17;   // module A0, any free GPIO
U8G2_SH1106_128X64_NONAME_F_4W_HW_SPI u8g2(U8G2_R0, PIN_OLED_CS, PIN_OLED_DC, PIN_OLED_RST);
#endif

constexpr uint8_t DEFAULT_CONTRAST = 0xCF;

uint32_t runCount = 0;

static const uint8_t PROGMEM bmpHeart[] = {
  0x00, 0x00, 0x3C, 0x3C, 0x7E, 0x7E, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFE, 0x7F, 0xFC, 0x3F, 0xF8, 0x1F, 0xF0, 0x0F, 0xE0, 0x07, 0xC0, 0x03, 0x80, 0x01, 0x00, 0x00, 0x00, 0x00
};

// Start-up logo: centred XBM bitmap, shown for LOGO_MS at the start of every run.
static void showLogo() {
  u8g2.clearBuffer();
  u8g2.drawXBM((SCREEN_WIDTH - TINKERBERG_LOGO_WIDTH) / 2, (SCREEN_HEIGHT - TINKERBERG_LOGO_HEIGHT) / 2,
               TINKERBERG_LOGO_WIDTH, TINKERBERG_LOGO_HEIGHT, tinkerberg_logo);
  u8g2.sendBuffer();
  delay(LOGO_MS);
}

#if OLED_INTERFACE == OLED_INTERFACE_I2C
// True if a device acknowledges the given I2C address.
static bool probeI2c(uint8_t addr) {
  Wire.beginTransmission(addr);
  return Wire.endTransmission() == 0;
}

// Scans the bus, lists every device on the serial console and blocks until a display
// answers at 0x3C or 0x3D. Returns that address.
static uint8_t waitForI2cDisplay() {
  for (;;) {
    Serial.println("I2C scan ...");
    uint8_t found = 0;
    for (uint8_t a = 0x08; a < 0x78; a++) {
      if (probeI2c(a)) {
        Serial.printf("  Device at 0x%02X\n", a);
        found++;
      }
    }
    if (probeI2c(0x3C)) return 0x3C;
    if (probeI2c(0x3D)) return 0x3D;

    Serial.printf("  %u device(s) found, but no display at 0x3C/0x3D.\n", found);
    Serial.printf("  Check: VCC/GND, SCL(GPIO%d), DIN=SDA(GPIO%d), RES(GPIO%d),\n",
                  PIN_SCL, PIN_SDA, PIN_OLED_RST);
    Serial.println("         A0+CS wired to GND, module really set to I2C? (factory setting is SPI)");
    delay(2000);
  }
}
#endif

// Resolution: border and both diagonals touch all four corners; the centred label names
// the resolution. A missing edge or a shifted image means the wrong driver or offset.
static void testResolution() {
  const char *label = "128x64";
  u8g2.clearBuffer();
  u8g2.drawFrame(0, 0, SCREEN_WIDTH, SCREEN_HEIGHT);
  u8g2.drawLine(0, 0, SCREEN_WIDTH - 1, SCREEN_HEIGHT - 1);
  u8g2.drawLine(0, SCREEN_HEIGHT - 1, SCREEN_WIDTH - 1, 0);
  u8g2.setFont(u8g2_font_6x10_tf);
  const int16_t w = u8g2.getStrWidth(label);
  const int16_t x = (SCREEN_WIDTH - w) / 2;
  u8g2.setDrawColor(0);
  u8g2.drawBox(x - 5, 24, w + 10, 15);
  u8g2.setDrawColor(1);
  u8g2.drawStr(x, 35, label);
  u8g2.sendBuffer();
  delay(3000);
}

// All pixels off, then all on: reveals dead or stuck pixels and shows how bright a fully
// lit panel gets (brightness drops when many pixels per row are lit).
static void testFillAndClear() {
  u8g2.clearBuffer();
  u8g2.sendBuffer();
  delay(700);
  u8g2.drawBox(0, 0, SCREEN_WIDTH, SCREEN_HEIGHT);
  u8g2.sendBuffer();
  delay(1500);
  u8g2.clearBuffer();
  u8g2.sendBuffer();
  delay(300);
}

// Checkerboards (single pixels and 8x8 blocks, both phases): every pixel is toggled, which
// exposes crosstalk and row/column addressing errors.
static void testCheckerboard() {
  for (uint8_t phase = 0; phase < 4; phase++) {
    u8g2.clearBuffer();
    for (int16_t y = 0; y < SCREEN_HEIGHT; y++) {
      for (int16_t x = 0; x < SCREEN_WIDTH; x++) {
        bool on;
        if (phase < 2) on = (((x + y + phase) & 1) == 0);
        else           on = ((((x >> 3) + (y >> 3) + phase) & 1) == 0);
        if (on) u8g2.drawPixel(x, y);
      }
    }
    u8g2.sendBuffer();
    delay(900);
  }
}

// Fonts: four font families, then a live mm:ss clock in a 32 px font (fast redraws).
static void testFonts() {
  u8g2.clearBuffer();
  u8g2.setFont(u8g2_font_6x10_tf);
  u8g2.drawStr(0, 9, "6x10 fixed");
  u8g2.setFont(u8g2_font_helvB10_tr);
  u8g2.drawStr(0, 25, "Helvetica bold");
  u8g2.setFont(u8g2_font_ncenB14_tr);
  u8g2.drawStr(0, 44, "Serif bold");
  u8g2.setFont(u8g2_font_courB12_tr);
  u8g2.drawStr(0, 60, "Courier bold");
  u8g2.sendBuffer();
  delay(2500);

  const uint32_t start = millis();
  while (millis() - start < 3000) {
    const uint32_t s = (millis() - start) / 1000;
    char clock[24];
    snprintf(clock, sizeof(clock), "%02lu:%02lu", (unsigned long)(s / 60), (unsigned long)(s % 60));
    u8g2.clearBuffer();
    u8g2.setFont(u8g2_font_logisoso32_tn);
    u8g2.drawStr((SCREEN_WIDTH - u8g2.getStrWidth(clock)) / 2, 48, clock);
    u8g2.sendBuffer();
    delay(100);
  }
}

// UTF-8: umlauts, euro, degree, arrows and symbols with drawUTF8() and the unifont symbols
// font. Unicode escapes keep this source file pure ASCII.
static void testUtf8() {
  u8g2.clearBuffer();
  u8g2.setFont(u8g2_font_unifont_t_symbols);
  u8g2.drawUTF8(0, 14, "UTF-8: \u00e4 \u00f6 \u00fc \u00df");
  u8g2.drawUTF8(0, 33, "\u20ac \u00b0C \u00b1 \u00b5 \u00a9");
  u8g2.drawUTF8(0, 52, "\u2192 \u2713 \u2605 \u2665 \u2600 \u2601");
  u8g2.sendBuffer();
  delay(3000);
}

// Icons: eight large and 18 small glyphs from the Open Iconic font, drawn like text.
static void testIcons() {
  static const uint16_t BIG[8] = {184, 281, 94, 129, 96, 183, 258, 259};
  u8g2.clearBuffer();
  u8g2.setFont(u8g2_font_open_iconic_all_4x_t);
  for (uint8_t i = 0; i < 8; i++) u8g2.drawGlyph((i % 4) * 32, 32 + (i / 4) * 32, BIG[i]);
  u8g2.sendBuffer();
  delay(2500);

  static const uint16_t SMALL[18] = {93, 202, 123, 165, 193, 207, 225, 220, 229,
                                     222, 223, 235, 271, 268, 209, 280, 253, 282};
  u8g2.clearBuffer();
  u8g2.setFont(u8g2_font_open_iconic_all_2x_t);
  for (uint8_t i = 0; i < 18; i++) u8g2.drawGlyph(3 + (i % 6) * 21, 19 + (i / 6) * 21, SMALL[i]);
  u8g2.sendBuffer();
  delay(2500);
}

// Text direction: the same string at 0, 90, 180 and 270 degrees (setFontDirection()).
static void testTextDirections() {
  u8g2.clearBuffer();
  u8g2.setFont(u8g2_font_6x10_tf);
  u8g2.setFontDirection(0);
  u8g2.drawStr(34, 9, "tinkerberg");
  u8g2.setFontDirection(1);
  u8g2.drawStr(118, 2, "tinkerberg");
  u8g2.setFontDirection(2);
  u8g2.drawStr(92, 54, "tinkerberg");
  u8g2.setFontDirection(3);
  u8g2.drawStr(9, 60, "tinkerberg");
  u8g2.setFontDirection(0);
  u8g2.drawStr((SCREEN_WIDTH - u8g2.getStrWidth("Text")) / 2, 33, "Text");
  u8g2.drawStr((SCREEN_WIDTH - u8g2.getStrWidth("directions")) / 2, 45, "directions");
  u8g2.sendBuffer();
  delay(3000);
}

// Graphics primitives: lines, frames, boxes, circles, discs, rounded frames, triangles and
// ellipses, 1.2 s each.
static void testShapes() {
  u8g2.clearBuffer();
  for (int16_t x = 0; x < SCREEN_WIDTH; x += 6) u8g2.drawLine(0, 0, x, SCREEN_HEIGHT - 1);
  for (int16_t y = SCREEN_HEIGHT - 1; y >= 0; y -= 6) u8g2.drawLine(0, 0, SCREEN_WIDTH - 1, y);
  u8g2.sendBuffer();
  delay(1200);

  u8g2.clearBuffer();
  for (int16_t i = 0; i < SCREEN_HEIGHT / 2; i += 4) u8g2.drawFrame(i, i, SCREEN_WIDTH - 2 * i, SCREEN_HEIGHT - 2 * i);
  u8g2.sendBuffer();
  delay(1200);

  u8g2.clearBuffer();
  for (int16_t i = 0; i < SCREEN_HEIGHT / 2; i += 3) {
    u8g2.setDrawColor((i / 3) % 2 == 0 ? 1 : 0);
    u8g2.drawBox(i, i, SCREEN_WIDTH - 2 * i, SCREEN_HEIGHT - 2 * i);
  }
  u8g2.setDrawColor(1);
  u8g2.sendBuffer();
  delay(1200);

  u8g2.clearBuffer();
  for (int16_t r = 4; r < SCREEN_HEIGHT / 2 + 8; r += 5) u8g2.drawCircle(SCREEN_WIDTH / 2, SCREEN_HEIGHT / 2, r);
  u8g2.sendBuffer();
  delay(1200);

  u8g2.clearBuffer();
  u8g2.drawDisc(32, 32, 26);
  u8g2.drawDisc(96, 32, 26);
  u8g2.setDrawColor(0);
  u8g2.drawDisc(32, 32, 14);
  u8g2.drawDisc(96, 32, 14);
  u8g2.setDrawColor(1);
  u8g2.sendBuffer();
  delay(1200);

  u8g2.clearBuffer();
  for (int16_t i = 0; i < SCREEN_HEIGHT / 2 - 2; i += 5) {
    u8g2.drawRFrame(i, i, SCREEN_WIDTH - 2 * i, SCREEN_HEIGHT - 2 * i, SCREEN_HEIGHT / 4);
  }
  u8g2.sendBuffer();
  delay(1200);

  u8g2.clearBuffer();
  for (int16_t i = 5; i < SCREEN_HEIGHT / 2; i += 5) {
    const int16_t cx = SCREEN_WIDTH / 2;
    const int16_t cy = SCREEN_HEIGHT / 2;
    u8g2.drawLine(cx, cy - i, cx - i, cy + i);
    u8g2.drawLine(cx - i, cy + i, cx + i, cy + i);
    u8g2.drawLine(cx + i, cy + i, cx, cy - i);
  }
  u8g2.sendBuffer();
  delay(1200);

  u8g2.clearBuffer();
  u8g2.drawFilledEllipse(32, 32, 28, 14);
  u8g2.drawEllipse(96, 32, 28, 14);
  u8g2.drawEllipse(96, 32, 14, 28);
  u8g2.sendBuffer();
  delay(1200);
}

// XOR draw mode (draw color 2): overlapping shapes and text invert what is underneath
// instead of overwriting it.
static void testXor() {
  u8g2.clearBuffer();
  u8g2.setDrawColor(1);
  u8g2.drawBox(6, 4, 50, 36);
  u8g2.setDrawColor(2);
  u8g2.drawBox(30, 16, 50, 36);
  u8g2.setDrawColor(1);
  u8g2.drawDisc(106, 28, 20);

  u8g2.setFontMode(1);
  u8g2.setFont(u8g2_font_helvB12_tr);
  u8g2.setDrawColor(2);
  u8g2.drawStr(106 - u8g2.getStrWidth("XOR") / 2, 33, "XOR");

  u8g2.setFontMode(0);
  u8g2.setDrawColor(1);
  u8g2.setFont(u8g2_font_6x10_tf);
  const char *label = "XOR draw mode";
  u8g2.drawStr((SCREEN_WIDTH - u8g2.getStrWidth(label)) / 2, 62, label);
  u8g2.sendBuffer();
  delay(3000);
}

// Bitmap: 16x16 XBM heart, drawn as-is (top row) and knocked out of a filled box (bottom row).
static void testBitmap() {
  u8g2.clearBuffer();
  u8g2.setFont(u8g2_font_6x10_tf);
  u8g2.drawStr(0, 9, "Bitmap 16x16");
  u8g2.setBitmapMode(1);
  for (int16_t i = 0; i < 6; i++) u8g2.drawXBM(4 + i * 20, 20, 16, 16, bmpHeart);
  for (int16_t i = 0; i < 6; i++) {
    u8g2.setDrawColor(1);
    u8g2.drawBox(4 + i * 20, 42, 16, 16);
    u8g2.setDrawColor(0);
    u8g2.drawXBM(4 + i * 20, 42, 16, 16, bmpHeart);
  }
  u8g2.setDrawColor(1);
  u8g2.setBitmapMode(0);
  u8g2.sendBuffer();
  delay(2500);
}

// Hardware invert: the controller's invert command (0xA7 / 0xA6) flips the whole image
// without touching display RAM.
static void testInvert() {
  u8g2.clearBuffer();
  u8g2.setFont(u8g2_font_helvB14_tr);
  u8g2.drawStr((SCREEN_WIDTH - u8g2.getStrWidth("INVERT")) / 2, 28, "INVERT");
  u8g2.setFont(u8g2_font_6x10_tf);
  u8g2.drawStr((SCREEN_WIDTH - u8g2.getStrWidth("Hardware invert")) / 2, 46, "Hardware invert");
  u8g2.drawStr((SCREEN_WIDTH - u8g2.getStrWidth("toggles 4x")) / 2, 58, "toggles 4x");
  u8g2.sendBuffer();
  for (uint8_t i = 0; i < 4; i++) {
    u8g2.sendF("c", 0xA7);
    delay(700);
    u8g2.sendF("c", 0xA6);
    delay(700);
  }
}

// Draws the current contrast value as a number and as a bar.
static void drawContrastFrame(uint8_t v) {
  char num[8];
  snprintf(num, sizeof(num), "%u", v);
  u8g2.clearBuffer();
  u8g2.setFont(u8g2_font_6x10_tf);
  u8g2.drawStr(0, 9, "Contrast");
  u8g2.setFont(u8g2_font_helvB14_tr);
  u8g2.drawStr(0, 34, num);
  u8g2.drawFrame(0, 42, SCREEN_WIDTH, 14);
  u8g2.drawBox(0, 42, map(v, 0, 255, 0, SCREEN_WIDTH), 14);
  u8g2.sendBuffer();
}

// Contrast: sweeps setContrast() from 0 up to 255 and back, then restores DEFAULT_CONTRAST.
static void testContrast() {
  for (int16_t v = 0; v <= 255; v += 5) {
    u8g2.setContrast(v);
    drawContrastFrame(v);
    delay(25);
  }
  for (int16_t v = 255; v >= 0; v -= 5) {
    u8g2.setContrast(v);
    drawContrastFrame(v);
    delay(25);
  }
  u8g2.setContrast(DEFAULT_CONTRAST);
}

// U8g2 has no hardware scroll: the picture is redrawn shifted.
// Software scrolling: a banner moves sideways, then three lines move upwards. The image is
// redrawn at every step, so this works on any controller.
static void testScroll() {
  const char *banner = "SOFTWARE SCROLL";
  u8g2.setFont(u8g2_font_helvB12_tr);
  const int16_t bannerW = u8g2.getStrWidth(banner);
  for (int16_t x = SCREEN_WIDTH; x > -bannerW; x -= 2) {
    u8g2.clearBuffer();
    u8g2.drawFrame(0, 0, SCREEN_WIDTH, SCREEN_HEIGHT);
    u8g2.drawStr(x, 38, banner);
    u8g2.sendBuffer();
    delay(12);
  }

  const char *lines[] = {"Software", "scroll", "vertical"};
  for (int16_t y = SCREEN_HEIGHT + 16; y > -3 * 20; y -= 2) {
    u8g2.clearBuffer();
    for (uint8_t i = 0; i < 3; i++) {
      u8g2.drawStr((SCREEN_WIDTH - u8g2.getStrWidth(lines[i])) / 2, y + i * 20, lines[i]);
    }
    u8g2.sendBuffer();
    delay(12);
  }
}

// Rotation: setDisplayRotation() 0..3. Width and height swap for 90 and 270 degrees; the
// triangle marks the top-left corner of each orientation.
static void testRotation() {
  static const u8g2_cb_t *const ROTATIONS[4] = {U8G2_R0, U8G2_R1, U8G2_R2, U8G2_R3};
  for (uint8_t r = 0; r < 4; r++) {
    u8g2.setDisplayRotation(ROTATIONS[r]);
    const int16_t w = u8g2.getDisplayWidth();
    const int16_t h = u8g2.getDisplayHeight();
    char size[16];
    snprintf(size, sizeof(size), "%d x %d", w, h);
    u8g2.clearBuffer();
    u8g2.drawFrame(0, 0, w, h);
    u8g2.drawTriangle(2, 2, 14, 2, 2, 14);
    u8g2.setFont(u8g2_font_5x8_tf);
    u8g2.drawStr(18, 10, "Rotation");
    u8g2.drawStr(18, 22, size);
    u8g2.setFont(u8g2_font_helvB14_tr);
    const char digit[2] = {(char)('0' + r), 0};
    u8g2.drawStr(18, 44, digit);
    u8g2.sendBuffer();
    delay(1500);
  }
  u8g2.setDisplayRotation(U8G2_R0);
}

// Display on/off: setPowerSave() switches the panel off and on three times; the picture
// stays in RAM and reappears unchanged.
static void testPowerSave() {
  u8g2.clearBuffer();
  u8g2.setFont(u8g2_font_helvB14_tr);
  u8g2.drawStr((SCREEN_WIDTH - u8g2.getStrWidth("ON / OFF")) / 2, 28, "ON / OFF");
  u8g2.setFont(u8g2_font_6x10_tf);
  u8g2.drawStr((SCREEN_WIDTH - u8g2.getStrWidth("Content stays in RAM")) / 2, 50, "Content stays in RAM");
  u8g2.sendBuffer();
  delay(800);
  for (uint8_t i = 0; i < 3; i++) {
    u8g2.setPowerSave(1);
    delay(400);
    u8g2.setPowerSave(0);
    delay(400);
  }
}

// Bus speed: times 20 full-frame transfers and reports the frame time, the resulting
// maximum frame rate and the bus clock in use.
static void testTransferSpeed() {
  u8g2.clearBuffer();
  u8g2.drawBox(0, 0, SCREEN_WIDTH, SCREEN_HEIGHT);
  const uint8_t runs = 20;
  const uint32_t t0 = micros();
  for (uint8_t i = 0; i < runs; i++) u8g2.sendBuffer();
  const float ms = (micros() - t0) / 1000.0f / runs;
  const unsigned long busKHz = u8g2.getU8x8()->bus_clock / 1000;

  char line[24];
  u8g2.clearBuffer();
  u8g2.setFont(u8g2_font_6x10_tf);
  u8g2.drawStr(0, 9, "Frame transfer:");
  u8g2.setFont(u8g2_font_helvB14_tr);
  snprintf(line, sizeof(line), "%.1f ms", ms);
  u8g2.drawStr(0, 30, line);
  u8g2.setFont(u8g2_font_6x10_tf);
  snprintf(line, sizeof(line), "max. %.0f FPS", 1000.0f / ms);
  u8g2.drawStr(0, 44, line);
  snprintf(line, sizeof(line), "Bus %lu kHz", busKHz);
  u8g2.drawStr(0, 58, line);
  u8g2.sendBuffer();
  Serial.printf("    Frame transfer: %.2f ms (max. %.0f FPS), bus %lu kHz\n", ms, 1000.0f / ms, busKHz);
  delay(3000);
}

// Animation: a dot bounces for 5 s (clear, draw, transfer per frame); the measured frame
// rate shows the real speed of the whole loop.
static void testAnimation() {
  const int16_t r = 4;
  int16_t x = 20, y = 12, dx = 3, dy = 2;
  uint32_t frames = 0;
  const uint32_t start = millis();
  while (millis() - start < 5000) {
    u8g2.clearBuffer();
    u8g2.drawFrame(0, 0, SCREEN_WIDTH, SCREEN_HEIGHT);
    u8g2.drawDisc(x, y, r);
    u8g2.sendBuffer();
    x += dx;
    y += dy;
    if (x <= r + 1 || x >= SCREEN_WIDTH - r - 2)  dx = -dx;
    if (y <= r + 1 || y >= SCREEN_HEIGHT - r - 2) dy = -dy;
    frames++;
  }
  const float fps = frames * 1000.0f / (millis() - start);
  Serial.printf("    Animation: %.1f FPS\n", fps);

  char line[16];
  snprintf(line, sizeof(line), "%.1f FPS", fps);
  u8g2.clearBuffer();
  u8g2.setFont(u8g2_font_6x10_tf);
  u8g2.drawStr(0, 9, "Animation");
  u8g2.setFont(u8g2_font_helvB14_tr);
  u8g2.drawStr(0, 32, line);
  u8g2.sendBuffer();
  delay(1500);
}

struct TestEntry {
  const char *name;
  void (*run)();
};

// Test list: name for the serial log, function to run. Order = order on the display.
static const TestEntry TESTS[] = {
  {"Resolution",            testResolution},
  {"All pixels off/on",     testFillAndClear},
  {"Checkerboard",          testCheckerboard},
  {"Fonts",                 testFonts},
  {"UTF-8 and symbols",     testUtf8},
  {"Icons",                 testIcons},
  {"Text directions",       testTextDirections},
  {"Graphics primitives",   testShapes},
  {"XOR drawing mode",      testXor},
  {"Bitmap",                testBitmap},
  {"Hardware invert",       testInvert},
  {"Contrast 0..255",       testContrast},
  {"Software scrolling",    testScroll},
  {"Rotation 0..3",         testRotation},
  {"Display on/off",        testPowerSave},
  {"Bus speed",             testTransferSpeed},
  {"Animation / FPS",       testAnimation},
};
constexpr size_t TEST_COUNT = sizeof(TESTS) / sizeof(TESTS[0]);

void setup() {
  Serial.begin(115200);
  delay(300);
  Serial.println();
  Serial.println("=== SH1106 OLED Test (128x64) ===");
#if OLED_INTERFACE == OLED_INTERFACE_I2C
  Serial.printf("Mode: I2C  (SDA=GPIO%d, SCL=GPIO%d, RES=GPIO%d)\n", PIN_SDA, PIN_SCL, PIN_OLED_RST);
  Wire.begin(PIN_SDA, PIN_SCL);
  const uint8_t addr = waitForI2cDisplay();
  Serial.printf("Display found at 0x%02X\n", addr);
  u8g2.setI2CAddress(addr << 1);
#else
  Serial.printf("Mode: SPI  (SCK=GPIO18, MOSI=GPIO23, CS=GPIO%d, DC=GPIO%d, RES=GPIO%d)\n",
                PIN_OLED_CS, PIN_OLED_DC, PIN_OLED_RST);
  Serial.println("Note: with SPI the display cannot be detected - check the screen visually.");
#endif
  u8g2.begin();
}

void loop() {
  runCount++;
  Serial.printf("\n--- Run %lu ---\n", (unsigned long)runCount);
  showLogo();

  for (size_t i = 0; i < TEST_COUNT; i++) {
    Serial.printf("[%u/%u] %s\n", (unsigned)(i + 1), (unsigned)TEST_COUNT, TESTS[i].name);
    TESTS[i].run();
  }

  char line[16];
  snprintf(line, sizeof(line), "Run #%lu", (unsigned long)runCount);
  u8g2.clearBuffer();
  u8g2.setFont(u8g2_font_helvB14_tr);
  u8g2.drawStr((SCREEN_WIDTH - u8g2.getStrWidth("TEST OK")) / 2, 26, "TEST OK");
  u8g2.setFont(u8g2_font_6x10_tf);
  u8g2.drawStr((SCREEN_WIDTH - u8g2.getStrWidth(line)) / 2, 42, line);
  u8g2.drawStr((SCREEN_WIDTH - u8g2.getStrWidth("next run in 3 s")) / 2, 58, "next run in 3 s");
  u8g2.sendBuffer();
  Serial.println("Run finished.");
  delay(3000);
}
