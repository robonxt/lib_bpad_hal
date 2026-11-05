// =============================================================================
//  touch_test.ino - bpad CST816S Capacitive Touch Diagnostic
// =============================================================================
//  Verifies touch coordinate tracking, gesture detection, and press states.
// =============================================================================

#include <Arduino.h>
#include <bpad_hal.h>

const bpadConfig hw = bpadConfig::defaultBoard();
bpadTouch touch(hw.touch);

void setup() {
  Serial.begin(115200);
  delay(1000);

  Serial.println("\n==============================================");
  Serial.println("           bpad CST816S Touch Test            ");
  Serial.println("==============================================");

  Wire.begin(hw.i2c.sda, hw.i2c.scl, hw.i2c.clock);

  if (!touch.begin(Wire)) {
    Serial.println("[ERROR] CST816S touch controller not detected on I2C bus!");
    while (1) delay(100);
  }

  Serial.println("[TOUCH] CST816S initialized successfully. Touch the circular screen!\n");
}

void loop() {
  bpadTouchData td = touch.read();

  if (td.pressed) {
    Serial.printf("[TOUCH] Pressed at X: %3u, Y: %3u | Delta: (%+3d, %+3d) | Gesture: %s\n",
                  td.x, td.y, td.deltaX, td.deltaY, bpadGestureName(td.gesture));
  }

  delay(15);
}
