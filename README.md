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
  imu.begin(Wire);
  audio.begin(Wire);
  haptics.begin(Wire);
  storage.mount("/sd");
}
```

## Hardware Profile: bpad 185

| Subsystem | Specification |
|---|---|
| **Display** | 1.85" Round QSPI ST77916 (360x360), hardware DC mode (GPIO 40), JTAG IO release |
| **Touch** | CST816S capacitive touch (IRQ=38, Addr=0x15) |
| **IMU** | ICM-42607-P 6-axis motion sensor (Addr=0x69, -90° PCB rotation offset) |
| **Compass** | MMC5603NJ geomagnetic sensor (Addr=0x30) |
| **Audio** | MCP4725 12-bit DAC (Addr=0x61) with amplifier power gate (GPIO 13) |
| **Haptics** | MCP4728 4-channel DAC (Addr=0x60) for 4 independent vibration channels |
| **Storage** | High-speed 4-bit `SD_MMC` (XinYY SD32G SD-NAND) |
| **Wireless** | Bluetooth 5.0 (LE) active; Wi-Fi RX verified (TX limited on v1.0 PCB rev) |

See [BOARD_MATRIX.md](BOARD_MATRIX.md) for pinout details and hardware notes.
