// =============================================================================
//  usb_msc.ino — bpad_hal USB Mass Storage
// =============================================================================
//  Exposes the bpad 185 SD-NAND flash chip as a USB Mass Storage drive on a PC.
//
//  HOW TO TOGGLE USB MSC MODE:
//  - Via USB Drive: Edit 'usb_msc_config.txt' on your computer ('true' / 'false'),
//    safely eject the drive, and press Reset on the bpad.
//  - Via Serial Monitor: When in serial mode, send 'true' or 'false' in Serial Monitor.
// =============================================================================

#include <Arduino.h>
#include <USB.h>
#include <USBMSC.h>
#include <SD_MMC.h>
#include <bpad_hal.h>

const bpadConfig hw = bpadConfig::defaultBoard();

USBMSC msc;
const char* configPath = "/usb_msc_config.txt";

static int32_t onRead(uint32_t lba, uint32_t offset, void* buffer, uint32_t bufsize) {
  (void)offset;
  uint8_t* buf = (uint8_t*)buffer;
  uint32_t sectors = bufsize / 512;
  for (uint32_t i = 0; i < sectors; i++) {
    if (!SD_MMC.readRAW(buf + (i * 512), lba + i)) return -1;
  }
  return (int32_t)bufsize;
}

static int32_t onWrite(uint32_t lba, uint32_t offset, uint8_t* buffer, uint32_t bufsize) {
  (void)offset;
  uint8_t* buf = (uint8_t*)buffer;
  uint32_t sectors = bufsize / 512;
  for (uint32_t i = 0; i < sectors; i++) {
    if (!SD_MMC.writeRAW(buf + (i * 512), lba + i)) return -1;
  }
  return (int32_t)bufsize;
}

static bool onStartStop(uint8_t power_condition, bool start, bool load_eject) {
  return true;
}

// Writes USB_MSC state to /usb_msc_config.txt
void setMscConfigFile(bool enable) {
  SD_MMC.remove(configPath);
  File file = SD_MMC.open(configPath, FILE_WRITE);

  if (file) {
    file.println("# =============================================================================");
    file.println("# bpad USB MSC Configuration");
    file.println("# =============================================================================");
    file.println("# To exit USB drive mode and re-enable normal Arduino IDE sketch flashing:");
    file.println("# Change 'USB_MSC = true' to 'USB_MSC = false', safely eject the drive, and reset.");
    file.println("# =============================================================================");
    file.printf("USB_MSC = %s\n", enable ? "true" : "false");
    file.flush();
    file.close();
    Serial.printf("[usb_msc] Saved to %s -> USB_MSC = %s\n", configPath, enable ? "true" : "false");
  } else {
    Serial.println("[usb_msc] ERROR: Failed to open config file for writing.");
  }
}

// Reads usb_msc_config.txt on storage root. Parses non-comment lines.
bool checkMscConfigEnabled() {
  File file = SD_MMC.open(configPath, FILE_READ);

  if (!file) {
    setMscConfigFile(true);
    return true;
  }

  bool enabled = true;
  while (file.available()) {
    String line = file.readStringUntil('\n');
    line.trim();
    if (line.startsWith("#") || line.startsWith("//") || line.length() == 0) {
      continue;
    }
    line.toLowerCase();
    if (line.indexOf("false") >= 0 || line.indexOf("0") >= 0) {
      enabled = false;
    } else if (line.indexOf("true") >= 0 || line.indexOf("1") >= 0) {
      enabled = true;
    }
  }
  file.close();
  return enabled;
}

void handleSerialCommands() {
  if (!Serial.available()) return;

  String cmd = Serial.readStringUntil('\n');
  cmd.trim();
  cmd.toLowerCase();

  if (cmd.length() == 0) return;

  if (cmd == "true" || cmd == "on" || cmd == "msc on" || cmd == "enable") {
    Serial.println("\n[usb_msc] Setting USB_MSC = true in config file...");
    setMscConfigFile(true);
    Serial.println("[usb_msc] Reset the bpad to boot into USB Mass Storage drive mode (or send 'reboot').\n");
  } else if (cmd == "false" || cmd == "off" || cmd == "msc off" || cmd == "disable") {
    Serial.println("\n[usb_msc] Setting USB_MSC = false in config file...");
    setMscConfigFile(false);
    Serial.println("[usb_msc] Reset the bpad to boot into Serial flashing mode (or send 'reboot').\n");
  } else if (cmd == "status") {
    Serial.printf("[usb_msc] Current config: USB_MSC = %s\n", checkMscConfigEnabled() ? "true" : "false");
  } else if (cmd == "reboot" || cmd == "restart") {
    Serial.println("[usb_msc] Rebooting...");
    delay(500);
    ESP.restart();
  } else if (cmd == "help" || cmd == "?") {
    Serial.println("\n--- bpad USB MSC Serial Commands ---");
    Serial.println("  'true'  or 'msc on'   - Set USB_MSC = true (boot as flash drive)");
    Serial.println("  'false' or 'msc off'  - Set USB_MSC = false (boot in Serial flashing mode)");
    Serial.println("  'status'              - Show current configuration");
    Serial.println("  'reboot'              - Restart the bpad");
    Serial.println("-------------------------------------\n");
  } else {
    Serial.printf("[usb_msc] Unknown command '%s'. Send 'help' for options.\n", cmd.c_str());
  }
}

void setup() {
  Serial.begin(115200);
#if ARDUINO_USB_CDC_ON_BOOT
  Serial.setTxTimeoutMs(0);
#endif
  delay(500);

  Serial.printf("[usb_msc] board=%s  storage=%s\n", hw.name, bpadStorageBackendName(hw.storage.backend));

  uint32_t sectorCount = 0;

  SD_MMC.setPins(hw.storage.sdmmcClk, hw.storage.sdmmcCmd,
                 hw.storage.sdmmcD0,  hw.storage.sdmmcD1,
                 hw.storage.sdmmcD2,  hw.storage.sdmmcD3);

  if (!SD_MMC.begin("/sdcard", false, false, 20000)) {
    Serial.println("[usb_msc] SD_MMC mount FAILED.");
    while (1) delay(1000);
  }
  sectorCount = SD_MMC.numSectors();
  Serial.printf("[usb_msc] Storage mounted. sectors=%u (%llu MB)\n",
                sectorCount, (uint64_t)sectorCount * 512 / (1024 * 1024));

  // Check if USB MSC is enabled in /usb_msc_config.txt
  bool mscEnabled = checkMscConfigEnabled();

  if (!mscEnabled) {
    Serial.println("\n==================================================================");
    Serial.println("[usb_msc] USB_MSC = false in /usb_msc_config.txt");
    Serial.println("[usb_msc] USB Mass Storage disabled. Normal USB Serial mode active.");
    Serial.println("[usb_msc] Ready for Arduino IDE sketch upload.");
    Serial.println("==================================================================");
    Serial.println("[usb_msc] Serial Commands available: 'true' (enable MSC), 'status', 'reboot', 'help'.\n");
    return;
  }

  // Start USB MSC Stack
  msc.vendorID("bpad");
  msc.productID(hw.name);
  msc.productRevision("1.0");
  msc.onStartStop(onStartStop);
  msc.onRead(onRead);
  msc.onWrite(onWrite);
  msc.mediaPresent(true);
  msc.begin(sectorCount, 512);

  USB.begin();

  Serial.println("[usb_msc] USB Mass Storage active.");
  Serial.println("[usb_msc] To exit: edit usb_msc_config.txt on the drive to 'USB_MSC = false', eject, and reset.");
}

void loop() {
  handleSerialCommands();
  delay(20);
}
