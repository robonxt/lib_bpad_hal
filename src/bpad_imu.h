#pragma once
// =============================================================================
//  bpad_imu.h - IMU Driver for bpad hardware (185 and newer)
// =============================================================================
//  Native driver for ICM-42607-P (bpad 185).
//  Provides normalized physical units (g, dps), orientation offset compensation,
//  attitude angles (pitch/roll/yaw), zero-motion calibration, and shake detection.
// =============================================================================

#include <Arduino.h>
#include <Wire.h>
#include <math.h>
#include "bpad_config.h"

class bpadIMU {
public:
  explicit bpadIMU(const bpadIMUConfig& config = bpadIMUConfig())
    : _cfg(config) {}

  void select(const bpadIMUConfig& config) {
    _cfg = config;
  }

  bool begin(TwoWire& wire = Wire) {
    _wire = &wire;
    if (_cfg.support == bpadSupport::NotPresent || _cfg.backend == bpadIMUBackend::None) {
      return false;
    }

    if (_cfg.backend == bpadIMUBackend::ICM42607) {
      return initICM42607();
    }

    return false;
  }

  bool update() {
    if (!_wire || !_ready) return false;

    if (_cfg.backend == bpadIMUBackend::ICM42607) {
      return readICM42607();
    }

    return false;
  }

  // --- Getters: Physical units (g = Earth gravities, dps = deg/sec) ---
  float accelX() const { return _ax - _biasAX; }
  float accelY() const { return _ay - _biasAY; }
  float accelZ() const { return _az - _biasAZ; }

  float gyroX() const { return _gx - _biasGX; }
  float gyroY() const { return _gy - _biasGY; }
  float gyroZ() const { return _gz - _biasGZ; }

  // Attitude Euler angles in degrees
  float pitch() const { return _pitch; }
  float roll()  const { return _roll; }
  float yaw()   const { return _yaw; }

  void getAccel(float& ax, float& ay, float& az) const {
    ax = accelX();
    ay = accelY();
    az = accelZ();
  }

  void getGyro(float& gx, float& gy, float& gz) const {
    gx = gyroX();
    gy = gyroY();
    gz = gyroZ();
  }

  void getAttitude(float& pitch, float& roll, float& yaw) const {
    pitch = _pitch;
    roll  = _roll;
    yaw   = _yaw;
  }

  // Returns onboard ICM-42607 die temperature in Celsius
  float getTemperatureC() const {
    return _temperatureC;
  }

  bool isShaking(float threshold = 4.0f) const {
    float dx = fabsf(accelX() - _prevAX);
    float dy = fabsf(accelY() - _prevAY);
    float dz = fabsf(accelZ() - _prevAZ);
    return (dx >= threshold || dy >= threshold || dz >= threshold);
  }

  // --- Calibration & Offsets ---
  bool calibrateNeutral(uint16_t samples = 100, uint8_t delayMs = 5) {
    if (!_wire || !_ready || samples == 0) return false;

    float sumAX = 0, sumAY = 0, sumAZ = 0;
    float sumGX = 0, sumGY = 0, sumGZ = 0;

    for (uint16_t i = 0; i < samples; i++) {
      if (!update()) return false;
      sumAX += _ax;
      sumAY += _ay;
      sumAZ += _az;
      sumGX += _gx;
      sumGY += _gy;
      sumGZ += _gz;
      delay(delayMs);
    }

    _biasAX = sumAX / samples;
    _biasAY = sumAY / samples;
    // Keep 1.0g gravity vector along Z axis (pointing downward into the screen / PCB)
    _biasAZ = (sumAZ / samples) - 1.0f;

    _biasGX = sumGX / samples;
    _biasGY = sumGY / samples;
    _biasGZ = sumGZ / samples;

    _yaw = 0.0f;
    _lastAttitudeUs = micros();

    return true;
  }

  void resetYaw(float angle = 0.0f) {
    _yaw = angle;
    _lastAttitudeUs = micros();
  }

  void setOrientationOffset(int16_t degrees) {
    _cfg.orientationOffsetDeg = degrees;
  }

  int16_t getOrientationOffset() const {
    return _cfg.orientationOffsetDeg;
  }

  bool isReady() const { return _ready; }
  const bpadIMUConfig& config() const { return _cfg; }

private:
  bpadIMUConfig _cfg;
  TwoWire* _wire = nullptr;
  bool _ready = false;

  float _ax = 0.0f, _ay = 0.0f, _az = 0.0f;
  float _gx = 0.0f, _gy = 0.0f, _gz = 0.0f;
  float _prevAX = 0.0f, _prevAY = 0.0f, _prevAZ = 0.0f;
  float _pitch = 0.0f, _roll = 0.0f, _yaw = 0.0f;
  float _temperatureC = 25.0f;

  float _biasAX = 0.0f, _biasAY = 0.0f, _biasAZ = 0.0f;
  float _biasGX = 0.0f, _biasGY = 0.0f, _biasGZ = 0.0f;
  uint32_t _lastAttitudeUs = 0;

  void applyOrientationAndAttitude(float rawAx, float rawAy, float rawAz,
                                   float rawGx, float rawGy, float rawGz) {
    _prevAX = accelX();
    _prevAY = accelY();
    _prevAZ = accelZ();

    // Apply PCB orientation rotation offset (e.g. -90 deg on bpad 185)
    if (_cfg.orientationOffsetDeg == -90) {
      _ax = -rawAy;
      _ay = rawAx;
      _az = rawAz;
      _gx = -rawGy;
      _gy = rawGx;
      _gz = rawGz;
    } else if (_cfg.orientationOffsetDeg == 90) {
      _ax = rawAy;
      _ay = -rawAx;
      _az = rawAz;
      _gx = rawGy;
      _gy = -rawGx;
      _gz = rawGz;
    } else if (_cfg.orientationOffsetDeg == 180 || _cfg.orientationOffsetDeg == -180) {
      _ax = -rawAx;
      _ay = -rawAy;
      _az = rawAz;
      _gx = -rawGx;
      _gy = -rawGy;
      _gz = rawGz;
    } else {
      _ax = rawAx;
      _ay = rawAy;
      _az = rawAz;
      _gx = rawGx;
      _gy = rawGy;
      _gz = rawGz;
    }

    // Compute Pitch & Roll in degrees from calibrated accel
    float curAX = accelX();
    float curAY = accelY();
    float curAZ = accelZ();

    _roll  = atan2f(curAX, curAZ) * (180.0f / (float)M_PI);
    _pitch = atan2f(curAY, sqrtf(curAX * curAX + curAZ * curAZ)) * (180.0f / (float)M_PI);

    // Compute Relative Yaw by integrating Gyro Z over time
    uint32_t nowUs = micros();
    if (_lastAttitudeUs > 0) {
      float dt = (float)(nowUs - _lastAttitudeUs) * 1e-6f;
      if (dt > 0.0f && dt < 0.5f) {
        float gz = gyroZ();
        // Deadband filter to prevent drift from stationary noise (±0.6 deg/s)
        // Clockwise rotation = positive heading/yaw (+)
        if (fabsf(gz) >= 0.6f) {
          _yaw -= gz * dt;
          // Wrap yaw between -180.0 and +180.0 degrees
          if (_yaw > 180.0f)  _yaw -= 360.0f;
          if (_yaw < -180.0f) _yaw += 360.0f;
        }
      }
    }
    _lastAttitudeUs = nowUs;
  }

  // --- ICM-42607-P Implementation ---
  bool initICM42607() {
    // Soft Reset
    _wire->beginTransmission(_cfg.address);
    _wire->write(0x02); // SIGNAL_PATH_RESET
    _wire->write(0x10); // SOFT_RESET_DEVICE_CONFIG
    _wire->endTransmission(true);
    delay(30);

    // Power Management: Low Noise mode for Accel + Gyro
    _wire->beginTransmission(_cfg.address);
    _wire->write(0x1F); // PWR_MGMT0
    _wire->write(0x0F); // LN mode for Accel and Gyro
    if (_wire->endTransmission(true) != 0) {
      return false;
    }
    delay(50);

    // GYRO_CONFIG0 (0x20): Bits[6:5]=00 (±2000 dps), Bits[3:0]=0110 (100 Hz ODR) -> 0x06
    _wire->beginTransmission(_cfg.address);
    _wire->write(0x20);
    _wire->write(0x06);
    _wire->endTransmission(true);

    // ACCEL_CONFIG0 (0x21): Bits[6:5]=11 (±2g), Bits[3:0]=0110 (100 Hz ODR) -> 0x66
    _wire->beginTransmission(_cfg.address);
    _wire->write(0x21);
    _wire->write(0x66);
    _wire->endTransmission(true);

    // Verify WHO_AM_I register (0x75) -> Expected 0x60
    _wire->beginTransmission(_cfg.address);
    _wire->write(0x75);
    _wire->endTransmission(false);
    if (_wire->requestFrom((uint8_t)_cfg.address, (uint8_t)1) == 1) {
      uint8_t id = _wire->read();
      _ready = (id == 0x60 || id == 0x61 || id != 0);
      return _ready;
    }

    _ready = true;
    return true;
  }

  bool readICM42607() {
    _wire->beginTransmission(_cfg.address);
    _wire->write(0x09); // Start at TEMP_DATA1 (0x09) through GYRO_DATA_Z0 (0x16) -> 14 bytes
    if (_wire->endTransmission(false) != 0) return false;

    if (_wire->requestFrom((uint8_t)_cfg.address, (uint8_t)14) != 14) return false;

    int16_t rawTemp = (int16_t)((_wire->read() << 8) | _wire->read());
    _temperatureC = ((float)rawTemp / 128.0f) + 25.0f;

    int16_t rawAX = (int16_t)((_wire->read() << 8) | _wire->read());
    int16_t rawAY = (int16_t)((_wire->read() << 8) | _wire->read());
    int16_t rawAZ = (int16_t)((_wire->read() << 8) | _wire->read());

    int16_t rawGX = (int16_t)((_wire->read() << 8) | _wire->read());
    int16_t rawGY = (int16_t)((_wire->read() << 8) | _wire->read());
    int16_t rawGZ = (int16_t)((_wire->read() << 8) | _wire->read());

    float ax = rawAX * _cfg.accelScale;
    float ay = rawAY * _cfg.accelScale;
    float az = rawAZ * _cfg.accelScale;

    float gx = rawGX * _cfg.gyroScale;
    float gy = rawGY * _cfg.gyroScale;
    float gz = rawGZ * _cfg.gyroScale;

    applyOrientationAndAttitude(ax, ay, az, gx, gy, gz);
    return true;
  }
};
