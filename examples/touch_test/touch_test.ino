// =============================================================================
//  touch_test.ino — bpad_hal touch test
// =============================================================================
//  Tests capacitive touch coordinates and gestures on bpad 185 (CST816S).
//
//  TOGGLE MODES:
//  1. Compile-time: Select default mode below.
//  2. Runtime: Type 'n' (Native Hardware) or 'c' (Custom Software) in Serial Monitor.
// =============================================================================

#include <Wire.h>
#include <bpad_hal.h>

// --- Default Gesture Mode Toggle ---
// Uncomment ONE of these:
#define GESTURE_MODE_NATIVE_HARDWARE   // Pure CST816 on-chip DSP
// #define GESTURE_MODE_CUSTOM_SOFTWARE   // Custom software distance/timing engine

const bpadConfig hw = bpadConfig::defaultBoard();
bpadTouch touch(hw.touch);

void printActiveMode() {
  Serial.printf("\n>>> ACTIVE TOUCH MODE: [%s] <<<\n",
    bpadTouchModeName(touch.getTouchMode()));
  if (touch.getTouchMode() == bpadTouchMode::NativeHardware) {
    Serial.println("    Uses CST816 on-chip hardware gesture detector with edge-triggering.");
  } else {
    Serial.println("    Uses custom software timing, delta tracking, and swipe engine.");
  }
  Serial.println("    (Send 'n' for Native Hardware, 'c' for Custom Software in Serial)\n");
}

void setup() {
  Serial.begin(115200);
#if ARDUINO_USB_CDC_ON_BOOT
  Serial.setTxTimeoutMs(0);
#endif
  delay(500);

  Serial.printf("[touch_test] board=%s  controller=%s  addr=0x%02X  rst=%d  irq=%d\n",
    hw.name,
    hw.touch.controller,
    hw.touch.address,
    hw.touch.rst,
    hw.touch.irq);

  Wire.begin(hw.i2c.sda, hw.i2c.scl);
  Wire.setClock(hw.i2c.clock);

  if (!touch.begin(Wire)) {
    Serial.println("[touch_test] Touch controller init FAILED (check I2C bus)");
    return;
  }
  Serial.println("[touch_test] Touch controller ready.");

#if defined(GESTURE_MODE_CUSTOM_SOFTWARE)
  touch.setTouchMode(bpadTouchMode::CustomSoftware);
#else
  touch.setTouchMode(bpadTouchMode::NativeHardware);
#endif

  printActiveMode();
}

void loop() {
  // Runtime mode switching via Serial Monitor
  if (Serial.available()) {
    char ch = Serial.read();
    if (ch == 'n' || ch == 'N') {
      touch.setTouchMode(bpadTouchMode::NativeHardware);
      printActiveMode();
    } else if (ch == 'c' || ch == 'C') {
      touch.setTouchMode(bpadTouchMode::CustomSoftware);
      printActiveMode();
    }
  }

  static bool lastPressed = false;
  static uint16_t lastX = 0, lastY = 0;

  bpadTouchData td = touch.read();

  // 1. Press / Release Events
  if (td.pressed != lastPressed) {
    lastPressed = td.pressed;
    if (td.pressed) {
      lastX = td.x;
      lastY = td.y;
      Serial.printf("[TOUCH] PRESS   x=%3u y=%3u\n", td.x, td.y);
    } else {
      Serial.printf("[TOUCH] RELEASE (last x=%3u y=%3u)\n", lastX, lastY);
    }
  }

  // 2. Drag / Movement Tracking (only while pressed and moved > 3px)
  if (td.pressed) {
    if (abs((int)td.x - (int)lastX) > 3 || abs((int)td.y - (int)lastY) > 3) {
      lastX = td.x;
      lastY = td.y;
      Serial.printf("[TOUCH] DRAG    x=%3u y=%3u  deltaX=%+d deltaY=%+d\n",
        td.x, td.y, td.deltaX, td.deltaY);
    }
  }

  // 3. Gesture Events (Edge-triggered: exactly once per gesture)
  if (td.gesture != bpadGesture::None) {
    Serial.printf("[TOUCH] GESTURE: %s (code 0x%02X)\n",
      bpadGestureName(td.gesture), td.rawGesture);
  }

  delay(10);
}
