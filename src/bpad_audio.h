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

namespace bpad {

enum class Waveform : uint8_t {
  Triangle = 0, // Default: cleanest and smoothest on miniature bpad speaker
  Sine     = 1,
  Square   = 2
};

class Audio {
public:
  explicit Audio(const AudioConfig& config = AudioConfig())
    : _cfg(config) {
    setSampleRate(_cfg.sampleRateHz);
  }

  void select(const AudioConfig& config) {
    _cfg = config;
    setSampleRate(_cfg.sampleRateHz);
  }

  bool begin(TwoWire& wire = Wire) {
    _wire = &wire;
    setSampleRate(_cfg.sampleRateHz);
    if (_cfg.support == Support::NotPresent || _cfg.backend == AudioBackend::None) {
      return false;
    }

    // Initialize amp power rail strictly LOW (muted)
    if (_cfg.ampPower != 0xFF) {
      pinMode(_cfg.ampPower, OUTPUT);
      digitalWrite(_cfg.ampPower, LOW);
      _ampEnabled = false;
    }

    if (_cfg.backend == AudioBackend::MCP4725) {
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
    if (_cfg.backend == AudioBackend::MCP4725) {
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

    if (_cfg.backend == AudioBackend::MCP4725) {
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

  void setSampleRate(uint32_t hz) {
    _cfg.sampleRateHz = constrain(hz, 2000UL, 16000UL);
    _sampleRateHz = _cfg.sampleRateHz;
    _sampleIntervalUs = 1000000UL / _sampleRateHz;
    if (_isPlayingTone && _currentFreqHz > 0) {
      _phaseIncrement = (uint32_t)(((uint64_t)_currentFreqHz << 16) / _sampleRateHz);
    }
  }

  uint32_t getSampleRate() const {
    return _sampleRateHz;
  }

  void setMaxSwing(uint16_t swing) {
    _cfg.maxSwing = (swing <= 2047) ? swing : 2047;
  }

  uint16_t getMaxSwing() const {
    return _cfg.maxSwing;
  }

  void setWaveform(Waveform wave) {
    _waveform = wave;
  }

  Waveform getWaveform() const {
    return _waveform;
  }

  // Smooth Non-blocking Tone Generator (Default: Triangle wave for cleanest acoustics)
  void playTone(uint32_t freqHz, uint32_t durationMs = 0, Waveform wave = Waveform::Triangle) {
    if (!_wire || !_ready || freqHz == 0) {
      stop();
      return;
    }

    uint32_t nowMs = millis();
    bool wasPlaying = _isPlayingTone;

    _waveform = wave;
    _currentFreqHz = freqHz;
    if (!_ampEnabled) {
      enableAmp(true);
    }

    _phaseIncrement = (uint32_t)(((uint64_t)freqHz << 16) / _sampleRateHz);

    if (!wasPlaying) {
      _phaseAccumulator = 0;
      _attackMs = (durationMs > 0 && durationMs < 20) ? (uint16_t)(durationMs / 4) : 5;
      _nextSampleUs = micros();
    } else {
      // Keep phase continuous when chaining notes (eliminates inter-note clicks)
      _attackMs = 0;
    }

    if (durationMs > 0) {
      _releaseMs = (durationMs < 24) ? (uint16_t)(durationMs / 4) : 8;
      _toneEndMs = nowMs + durationMs;
    } else {
      _releaseMs = 0;
      _toneEndMs = 0;
    }

    _toneStartMs = nowMs;
    _isPlayingTone = true;
  }

  // Immediately shuts off DAC and amplifier rail (zero idle noise)
  void stop() {
    _isPlayingTone = false;
    _toneEndMs = 0;
    _currentFreqHz = 0;
    enableAmp(false); // Shuts off amplifier power rail cleanly with anti-pop sequence
  }

  void update() {
    if (!_isPlayingTone || !_ready) return;

    uint32_t nowMs = millis();
    if (_toneEndMs > 0 && (int32_t)(nowMs - _toneEndMs) >= 0) {
      stop();
      return;
    }

    uint32_t nowUs = micros();
    if ((int32_t)(nowUs - _nextSampleUs) >= 0) {
      // Single-step catch-up clamp: drop missed ticks to prevent I2C burst saturation
      if ((int32_t)(nowUs - _nextSampleUs) >= (int32_t)(_sampleIntervalUs * 2)) {
        _nextSampleUs = nowUs + _sampleIntervalUs;
      } else {
        _nextSampleUs += _sampleIntervalUs;
      }

      _phaseAccumulator += _phaseIncrement;
      uint16_t phase16 = _phaseAccumulator & 0xFFFF;

      // Attack / release envelope (0..256 fixed point) to eliminate click/pop artifacts
      int32_t env256 = 256;
      if (_attackMs > 0) {
        uint32_t elapsed = nowMs - _toneStartMs;
        if (elapsed < _attackMs) {
          env256 = (int32_t)((elapsed << 8) / _attackMs);
        }
      }
      if (_releaseMs > 0 && _toneEndMs > 0) {
        int32_t remaining = (int32_t)(_toneEndMs - nowMs);
        if (remaining > 0 && remaining < (int32_t)_releaseMs) {
          int32_t rel256 = (remaining << 8) / _releaseMs;
          if (rel256 < env256) env256 = rel256;
        }
      }

      int32_t maxAmp = ((int32_t)(_cfg.maxSwing * _volMultiplier) * env256) >> 8;
      int32_t sample = 2048;

      switch (_waveform) {
        case Waveform::Triangle: // Cleanest on micro-speaker
        default:
          if (phase16 < 32768) {
            sample = (2048 - maxAmp) + ((maxAmp * (int32_t)phase16) >> 14);
          } else {
            sample = (2048 + maxAmp) - ((maxAmp * (int32_t)(phase16 - 32768)) >> 14);
          }
          break;

        case Waveform::Square:
          sample = (phase16 < 32768) ? (2048 + maxAmp) : (2048 - maxAmp);
          break;

        case Waveform::Sine: {
          uint8_t idx = (phase16 >> 11) & 0x1F;
          sample = 2048 + (((int32_t)SINE_32[idx] * maxAmp) / 1000);
          break;
        }
      }

      writeSampleDirect((uint16_t)constrain(sample, 0, 4095));
    }
  }

  bool isPlaying() const { return _isPlayingTone; }
  bool isReady() const { return _ready; }
  const AudioConfig& config() const { return _cfg; }

private:
  AudioConfig _cfg;
  TwoWire* _wire = nullptr;
  bool _ready = false;
  bool _ampEnabled = false;

  Waveform _waveform = Waveform::Triangle;
  uint8_t _volumeLevel = 3;
  uint8_t _unmuteVolume = 3;
  bool _isMuted = false;
  float _volMultiplier = 1.0f;

  bool _isPlayingTone = false;
  uint32_t _sampleRateHz = 8000;
  uint32_t _sampleIntervalUs = 125;
  uint32_t _currentFreqHz = 0;
  uint32_t _phaseIncrement = 0;
  uint32_t _phaseAccumulator = 0;
  uint32_t _nextSampleUs = 0;
  uint32_t _toneStartMs = 0;
  uint32_t _toneEndMs = 0;
  uint16_t _attackMs = 0;
  uint16_t _releaseMs = 0;

  static constexpr int16_t SINE_32[32] = {
       0,  195,  383,  556,  707,  831,  924,  981,
    1000,  981,  924,  831,  707,  556,  383,  195,
       0, -195, -383, -556, -707, -831, -924, -981,
   -1000, -981, -924, -831, -707, -556, -383, -195
  };
};

} // namespace bpad
