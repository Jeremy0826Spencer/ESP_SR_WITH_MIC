
#ifndef PINS_H
#define PINS_H
/*
// ──────────────────────────────────────────────────────────────────────────────
// pins.h — Central pin map and timing constants for the project
//
// What this file does
// -------------------
// Defines all GPIO assignments and key timing/buffer parameters used across
// the firmware:
//   • ST7796 SPI display pins and LVGL/display timing/buffer sizes
//   • FT5x06/FT6336U touch I²C pins (INT/RST)
//   • I²S microphone (INMP441) pins + audio format used by ESP-SR
//
// Notes
// -----
// • Adjust these definitions to match your hardware wiring.
// • PIN_* = -1 means “unused/not connected”.
// • LVGL_BUF_SIZE is sized as (HRES * lines). Increase lines for more throughput
//   (more RAM) or reduce to save RAM.
// • LCD_PIXEL_CLOCK_HZ should be within panel + wiring limits (SPI clock).
// ──────────────────────────────────────────────────────────────────────────────


// ===== Display (ST7796 over SPI) =============================================
// SPI signals: SCK, MOSI, (MISO optional), CS; plus DC, RST, and backlight.
#define PIN_LCD_SCK   12   // SPI clock to display
#define PIN_LCD_MOSI  11   // SPI MOSI (data to panel)
#define PIN_LCD_MISO  -1   // SPI MISO (readback). -1 = write-only
#define PIN_LCD_CS    10   // Panel chip-select
#define PIN_LCD_DC    48   // Data/Command (aka RS)
#define PIN_LCD_RST   47   // Hardware reset for panel
#define PIN_LCD_BL    45   // Backlight enable (active HIGH on most boards)

// Logical resolution and timing for LVGL/esp_lcd
#define LCD_HRES              480                 // horizontal pixels
#define LCD_VRES              320                 // vertical pixels
#define LCD_PIXEL_CLOCK_HZ    (26 * 1000 * 1000)  // SPI clock used by panel IO

// LVGL tick and draw buffer sizing
#define LVGL_TICK_PERIOD_MS   5                   // lv_tick_inc() period
#define LCD_BUF_LINES         30                  // number of lines per draw
#define LVGL_BUF_SIZE         (LCD_HRES * LCD_BUF_LINES) // pixels in buffer


// ===== Touch (FT5x06 / FT6336U over I²C) =====================================
// I²C pins and optional interrupt/reset for the touch controller.
#define TP_SDA   9    // I²C SDA
#define TP_SCL   8    // I²C SCL
#define TP_INT   3    // Touch interrupt (active LOW, typically)
#define TP_RST   46   // Touch controller reset (optional)

*/
// ===== I²S microphone (INMP441) ==============================================
// INMP441 wiring: VDD=3V3, GND=GND, L/R pin to GND selects Left channel.
// MCLK is not required by INMP441 on ESP32; leave at -1.
//
// ESP32-S3 I²S standard mode pins below are an example mapping; change if
// your board routes them differently.
#define I2S_MCLK   -1   // Not used by INMP441
#define I2S_BCLK   14   // Bit clock (SCK/BCLK)
#define I2S_WS     15   // Word select (LRCLK)
#define I2S_DIN    16   // Data in to ESP32 (from mic DOUT)

// Audio capture parameters for ESP-SR AFE pipeline
#define SR_SAMPLE_RATE    16000  // 16 kHz sample rate for speech/wake word
#define SR_CHANNELS       1      // Mono
#define SR_BITS_PER_SAMP  36     // Read 24/32-bit from mic; downshift to 16-bit later

#endif // PINS_H
