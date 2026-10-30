#pragma once
// =============================================================================
//  bpad_audio.h - Audio DAC Driver for bpad hardware (185 and newer)
// =============================================================================
//  Dedicated driver for MCP4725 (0x61) with amplifier power rail gating (GPIO 13).
//  Provides volume scaling, 12-bit sample streaming, and non-blocking tone synthesis.
// =============================================================================

#include <Arduino.h>
#include <Wire.h>
#include "bpad_config.h"

class bpadAudio {
public:
  explicit bpadAudio(const bpadAudioConfig& config = bpadAudioConfig())
    : _cfg(config) {}

  void select(const bpadAudioConfig& config) {
    _cfg = config;
  }

  bool begin(TwoWire& wire = Wire) {
    _wire = &wire;
    if (_cfg.support == bpadSupport::NotPresent || _cfg.backend == bpadAudioBackend::None) {
      return false;
    }

    if (_cfg.ampPower != 0xFF) {
      pinMode(_cfg.ampPower, OUTPUT);
      digitalWrite(_cfg.ampPower, LOW);
      _ampEnabled = false;
    }

    if (_cfg.backend == bpadAudioBackend::MCP4725) {
      _wire->beginTransmission(_cfg.mcp4725Address);
      _ready = (_wire->endTransmission() == 0);
      return _ready;
    }

    return false;
  }

  void enableAmp(bool enable) {
    if (_cfg.ampPower != 0xFF) {
      digitalWrite(_cfg.ampPower, enable ? HIGH : LOW);
      _ampEnabled = enable;
    }
  }

  bool isAmpEnabled() const {
    return _ampEnabled;
  }

  void setVolume(uint8_t level) {
    _volumeLevel = constrain(level, 0, 3);
    _volMultiplier = (float)_cfg.volumeLevels[_volumeLevel] / 12.0f;
  }

  uint8_t getVolume() const {
    return _volumeLevel;
  }

  void writeSample(uint16_t sample12bit) {
    if (!_wire || !_ready) return;

    uint16_t scaled = constrain((uint32_t)(sample12bit * _volMultiplier), 0, 4095);

    if (_cfg.backend == bpadAudioBackend::MCP4725) {
      _wire->beginTransmission(_cfg.mcp4725Address);
      _wire->write((scaled >> 8) & 0x0F);
      _wire->write(scaled & 0xFF);
      _wire->endTransmission();
    }
  }

  // Non-blocking Tone Generator with automatic period calculation
  void playTone(uint32_t freqHz, uint32_t durationMs = 0) {
    if (!_wire || !_ready || freqHz == 0) {
      stop();
      return;
    }

    if (!_ampEnabled) {
      enableAmp(true);
      delay(15); // Single initial warm-up for amplifier power rail
    }

    uint32_t periodUs = 1000000 / freqHz;
    _halfPeriodUs = periodUs / 2;
    _toneEndMs = (durationMs > 0) ? (millis() + durationMs) : 0;
    _isPlayingTone = true;
    _waveState = false;
    _lastToggleUs = micros();
  }

  void stop() {
    _isPlayingTone = false;
    _toneEndMs = 0;
    writeSample(0);
  }

  void update() {
    if (!_isPlayingTone || !_ready) return;

    if (_toneEndMs > 0 && millis() >= _toneEndMs) {
      stop();
      return;
    }

    uint32_t nowUs = micros();
    if (nowUs - _lastToggleUs >= _halfPeriodUs) {
      _lastToggleUs = nowUs;
      _waveState = !_waveState;
      writeSample(_waveState ? 4095 : 0);
    }
  }

  bool isPlaying() const { return _isPlayingTone; }
  bool isReady() const { return _ready; }
  const bpadAudioConfig& config() const { return _cfg; }

private:
  bpadAudioConfig _cfg;
  TwoWire* _wire = nullptr;
  bool _ready = false;
  bool _ampEnabled = false;

  uint8_t _volumeLevel = 3;
  float _volMultiplier = 1.0f;

  bool _isPlayingTone = false;
  bool _waveState = false;
  uint32_t _halfPeriodUs = 0;
  uint32_t _lastToggleUs = 0;
  uint32_t _toneEndMs = 0;
};
