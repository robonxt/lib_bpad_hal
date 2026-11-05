# bpad_hal

Hardware Abstraction Layer for bpad 185 and next-generation bpad hardware. 
Built natively for **ESP32 Arduino Core 3.x+ / ESP-IDF 5.x**.

## Quick Start

```cpp
#include <bpad_hal.h>

const bpadConfig hw = bpadConfig::defaultBoard(); // defaults to bpad185()

// Subsystem Drivers
bpadDisplay display(hw.display);
bpadTouch   touch(hw.touch);
bpadPower   power(hw.button);
bpadIMU     imu(hw.imu);
bpadCompass compass(hw.compass);
bpadAudio   audio(hw.audio);
bpadHaptics haptics(hw.haptics);
bpadStorage storage(hw.storage);

void setup() {
  Serial.begin(115200);
  Wire.begin(hw.i2c.sda, hw.i2c.scl, hw.i2c.clock);

  display.begin();
  touch.begin(Wire);
  power.begin();               // GPIO 21 SW3 / IP5306 KEY
  imu.begin(Wire);
  audio.begin(Wire);
  haptics.begin(Wire);
  storage.mount("/sd");
}
```

## Hardware Profile: bpad 185

| Subsystem | Specification |
|---|---|
| **Display** | 1.85" Round QSPI ST77916 (360x360), DC mode (GPIO 40), Tearing Effect / VSync (GPIO 41) |
| **Touch** | CST816S capacitive touch (Addr=0x15, IRQ=38) |
| **Power / Button** | Physical switch SW3 & IP5306 PMIC KEY (GPIO 21), debounced clicks & soft power-off |
| **IMU** | ICM-42607-P 6-axis motion sensor (Addr=0x69, -90° PCB rotation offset) |
| **Compass** | MMC5603NJ geomagnetic sensor (Addr=0x30) |
| **Audio** | MCP4725 12-bit DAC (Addr=0x61) with amplifier power gate (GPIO 13) |
| **Haptics** | MCP4728 4-channel DAC (Addr=0x60) for 4 independent vibration channels |
| **Storage** | High-speed 4-bit `SD_MMC` (XinYY SD32G SD-NAND) |
| **Wireless** | Bluetooth 5.0 (LE) active; Wi-Fi RX verified (TX limited on v1.0 PCB rev) |

## Diagnostic & Validation Examples

The library includes comprehensive test examples for every hardware subsystem on bpad:

| Example | Purpose |
|---|---|
| `display_test` | Full-screen graphics, color bars, shapes, and brightness control |
| `display_te_test` | ST77916 hardware TE pin (GPIO 41) 61 FPS tearing-free graphics |
| `touch_test` | CST816S capacitive touch coordinates, press state, and gestures |
| `power_button_test` | SW3 / IP5306 KEY (GPIO 21) clicks, holds, deep sleep, and power-off |
| `audio_synth` | Non-blocking tone synthesis (default Triangle wave for cleanest acoustics) |
| `audio_mp3` | 12-bit audio sample playback via MCP4725 DAC |
| `audio_sd_wav_player` | High-speed SD-NAND WAV audio player with SW3 USB Mass Storage toggle |
| `haptics_test` | MCP4728 quad-motor X-array vibration and directional pulses |
| `imu_test` | ICM-42607-P 6-axis accelerometer and gyroscope motion tracking |
| `compass_test` | MMC5603NJ geomagnetic sensor diagnostics |
| `storage_test` | High-speed 4-bit `SD_MMC` SD-NAND read/write performance |
| `usb_msc` | Exposes internal SD-NAND as a USB Mass Storage drive to PC |
| `ble_test` | Bluetooth 5.0 Low Energy advertising and connectivity |
| `wifi_test` | Wi-Fi network scanner and diagnostics |
| `i2c_scan` | Complete I2C bus scanner (0x15, 0x60, 0x61, 0x69) |
| `board_map_reference` | Full pinout and peripheral hardware mapping dump |

See [BOARD_MATRIX.md](BOARD_MATRIX.md) for pinout details and hardware notes.
