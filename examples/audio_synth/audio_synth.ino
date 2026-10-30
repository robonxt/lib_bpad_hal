// =============================================================================
//  audio_synth.ino — bpad_hal 8-Bit Chiptune & Tone Synth Example
// =============================================================================
//  Demonstrates non-blocking tone generation, retro sound effects, and melody synthesis
//  on bpad 185 (MCP4725 12-bit DAC + GPIO 13 amp power rail).
// =============================================================================

#include <Wire.h>
#include <bpad_hal.h>
#include <math.h>

const bpadConfig hw = bpadConfig::defaultBoard();
bpadAudio audio(hw.audio);

// Note Frequencies (Hz)
#define NOTE_G4  392
#define NOTE_B4  494
#define NOTE_D5  587
#define NOTE_G5  784
#define NOTE_B5  988
#define NOTE_D6  1175
#define NOTE_E6  1319

inline uint16_t midiToFreq(uint8_t midiNote) {
  if (midiNote < 21 || midiNote > 108) return 0;
  return (uint16_t)(440.0f * powf(2.0f, (float)(midiNote - 69) / 12.0f));
}

// Retro 8-bit Sound Effects
void sfxCoin() {
  Serial.println("[sfx] Coin!");
  audio.playTone(NOTE_B5, 80);
  while (audio.isPlaying()) audio.update();
  audio.playTone(NOTE_E6, 300);
  while (audio.isPlaying()) audio.update();
}

void sfxJump() {
  Serial.println("[sfx] Jump!");
  for (uint16_t f = 200; f <= 800; f += 40) {
    audio.playTone(f, 15);
    while (audio.isPlaying()) audio.update();
  }
}

void sfxPowerUp() {
  Serial.println("[sfx] Power-Up!");
  const uint16_t arpeggio[] = {NOTE_G4, NOTE_B4, NOTE_D5, NOTE_G5, NOTE_B5, NOTE_D6};
  for (int i = 0; i < 6; i++) {
    audio.playTone(arpeggio[i], 60);
    while (audio.isPlaying()) audio.update();
  }
}

void setup() {
  Serial.begin(115200);
#if ARDUINO_USB_CDC_ON_BOOT
  Serial.setTxTimeoutMs(0);
#endif
  delay(500);

  Serial.printf("[audio_synth] board=%s  backend=%s  addr=0x%02X  ampPower=GPIO%d\n",
    hw.name,
    bpadAudioBackendName(hw.audio.backend),
    hw.audio.mcp4725Address,
    hw.audio.ampPower);

  Wire.begin(hw.i2c.sda, hw.i2c.scl);
  Wire.setClock(hw.i2c.clock);

  if (!audio.begin(Wire)) {
    Serial.println("[audio_synth] FAILED to initialize Audio DAC!");
    while (1) delay(100);
  }

  Serial.println("[audio_synth] DAC ready. Testing retro SFX...");
  audio.setVolume(3); // Max volume level (0-3)

  delay(500);
  sfxCoin();
  delay(400);
  sfxJump();
  delay(400);
  sfxPowerUp();
  delay(600);

  Serial.println("Starting continuous non-blocking arpeggio in loop()...");
}

void loop() {
  static uint32_t lastNoteTime = 0;
  static int noteIdx = 0;
  const uint16_t scale[] = {262, 294, 330, 349, 392, 440, 494, 523};

  // Must call audio.update() frequently to generate the square-wave audio frequency
  audio.update();

  if (millis() - lastNoteTime >= 250) {
    lastNoteTime = millis();
    audio.playTone(scale[noteIdx], 180);
    noteIdx = (noteIdx + 1) % (sizeof(scale) / sizeof(scale[0]));
  }
}
