// =============================================================================
//  i2c_scan.ino — bpad_hal I2C Bus Scanner & Manifest Cross-Check
// =============================================================================
//  Scans the I2C bus (0x01-0x7F), identifies detected hardware against known
//  bpad chip addresses, and cross-references against the bpad 185 manifest.
// =============================================================================

#include <Wire.h>
#include <bpad_hal.h>

const bpadConfig hw = bpadConfig::defaultBoard();

struct KnownDevice {
  uint8_t addr;
  const char* name;
};

static const KnownDevice knownDevices[] = {
  {0x15, "CST816S (Capacitive Touch)"},
  {0x30, "MMC5603NJ (Compass / Magnetometer)"},
  {0x60, "MCP4728 (4-Channel DAC - Haptics)"},
  {0x61, "MCP4725 (Single DAC - Speaker Audio)"},
  {0x69, "ICM-42607-P (6-Axis IMU)"},
};
static const int knownCount = sizeof(knownDevices) / sizeof(knownDevices[0]);

const char* lookupDevice(uint8_t addr) {
  for (int i = 0; i < knownCount; i++) {
    if (knownDevices[i].addr == addr) return knownDevices[i].name;
  }
  return nullptr;
}

void probeUnknown(uint8_t addr) {
  Serial.printf("    full register dump for 0x%02X:\n", addr);
  Serial.printf("         00 01 02 03 04 05 06 07 08 09 0A 0B 0C 0D 0E 0F\n");
  for (int row = 0; row < 16; row++) {
    Serial.printf("    0x%02X: ", row * 16);
    for (int col = 0; col < 16; col++) {
      uint8_t reg = (uint8_t)(row * 16 + col);
      Wire.beginTransmission(addr);
      Wire.write(reg);
      if (Wire.endTransmission(false) != 0) {
        Serial.print("-- ");
        continue;
      }
      Wire.requestFrom((int)addr, 1);
      if (Wire.available()) {
        Serial.printf("%02X ", Wire.read());
      } else {
        Serial.print("?? ");
      }
    }
    Serial.println();
  }
}

void setup() {
  Serial.begin(115200);
#if ARDUINO_USB_CDC_ON_BOOT
  Serial.setTxTimeoutMs(0);
#endif
  delay(500);

  Serial.printf("[i2c_scan] board=%s  SDA=%u SCL=%u  clock=%u Hz\n",
    hw.name, hw.i2c.sda, hw.i2c.scl, hw.i2c.clock);

  Wire.begin(hw.i2c.sda, hw.i2c.scl);
  Wire.setClock(hw.i2c.clock);
  delay(100);

  Serial.println("\n[i2c_scan] scanning 0x01-0x7F...");

  bool detected[128] = {false};
  int found = 0;

  for (uint8_t addr = 1; addr < 127; addr++) {
    Wire.beginTransmission(addr);
    uint8_t err = Wire.endTransmission();
    if (err == 0) {
      detected[addr] = true;
      const char* name = lookupDevice(addr);
      if (name) {
        Serial.printf("  0x%02X  %s\n", addr, name);
      } else {
        Serial.printf("  0x%02X  (unknown device)\n", addr);
        probeUnknown(addr);
      }
      found++;
    }
  }

  Serial.printf("[i2c_scan] %d device(s) found on bus.\n", found);

  // Cross-reference against manifest expected devices
  Serial.println("\n[i2c_scan] Manifest Cross-Check:");

  auto checkDevice = [&](const char* label, uint8_t addr, const char* desc, bool present) {
    if (!present || addr == 0 || addr == 0xFF) return;
    bool ok = detected[addr];
    Serial.printf("  %-8s 0x%02X (%-20s) -> %s\n", label, addr, desc, ok ? "[OK]" : "[MISSING]");
  };

  checkDevice("touch",   hw.touch.address,   hw.touch.controller,                     hw.touch.support == bpadSupport::Present);
  checkDevice("imu",     hw.imu.address,     bpadIMUBackendName(hw.imu.backend),      hw.imu.support == bpadSupport::Present);
  checkDevice("compass", hw.compass.address, hw.compass.device,                       hw.compass.support == bpadSupport::Present);
  checkDevice("audio",   hw.audio.mcp4725Address, "MCP4725 DAC",                      hw.audio.support == bpadSupport::Present);
  checkDevice("haptics", hw.haptics.mcp4728Address, "MCP4728 DAC",                    hw.haptics.support == bpadSupport::Present);

  Serial.println("\n[i2c_scan] Scan complete.");
}

void loop() {}
