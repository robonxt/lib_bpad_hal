// =============================================================================
//  power_button_test.ino - bpad Power & Button Driver Diagnostic
// =============================================================================
//  Tests SW3 / IP5306 KEY on GPIO 21:
//  - Single click & long-press detection
//  - Software IP5306 soft power-off
//  - ESP32-S3 deep sleep & wake on GPIO 21
// =============================================================================

#include <Arduino.h>
#include <bpad_hal.h>

const bpad::Config hw = bpad::Config::defaultBoard();
bpad::Power pwr(hw.button);

void setup() {
  Serial.begin(115200);
  delay(1000);

  Serial.println("\n==============================================");
  Serial.println("         bpad Power & Button Test             ");
  Serial.println("==============================================");
  Serial.printf("Configured Button Pin: GPIO %d (Active LOW, Internal Pullup)\n", hw.button.pin);

  pwr.begin();

  Serial.println("\nInstructions:");
  Serial.println("  - Click SW3: Single click event");
  Serial.println("  - Hold SW3 > 1.5s: Long press event");
  Serial.println("  - Hold SW3 > 3.0s: Enters Deep Sleep (Wake by clicking SW3 again)");
  Serial.println("  - Type 'off' in Serial: Commands IP5306 soft-shutdown\n");
}

void loop() {
  pwr.update();

  if (pwr.isClicked()) {
    Serial.printf("[BUTTON] Single Click! (Hold duration: %lu ms)\n", pwr.lastPressDurationMs());
  }

  if (pwr.isLongPressed()) {
    Serial.printf("[BUTTON] Long Press (> %lu ms) detected!\n", pwr.lastPressDurationMs());
  }

  // Automatic deep sleep after 3 second hold
  if (pwr.isPressed() && (pwr.pressDurationMs() > 3000)) {
    Serial.println("[POWER] 3-second hold! Entering Deep Sleep now...");
    delay(200);
    pwr.sleep();
  }

  // Serial command for IP5306 PMIC shutdown
  if (Serial.available()) {
    String cmd = Serial.readStringUntil('\n');
    cmd.trim();
    if (cmd.equalsIgnoreCase("off") || cmd.equalsIgnoreCase("poweroff")) {
      Serial.println("[POWER] Sending double-pulse to IP5306 KEY to shut off 5V boost...");
      pwr.powerOff();
    }
  }

  delay(10);
}
