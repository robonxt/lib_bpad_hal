// =============================================================================
//  compass_test.ino — bpad_hal MMC5603NJ Magnetometer Example
// =============================================================================
//  Reads geomagnetic field data (uT) and computes calibrated compass heading.
//  Present on bpad 185 at I2C address 0x30.
// =============================================================================

#include <Wire.h>
#include <bpad_hal.h>

const bpadConfig hw = bpadConfig::defaultBoard();
bpadCompass compass(hw.compass);

void setup() {
  Serial.begin(115200);
#if ARDUINO_USB_CDC_ON_BOOT
  Serial.setTxTimeoutMs(0);
#endif
  delay(500);

  Serial.printf("[compass_test] board=%s  device=%s  addr=0x%02X\n",
    hw.name, hw.compass.device, hw.compass.address);

  Wire.begin(hw.i2c.sda, hw.i2c.scl);
  Wire.setClock(hw.i2c.clock);

  if (!compass.begin(Wire)) {
    Serial.println("[compass_test] FAILED to initialize MMC5603NJ!");
    while (1) delay(100);
  }

  compass.setAutoCalibration(true);

  Serial.println("[compass_test] MMC5603NJ ready (auto-calibration active).");
  Serial.println("Rotate the board in a figure-8 to calibrate and observe heading.");
}

void loop() {
  if (compass.update()) {
    Serial.printf("X:%6.1f uT  Y:%6.1f uT  Z:%6.1f uT  | Heading: %5.1f deg\n",
      compass.x(), compass.y(), compass.z(), compass.heading());
  }

  delay(100);
}
