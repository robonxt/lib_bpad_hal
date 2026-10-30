#pragma once
// =============================================================================
//  bpad_haptics.h - Haptics Driver for bpad hardware (185 and newer)
// =============================================================================
//  Multi-channel haptics driver (MCP4728 4-channel DAC or GPIO PWM).
//  Provides multi-motor vibration control, spatial positioning, directional pulses,
//  non-blocking timed pulses, and status queries.
//
//  bpad 185 Quad Motor Layout (X format):
//    Top-Left:     Channel 2 (Red wires)
//    Top-Right:    Channel 3 (Blue wires)
//    Bottom-Left:  Channel 0 (White wires)
//    Bottom-Right: Channel 1 (Black wires)
// =============================================================================

#include <Arduino.h>
#include <Wire.h>
#include "bpad_config.h"

enum class bpadMotor : uint8_t {
  BottomLeft  = 0, // Channel 0 (White wires)
  BottomRight = 1, // Channel 1 (Black wires)
  TopLeft     = 2, // Channel 2 (Red wires)
  TopRight    = 3  // Channel 3 (Blue wires)
};

constexpr uint8_t BPAD_MOTOR_BL = 0;
constexpr uint8_t BPAD_MOTOR_BR = 1;
constexpr uint8_t BPAD_MOTOR_TL = 2;
constexpr uint8_t BPAD_MOTOR_TR = 3;

inline const char* bpadMotorName(bpadMotor motor) {
  switch (motor) {
    case bpadMotor::BottomLeft:  return "Bottom-Left (White)";
    case bpadMotor::BottomRight: return "Bottom-Right (Black)";
    case bpadMotor::TopLeft:     return "Top-Left (Red)";
    case bpadMotor::TopRight:    return "Top-Right (Blue)";
    default: return "Unknown";
  }
}

inline const char* bpadMotorName(uint8_t index) {
  return bpadMotorName(static_cast<bpadMotor>(index));
}

class bpadHaptics {
public:
  explicit bpadHaptics(const bpadHapticsConfig& config = bpadHapticsConfig())
    : _cfg(config) {}

  void select(const bpadHapticsConfig& config) {
    _cfg = config;
  }

  bool begin(TwoWire& wire = Wire) {
    _wire = &wire;
    if (_cfg.support == bpadSupport::NotPresent || _cfg.backend == bpadHapticsBackend::None) {
      return false;
    }

    if (_cfg.backend == bpadHapticsBackend::GPIO_PWM) {
      for (uint8_t i = 0; i < _cfg.count && i < 4; i++) {
        ledcAttach(_cfg.gpioPins[i], _cfg.pwmFreq, _cfg.pwmResolution);
        ledcWrite(_cfg.gpioPins[i], 0);
      }
      _ready = true;
      return true;
    } else if (_cfg.backend == bpadHapticsBackend::MCP4728_DAC) {
      _wire->beginTransmission(_cfg.mcp4728Address);
      if (_wire->endTransmission() == 0) {
        _ready = true;
        stopAll();
        return true;
      }
    }

    return false;
  }

  // --- Motor control by index (0-3) ---
  void setMotor(uint8_t index, uint16_t strength) {
    if (!_ready || index >= _cfg.count || index >= 4) return;

    _strengths[index] = strength;

    if (_cfg.backend == bpadHapticsBackend::GPIO_PWM) {
      uint32_t duty = (strength > 255) ? (strength >> 4) : strength;
      ledcWrite(_cfg.gpioPins[index], duty);
    } else if (_cfg.backend == bpadHapticsBackend::MCP4728_DAC) {
      uint8_t ch = _cfg.mcp4728Channels[index];
      if (ch == 0xFF || !_wire) return;

      uint16_t val = constrain(strength, 0, 4095);
      uint8_t cmd = 0x40 | ((ch & 0x03) << 1);
      _wire->beginTransmission(_cfg.mcp4728Address);
      _wire->write(cmd);
      _wire->write((val >> 8) & 0x0F);
      _wire->write(val & 0xFF);
      _wire->endTransmission();
    }
  }

  void pulse(uint8_t index, uint16_t onDurationMs, uint16_t strength = 4095) {
    if (index >= _cfg.count || index >= 4) return;

    _duration[index] = onDurationMs;
    _startTime[index] = millis();
    _active[index] = true;
    setMotor(index, strength);
  }

  void stop(uint8_t index) {
    if (index >= 4) return;
    _active[index] = false;
    setMotor(index, 0);
  }

  // --- Motor control by spatial position enum ---
  void setMotor(bpadMotor motor, uint16_t strength) {
    setMotor(static_cast<uint8_t>(motor), strength);
  }

  void pulse(bpadMotor motor, uint16_t onDurationMs, uint16_t strength = 4095) {
    pulse(static_cast<uint8_t>(motor), onDurationMs, strength);
  }

  void stop(bpadMotor motor) {
    stop(static_cast<uint8_t>(motor));
  }

  // --- Group / Directional Pulses ---
  void pulseAll(uint16_t onDurationMs, uint16_t strength = 4095) {
    for (uint8_t i = 0; i < _cfg.count && i < 4; i++) {
      pulse(i, onDurationMs, strength);
    }
  }

  void pulseLeft(uint16_t onDurationMs, uint16_t strength = 4095) {
    pulse(bpadMotor::TopLeft, onDurationMs, strength);
    pulse(bpadMotor::BottomLeft, onDurationMs, strength);
  }

  void pulseRight(uint16_t onDurationMs, uint16_t strength = 4095) {
    pulse(bpadMotor::TopRight, onDurationMs, strength);
    pulse(bpadMotor::BottomRight, onDurationMs, strength);
  }

  void pulseTop(uint16_t onDurationMs, uint16_t strength = 4095) {
    pulse(bpadMotor::TopLeft, onDurationMs, strength);
    pulse(bpadMotor::TopRight, onDurationMs, strength);
  }

  void pulseBottom(uint16_t onDurationMs, uint16_t strength = 4095) {
    pulse(bpadMotor::BottomLeft, onDurationMs, strength);
    pulse(bpadMotor::BottomRight, onDurationMs, strength);
  }

  void stopAll() {
    for (uint8_t i = 0; i < 4; i++) {
      stop(i);
    }
  }

  void update() {
    if (!_ready) return;

    unsigned long now = millis();
    for (uint8_t i = 0; i < _cfg.count && i < 4; i++) {
      if (_active[i] && _duration[i] > 0) {
        if (now - _startTime[i] >= _duration[i]) {
          stop(i);
        }
      }
    }
  }

  bool isActive(uint8_t index) const {
    if (index >= 4) return false;
    return _active[index];
  }

  bool isActive(bpadMotor motor) const {
    return isActive(static_cast<uint8_t>(motor));
  }

  bool isAnyActive() const {
    for (uint8_t i = 0; i < 4; i++) {
      if (_active[i]) return true;
    }
    return false;
  }

  uint16_t getStrength(uint8_t index) const {
    if (index >= 4) return 0;
    return _strengths[index];
  }

  uint16_t getStrength(bpadMotor motor) const {
    return getStrength(static_cast<uint8_t>(motor));
  }

  bool isReady() const { return _ready; }
  const bpadHapticsConfig& config() const { return _cfg; }

private:
  bpadHapticsConfig _cfg;
  TwoWire* _wire = nullptr;
  bool _ready = false;

  bool _active[4] = {false, false, false, false};
  unsigned long _startTime[4] = {0, 0, 0, 0};
  uint16_t _duration[4] = {0, 0, 0, 0};
  uint16_t _strengths[4] = {0, 0, 0, 0};
};
