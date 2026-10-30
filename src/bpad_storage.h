#pragma once
// =============================================================================
//  bpad_storage.h - Storage Driver for bpad hardware (185 and newer)
// =============================================================================
//  High-speed 4-bit SD_MMC storage driver for bpad 185 (XinYY SD32G SD-NAND).
//  Provides mounting with hardware stabilization delay, drive capability setup,
//  capacity and space metrics.
// =============================================================================

#include <Arduino.h>
#include <FS.h>
#include <SD_MMC.h>
#include "bpad_config.h"

class bpadStorage {
public:
  explicit bpadStorage(const bpadStorageConfig& config = bpadStorageConfig())
    : _cfg(config) {}

  void select(const bpadStorageConfig& config) {
    _cfg = config;
  }

  bool mount(const char* mountPoint = "/sd", bool formatOnFail = false) {
    if (_cfg.support == bpadSupport::NotPresent || _cfg.backend == bpadStorageBackend::None) {
      return false;
    }

    if (_cfg.backend == bpadStorageBackend::SD_MMC_4Wire) {
      // Stabilization delay for SD-NAND chips (XinYY SD32G requires >= 250ms)
      delay(300);

      // Setup pins
      pinMode(_cfg.sdmmcD3, OUTPUT);
      digitalWrite(_cfg.sdmmcD3, HIGH);

      pinMode(_cfg.sdmmcClk, OUTPUT);
      pinMode(_cfg.sdmmcCmd, INPUT_PULLUP);
      pinMode(_cfg.sdmmcD0,  INPUT_PULLUP);
      pinMode(_cfg.sdmmcD1,  INPUT_PULLUP);
      pinMode(_cfg.sdmmcD2,  INPUT_PULLUP);
      delay(10);

      gpio_set_drive_capability((gpio_num_t)_cfg.sdmmcClk, GPIO_DRIVE_CAP_3);
      gpio_set_drive_capability((gpio_num_t)_cfg.sdmmcCmd, GPIO_DRIVE_CAP_3);

      SD_MMC.setPins(_cfg.sdmmcClk, _cfg.sdmmcCmd, _cfg.sdmmcD0, _cfg.sdmmcD1, _cfg.sdmmcD2, _cfg.sdmmcD3);

      // Mount 4-bit mode (fallback to 1-bit / 10MHz if needed)
      _mounted = SD_MMC.begin(mountPoint, false, formatOnFail, 20000);
      if (!_mounted) {
        _mounted = SD_MMC.begin(mountPoint, false, formatOnFail, 10000);
      }
      return _mounted;
    }

    return false;
  }

  void unmount() {
    if (!_mounted) return;
    if (_cfg.backend == bpadStorageBackend::SD_MMC_4Wire) {
      SD_MMC.end();
    }
    _mounted = false;
  }

  // --- Getters ---
  bool isMounted() const { return _mounted; }

  uint64_t totalBytes() const {
    if (!_mounted) return 0;
    if (_cfg.backend == bpadStorageBackend::SD_MMC_4Wire) {
      return SD_MMC.totalBytes();
    }
    return 0;
  }

  uint64_t usedBytes() const {
    if (!_mounted) return 0;
    if (_cfg.backend == bpadStorageBackend::SD_MMC_4Wire) {
      return SD_MMC.usedBytes();
    }
    return 0;
  }

  float totalMB() const {
    return (float)totalBytes() / (1024.0f * 1024.0f);
  }

  float usedMB() const {
    return (float)usedBytes() / (1024.0f * 1024.0f);
  }

  float freeMB() const {
    uint64_t total = totalBytes();
    uint64_t used = usedBytes();
    if (total > used) {
      return (float)(total - used) / (1024.0f * 1024.0f);
    }
    return 0.0f;
  }

  fs::FS& fs() { return SD_MMC; }

  // Normalizes path so callers can use "/file.txt" or "/sd/file.txt" interchangeably
  static const char* normalizePath(const char* path) {
    if (path && strncmp(path, "/sd/", 4) == 0) {
      return path + 3; // Strip "/sd", leaving "/..."
    }
    return path;
  }

  File open(const char* path, const char* mode = FILE_READ, const bool create = false) {
    if (!_mounted) return File();
    return SD_MMC.open(normalizePath(path), mode, create);
  }

  bool exists(const char* path) {
    if (!_mounted) return false;
    return SD_MMC.exists(normalizePath(path));
  }

  bool remove(const char* path) {
    if (!_mounted) return false;
    return SD_MMC.remove(normalizePath(path));
  }

  bool mkdir(const char* path) {
    if (!_mounted) return false;
    return SD_MMC.mkdir(normalizePath(path));
  }

  const bpadStorageConfig& config() const { return _cfg; }

private:
  bpadStorageConfig _cfg;
  bool _mounted = false;
};
