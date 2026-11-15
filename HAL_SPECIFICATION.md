# bpad_hal Hardware Specification & Driver Contracts

Authoritative hardware driver specification, capabilities, and interface contracts for `bpad_hal`.

---

## 1. HAL Scope & Design Guarantees

`bpad_hal` is strictly the **hardware driver layer** for custom bpad board silicon and pin peripherals.

### Capabilities Provided
* Direct hardware initialization, register configuration, and peripheral pin routing.
* Hardware power-sequencing (anti-pop amplifier gating, DAC power-down, display reset/TE).
* Board configuration manifests (`bpad::Config`) defining pin mappings, I2C addresses, and hardware capabilities.
* Diagnostic examples and peripheral verification test sketches (`examples/`).
* Lock-free, non-blocking C++ API in `namespace bpad`.

### Thread-Safety & Concurrency Model
* **HAL drivers contain zero internal mutexes or locks.**
* All HAL driver calls are lightweight and return immediately without thread-blocking.
* If a caller accesses HAL I2C drivers across multiple concurrent threads or tasks, **the caller is responsible for serializing access to the shared bus**.

---

## 2. Shared I2C Bus & Concurrency Model

All I2C silicon on current bpad hardware shares **one physical bus** (`SDA = GPIO 18`, `SCL = GPIO 17`):

| Device | Address | Function | Bus Clock Limit |
| :--- | :--- | :--- | :--- |
| **CST816S** | `0x15` | Capacitive Touch Controller | 400 kHz (Datasheet max) |
| **MCP4728** | `0x60` | Quad 12-bit DAC (Haptics X-Array) | Up to 3.4 MHz (High-Speed) |
| **MCP4725** | `0x61` | Single 12-bit DAC (Audio) | Up to 3.4 MHz (High-Speed) |
| **ICM-42607-P**| `0x69` | 6-Axis Motion Sensor (Accel/Gyro) | Up to 1.0 MHz (Fast-Mode+) |

### Bus Specifications
1. **Clock Rate**: Default is **400 kHz** (`bpad::I2CConfig::clock = 400000`), bounded by the CST816S touch controller limit.
2. **Serialization**: Because HAL drivers do not lock the bus internally, concurrent multi-task callers must synchronize access before calling I2C-backed HAL methods.

---

## 3. Subsystem Driver Specifications

### A. Audio (`bpad::Audio`)
* **Hardware**: MCP4725 I2C DAC (`0x61`) + PAM8302 2.5W Class-D mono amplifier (`GPIO 13` power gate).
* **Capabilities**:
  - [`enableAmp(bool)`](src/bpad_audio.h): Controls the `GPIO 13` amp power rail with built-in anti-pop sequencing (biases DAC to mid-rail 2048 before unmuting; mutes amp before DAC power-down).
  - [`writeSampleDirect(uint16_t)`](src/bpad_audio.h): Direct, unscaled 12-bit sample write (0..4095) to MCP4725 in Fast Mode.
  - [`writeSample(uint16_t)`](src/bpad_audio.h): Writes a 12-bit sample centered at mid-rail (2048) scaled by driver volume level (`0..3`).
  - [`playTone(freqHz, durationMs, wave)`](src/bpad_audio.h): Built-in single-voice hardware bring-up / diagnostic tone generator.
    - Default waveform: `Waveform::Triangle` (cleanest acoustic profile on micro-speaker).
    - Features: Automatic 5ms attack / 8ms release envelope, continuous phase tracking across chained calls, and catch-up burst clamping.
  - [`setSampleRate(uint32_t)`](src/bpad_audio.h) / [`setMaxSwing(uint16_t)`](src/bpad_audio.h): Runtime tuning for sample pacing (default 8000 Hz) and peak amplitude ceiling (default 1400 / 0..2047).
* **Hardware Constraints**:
  - The MCP4725 is a general-purpose I2C voltage DAC. Streaming real-time waveform points over a shared 400 kHz serial bus has physical throughput and jitter limits.
  - The upcoming hardware revision migrates to the **NS4168 I2S digital codec** (DMA-driven, zero I2C traffic). `bpad_hal` will update its backend to I2S while preserving stable driver interfaces.

### B. Display (`bpad::Display`)
* **Hardware**: ST77916 QSPI 360x360 circular panel.
* **Pins**: `BL=39`, `CS=14`, `RST=42`, `DC=40`, `SCK=6`, `D0=5`, `D1=4`, `D2=2`, `D3=1`, `TE=41`.
* **Capabilities**:
  - Hardware initialization and panel reset.
  - Backlight PWM brightness control via LEDC on `GPIO 39`.
  - Tearing Effect (TE / VSync) interrupt on `GPIO 41` (~61 Hz) for tearing-free frame synchronization.

### C. Touch (`bpad::Touch`)
* **Hardware**: CST816S capacitive touch controller at I2C address `0x15`.
* **Interrupt**: `GPIO 38` (Falling edge on touch detection).
* **Capabilities**:
  - Point coordinates `(x, y)` and touch state query.
  - Hardware gesture reporting (`SwipeUp`, `SwipeDown`, `SwipeLeft`, `SwipeRight`, `SingleClick`, `LongPress`).

### D. Power & Button (`bpad::Power`)
* **Hardware**: Tactile switch SW3 & IP5306 PMIC KEY line on `GPIO 21`.
* **Capabilities**:
  - Debounced input processing (single-click and long-press hold events).
  - [`powerOff()`](src/bpad_power.h): Sends hardware double-pulse sequence on GPIO 21 to trigger IP5306 5V rail shutdown.
  - [`deepSleep()`](src/bpad_power.h): Configures GPIO 21 as low-level wake source and enters ESP32 deep sleep.

### E. Haptics (`bpad::Haptics`)
* **Hardware**: MCP4728 quad 12-bit DAC at I2C address `0x60`, driving 4 independent ERM vibration motors in an X-layout.
* **Channels**:
  - Ch 0: Bottom-Left (White wire)
  - Ch 1: Bottom-Right (Black wire)
  - Ch 2: Top-Left (Red wire)
  - Ch 3: Top-Right (Blue wire)
* **Capabilities**:
  - Individual motor drive (0..4095).
  - Multi-motor synchronous update using MCP4728 Multi-Write command `0x40` (single 9-byte I2C transaction).

### F. IMU (`bpad::IMU`)
* **Hardware**: ICM-42607-P 6-axis accelerometer & gyroscope at I2C address `0x69`.
* **Capabilities**:
  - Scaled $\pm 2\text{g}$ linear acceleration ($g$) and $\pm 2000^\circ/\text{s}$ angular velocity ($^\circ/\text{s}$).
  - Hardware mounting rotation (-90° PCB offset) automatically compensated in driver reads.

### G. Storage (`bpad::Storage`)
* **Hardware**: XinYY SD32G SD-NAND on 4-wire `SD_MMC` peripheral slot.
* **Pins**: `CLK=9`, `CMD=12`, `D0=11`, `D1=10`, `D2=7`, `D3=8`.
* **Capabilities**:
  - Mounts SD-NAND filesystem to standard `<FS.h>` via `storage.mount("/sd")`.
  - Raw sector access interface for USB Mass Storage mode.

---

## 4. Hardware Evolution Matrix (Current vs Next Prototype)

| Feature | bpad 185 V1.0 (Current in Hand) | Next Prototype Spin (In Manufacturing) |
| :--- | :--- | :--- |
| **Audio** | MCP4725 I2C DAC + PAM8302 | **NS4168 I2S Digital Codec + Mic** |
| **Battery Sensing** | None (4 external LEDs only) | **CW2015CHBD I2C Fuel Gauge (0x62)** |
| **Real-Time Clock** | ESP32 internal RTC timer | **Dedicated External I2C RTC** |
| **Wi-Fi RF** | Broken (Missing CLC π-matching) | **50Ω CPW-G Microstrip + CLC π-Network** |
| **LCD-TE (VSync)** | Floating on some early units | **Hardwired to GPIO for clean 61 FPS** |
| **Power Button** | SW3 / IP5306 KEY on GPIO 21 | **Direct GPIO0 with crystal Y4 removed** |
