// =============================================================================
//  audio_mp3.ino — bpad_hal WAV Playback Example
// =============================================================================
//  Plays a WAV embedded in PROGMEM with microsecond pacing.
//  Prerequisites: Install "ESP8266Audio" via Arduino Library Manager.
//  Use audio_converter.html to generate test_audio.h.
// =============================================================================

#include <Arduino.h>
#include <Wire.h>
#include <bpad_hal.h>
#include <AudioGeneratorWAV.h>
#include <AudioFileSourcePROGMEM.h>
#include <AudioOutput.h>

#include "test_audio.8k.h"

const bpad::Config hw = bpad::Config::defaultBoard();

class AudioOutputI2CDAC : public AudioOutput {
private:
  uint32_t _targetInterval = 250;
  uint32_t _lastMicros = 0;

public:
  bool begin() override { return true; }

  bool SetRate(int hz) override {
    if (hz > 0) _targetInterval = 1000000 / hz;
    return true;
  }

  bool ConsumeSample(int16_t sample[2]) override {
    while (micros() - _lastMicros < _targetInterval) {
      yield();
    }
    _lastMicros = micros();

    int32_t s = sample[LEFTCHANNEL];
    uint16_t v = (uint16_t)(((s * 8 / 10) + 32768) >> 4);

    Wire.beginTransmission(hw.audio.mcp4725Address);
    Wire.write((v >> 8) & 0x0F);
    Wire.write(v & 0xFF);
    Wire.endTransmission();

    return true;
  }

  bool stop() override {
    Wire.beginTransmission(hw.audio.mcp4725Address);
    Wire.write(0x08);
    Wire.write(0x00);
    Wire.endTransmission();
    return true;
  }
};

AudioGeneratorWAV*      wav    = nullptr;
AudioFileSourcePROGMEM* source = nullptr;
AudioOutputI2CDAC*      output = nullptr;

void setup() {
  Serial.begin(115200);
#if ARDUINO_USB_CDC_ON_BOOT
  Serial.setTxTimeoutMs(0);
#endif
  delay(500);

  Serial.printf("[audio_mp3] board=%s  size=%u bytes\n", hw.name, audio_data_len);

  Wire.begin(hw.i2c.sda, hw.i2c.scl);
  Wire.setClock(hw.i2c.clock);

  if (hw.audio.ampPower != 0xFF) {
    pinMode(hw.audio.ampPower, OUTPUT);
    digitalWrite(hw.audio.ampPower, HIGH);
    delay(20);
  }

  source = new AudioFileSourcePROGMEM(audio_data, audio_data_len);
  output = new AudioOutputI2CDAC();
  wav    = new AudioGeneratorWAV();

  if (!wav->begin(source, output)) {
    Serial.println("[audio_mp3] WAV begin FAILED");
    while (1) delay(100);
  }

  Serial.println("[audio_mp3] Playing...");
}

void loop() {
  if (wav && wav->isRunning()) {
    if (!wav->loop()) {
      wav->stop();
      if (hw.audio.ampPower != 0xFF) {
        digitalWrite(hw.audio.ampPower, LOW);
      }
      Serial.println("[audio_mp3] Done.");
    }
  }
}
