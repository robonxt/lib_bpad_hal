// =============================================================================
//  display_test.ino — bpad_hal display test
// =============================================================================
//  Tests circular display rendering on bpad 185 (360x360 Round QSPI ST77916)
//  using native hardware-accelerated esp_lcd.
// =============================================================================

#include <Arduino.h>
#include <bpad_hal.h>

const bpadConfig hw = bpadConfig::defaultBoard();
bpadDisplay display(hw.display);

void setup() {
  Serial.begin(115200);
#if ARDUINO_USB_CDC_ON_BOOT
  Serial.setTxTimeoutMs(0);
#endif
  delay(500);

  Serial.printf("[display_test] board=%s  backend=%s  res=%ux%u\n",
    hw.name,
    bpadDisplayBackendName(hw.display.backend),
    hw.display.width,
    hw.display.height);

  if (!display.begin()) {
    Serial.println("[display_test] Display initialization failed!");
    return;
  }

  // Draw concentric color rings pattern into allocated PSRAM buffer
  uint16_t* fb = (uint16_t*)heap_caps_malloc(hw.display.width * hw.display.height * sizeof(uint16_t), MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT);
  if (!fb) {
    Serial.println("[display_test] Failed to allocate PSRAM frame buffer!");
    return;
  }

  int cx = hw.display.width / 2;
  int cy = hw.display.height / 2;
  int rMax = hw.display.width / 2;

  // 16-bit RGB565 test palette (Big Endian swapped for ST77916 / GC9A01)
  const uint16_t colors[] = {
    0x00F8, // Red (bytes: 0xF8, 0x00)
    0xE007, // Green (bytes: 0x07, 0xE0)
    0x1F00, // Blue (bytes: 0x00, 0x1F)
    0xFFFD, // Yellow
    0x1FE0, // Cyan
    0x1FF8, // Magenta
    0xFFFF  // White
  };
  int numColors = sizeof(colors) / sizeof(colors[0]);

  for (int y = 0; y < hw.display.height; y++) {
    for (int x = 0; x < hw.display.width; x++) {
      int dx = x - cx;
      int dy = y - cy;
      int dist = (int)sqrt(dx * dx + dy * dy);
      if (dist >= rMax) {
        fb[y * hw.display.width + x] = 0x0000; // Black outside circular active area
      } else {
        int ringIdx = (dist / 18) % numColors;
        fb[y * hw.display.width + x] = colors[ringIdx];
      }
    }
  }

  display.drawFrame(fb);
  Serial.println("[display_test] Framebuffer rendered to screen.");
}

void loop() {
  // Cycle brightness
  for (int b = 0; b <= 255; b += 5) {
    display.setBrightness(b);
    delay(25);
  }
  for (int b = 255; b >= 0; b -= 5) {
    display.setBrightness(b);
    delay(25);
  }
}
