// =============================================================================
//  compass_test.ino — bpad_hal MMC5603NJ Magnetometer Example
// =============================================================================
//  Reads geomagnetic field data (uT) and computes calibrated compass heading.
//  Note: Unpopulated on bpad 185 V1 board.
// =============================================================================

#include <Wire.h>
#include <bpad_hal.h>

const bpad::Config hw = bpad::Config::defaultBoard();
bpad::Compass compass(hw.compass);

void setup() {
  Serial.begin(115200);
#if ARDUINO_USB_CDC_ON_BOOT
  Serial.setTxTimeoutMs(0);
#endif
  delay(500);

  Serial.printf("[compass_test] board=%s  device=%s  addr=0x%02X  status=%s\n",
    hw.name, hw.compass.device, hw.compass.address, bpad::supportName(hw.compass.support));

  if (hw.compass.support == bpad::Support::NotPresent) {
    Serial.println("[compass_test] Notice: Compass is not populated on this board profile.");
    if (hw.compass.note && strlen(hw.compass.note) > 0) {
      Serial.printf("[compass_test] Note: %s\n", hw.compass.note);
    }
    Serial.println("[compass_test] Exiting test.");
    return;
  }

  Wire.begin(hw.i2c.sda, hw.i2c.scl);
  Wire.setClock(hw.i2c.clock);
  Wire.setTimeOut(50);

  if (!compass.begin(Wire)) {
    Serial.println("[compass_test] FAILED to initialize MMC5603NJ!");
    while (1) delay(100);
  }

  compass.setAutoCalibration(true);

  Serial.println("[compass_test] MMC5603NJ ready (auto-calibration active).");
  Serial.println("Rotate the board in a figure-8 to calibrate and observe heading.");
}

void loop() {
  if (hw.compass.support == bpad::Support::NotPresent) {
    delay(1000);
    return;
  }

  if (compass.update()) {
    Serial.printf("X:%6.1f uT  Y:%6.1f uT  Z:%6.1f uT  | Heading: %5.1f deg\n",
      compass.x(), compass.y(), compass.z(), compass.heading());
  }

  delay(100);
}
