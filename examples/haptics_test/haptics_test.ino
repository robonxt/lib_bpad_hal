// =============================================================================
//  haptics_test.ino — bpad_hal haptics test
// =============================================================================
//  Tests all 4 vibration motors on bpad 185 (MCP4728 DAC 0x60 in X-format):
//    Ch 0: Bottom-Left  (White wires)
//    Ch 1: Bottom-Right (Black wires)
//    Ch 2: Top-Left     (Red wires)
//    Ch 3: Top-Right    (Blue wires)
// =============================================================================

#include <Wire.h>
#include <bpad_hal.h>

const bpadConfig hw = bpadConfig::defaultBoard();
bpadHaptics haptics(hw.haptics);

void setup() {
  Serial.begin(115200);
#if ARDUINO_USB_CDC_ON_BOOT
  Serial.setTxTimeoutMs(0);
#endif
  delay(500);

  Serial.printf("[haptics_test] board=%s  backend=%s  count=%u\n",
    hw.name,
    bpadHapticsBackendName(hw.haptics.backend),
    hw.haptics.count);

  Wire.begin(hw.i2c.sda, hw.i2c.scl);
  Wire.setClock(hw.i2c.clock);

  if (!haptics.begin(Wire)) {
    Serial.println("[haptics_test] Haptics init FAILED!");
  } else {
    Serial.println("[haptics_test] Haptics ready");
  }

  Serial.println("\n--- bpad 185 Quad Haptics (X Layout) ---");
  Serial.println("  Top-Left     (Red)   = Ch 2");
  Serial.println("  Top-Right    (Blue)  = Ch 3");
  Serial.println("  Bottom-Left  (White) = Ch 0");
  Serial.println("  Bottom-Right (Black) = Ch 1\n");
}

void loop() {
  // 1. Test individual motors in sequence
  const bpadMotor motors[] = {
    bpadMotor::TopLeft,
    bpadMotor::TopRight,
    bpadMotor::BottomRight,
    bpadMotor::BottomLeft
  };

  for (bpadMotor m : motors) {
    Serial.printf("  Testing %s\n", bpadMotorName(m));
    haptics.pulse(m, 200, hw.haptics.dacOnValue);

    while (haptics.isActive(m)) {
      haptics.update();
      delay(5);
    }
    delay(300);
  }

  delay(600);

  // 2. Test directional pairs
  Serial.println("  Testing TOP pair (TL + TR)...");
  haptics.pulseTop(200, hw.haptics.dacOnValue);
  while (haptics.isAnyActive()) { haptics.update(); delay(5); }
  delay(400);

  Serial.println("  Testing BOTTOM pair (BL + BR)...");
  haptics.pulseBottom(200, hw.haptics.dacOnValue);
  while (haptics.isAnyActive()) { haptics.update(); delay(5); }
  delay(400);

  Serial.println("  Testing LEFT pair (TL + BL)...");
  haptics.pulseLeft(200, hw.haptics.dacOnValue);
  while (haptics.isAnyActive()) { haptics.update(); delay(5); }
  delay(400);

  Serial.println("  Testing RIGHT pair (TR + BR)...");
  haptics.pulseRight(200, hw.haptics.dacOnValue);
  while (haptics.isAnyActive()) { haptics.update(); delay(5); }
  delay(400);

  // 3. Test all motors
  Serial.println("  Testing ALL motors...");
  haptics.pulseAll(250, hw.haptics.dacOnValue);
  while (haptics.isAnyActive()) { haptics.update(); delay(5); }

  Serial.println("Cycle complete.\n");
  delay(2000);
}
