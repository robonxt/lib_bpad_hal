// =============================================================================
//  touch_test.ino — bpad_hal touch test
// =============================================================================
//  Tests capacitive touch coordinates and gestures on bpad 185 (CST816S).
// =============================================================================

#include <Wire.h>
#include <bpad_hal.h>

const bpadConfig hw = bpadConfig::defaultBoard();
bpadTouch touch(hw.touch);

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
  } else {
    Serial.println("[touch_test] Touch controller ready");
  }

  Serial.println("Touch or swipe the screen to test.");
}

void loop() {
  static bool lastPressed = false;
  static uint16_t lastX = 0, lastY = 0;

  bpadTouchData td = touch.read();

  if (td.pressed != lastPressed) {
    lastPressed = td.pressed;
    if (td.pressed) {
      Serial.printf("[TOUCH] PRESS  x=%3u y=%3u\n", td.x, td.y);
    } else {
      Serial.printf("[TOUCH] RELEASE (last x=%3u y=%3u)\n", lastX, lastY);
    }
  }

  if (td.pressed && (abs((int)td.x - (int)lastX) > 2 || abs((int)td.y - (int)lastY) > 2)) {
    lastX = td.x;
    lastY = td.y;
    Serial.printf("[TOUCH] DRAG   x=%3u y=%3u  deltaX=%+d deltaY=%+d\n",
      td.x, td.y, td.deltaX, td.deltaY);
  }

  if (td.gesture != bpadGesture::None) {
    Serial.printf("[TOUCH] GESTURE: %s (code 0x%02X)\n",
      bpadGestureName(td.gesture), td.rawGesture);
  }

  delay(10);
}
