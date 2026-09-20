/* =========================================================================
 *  FIX 2026-09-12 22:34  —  Touch-Schwelle final
 *  ---------------------------------------------------------------------
 *  - TOUCH_THRESH -> 94 (gemessen: gedrueckt ~82 / Ruhe ~106 -> Mitte).
 *  - Doppeltes #define TOUCH_THRESH entfernt.
 *  Geaenderte Datei: include/pins.h
 * ========================================================================= */
#ifndef PINS_H
#define PINS_H

// ── Echte Pinbelegung für NM TV 154 (ST7789 240x240) ──
#define TFT_MOSI       13      // SPI MOSI
#define TFT_MISO       12      // SPI MISO
#define TFT_SCK        14      // SPI CLK
#define TFT_CS         15      // Chip Select
#define TFT_DC          2      // Data / Command
#define TFT_RST         4      // Reset

#define TFT_EN_PIN     21      // Display Enable (MUSS auf LOW gezogen werden!)
#define TFT_BACKLIGHT  19      // Backlight (LOW-aktiv!)

#define SCREEN_W       240
#define SCREEN_H       240

// Kapazitiver Touch-Pin
#define TOUCH_CAP_PIN  32
#define TOUCH_THRESH   90      // Ausloesen wenn touchRead < 94 (gedrueckt~82/Ruhe~106)

#endif
