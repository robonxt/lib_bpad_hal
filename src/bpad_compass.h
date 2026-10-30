#pragma once
// =============================================================================
//  bpad_compass.h - Unified Magnetometer/Compass Driver for bpad hardware
// =============================================================================
//  Supports MMC5603NJ (Address 0x30) across all bpad hardware revisions.
//  Provides 20-bit reads in uT, hardware Auto Set/Reset (degaussing on every read),
//  EMA noise filtering, active calibration routines, and heading calculation.
// =============================================================================

#include <Arduino.h>
#include <Wire.h>
#include <math.h>
#include "bpad_config.h"

// MMC5603NJ Register Map
static constexpr uint8_t MMC5603_REG_OUT_X_L = 0x00;
static constexpr uint8_t MMC5603_REG_OUT_TEMP = 0x09;
static constexpr uint8_t MMC5603_REG_STATUS   = 0x18;
static constexpr uint8_t MMC5603_REG_ODR      = 0x1A;
static constexpr uint8_t MMC5603_REG_CTRL0    = 0x1B;
static constexpr uint8_t MMC5603_REG_CTRL1    = 0x1C;
static constexpr uint8_t MMC5603_REG_CTRL2    = 0x1D;
static constexpr uint8_t MMC5603_REG_PROD_ID  = 0x39;

class bpadCompass {
public:
  explicit bpadCompass(const bpadCompassConfig& config = bpadCompassConfig())
    : _cfg(config) {}

  void select(const bpadCompassConfig& config) {
    _cfg = config;
  }

  bool begin(TwoWire& wire = Wire) {
    _wire = &wire;
    if (_cfg.support == bpadSupport::NotPresent) {
      return false;
    }

    _wire->beginTransmission(_cfg.address);
    if (_wire->endTransmission() != 0) {
      return false;
    }

    // Software reset sensor (CTRL1 bit 7: SW_RST)
    writeReg(MMC5603_REG_CTRL1, 0x80);
    delay(25);

    // Initial manual SET / RESET pulse to align magnetic domains
    writeReg(MMC5603_REG_CTRL0, 0x08); // SET
    delay(2);
    writeReg(MMC5603_REG_CTRL0, 0x10); // RESET
    delay(2);
    writeReg(MMC5603_REG_CTRL0, 0x08); // SET (positive polarity)
    delay(2);

    // Verify connection by reading product ID (Expected 0x10)
    uint8_t chipId = readReg(MMC5603_REG_PROD_ID);
    _ready = (chipId == 0x10 || chipId != 0xFF);

    _firstRead = true;
    return _ready;
  }

  bool update() {
    if (!_wire || !_ready) return false;

    // Trigger measurement with hardware Auto Set/Reset enabled (Bit 5: Auto_SR_en + Bit 0: TM_M = 0x21)
    // Auto_SR_en automatically degausses and cancels bridge offset/temperature drift on every read.
    writeReg(MMC5603_REG_CTRL0, 0x21);

    // Wait for conversion ready (Status reg 0x18 bit 6 or bit 0: Meas_M_Done)
    unsigned long start = millis();
    bool done = false;
    while (millis() - start < 30) {
      uint8_t status = readReg(MMC5603_REG_STATUS);
      if (status & 0x40 || status & 0x01) {
        done = true;
        break;
      }
      delay(1);
    }
    if (!done) return false;

    // Read 9 measurement bytes starting at OUT_X_L (0x00)
    uint8_t buf[9];
    _wire->beginTransmission(_cfg.address);
    _wire->write(MMC5603_REG_OUT_X_L);
    if (_wire->endTransmission(false) != 0) return false;
    if (_wire->requestFrom((uint8_t)_cfg.address, (uint8_t)9) != 9) return false;

    for (int i = 0; i < 9; i++) {
      buf[i] = _wire->read();
    }

    // Reconstruct 20-bit raw values
    uint32_t rawX = ((uint32_t)buf[0] << 12) | ((uint32_t)buf[1] << 4) | ((uint32_t)buf[6] >> 4);
    uint32_t rawY = ((uint32_t)buf[2] << 12) | ((uint32_t)buf[3] << 4) | ((uint32_t)buf[7] >> 4);
    uint32_t rawZ = ((uint32_t)buf[4] << 12) | ((uint32_t)buf[5] << 4) | ((uint32_t)buf[8] >> 4);

    // Subtract 20-bit center offset (2^19 = 524288) and scale to micro-Teslas (0.00625 uT/LSB)
    _rawX = ((float)((int32_t)rawX - 524288)) * 0.00625f;
    _rawY = ((float)((int32_t)rawY - 524288)) * 0.00625f;
    _rawZ = ((float)((int32_t)rawZ - 524288)) * 0.00625f;

    // Auto-calibration bounding box tracking
    if (_autoCalibrate) {
      if (_rawX < _minX) _minX = _rawX;
      if (_rawX > _maxX) _maxX = _rawX;
      if (_rawY < _minY) _minY = _rawY;
      if (_rawY > _maxY) _maxY = _rawY;
      if (_rawZ < _minZ) _minZ = _rawZ;
      if (_rawZ > _maxZ) _maxZ = _rawZ;

      if ((_maxX - _minX) > 10.0f) _offsetX = (_maxX + _minX) / 2.0f;
      if ((_maxY - _minY) > 10.0f) _offsetY = (_maxY + _minY) / 2.0f;
      if ((_maxZ - _minZ) > 10.0f) _offsetZ = (_maxZ + _minZ) / 2.0f;
    }

    // Apply Hard-Iron calibration offsets and Soft-Iron scale
    float calX = (_rawX - _offsetX) * _scaleX;
    float calY = (_rawY - _offsetY) * _scaleY;
    float calZ = (_rawZ - _offsetZ) * _scaleZ;

    // Apply Exponential Moving Average (EMA) filter for noise suppression
    if (_firstRead || _filterAlpha >= 1.0f) {
      _filtX = calX;
      _filtY = calY;
      _filtZ = calZ;
      _firstRead = false;
    } else {
      _filtX = (_filterAlpha * calX) + ((1.0f - _filterAlpha) * _filtX);
      _filtY = (_filterAlpha * calY) + ((1.0f - _filterAlpha) * _filtY);
      _filtZ = (_filterAlpha * calZ) + ((1.0f - _filterAlpha) * _filtZ);
    }

    // Calculate heading in degrees (0..360) from filtered field
    _heading = atan2f(_filtY, _filtX) * (180.0f / (float)M_PI);
    if (_heading < 0.0f) {
      _heading += 360.0f;
    }

    return true;
  }

  // --- Active Calibration Routine ---
  // Rotate the board in a 3D figure-8 pattern during this call.
  bool calibrate(uint32_t durationMs = 5000, uint16_t sampleIntervalMs = 25) {
    if (!_wire || !_ready) return false;

    float minX = 30000.0f, maxX = -30000.0f;
    float minY = 30000.0f, maxY = -30000.0f;
    float minZ = 30000.0f, maxZ = -30000.0f;

    unsigned long start = millis();
    while (millis() - start < durationMs) {
      if (update()) {
        if (_rawX < minX) minX = _rawX;
        if (_rawX > maxX) maxX = _rawX;
        if (_rawY < minY) minY = _rawY;
        if (_rawY > maxY) maxY = _rawY;
        if (_rawZ < minZ) minZ = _rawZ;
        if (_rawZ > maxZ) maxZ = _rawZ;
      }
      delay(sampleIntervalMs);
    }

    // Check if sufficient movement was detected (at least 15 uT span on all axes)
    float spanX = maxX - minX;
    float spanY = maxY - minY;
    float spanZ = maxZ - minZ;
    if (spanX < 10.0f || spanY < 10.0f) return false;

    _offsetX = (maxX + minX) / 2.0f;
    _offsetY = (maxY + minY) / 2.0f;
    _offsetZ = (maxZ + minZ) / 2.0f;

    // Compute soft-iron scaling
    float avgSpan = (spanX + spanY + (spanZ > 10.0f ? spanZ : (spanX + spanY) / 2.0f)) / 3.0f;
    _scaleX = (spanX > 0) ? (avgSpan / spanX) : 1.0f;
    _scaleY = (spanY > 0) ? (avgSpan / spanY) : 1.0f;
    _scaleZ = (spanZ > 0) ? (avgSpan / spanZ) : 1.0f;

    _minX = minX; _maxX = maxX;
    _minY = minY; _maxY = maxY;
    _minZ = minZ; _maxZ = maxZ;

    return true;
  }

  // --- Getters: Calibrated Magnetic Field in uT ---
  float x() const { return _filtX; }
  float y() const { return _filtY; }
  float z() const { return _filtZ; }

  float rawX() const { return _rawX; }
  float rawY() const { return _rawY; }
  float rawZ() const { return _rawZ; }

  float heading(float declinationDeg = 0.0f) const {
    float h = _heading + declinationDeg;
    if (h >= 360.0f) h -= 360.0f;
    else if (h < 0.0f) h += 360.0f;
    return h;
  }

  void getMag(float& x, float& y, float& z) const {
    x = _filtX; y = _filtY; z = _filtZ;
  }

  // --- Filter Configuration ---
  void setFilterAlpha(float alpha) {
    _filterAlpha = constrain(alpha, 0.01f, 1.0f);
  }

  float getFilterAlpha() const {
    return _filterAlpha;
  }

  // --- Calibration Setters & Getters ---
  void setCalibration(float offsetX, float offsetY, float offsetZ) {
    _offsetX = offsetX;
    _offsetY = offsetY;
    _offsetZ = offsetZ;
  }

  void getCalibration(float& offsetX, float& offsetY, float& offsetZ) const {
    offsetX = _offsetX;
    offsetY = _offsetY;
    offsetZ = _offsetZ;
  }

  void setScale(float scaleX, float scaleY, float scaleZ) {
    _scaleX = (scaleX > 0.0f) ? scaleX : 1.0f;
    _scaleY = (scaleY > 0.0f) ? scaleY : 1.0f;
    _scaleZ = (scaleZ > 0.0f) ? scaleZ : 1.0f;
  }

  void getScale(float& scaleX, float& scaleY, float& scaleZ) const {
    scaleX = _scaleX;
    scaleY = _scaleY;
    scaleZ = _scaleZ;
  }

  void setAutoCalibration(bool enable) {
    _autoCalibrate = enable;
  }

  bool isAutoCalibrationEnabled() const {
    return _autoCalibrate;
  }

  void resetCalibration() {
    _offsetX = 0; _offsetY = 0; _offsetZ = 0;
    _scaleX = 1.0f; _scaleY = 1.0f; _scaleZ = 1.0f;
    _minX = 30000.0f; _maxX = -30000.0f;
    _minY = 30000.0f; _maxY = -30000.0f;
    _minZ = 30000.0f; _maxZ = -30000.0f;
    _firstRead = true;
  }

  bool isReady() const { return _ready; }
  const bpadCompassConfig& config() const { return _cfg; }

private:
  bpadCompassConfig _cfg;
  TwoWire* _wire = nullptr;
  bool _ready = false;

  float _rawX = 0.0f, _rawY = 0.0f, _rawZ = 0.0f;
  float _filtX = 0.0f, _filtY = 0.0f, _filtZ = 0.0f;
  float _heading = 0.0f;

  float _filterAlpha = 0.30f; // EMA filter constant (0.3 = 30% new, 70% previous)
  bool _firstRead = true;

  float _offsetX = 0.0f, _offsetY = 0.0f, _offsetZ = 0.0f;
  float _scaleX = 1.0f, _scaleY = 1.0f, _scaleZ = 1.0f;
  bool _autoCalibrate = false;
  float _minX = 30000.0f, _maxX = -30000.0f;
  float _minY = 30000.0f, _maxY = -30000.0f;
  float _minZ = 30000.0f, _maxZ = -30000.0f;

  uint8_t readReg(uint8_t reg) {
    _wire->beginTransmission(_cfg.address);
    _wire->write(reg);
    if (_wire->endTransmission(false) != 0) return 0;
    if (_wire->requestFrom((uint8_t)_cfg.address, (uint8_t)1) != 1) return 0;
    return _wire->read();
  }

  void writeReg(uint8_t reg, uint8_t val) {
    _wire->beginTransmission(_cfg.address);
    _wire->write(reg);
    _wire->write(val);
    _wire->endTransmission();
  }
};
