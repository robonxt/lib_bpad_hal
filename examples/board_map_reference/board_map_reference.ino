// =============================================================================
//  board_map_reference.ino - bpad_hal hardware manifest reference
// =============================================================================
//  Prints all hardware configuration fields and subsystem specs for bpad 185+.
// =============================================================================

#include <Arduino.h>
#include <bpad_hal.h>

const bpad::Config hw = bpad::Config::defaultBoard();

void printSupport(const char* label, bpad::Support support) {
  Serial.print(label);
  Serial.print(": ");
  Serial.println(bpad::supportName(support));
}

void setup() {
  Serial.begin(115200);
#if ARDUINO_USB_CDC_ON_BOOT
  Serial.setTxTimeoutMs(0);
#endif
  delay(1500);

  Serial.println();
  Serial.print("[bpad_hal] board: ");
  Serial.println(hw.name);
  Serial.print("[bpad_hal] note: ");
  Serial.println(hw.note);

  Serial.print("[i2c] SDA=");
  Serial.print(hw.i2c.sda);
  Serial.print(" SCL=");
  Serial.print(hw.i2c.scl);
  Serial.print(" clock=");
  Serial.println(hw.i2c.clock);

  Serial.print("[display] ");
  Serial.print(hw.display.controller);
  Serial.print(" (");
  Serial.print(bpad::displayBackendName(hw.display.backend));
  Serial.print(") ");
  Serial.print(hw.display.width);
  Serial.print("x");
  Serial.print(hw.display.height);
  Serial.print(" BL=");
  Serial.print(hw.display.bl);
  Serial.print(" CS=");
  Serial.print(hw.display.cs);
  Serial.print(" RST=");
  Serial.print(hw.display.rst);
  Serial.print(" DC=");
  Serial.print(hw.display.dc);
  Serial.print(" SCLK=");
  Serial.print(hw.display.sclk);
  Serial.print(" D0=");
  Serial.print(hw.display.d0);
  Serial.print(" D1=");
  Serial.print(hw.display.d1);
  Serial.print(" D2=");
  Serial.print(hw.display.d2);
  Serial.print(" D3=");
  Serial.println(hw.display.d3);

  Serial.print("[touch] ");
  Serial.print(hw.touch.controller);
  Serial.print(" addr=0x");
  Serial.print(hw.touch.address, HEX);
  Serial.print(" IRQ=");
  Serial.print(hw.touch.irq);
  Serial.print(" RST=");
  Serial.println(hw.touch.rst);

  Serial.print("[audio] backend=");
  Serial.print(bpad::audioBackendName(hw.audio.backend));
  Serial.print(" ampPower=GPIO");
  Serial.print(hw.audio.ampPower);
  Serial.print(" mcp4725=0x");
  Serial.println(hw.audio.mcp4725Address, HEX);

  Serial.print("[haptics] backend=");
  Serial.print(bpad::hapticsBackendName(hw.haptics.backend));
  Serial.print(" motors=");
  Serial.print(hw.haptics.count);
  Serial.print(" mcp4728=0x");
  Serial.println(hw.haptics.mcp4728Address, HEX);

  Serial.print("[imu] backend=");
  Serial.print(bpad::imuBackendName(hw.imu.backend));
  Serial.print(" addr=0x");
  Serial.print(hw.imu.address, HEX);
  Serial.print(" rotation=");
  Serial.print(hw.imu.orientationOffsetDeg);
  Serial.println(" deg");

  Serial.print("[compass] ");
  Serial.print(hw.compass.device);
  Serial.print(" addr=0x");
  Serial.println(hw.compass.address, HEX);

  Serial.print("[storage] backend=");
  Serial.print(bpad::storageBackendName(hw.storage.backend));
  Serial.print(" device=");
  Serial.println(hw.storage.device);

  printSupport("[wireless] wifi", hw.wireless.wifi);
  printSupport("[wireless] bluetooth", hw.wireless.bluetooth);
  printSupport("[system] psram", hw.system.psram);
  printSupport("[system] usbMsc", hw.system.usbMsc);
}

void loop() {
  delay(1000);
}
