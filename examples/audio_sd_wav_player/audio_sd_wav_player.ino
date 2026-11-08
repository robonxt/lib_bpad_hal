// =============================================================================
//  audio_sd_wav_player.ino — High-Speed SD-NAND WAV Audio Player
// =============================================================================
//  Plays any 8kHz or 11kHz WAV file directly from the internal SD-NAND flash chip!
//
//  Features:
//  - Compiles in 3 seconds (NO huge 5MB .h files!)
//  - USB Mass Storage (MSC) toggle built-in:
//      * Send 'msc' in Serial Monitor -> connects to PC as a USB Drive so you
//        can drag-and-drop .wav files directly into the SD card!
//      * Send 'play <filename>' in Serial Monitor -> immediately plays the WAV file!
//      * Send 'list' in Serial Monitor -> lists all files on the SD card!
// =============================================================================

#include <Arduino.h>
#include <Wire.h>
#include <USB.h>
#include <USBMSC.h>
#include <SD_MMC.h>
#include <bpad_hal.h>
#include <AudioGeneratorWAV.h>
#include <AudioFileSourceFS.h>
#include <AudioOutput.h>

const bpad::Config hw = bpad::Config::defaultBoard();
bpad::Audio audio(hw.audio);
bpad::Power pwr(hw.button);

USBMSC msc;
bool mscActive = false;

// Custom jitter-free AudioOutput stream for MCP4725 DAC
class AudioOutputMCP4725 : public AudioOutput {
private:
  uint32_t _targetIntervalUs = 125; // 8kHz = 125us, 11kHz = 90us
  uint32_t _lastMicros = 0;

public:
  bool begin() override { return true; }

  bool SetRate(int hz) override {
    if (hz > 0) _targetIntervalUs = 1000000 / hz;
    return true;
  }

  bool ConsumeSample(int16_t sample[2]) override {
    while (micros() - _lastMicros < _targetIntervalUs) {
      yield();
    }
    _lastMicros = micros();

    int32_t s = sample[LEFTCHANNEL];
    uint16_t dacVal = (uint16_t)(((s * 8 / 10) + 32768) >> 4);
    audio.writeSampleDirect(dacVal);
    return true;
  }

  bool stop() override {
    audio.writeSampleDirect(2048);
    return true;
  }
};

AudioGeneratorWAV* wav = nullptr;
AudioFileSourceFS* fileSource = nullptr;
AudioOutputMCP4725* audioOut = nullptr;

// MSC Callbacks for PC USB Drive access
static int32_t onMscRead(uint32_t lba, uint32_t offset, void* buffer, uint32_t bufsize) {
  (void)offset;
  uint8_t* buf = (uint8_t*)buffer;
  uint32_t sectors = bufsize / 512;
  for (uint32_t i = 0; i < sectors; i++) {
    if (!SD_MMC.readRAW(buf + (i * 512), lba + i)) return -1;
  }
  return (int32_t)bufsize;
}

static int32_t onMscWrite(uint32_t lba, uint32_t offset, uint8_t* buffer, uint32_t bufsize) {
  (void)offset;
  uint8_t* buf = (uint8_t*)buffer;
  uint32_t sectors = bufsize / 512;
  for (uint32_t i = 0; i < sectors; i++) {
    if (!SD_MMC.writeRAW(buf + (i * 512), lba + i)) return -1;
  }
  return (int32_t)bufsize;
}

void listSDFiles() {
  Serial.println("\n--- SD-NAND File Listing ---");
  File root = SD_MMC.open("/");
  if (!root || !root.isDirectory()) {
    Serial.println("[ERROR] Failed to open SD root directory.");
    return;
  }
  File file = root.openNextFile();
  int count = 0;
  while (file) {
    if (file.isDirectory()) {
      Serial.printf("  [DIR]  %s\n", file.name());
    } else {
      Serial.printf("  [FILE] %-24s  (%u bytes)\n", file.name(), file.size());
      count++;
    }
    file = root.openNextFile();
  }
  Serial.printf("Total files found: %d\n----------------------------\n", count);
}

void startWavPlayback(const char* filename) {
  if (mscActive) {
    Serial.println("[ERROR] Cannot play audio while USB Drive mode is active. Type 'exit' to leave MSC mode first.");
    return;
  }

  if (wav && wav->isRunning()) {
    wav->stop();
  }

  delete fileSource;
  fileSource = new AudioFileSourceFS(SD_MMC, filename);

  if (!fileSource->isOpen()) {
    Serial.printf("[ERROR] File '%s' not found on SD card!\n", filename);
    return;
  }

  audio.enableAmp(true);
  if (!wav->begin(fileSource, audioOut)) {
    Serial.println("[ERROR] WAV decoder failed to start!");
    audio.enableAmp(false);
    return;
  }

  Serial.printf("[AUDIO] Playing '%s'...\n", filename);
}

void enterMscMode() {
  if (mscActive) return;
  if (wav && wav->isRunning()) wav->stop();
  audio.enableAmp(false);

  Serial.println("[MSC] Starting USB Mass Storage drive...");
  msc.vendorID("bpad");
  msc.productID("SD-NAND Flash");
  msc.productRevision("1.0");
  msc.onRead(onMscRead);
  msc.onWrite(onMscWrite);
  msc.mediaPresent(true);
  msc.begin(SD_MMC.numSectors(), 512);
  USB.begin();

  mscActive = true;
  Serial.println("[MSC] Connected! Look for the bpad USB Drive on your PC.");
  Serial.println("[MSC] Drag your .wav files over, safely eject the drive, then type 'exit' here.");
}

void exitMscMode() {
  if (!mscActive) return;
  Serial.println("[MSC] Exiting USB drive mode and remounting SD filesystem...");
  msc.end();
  delay(200);
  SD_MMC.end();
  delay(100);

  SD_MMC.setPins(hw.storage.sdmmcClk, hw.storage.sdmmcCmd, hw.storage.sdmmcD0,
                 hw.storage.sdmmcD1, hw.storage.sdmmcD2, hw.storage.sdmmcD3);
  SD_MMC.begin("/sdcard", false, false, 40000);

  mscActive = false;
  Serial.println("[MSC] Remount complete.");
  listSDFiles();
}

void setup() {
  Serial.begin(115200);
  delay(800);

  Serial.println("\n==============================================");
  Serial.println("    bpad High-Speed SD-NAND WAV Player        ");
  Serial.println("==============================================");

  // 1. Initialize Power Button (SW3)
  pwr.begin();

  // 1. Initialize Audio DAC
  Wire.begin(hw.i2c.sda, hw.i2c.scl);
  Wire.setClock(hw.i2c.clock);
  audio.begin(Wire);
  audio.setVolume(3);

  audioOut = new AudioOutputMCP4725();
  wav = new AudioGeneratorWAV();

  // 2. Initialize SD_MMC storage
  SD_MMC.setPins(hw.storage.sdmmcClk, hw.storage.sdmmcCmd, hw.storage.sdmmcD0,
                 hw.storage.sdmmcD1, hw.storage.sdmmcD2, hw.storage.sdmmcD3);
  if (!SD_MMC.begin("/sdcard", false, false, 40000)) {
    Serial.println("[ERROR] Failed to mount SD-NAND chip!");
  } else {
    Serial.printf("[STORAGE] SD-NAND mounted: %llu MB\n", SD_MMC.totalBytes() / (1024 * 1024));
    listSDFiles();
  }

  Serial.println("\nCommands (via Serial Monitor):");
  Serial.println("  - 'list'              : Show files on SD card");
  Serial.println("  - 'msc'               : Turn on USB Drive mode (drag & drop WAVs from PC)");
  Serial.println("  - 'exit'              : Exit USB Drive mode & return to playback");
  Serial.println("  - 'play <filename>'   : Play a WAV file (e.g. 'play /test.wav')\n");
}

void loop() {
  pwr.update();

  // Pressing SW3 physical button toggles MSC on/off safely!
  if (pwr.isClicked()) {
    if (mscActive) {
      exitMscMode();
    } else {
      enterMscMode();
    }
  }

  // WAV audio stream pump
  if (!mscActive && wav && wav->isRunning()) {
    if (!wav->loop()) {
      wav->stop();
      audio.enableAmp(false);
      Serial.println("[AUDIO] Playback finished.");
    }
  }

  // Serial commands
  if (Serial.available()) {
    String line = Serial.readStringUntil('\n');
    line.trim();

    if (line.equalsIgnoreCase("list")) {
      listSDFiles();
    } else if (line.equalsIgnoreCase("msc")) {
      enterMscMode();
    } else if (line.equalsIgnoreCase("exit")) {
      exitMscMode();
    } else if (line.startsWith("play ") || line.startsWith("play/")) {
      String fname = line.substring(4);
      fname.trim();
      if (!fname.startsWith("/")) fname = "/" + fname;
      startWavPlayback(fname.c_str());
    } else if (line.equalsIgnoreCase("stop")) {
      if (wav && wav->isRunning()) wav->stop();
      audio.enableAmp(false);
      Serial.println("[AUDIO] Stopped.");
    }
  }
}
