// =============================================================================
//  storage_test.ino — bpad_hal Storage Subsystem Test
// =============================================================================
//  Tests 4-bit SD_MMC storage (XinYY SD32G SD-NAND) mounting, capacity reporting,
//  and file read/write on bpad 185.
// =============================================================================

#include <Arduino.h>
#include <bpad_hal.h>

const bpadConfig hw = bpadConfig::defaultBoard();
bpadStorage storage(hw.storage);

void setup() {
  Serial.begin(115200);
#if ARDUINO_USB_CDC_ON_BOOT
  Serial.setTxTimeoutMs(0);
#endif
  delay(500);

  Serial.printf("\n[storage_test] board=%s  backend=%s  device=%s\n",
    hw.name,
    bpadStorageBackendName(hw.storage.backend),
    hw.storage.device);

  Serial.println("[storage_test] Mounting storage subsystem...");
  if (!storage.mount("/sd", false)) {
    Serial.println("[storage_test] Mount FAILED! Check storage chip soldering / power / pins.");
    return;
  }

  Serial.println("[storage_test] Storage mounted successfully!");

  uint64_t total = storage.totalBytes();
  uint64_t used = storage.usedBytes();
  Serial.printf("[storage_test] Total space: %llu bytes (%.2f MB)\n", total, storage.totalMB());
  Serial.printf("[storage_test] Used space:  %llu bytes (%.2f MB)\n", used, storage.usedMB());
  Serial.printf("[storage_test] Free space:  %.2f MB\n", storage.freeMB());

  // Test write/read verification
  const char* testPath = "/hal_test.txt";
  const char* testPayload = "Hello from bpad_hal! Modern HAL running on bpad 185.";

  Serial.printf("[storage_test] Writing test payload to %s ...\n", testPath);
  File f = storage.open(testPath, FILE_WRITE);
  if (!f) {
    Serial.println("[storage_test] Failed to open file for writing!");
    return;
  }
  f.println(testPayload);
  f.close();

  Serial.println("[storage_test] Reading test payload back...");
  f = storage.open(testPath, FILE_READ);
  if (!f) {
    Serial.println("[storage_test] Failed to open file for reading!");
    return;
  }
  String line = f.readStringUntil('\n');
  f.close();
  line.trim();

  Serial.printf("[storage_test] Content read: \"%s\"\n", line.c_str());
  if (line == testPayload) {
    Serial.println("[storage_test] >>> PASS: File read/write verified successfully! <<<");
  } else {
    Serial.println("[storage_test] >>> FAIL: Read content mismatch! <<<");
  }

  // Clean up
  storage.remove(testPath);
}

void loop() {
  delay(1000);
}
