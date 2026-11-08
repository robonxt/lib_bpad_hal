#pragma once
// =============================================================================
//  bpad_config.h - Hardware manifest for bpad boards (185 and newer)
// =============================================================================
//  Part of bpad_hal. Built for ESP32 Core 3.x+ / IDF 5.x.
// =============================================================================

#include <stdint.h>

namespace bpad {

enum class Board : uint8_t {
  bpad185_V1 = 1,
  Production = 10
};

enum class Support : uint8_t {
  NotPresent,
  Present,
  Unverified
};

enum class DisplayBackend : uint8_t {
  None,
  ST77916_QSPI,
  Unknown
};

enum class AudioBackend : uint8_t {
  None,
  MCP4725
};

enum class HapticsBackend : uint8_t {
  None,
  MCP4728_DAC,
  GPIO_PWM
};

enum class IMUBackend : uint8_t {
  None,
  ICM42607,
  ICM42670,
  Unknown
};

enum class StorageBackend : uint8_t {
  None,
  SD_MMC_4Wire,
  LittleFS,
  Unknown
};

struct I2CConfig {
  uint8_t sda = 18;
  uint8_t scl = 17;
  uint32_t clock = 400000;
};

struct DisplayConfig {
  Support support = Support::Present;
  DisplayBackend backend = DisplayBackend::ST77916_QSPI;
  const char* controller = "ST77916";
  uint16_t width = 360;
  uint16_t height = 360;
  uint8_t bl = 39;
  uint8_t cs = 14;
  uint8_t rst = 42;
  uint8_t dc = 40;
  int8_t te = 41;   // LCD Tearing Effect (VSync signal)
  uint8_t sclk = 6;
  uint8_t mosi = 5;
  int8_t miso = -1;
  // QSPI data lines
  int8_t d0 = 5;
  int8_t d1 = 4;
  int8_t d2 = 2;
  int8_t d3 = 1;
  uint32_t backlightFreq = 20000;
  uint8_t backlightResolution = 8;
  uint8_t brightnessLevels[4] = {50, 120, 185, 255};
};

struct TouchConfig {
  Support support = Support::Present;
  const char* controller = "CST816S";
  uint8_t address = 0x15;
  int8_t irq = 38;
  int8_t rst = -1;  // Shared with LCD_RST (GPIO 42)
  bool pollingKnownGood = true;
};

struct AudioConfig {
  Support support = Support::Present;
  AudioBackend backend = AudioBackend::MCP4725;
  uint8_t ampPower = 13;
  uint8_t mcp4725Address = 0x61;
  uint8_t volumeLevels[4] = {0, 5, 9, 12};
};

struct HapticsConfig {
  Support support = Support::Present;
  HapticsBackend backend = HapticsBackend::MCP4728_DAC;
  uint8_t count = 4;
  uint8_t mcp4728Address = 0x60;
  uint8_t gpioPins[4] = {8, 9, 10, 11};
  uint8_t mcp4728Channels[4] = {0, 1, 2, 3};
  uint16_t dacOnValue = 4095;
  uint32_t pwmFreq = 5000;
  uint8_t pwmResolution = 8;
  uint16_t levels[4] = {0, 2000, 3100, 4095};
};

struct IMUConfig {
  Support support = Support::Present;
  IMUBackend backend = IMUBackend::ICM42607;
  uint8_t address = 0x69;
  float accelScale = 1.0f / 16384.0f; // ±2g mode: 16384 LSB/g
  float gyroScale  = 1.0f / 16.4f;    // ±2000dps mode: 16.4 LSB/(deg/s)
  int16_t orientationOffsetDeg = -90; // ICM mounted at -90 deg rotation relative to screen top
  const char* note = "ICM-42607-P mounted at -90 deg rotation. Repeated-Start I2C reads.";
};

struct CompassConfig {
  Support support = Support::NotPresent;
  const char* device = "MMC5603NJ";
  uint8_t address = 0x30;
  const char* note = "Not populated on bpad 185 V1 hardware.";
};

struct RTCConfig {
  Support support = Support::NotPresent;
  const char* device = "";
  uint8_t address = 0xFF;
  const char* note = "";
};

struct StorageConfig {
  Support support = Support::Present;
  StorageBackend backend = StorageBackend::SD_MMC_4Wire;
  const char* device = "XinYY SD32G";
  int8_t cs = -1;
  int8_t mosi = -1;
  int8_t sck = -1;
  int8_t miso = -1;
  uint8_t sdmmcClk = 9;
  uint8_t sdmmcCmd = 12;
  uint8_t sdmmcD0 = 11;
  uint8_t sdmmcD1 = 10;
  uint8_t sdmmcD2 = 7;
  uint8_t sdmmcD3 = 8;
};

struct WirelessConfig {
  Support wifi = Support::Present;
  Support bluetooth = Support::Present;
};

struct ButtonConfig {
  Support support = Support::Present;
  int8_t pin = 21;          // SW3 & IP5306 KEY pin
  bool activeLow = true;
  uint16_t debounceMs = 30;
  uint16_t longPressMs = 800;
};

struct SystemConfig {
  Support psram = Support::Present;
  Support usbMsc = Support::Present;
  uint32_t flashBytes = 16UL * 1024UL * 1024UL;
  const char* note = "";
};

struct Config {
  Board board = Board::bpad185_V1;
  const char* name = "bpad185";
  const char* note = "1.85\" Round QSPI ST77916 (360x360), ICM-42607-P IMU (0x69), XinYY SD32G 4-bit SDMMC, MCP4725 audio, MCP4728 haptics.";

  I2CConfig i2c;
  DisplayConfig display;
  TouchConfig touch;
  AudioConfig audio;
  HapticsConfig haptics;
  IMUConfig imu;
  CompassConfig compass;
  RTCConfig rtc;
  StorageConfig storage;
  ButtonConfig button;
  WirelessConfig wireless;
  SystemConfig system;

  static Config bpad185() {
    Config cfg;
    return cfg;
  }

  static Config defaultBoard() {
    return bpad185();
  }
};

inline const char* boardName(Board board) {
  switch (board) {
    case Board::bpad185_V1: return "bpad185";
    case Board::Production: return "production";
    default: return "unknown";
  }
}

inline const char* supportName(Support support) {
  switch (support) {
    case Support::NotPresent: return "not present";
    case Support::Present: return "present";
    case Support::Unverified: return "unverified";
    default: return "unknown";
  }
}

inline const char* displayBackendName(DisplayBackend backend) {
  switch (backend) {
    case DisplayBackend::None: return "none";
    case DisplayBackend::ST77916_QSPI: return "ST77916 (QSPI)";
    default: return "unknown";
  }
}

inline const char* audioBackendName(AudioBackend backend) {
  switch (backend) {
    case AudioBackend::None: return "none";
    case AudioBackend::MCP4725: return "MCP4725";
    default: return "unknown";
  }
}

inline const char* hapticsBackendName(HapticsBackend backend) {
  switch (backend) {
    case HapticsBackend::None: return "none";
    case HapticsBackend::GPIO_PWM: return "GPIO PWM";
    case HapticsBackend::MCP4728_DAC: return "MCP4728 DAC";
    default: return "unknown";
  }
}

inline const char* imuBackendName(IMUBackend backend) {
  switch (backend) {
    case IMUBackend::None: return "none";
    case IMUBackend::ICM42670: return "ICM-42670";
    case IMUBackend::ICM42607: return "ICM-42607-P";
    default: return "unknown";
  }
}

inline const char* storageBackendName(StorageBackend backend) {
  switch (backend) {
    case StorageBackend::None: return "none";
    case StorageBackend::LittleFS: return "LittleFS";
    case StorageBackend::SD_MMC_4Wire: return "SD_MMC 4-wire";
    default: return "unknown";
  }
}

} // namespace bpad
