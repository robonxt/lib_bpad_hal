// =============================================================================
//  imu_test.ino — bpad_hal IMU test
// =============================================================================
//  Tests ICM-42607-P 6-axis motion sensor on bpad 185 (0x69, -90 deg rotation).
// =============================================================================

#include <Wire.h>
#include <bpad_hal.h>

const bpadConfig hw = bpadConfig::defaultBoard();
bpadIMU imu(hw.imu);

void setup() {
  Serial.begin(115200);
#if ARDUINO_USB_CDC_ON_BOOT
  Serial.setTxTimeoutMs(0);
#endif
  delay(500);

  Serial.printf("[imu_test] board=%s  backend=%s  addr=0x%02X  offset=%d deg\n",
    hw.name,
    bpadIMUBackendName(hw.imu.backend),
    hw.imu.address,
    hw.imu.orientationOffsetDeg);

  Wire.begin(hw.i2c.sda, hw.i2c.scl);
  Wire.setClock(hw.i2c.clock);

  if (!imu.begin(Wire)) {
    Serial.printf("[imu_test] IMU init FAILED (no response from %s at 0x%02X on SDA=%u SCL=%u)\n",
      bpadIMUBackendName(hw.imu.backend),
      hw.imu.address,
      hw.i2c.sda,
      hw.i2c.scl);
    return;
  }

  Serial.println("[imu_test] IMU initialized. Calibrating neutral position (keep device still)...");
  if (imu.calibrateNeutral(100, 5)) {
    Serial.println("[imu_test] Calibration complete!");
  } else {
    Serial.println("[imu_test] Calibration failed!");
  }

  Serial.println("Reading IMU at ~20 Hz...");
}

void loop() {
  if (imu.update()) {
    float ax, ay, az;
    float gx, gy, gz;
    float pitch, roll, yaw;

    imu.getAccel(ax, ay, az);
    imu.getGyro(gx, gy, gz);
    imu.getAttitude(pitch, roll, yaw);

    Serial.printf("[IMU] A: %+6.2fg %+6.2fg %+6.2fg | G: %+7.1f %+7.1f %+7.1f dps | Att: P=%+5.1f R=%+5.1f Y=%+6.1f deg\n",
      ax, ay, az,
      gx, gy, gz,
      pitch, roll, yaw);

    if (imu.isShaking(3.5f)) {
      Serial.println("  >>> SHAKE DETECTED <<<");
    }
  }

  delay(50);
}
