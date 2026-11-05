#pragma once
// =============================================================================
//  bpad_power.h - Power Management & Button Driver for bpad hardware
// =============================================================================
//  Controls the physical tactile switch (SW3) wired to GPIO 21 and the KEY pin
//  of the IP5306 PMIC.
//
//  Capabilities:
//  - Debounced button input (clicks, double-clicks, long-press holds)
//  - Software power-off via IP5306 hardware double-pulse
//  - Deep-sleep entry with GPIO 21 wake-on-press
// =============================================================================

#include <Arduino.h>
#include "bpad_config.h"

#if defined(ESP_PLATFORM)
#include "esp_sleep.h"
#endif

enum class bpadButtonEvent : uint8_t {
  None = 0,
  Click,
  LongPress
};

class bpadPower {
public:
  explicit bpadPower(const bpadButtonConfig& config = bpadButtonConfig())
    : _cfg(config) {}

  void select(const bpadButtonConfig& config) {
    _cfg = config;
  }

  bool begin() {
    if (_cfg.support == bpadSupport::NotPresent || _cfg.pin < 0) {
      return false;
    }

    pinMode(_cfg.pin, INPUT_PULLUP);
    _lastState = readRawState();
    _ready = true;
    return true;
  }

  // --- System & Power Metrics ---
  uint32_t getUptimeSeconds() const {
    return millis() / 1000;
  }

  uint32_t getUptimeMillis() const {
    return millis();
  }

  uint32_t getFreeHeap() const {
    return ESP.getFreeHeap();
  }

  uint32_t getFreePSRAM() const {
    return ESP.getFreePsram();
  }

  esp_reset_reason_t getResetReason() const {
    return esp_reset_reason();
  }

  // --- Button State Queries ---
  bool isPressed() const {
    return _pressed;
  }

  uint32_t pressDurationMs() const {
    if (!_pressed) return 0;
    return millis() - _pressStartMs;
  }

  uint32_t lastPressDurationMs() const {
    return _lastPressDuration;
  }

  bool isClicked() {
    return checkAndClearEvent(bpadButtonEvent::Click);
  }

  bool isLongPressed() {
    return checkAndClearEvent(bpadButtonEvent::LongPress);
  }

  bpadButtonEvent readEvent() {
    bpadButtonEvent ev = _pendingEvent;
    _pendingEvent = bpadButtonEvent::None;
    return ev;
  }

  // Polls button and processes click, double-click, and long-press state machine
  void update() {
    if (!_ready || _cfg.pin < 0) return;

    bool raw = readRawState();
    uint32_t now = millis();

    // Debounce
    if (raw != _lastRaw) {
      _lastRaw = raw;
      _lastDebounceMs = now;
    }

    if ((now - _lastDebounceMs) > _cfg.debounceMs) {
      if (raw != _lastState) {
        _lastState = raw;
        if (raw) {
          // Transition to PRESSED
          _pressed = true;
          _pressStartMs = now;
          _longPressFired = false;
        } else {
          // Transition to RELEASED
          _pressed = false;
          uint32_t duration = now - _pressStartMs;
          _lastPressDuration = duration;

          // If released before long-press threshold, register immediate Click
          if (!_longPressFired && duration < _cfg.longPressMs) {
            _pendingEvent = bpadButtonEvent::Click;
          }
        }
      }
    }

    // Check for long press while held down
    if (_pressed && !_longPressFired && (now - _pressStartMs) >= _cfg.longPressMs) {
      _pendingEvent = bpadButtonEvent::LongPress;
      _longPressFired = true;
    }
  }

  // --- IP5306 Software Power-Off ---
  // In the IP5306 datasheet, double-clicking the KEY pin turns OFF the 5V boost converter.
  // We pulse GPIO 21 LOW twice to trigger full hardware power cut.
  void powerOff() {
    if (_cfg.pin < 0) return;

    Serial.println("[POWER] Sending IP5306 shutdown sequence via GPIO 21...");

    pinMode(_cfg.pin, OUTPUT);
    digitalWrite(_cfg.pin, HIGH);
    delay(50);

    // Pulse 1
    digitalWrite(_cfg.pin, LOW);
    delay(80);
    digitalWrite(_cfg.pin, HIGH);
    delay(150);

    // Pulse 2
    digitalWrite(_cfg.pin, LOW);
    delay(80);
    digitalWrite(_cfg.pin, HIGH);
    delay(50);

    pinMode(_cfg.pin, INPUT_PULLUP);
    delay(200);
  }

  // --- Deep Sleep with Button Wakeup ---
  // Powers down ESP32-S3 and wakes up when GPIO 21 (SW3) is pressed (pulled LOW)
  void deepSleep() {
#if defined(ESP_PLATFORM)
    if (_cfg.pin >= 0) {
      Serial.println("[POWER] Configuring GPIO 21 wakeup and entering Deep Sleep...");
      esp_sleep_enable_ext0_wakeup((gpio_num_t)_cfg.pin, 0); // 0 = LOW
      delay(50);
      esp_deep_sleep_start();
    }
#endif
  }

  void sleep() {
    deepSleep();
  }

  bool isReady() const { return _ready; }
  const bpadButtonConfig& config() const { return _cfg; }

private:
  bool checkAndClearEvent(bpadButtonEvent target) {
    if (_pendingEvent == target) {
      _pendingEvent = bpadButtonEvent::None;
      return true;
    }
    return false;
  }

  bpadButtonConfig _cfg;
  bool _ready = false;

  bool _pressed = false;
  bool _lastRaw = false;
  bool _lastState = false;
  bool _longPressFired = false;

  uint32_t _lastDebounceMs = 0;
  uint32_t _pressStartMs = 0;
  uint32_t _lastPressDuration = 0;
  bpadButtonEvent _pendingEvent = bpadButtonEvent::None;

  bool readRawState() const {
    if (_cfg.pin < 0) return false;
    int val = digitalRead(_cfg.pin);
    return _cfg.activeLow ? (val == LOW) : (val == HIGH);
  }
};
