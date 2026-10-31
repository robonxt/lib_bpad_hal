#pragma once
// =============================================================================
//  bpad_touch.h - Capacitive Touch Driver for bpad hardware (185 and newer)
// =============================================================================
//  Supports CST816S (bpad 185) and modern CST816 capacitive touch controllers.
//  Provides coordinates, press state, rotation remapping, gestures, and tracking.
// =============================================================================

#include <Arduino.h>
#include <Wire.h>
#include "bpad_config.h"

enum class bpadTouchMode : uint8_t {
  NativeHardware = 0, // Pure CST816 on-chip DSP gestures (register 0x01) with edge-triggering
  CustomSoftware = 1  // Custom software timing, delta tracking, and swipe engine
};

inline const char* bpadTouchModeName(bpadTouchMode mode) {
  switch (mode) {
    case bpadTouchMode::NativeHardware: return "Native Hardware (CST816 DSP)";
    case bpadTouchMode::CustomSoftware: return "Custom Software Engine";
    default: return "Unknown";
  }
}

enum class bpadGesture : uint8_t {
  None = 0x00,
  SwipeDown = 0x01,
  SwipeUp = 0x02,
  SwipeLeft = 0x03,
  SwipeRight = 0x04,
  Tap = 0x05,
  DoubleTap = 0x0B,
  LongPress = 0x0C,
  Unknown = 0xFF
};

inline const char* bpadGestureName(bpadGesture gesture) {
  switch (gesture) {
    case bpadGesture::None: return "none";
    case bpadGesture::SwipeDown: return "swipe down";
    case bpadGesture::SwipeUp: return "swipe up";
    case bpadGesture::SwipeLeft: return "swipe left";
    case bpadGesture::SwipeRight: return "swipe right";
    case bpadGesture::Tap: return "tap";
    case bpadGesture::DoubleTap: return "double tap";
    case bpadGesture::LongPress: return "long press";
    default: return "unknown";
  }
}

struct bpadTouchData {
  uint16_t x = 0;
  uint16_t y = 0;
  uint16_t rawX = 0;
  uint16_t rawY = 0;
  bool pressed = false;
  uint8_t rawGesture = 0;
  bpadGesture gesture = bpadGesture::None;
  int16_t deltaX = 0;
  int16_t deltaY = 0;
};

class bpadTouch {
public:
  explicit bpadTouch(const bpadTouchConfig& config = bpadTouchConfig())
    : _cfg(config) {}

  void select(const bpadTouchConfig& config) {
    _cfg = config;
  }

  bool begin(TwoWire& wire = Wire) {
    _wire = &wire;
    if (_cfg.support == bpadSupport::NotPresent) {
      return false;
    }

    // Handle IRQ pin wake pulse
    if (_cfg.irq >= 0) {
      pinMode(_cfg.irq, OUTPUT);
      digitalWrite(_cfg.irq, HIGH);
      delay(1);
      digitalWrite(_cfg.irq, LOW);
      delay(1);
      pinMode(_cfg.irq, INPUT_PULLUP);
    }

    // Handle hardware reset pulse
    if (_cfg.rst >= 0) {
      pinMode(_cfg.rst, OUTPUT);
      digitalWrite(_cfg.rst, LOW);
      delay(10);
      digitalWrite(_cfg.rst, HIGH);
      delay(10);
    }

    delay(5);

    // Disable auto-sleep on CST816D / CST816S
    writeReg(0xFE, 0xFF);

    _ready = ping();
    return _ready;
  }

  bpadTouchData read() {
    uint32_t now = millis();
    if (now - _lastPollMs < 10) {
      return _lastData;
    }
    _lastPollMs = now;

    bpadTouchData td;
    if (!_wire) return td;

    // Read 6 bytes starting at register 0x01:
    // [0] = Gesture ID (reg 0x01)
    // [1] = Finger count (reg 0x02) - 0 = released, 1 = touched
    // [2] = Event flag & X high (reg 0x03) - bits [7:6] event flag, bits [3:0] X[11:8]
    // [3] = X low (reg 0x04)
    // [4] = Y high (reg 0x05) - bits [3:0] Y[11:8]
    // [5] = Y low (reg 0x06)
    uint8_t buf[6] = {0};
    _wire->beginTransmission((uint8_t)_cfg.address);
    _wire->write(0x01);
    if (_wire->endTransmission(false) != 0 || _wire->requestFrom((uint8_t)_cfg.address, (uint8_t)6) != 6) {
      if (_tracking) {
        td.pressed = false;
        td.rawX = _lastRawX;
        td.rawY = _lastRawY;
        remapCoordinates(td.rawX, td.rawY, td.x, td.y);
        td.gesture = processGesture(td);
        _lastData = td;
      }
      return _lastData;
    }

    for (uint8_t i = 0; i < 6; i++) {
      buf[i] = _wire->read();
    }

    td.rawGesture = buf[0];
    uint8_t fingers = buf[1];
    uint8_t event = buf[2] >> 6; // 0=Press, 1=LiftUp, 2=Contact, 3=NoEvent
    uint16_t rx = ((buf[2] & 0x0F) << 8) | buf[3];
    uint16_t ry = ((buf[4] & 0x0F) << 8) | buf[5];

    // CST816S/CST816D valid press validation:
    // Fingers > 0, event not LiftUp (event == 1), and coordinates strictly within display bounds.
    bool validCoordinates = (rx < _width && ry < _height);
    bool notLiftUp = (event != 1);
    td.pressed = (fingers > 0 && notLiftUp && validCoordinates);

    if (td.pressed) {
      td.rawX = rx;
      td.rawY = ry;
      _lastRawX = rx;
      _lastRawY = ry;
    } else {
      td.rawX = _lastRawX;
      td.rawY = _lastRawY;
    }

    // Apply rotation & inversion mapping
    remapCoordinates(td.rawX, td.rawY, td.x, td.y);

    td.gesture = processGesture(td);
    td.deltaX = _deltaX;
    td.deltaY = _deltaY;

    _lastData = td;
    return td;
  }

  // Classic getTouch signature matching CST816D driver API
  bool getTouch(uint16_t* x, uint16_t* y, uint8_t* gesture = nullptr) {
    bpadTouchData td = read();
    if (x) *x = td.x;
    if (y) *y = td.y;
    if (gesture) *gesture = td.rawGesture;
    return td.pressed;
  }

  // Overload returning normalized bpadGesture enum
  bool getTouch(uint16_t* x, uint16_t* y, bpadGesture* gesture) {
    bpadTouchData td = read();
    if (x) *x = td.x;
    if (y) *y = td.y;
    if (gesture) *gesture = td.gesture;
    return td.pressed;
  }

  // --- Getters ---
  bool isPressed() const { return _lastData.pressed; }
  uint16_t x() const { return _lastData.x; }
  uint16_t y() const { return _lastData.y; }
  int16_t deltaX() const { return _lastData.deltaX; }
  int16_t deltaY() const { return _lastData.deltaY; }
  bpadGesture gesture() const { return _lastData.gesture; }
  uint8_t rawGesture() const { return _lastData.rawGesture; }

  // --- Coordinate Transformation Setters ---
  void setRotation(uint8_t rotation) {
    _rotation = rotation % 4;
  }

  uint8_t getRotation() const {
    return _rotation;
  }

  void setScreenDimensions(uint16_t width, uint16_t height) {
    _width = width;
    _height = height;
  }

  void setInvertX(bool invert) { _invertX = invert; }
  void setInvertY(bool invert) { _invertY = invert; }
  bool isInvertedX() const { return _invertX; }
  bool isInvertedY() const { return _invertY; }

  // --- Gesture Engine Mode ---
  void setTouchMode(bpadTouchMode mode) {
    _touchMode = mode;
    _lastReportedHwGesture = 0;
    _softwareGestureReported = false;
  }

  bpadTouchMode getTouchMode() const {
    return _touchMode;
  }

  bool isReady() const { return _ready; }
  const bpadTouchConfig& config() const { return _cfg; }

private:
  bpadTouchConfig _cfg;
  TwoWire* _wire = nullptr;
  bool _ready = false;
  uint32_t _lastPollMs = 0;

  bpadTouchMode _touchMode = bpadTouchMode::NativeHardware;
  uint8_t _lastReportedHwGesture = 0;
  bool _softwareGestureReported = false;

  uint16_t _width = 360;
  uint16_t _height = 360;
  uint8_t _rotation = 0;
  bool _invertX = false;
  bool _invertY = false;

  bpadTouchData _lastData;
  uint16_t _lastRawX = 0;
  uint16_t _lastRawY = 0;
  uint16_t _startX = 0;
  uint16_t _startY = 0;
  int16_t _deltaX = 0;
  int16_t _deltaY = 0;
  unsigned long _touchStartMs = 0;
  bool _tracking = false;

  void remapCoordinates(uint16_t rx, uint16_t ry, uint16_t& outX, uint16_t& outY) {
    uint16_t tx = rx;
    uint16_t ty = ry;

    if (_invertX) tx = (_width > tx) ? (_width - tx) : 0;
    if (_invertY) ty = (_height > ty) ? (_height - ty) : 0;

    switch (_rotation) {
      case 1: // 90 deg clockwise
        outX = (_height > ty) ? (_height - ty) : 0;
        outY = tx;
        break;
      case 2: // 180 deg
        outX = (_width > tx) ? (_width - tx) : 0;
        outY = (_height > ty) ? (_height - ty) : 0;
        break;
      case 3: // 270 deg
        outX = ty;
        outY = (_width > tx) ? (_width - tx) : 0;
        break;
      case 0:
      default:
        outX = tx;
        outY = ty;
        break;
    }
  }

  bool ping() {
    _wire->beginTransmission(_cfg.address);
    return (_wire->endTransmission() == 0);
  }

  uint8_t readReg(uint8_t reg) {
    _wire->beginTransmission(_cfg.address);
    _wire->write(reg);
    if (_wire->endTransmission(false) != 0) return 0;
    if (_wire->requestFrom((uint8_t)_cfg.address, (uint8_t)1) != 1) return 0;
    return _wire->read();
  }

  bool readBurst(uint8_t reg, uint8_t* data, uint8_t length) {
    _wire->beginTransmission(_cfg.address);
    _wire->write(reg);
    if (_wire->endTransmission(false) != 0) return false;
    if (_wire->requestFrom((uint8_t)_cfg.address, length) != length) return false;
    for (uint8_t i = 0; i < length; i++) {
      data[i] = _wire->read();
    }
    return true;
  }

  void writeReg(uint8_t reg, uint8_t value) {
    _wire->beginTransmission(_cfg.address);
    _wire->write(reg);
    _wire->write(value);
    _wire->endTransmission();
  }

  bpadGesture normalizeRawGesture(uint8_t raw) {
    switch (raw) {
      case 0x01: return bpadGesture::SwipeDown;
      case 0x02: return bpadGesture::SwipeUp;
      case 0x03: return bpadGesture::SwipeLeft;
      case 0x04: return bpadGesture::SwipeRight;
      case 0x05: return bpadGesture::Tap;
      case 0x0B: return bpadGesture::DoubleTap;
      case 0x0C: return bpadGesture::LongPress;
      default:   return bpadGesture::None;
    }
  }

  bpadGesture processGesture(const bpadTouchData& td) {
    if (_touchMode == bpadTouchMode::NativeHardware) {
      // --- Pure CST816 Hardware Gesture Mode ---
      // Hardware registers hold the gesture for ~20-50ms. Edge-trigger so it only fires ONCE.
      if (td.rawGesture != 0) {
        if (td.rawGesture != _lastReportedHwGesture) {
          _lastReportedHwGesture = td.rawGesture;
          return normalizeRawGesture(td.rawGesture);
        }
      } else {
        _lastReportedHwGesture = 0;
      }
      return bpadGesture::None;
    }

    // --- Custom Software Gesture Mode ---
    bpadGesture hwGesture = normalizeRawGesture(td.rawGesture);

    if (td.pressed) {
      if (!_tracking) {
        _tracking = true;
        _startX = td.x;
        _startY = td.y;
        _touchStartMs = millis();
        _deltaX = 0;
        _deltaY = 0;
        _softwareGestureReported = false;
        return bpadGesture::None;
      }

      _deltaX = (int16_t)td.x - (int16_t)_startX;
      _deltaY = (int16_t)td.y - (int16_t)_startY;

      // Software Long Press (held >= 600ms within 10px radius)
      if ((millis() - _touchStartMs >= 600) && abs(_deltaX) <= 10 && abs(_deltaY) <= 10) {
        if (!_softwareGestureReported) {
          _softwareGestureReported = true;
          return bpadGesture::LongPress;
        }
      }

      return bpadGesture::None;
    }

    // Finger released in Software mode
    if (_tracking) {
      _tracking = false;
      unsigned long duration = millis() - _touchStartMs;
      int16_t dx = (int16_t)td.x - (int16_t)_startX;
      int16_t dy = (int16_t)td.y - (int16_t)_startY;
      _deltaX = dx;
      _deltaY = dy;

      if (_softwareGestureReported) {
        _softwareGestureReported = false;
        return bpadGesture::None;
      }

      // Software Swipe detection (movement >= 20px in >= 50ms)
      if (duration >= 50 && (abs(dx) >= 20 || abs(dy) >= 20)) {
        if (abs(dx) >= abs(dy)) {
          return (dx > 0) ? bpadGesture::SwipeRight : bpadGesture::SwipeLeft;
        } else {
          return (dy > 0) ? bpadGesture::SwipeDown : bpadGesture::SwipeUp;
        }
      }

      // Software Tap detection (movement <= 10px in <= 250ms)
      if (duration <= 250 && abs(dx) <= 10 && abs(dy) <= 10) {
        return bpadGesture::Tap;
      }

      // Fallback to hardware gesture if software thresholds weren't met
      if (hwGesture != bpadGesture::None) {
        return hwGesture;
      }
    }

    _softwareGestureReported = false;
    return bpadGesture::None;
  }
};
