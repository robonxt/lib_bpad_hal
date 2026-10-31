// =============================================================================
//  wifi_test.ino — bpad_hal Wi-Fi subsystem test
// =============================================================================
//  Tests both Wi-Fi modes on bpad 185:
//  1. Station Mode (Scan): Discovers 2.4 GHz access points and tests RF signal (RSSI).
//  2. SoftAP Mode: Broadcasts a standalone access point ("bpad185_Setup").
// =============================================================================

#include <Arduino.h>
#include <WiFi.h>
#include <bpad_hal.h>

const bpadConfig hw = bpadConfig::defaultBoard();

void setup() {
  Serial.begin(115200);
#if ARDUINO_USB_CDC_ON_BOOT
  Serial.setTxTimeoutMs(0);
#endif
  delay(500);

  Serial.println("\n==============================================");
  Serial.println("         bpad 185 Wi-Fi Diagnostic");
  Serial.println("==============================================");
  Serial.printf("Board: %s  | MAC Address: %s\n", hw.name, WiFi.macAddress().c_str());

  // 1. Station Mode Scan Test (Tests RF Receiver & Antenna)
  Serial.println("\n[1/2] Testing Wi-Fi Scan (Station Mode)...");
  WiFi.mode(WIFI_STA);
  WiFi.disconnect();
  delay(100);

  int n = WiFi.scanNetworks();
  Serial.printf("[SCAN] Scan complete. Found %d network(s):\n", n);

  if (n == 0) {
    Serial.println("  >>> WARNING: No networks found! Check PCB antenna / IPEX connection. <<<");
  } else {
    for (int i = 0; i < n; ++i) {
      Serial.printf("  %2d: [%-4d dBm] %-32s (Ch: %2d, Enc: %s)\n",
        i + 1,
        WiFi.RSSI(i),
        WiFi.SSID(i).c_str(),
        WiFi.channel(i),
        (WiFi.encryptionType(i) == WIFI_AUTH_OPEN) ? "Open" : "Secured");
    }
  }

  // 2. SoftAP Mode Test (Tests RF Transmitter & Baseband)
  Serial.println("\n[2/2] Testing Wi-Fi SoftAP Mode (Broadcasting)...");
  const char* apSSID = "bpad185_Setup";
  const char* apPass = "12345678";

  WiFi.mode(WIFI_AP);
  bool apSuccess = WiFi.softAP(apSSID, apPass);

  if (apSuccess) {
    IPAddress ip = WiFi.softAPIP();
    Serial.printf("[AP] SoftAP started successfully!\n");
    Serial.printf("     SSID:     %s\n", apSSID);
    Serial.printf("     Password: %s\n", apPass);
    Serial.printf("     IP Addr:  %s\n", ip.toString().c_str());
    Serial.println("\n>>> Connect your phone or laptop to 'bpad185_Setup' to verify! <<<");
  } else {
    Serial.println("[AP] >>> FAILED to start SoftAP! <<<");
  }
}

void loop() {
  static uint32_t lastCheck = 0;
  if (millis() - lastCheck >= 3000) {
    lastCheck = millis();
    int clients = WiFi.softAPgetStationNum();
    Serial.printf("[AP Monitor] Active connected client devices: %d\n", clients);
  }
}
