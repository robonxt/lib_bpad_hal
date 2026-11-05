#pragma once
// =============================================================================
//  bpad_audio.h - Audio DAC Driver for bpad hardware (185 and newer)
// =============================================================================
//  Dedicated driver for MCP4725 (0x61) with amplifier power rail gating (GPIO 13).
//  Provides volume scaling, 12-bit sample streaming, and synthesis.
//
//  NOTE: Benchmarking on bpad 185 hardware verified that Triangle waves produce
//  the cleanest, smoothest acoustic output on the miniature watch speaker,
//  free from the harsh resonance distortion of square waves or quantization noise.
// =============================================================================

#include <Arduino.h>
#include <Wire.h>
#include "bpad_config.h"

enum class bpadWaveform : uint8_t {
  Triangle = 0, // Default: cleanest and smoothest on miniature bpad speaker
  Sine     = 1,
  Square   = 2
};

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

    // Initialize amp power rail strictly LOW (muted)
    if (_cfg.ampPower != 0xFF) {
      pinMode(_cfg.ampPower, OUTPUT);
      digitalWrite(_cfg.ampPower, LOW);
      _ampEnabled = false;
    }

    if (_cfg.backend == bpadAudioBackend::MCP4725) {
      _wire->beginTransmission(_cfg.mcp4725Address);
      _ready = (_wire->endTransmission() == 0);
      if (_ready) {
        powerDownDAC(); // Keep DAC grounded and silent
      }
      return _ready;
    }

    return false;
  }

  void enableAmp(bool enable) {
    if (_cfg.ampPower != 0xFF) {
      if (enable && !_ampEnabled) {
        // Anti-pop sequence: Bias DAC to mid-rail (2048 / 1.65V) BEFORE unmuting amp
        writeSampleDirect(2048);
        delayMicroseconds(500);
        digitalWrite(_cfg.ampPower, HIGH);
        _ampEnabled = true;
      } else if (!enable && _ampEnabled) {
        // Anti-pop sequence: Mute amp FIRST, then power-down DAC
        digitalWrite(_cfg.ampPower, LOW);
        _ampEnabled = false;
        delayMicroseconds(500);
        powerDownDAC();
      }
    }
  }

  bool isAmpEnabled() const {
    return _ampEnabled;
  }

  void setVolume(uint8_t level) {
    _volumeLevel = constrain(level, 0, 3);
    _isMuted = (_volumeLevel == 0);
    _volMultiplier = (float)_cfg.volumeLevels[_volumeLevel] / 12.0f;
  }

  uint8_t getVolume() const {
    return _volumeLevel;
  }

  void mute() {
    if (!_isMuted) {
      _unmuteVolume = (_volumeLevel > 0) ? _volumeLevel : 3;
      setVolume(0);
      _isMuted = true;
    }
  }

  void unmute() {
    if (_isMuted) {
      _isMuted = false;
      setVolume((_unmuteVolume > 0) ? _unmuteVolume : 3);
    }
  }

  bool isMuted() const {
    return _isMuted;
  }

  // MCP4725 Power-Down mode: pulls VOUT to GND with 1k resistor
  void powerDownDAC() {
    if (!_wire || !_ready) return;
    if (_cfg.backend == bpadAudioBackend::MCP4725) {
      _wire->beginTransmission(_cfg.mcp4725Address);
      // Fast mode write: bits 7-6 = 00, bits 5-4 = 01 (1k pull-down to GND power down)
      _wire->write(0x10);
      _wire->write(0x00);
      _wire->endTransmission();
    }
  }

  // Direct raw sample write to MCP4725 (0..4095)
  void writeSampleDirect(uint16_t sample12bit) {
    if (!_wire || !_ready) return;

    if (_cfg.backend == bpadAudioBackend::MCP4725) {
      _wire->beginTransmission(_cfg.mcp4725Address);
      _wire->write((sample12bit >> 8) & 0x0F);
      _wire->write(sample12bit & 0xFF);
      _wire->endTransmission();
    }
  }

  // Centered AC sample write with volume scaling
  void writeSample(uint16_t sample12bit) {
    if (!_wire || !_ready) return;

    int32_t centered = (int32_t)sample12bit - 2048;
    int32_t scaled = 2048 + (int32_t)(centered * _volMultiplier);
    writeSampleDirect((uint16_t)constrain(scaled, 0, 4095));
  }

  void setWaveform(bpadWaveform wave) {
    _waveform = wave;
  }

  bpadWaveform getWaveform() const {
    return _waveform;
  }

  // Smooth Non-blocking Tone Generator (Default: Triangle wave for cleanest acoustics)
  void playTone(uint32_t freqHz, uint32_t durationMs = 0, bpadWaveform wave = bpadWaveform::Triangle) {
    if (!_wire || !_ready || freqHz == 0) {
      stop();
      return;
    }

    _waveform = wave;
    if (!_ampEnabled) {
      enableAmp(true);
    }

    _sampleRateHz = 8000;
    _sampleIntervalUs = 1000000 / _sampleRateHz; // 125 us pacing
    _phaseIncrement = (uint32_t)(((uint64_t)freqHz << 16) / _sampleRateHz);
    _phaseAccumulator = 0;

    _toneEndMs = (durationMs > 0) ? (millis() + durationMs) : 0;
    _isPlayingTone = true;
    _nextSampleUs = micros();
  }

  // Immediately shuts off DAC and amplifier rail (zero idle noise)
  void stop() {
    _isPlayingTone = false;
    _toneEndMs = 0;
    enableAmp(false); // Shuts off amplifier power rail cleanly with anti-pop sequence
  }

  void update() {
    if (!_isPlayingTone || !_ready) return;

    if (_toneEndMs > 0 && millis() >= _toneEndMs) {
      stop();
      return;
    }

    uint32_t nowUs = micros();
    if ((int32_t)(nowUs - _nextSampleUs) >= 0) {
      _nextSampleUs += _sampleIntervalUs;

      _phaseAccumulator += _phaseIncrement;
      uint16_t phase16 = _phaseAccumulator & 0xFFFF;

      int32_t sample = 2048;
      int32_t maxAmp = (int32_t)(1400.0f * _volMultiplier);

      switch (_waveform) {
        case bpadWaveform::Triangle: // Cleanest on micro-speaker
        default:
          if (phase16 < 32768) {
            sample = (2048 - maxAmp) + (int32_t)((int64_t)maxAmp * 2 * phase16 / 32768);
          } else {
            sample = (2048 + maxAmp) - (int32_t)((int64_t)maxAmp * 2 * (phase16 - 32768) / 32768);
          }
          break;

        case bpadWaveform::Square:
          sample = (phase16 < 32768) ? (2048 + maxAmp) : (2048 - maxAmp);
          break;

        case bpadWaveform::Sine: {
          uint8_t idx = (phase16 >> 11) & 0x1F;
          sample = 2048 + ((int32_t)SINE_32[idx] * maxAmp / 1000);
          break;
        }
      }

      writeSampleDirect((uint16_t)constrain(sample, 0, 4095));
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

  bpadWaveform _waveform = bpadWaveform::Triangle;
  uint8_t _volumeLevel = 3;
  uint8_t _unmuteVolume = 3;
  bool _isMuted = false;
  float _volMultiplier = 1.0f;

  bool _isPlayingTone = false;
  uint32_t _sampleRateHz = 8000;
  uint32_t _sampleIntervalUs = 125;
  uint32_t _phaseIncrement = 0;
  uint32_t _phaseAccumulator = 0;
  uint32_t _nextSampleUs = 0;
  uint32_t _toneEndMs = 0;

  static constexpr int16_t SINE_32[32] = {
       0,  195,  383,  556,  707,  831,  924,  981,
    1000,  981,  924,  831,  707,  556,  383,  195,
       0, -195, -383, -556, -707, -831, -924, -981,
   -1000, -981, -924, -831, -707, -556, -383, -195
  };
};
