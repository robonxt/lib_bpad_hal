# bpad_hal Architecture & Specification Contract

Living hardware abstraction specification for `bpad_hal` across bpad revisions (bpad 185 V1 and upcoming prototype spins).

---

## 1. System Philosophy & Architectural Boundary

`bpad_hal` is strictly a **hardware driver layer** for custom bpad board silicon and pin peripherals. It is **not** an application framework or an operating system.

### HAL Responsibility (What HAL Owns)
* Low-level register configuration, pin initialization, and bus transactions.
* Hardware power-sequencing (anti-pop amplifier gating, DAC power-down, display reset/TE).
* Board configuration manifests (`bpad::Config`) defining pin mappings, I2C addresses, and hardware capabilities.
* Hardware diagnostics, bring-up tests, and single-peripheral example sketches (`examples/`).
* **Zero FreeRTOS locks / Zero RTOS dependencies**: HAL methods are non-blocking, fast, and thread-neutral.

### OS Responsibility (What OS Owns)
* FreeRTOS tasks, scheduling, Core 0 vs Core 1 assignment, and thread synchronization.
* Shared I2C bus concurrency arbitration (OS holds the `i2cMutex` before calling HAL I2C drivers).
* Polyphonic sound synthesis, audio mixing, WAV/PCM streaming, and chiptune sequencers.
* Circular UI rendering, scene graphs, touch gesture arbitration, and chord math.
* Game engines, Lua cartridge runtimes (`.LOOP`), and file asset loading.

---

## 2. Shared I2C Bus Architecture

All I2C silicon on current bpad hardware shares **one physical bus** (`SDA = GPIO 18`, `SCL = GPIO 17`):

| Device | Address | Function | Speed Capability |
| :--- | :--- | :--- | :--- |
| **CST816S** | `0x15` | Capacitive Touch Controller | 400 kHz (Datasheet max) |
| **MCP4728** | `0x60` | Quad 12-bit DAC (Haptics X-Array) | Up to 3.4 MHz (High-Speed) |
| **MCP4725** | `0x61` | Single 12-bit DAC (Audio) | Up to 3.4 MHz (High-Speed) |
| **ICM-42607-P**| `0x69` | 6-Axis Motion Sensor (Accel/Gyro) | Up to 1.0 MHz (Fast-Mode+) |

### Bus Rules for OS Developers
1. **Clock Rate**: Default is **400 kHz** (`bpad::I2CConfig::clock = 400000`). This is the safe limit set by the CST816S touch controller. Do not switch clock rates dynamically on a shared multi-threaded bus.
2. **Concurrency**: HAL drivers contain **zero internal mutexes**. If multiple threads access I2C (e.g., Core 0 Audio task vs Core 1 Touch loop), **the OS must wrap calls in an external mutex** (`xSemaphoreTake` / `xSemaphoreGive`).

---

## 3. Subsystem Specifications & Contracts

### A. Audio (`bpad::Audio`)
* **Hardware**: MCP4725 I2C DAC (`0x61`) + PAM8302 2.5W Class-D mono amplifier (`GPIO 13` power gate).
* **Role of `playTone()`**:
  - `playTone()` is a **monophonic hardware diagnostic beeper**.
  - Intended for: Hardware bring-up, standalone tests, and simple UI clicks.
  - Features: Automatic 5ms attack / 8ms release envelope, continuous phase tracking on chained notes, and catch-up burst clamping.
* **Role of `writeSample()` / `writeSampleDirect()`**:
  - **The official public API for OS audio engines.**
  - If the OS runs a multi-channel synthesizer or PCM stream on Core 0, bypass `Audio::update()` completely and feed 12-bit samples directly via `writeSample()`.
* **Hardware Reality of V1.0 Prototype Audio**:
  - Streaming real-time audio points over a shared 400 kHz I2C bus into an unfiltered Class-D amp has physical limitations (sampling jitter, bus contention, and lack of an analog low-pass reconstruction filter).
  - *Do not expect 16-bit console-grade chiptune synthesis from an I2C voltage DAC.*
* **Upcoming Hardware Migration**:
  - Chris's next prototype spin completely replaces the MCP4725 with the **NS4168 I2S digital audio codec + digital mic**.
  - Audio will stream over dedicated ESP32 hardware DMA (zero I2C bus traffic, 16-bit / 44.1 kHz).
  - `bpad_hal` will update the internal audio backend to I2S under the hood; the OS public interface will remain clean and stable.

### B. Display (`bpad::Display`)
* **Hardware**: ST77916 QSPI 360x360 circular panel.
* **Pins**: `BL=39`, `CS=14`, `RST=42`, `DC=40`, `SCK=6`, `D0=5`, `D1=4`, `D2=2`, `D3=1`, `TE=41`.
* **Tearing Effect (TE / VSync)**: Connected to `GPIO 41`. Generates falling-edge interrupts at ~61 Hz for tearing-free DMA buffer flushes.
* **Backlight**: Hardware PWM on `GPIO 39` via LEDC timer.

### C. Touch (`bpad::Touch`)
* **Hardware**: CST816S capacitive touch controller at I2C address `0x15`.
* **Interrupt**: `GPIO 38` (Falling edge when finger touches or moves).
* **Driver Contract**: Provides raw coordinates `(x, y)`, press state, and hardware gesture decoding (`SwipeUp`, `SwipeDown`, `SwipeLeft`, `SwipeRight`, `SingleClick`, `LongPress`).

### D. Power & Button (`bpad::Power`)
* **Hardware**: Tactile switch SW3 & IP5306 PMIC KEY line on `GPIO 21`.
* **Features**:
  - Software debouncing (click vs long-press hold).
  - Hardware double-pulse power-off sequence (`powerOff()`) to command IP5306 to kill the 5V boost rail.
  - Deep-sleep wake on `GPIO 21` low-level interrupt.
* **Note on IP5306 Standby**:
  - The IP5306 cuts 5V power if load current stays $< 45\text{ mA}$ for 32 seconds.
  - Keep-alive pulse was removed from HAL per project direction. The OS sleep policy should maintain a minimal dim state (`dimmedDuty = 15`) rather than hard display shut-off on V1.0 prototypes.

### E. Haptics (`bpad::Haptics`)
* **Hardware**: MCP4728 quad 12-bit DAC at I2C address `0x60`, driving 4 independent ERM vibration motors in an X-layout.
* **Channels**:
  - Ch 0: Bottom-Left (White wire)
  - Ch 1: Bottom-Right (Black wire)
  - Ch 2: Top-Left (Red wire)
  - Ch 3: Top-Right (Blue wire)
* **Performance**: Uses MCP4728 Multi-Write command `0x40` to update all 4 channels in a single 9-byte I2C transaction.

### F. IMU (`bpad::IMU`)
* **Hardware**: ICM-42607-P 6-axis accelerometer & gyroscope at I2C address `0x69`.
* **Mounting**: Mounted with a **-90° PCB rotation offset**.
* **Driver Contract**: Returns scaled $\pm 2\text{g}$ linear acceleration ($g$) and $\pm 2000^\circ/\text{s}$ angular velocity ($^\circ/\text{s}$) with the -90° rotation automatically corrected.

### G. Storage (`bpad::Storage`)
* **Hardware**: XinYY SD32G SD-NAND on the 4-wire `SD_MMC` peripheral slot.
* **Pins**: `CLK=9`, `CMD=12`, `D0=11`, `D1=10`, `D2=7`, `D3=8`.
* **Driver Contract**: Mounts cleanly to standard `<FS.h>` via `storage.mount("/sd")`.
* **USB Mass Storage Mode**: During USB MSC mode, all FreeRTOS tasks and sensor polling must halt; HAL hands raw sector control of the SD-NAND to the PC USB host.

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
