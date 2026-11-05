// =============================================================================
//  display_te_test.ino - bpad Display VSync & Tearing Effect (TE) Diagnostic
// =============================================================================
//  Tests ST77916 LCD-TE pin on GPIO 41:
//  - Enables hardware TE pulse via command 0x35
//  - Synchronizes frame rendering with V-Blank interval
//  - Compares vsync-synchronized animation vs unsynchronized drawing
// =============================================================================

#include <Arduino.h>
#include <bpad_hal.h>

const bpadConfig hw = bpadConfig::defaultBoard();
bpadDisplay display(hw.display);

// Full-frame double buffer (PSRAM allocated)
static uint16_t* frameBuffer = nullptr;
static int barX = 0;
static int barSpeed = 6;
static bool syncEnabled = true;
static uint32_t frameCount = 0;
static uint32_t lastFpsMs = 0;

void setup() {
  Serial.begin(115200);
  delay(1000);

  Serial.println("\n==============================================");
  Serial.println("       bpad ST77916 TE / VSync Test           ");
  Serial.println("==============================================");
  Serial.printf("TE Hardware Pin: GPIO %d\n", hw.display.te);

  // Allocate 360x360 16-bit RGB565 frame buffer in PSRAM
  frameBuffer = (uint16_t*)ps_malloc(360 * 360 * sizeof(uint16_t));
  if (!frameBuffer) {
    frameBuffer = (uint16_t*)malloc(360 * 360 * sizeof(uint16_t));
  }
  if (!frameBuffer) {
    Serial.println("[ERROR] Failed to allocate frame buffer!");
    while (1) delay(100);
  }
  memset(frameBuffer, 0, 360 * 360 * sizeof(uint16_t));

  if (!display.begin()) {
    Serial.println("[ERROR] Display initialization failed!");
    while (1) delay(100);
  }

  // Enable hardware TE signal from ST77916
  display.enableTE(true);

  Serial.println("\nControls (via Serial Monitor):");
  Serial.println("  - Type 'sync'   : Toggle VSync alignment ON/OFF");
  Serial.println("  - Type 'b <val>': Adjust brightness (e.g. 'b 100' or 'b 200')\n");

  lastFpsMs = millis();
}

void renderSceneToBuffer(int xPos, uint16_t barColor) {
  // Draw circular scene into memory buffer
  const int cx = 180;
  const int cy = 180;
  const int r2 = 178 * 178;

  for (int y = 0; y < 360; y++) {
    int dy = y - cy;
    int dy2 = dy * dy;
    uint16_t* row = frameBuffer + (y * 360);

    for (int x = 0; x < 360; x++) {
      int dx = x - cx;
      if ((dx * dx + dy2) <= r2) {
        // Inside circle
        if (x >= xPos && x < xPos + 24) {
          row[x] = barColor;
        } else {
          row[x] = 0x18E3; // Dark slate background
        }
      } else {
        row[x] = 0x0000; // Black outside bezel
      }
    }
  }
}

void loop() {
  // Check for serial toggle
  if (Serial.available()) {
    String cmd = Serial.readStringUntil('\n');
    cmd.trim();
    if (cmd.equalsIgnoreCase("sync")) {
      syncEnabled = !syncEnabled;
      Serial.printf("[TE] VSync Synchronization: %s\n", syncEnabled ? "ENABLED" : "DISABLED");
    } else if (cmd.startsWith("b ")) {
      uint8_t b = cmd.substring(2).toInt();
      display.setBrightness(b);
      Serial.printf("[DISPLAY] Brightness set to: %d\n", b);
    }
  }

  // 1. Prepare next frame in memory while display/previous frame finishes
  renderSceneToBuffer(barX, syncEnabled ? 0x07E0 : 0xF800);

  // 2. Wait for VSync right before pushing pixel data over QSPI
  if (syncEnabled) {
    display.waitForVSync(30);
  }

  // 3. Blit memory buffer to panel
  display.drawBitmap(0, 0, 360, 360, frameBuffer);

  barX += barSpeed;
  if (barX > 340 || barX < 0) {
    barSpeed = -barSpeed;
  }

  frameCount++;
  if (millis() - lastFpsMs >= 1000) {
    Serial.printf("[PERF] FPS: %lu | VSync: %s\n", frameCount, syncEnabled ? "ON" : "OFF");
    frameCount = 0;
    lastFpsMs = millis();
  }
}
