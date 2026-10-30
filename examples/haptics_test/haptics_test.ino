// =============================================================================
//  haptics_test.ino — bpad_hal haptics test
// =============================================================================
//  Pulses each of the 4 haptic motors one at a time on bpad 185 (MCP4728 DAC 0x60).
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

  Serial.println("[haptics_test] pulsing each motor...");
}

void loop() {
  for (uint8_t i = 0; i < hw.haptics.count; i++) {
    Serial.printf("  motor %u ON\n", i);
    haptics.pulse(i, 200, hw.haptics.dacOnValue);

    while (haptics.isActive(i)) {
      haptics.update();
      delay(5);
    }
    delay(300);
  }

  Serial.println("  pulse ALL motors together");
  haptics.pulseAll(250, hw.haptics.dacOnValue);
  while (haptics.isAnyActive()) {
    haptics.update();
    delay(5);
  }

  delay(2000);
}
