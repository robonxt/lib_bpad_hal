# bpad_hal Hardware Matrix

This document tracks the hardware specifications for `bpad_hal` (bpad 185 and future hardware iterations).

| Subsystem | bpad 185 | Future Iterations | Confidence |
|---|---|---|---|
| Core Platform | ESP32-S3 (Octal PSRAM / Flash) | ESP32-S3 / Next Gen | High |
| I2C Bus | `SDA=18`, `SCL=17` (400kHz Fast Mode) | Same default | High |
| Display | ST77916, 360x360 Round QSPI | Modular esp_lcd panel drivers | High |
| Display Pins | `BL=39, CS=14, RST=42, DC=40, SCK=6, D0=5, D1=4, D2=2, D3=1` | Configurable | High |
| Touch Controller | CST816S (`0x15`) | CST816S / I2C touch | High |
| Touch Pins | `IRQ=38, RST=-1` (Shared with LCD_RST 42) | Configurable | High |
| Audio DAC | MCP4725 at `0x61` (Dedicated) | MCP4725 / I2S | High |
| Amp Power | `GPIO13` | Configurable | High |
| Haptics | MCP4728 DAC at `0x60`, 4 motors (Ch A/B/C/D) in X layout | 4-channel DAC / PWM | High |
| IMU (6-Axis) | ICM-42607-P at `0x69` (±2g / ±2000dps, -90° rotation) | Modern 6-axis I2C IMUs | High |
| Compass | Not Present (Unpopulated on v1 PCB, MMC5603NJ footprint at `0x30`) | MMC5603NJ / Optional | High |
| Storage | SD_MMC 4-wire (XinYY SD32G SD-NAND) | High-speed 4-bit SDMMC | High |
| Storage Pins | `CLK=9, CMD=12, D0=11, D1=10, D2=7, D3=8` | Configurable | High |
| Wireless | Wi-Fi 802.11 b/g/n, Bluetooth 5 (LE) | Built-in | High |
| PSRAM | Present (Octal PSRAM) | Present | High |
| USB MSC | Present (Native USB CDC/OTG) | Present | High |

---

## bpad 185 Haptics Layout (X Format)

Four vibration motors driven by MCP4728 12-bit DAC at I2C address `0x60`:

```
          [ TOP ]
     (TL)         (TR)
   Ch 2 (Red)   Ch 3 (Blue)
         \     /
          \   /
           \ /
            X
           / \
          /   \
         /     \
  Ch 0 (White)  Ch 1 (Black)
     (BL)         (BR)
        [ BOTTOM ]
```

| DAC Channel | Motor Position | Wire Color | Enum Constant |
|---|---|---|---|
| **Channel 0** | Bottom-Left (BL) | White | `bpadMotor::BottomLeft` (`BPAD_MOTOR_BL`) |
| **Channel 1** | Bottom-Right (BR) | Black | `bpadMotor::BottomRight` (`BPAD_MOTOR_BR`) |
| **Channel 2** | Top-Left (TL) | Red | `bpadMotor::TopLeft` (`BPAD_MOTOR_TL`) |
| **Channel 3** | Top-Right (TR) | Blue | `bpadMotor::TopRight` (`BPAD_MOTOR_TR`) |
