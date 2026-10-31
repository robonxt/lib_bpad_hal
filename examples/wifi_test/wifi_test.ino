// =============================================================================
//  wifi_test.ino — bpad_hal Wi-Fi subsystem test
// =============================================================================
//  Tests Wi-Fi scanning (receiver RF) and SoftAP broadcasting (transmitter RF).
//  Also allows testing connection to your home network via Serial.
// =============================================================================

#include <Arduino.h>
#include <WiFi.h>
#include <esp_mac.h>
#include <bpad_hal.h>

const bpadConfig hw = bpadConfig::defaultBoard();

void printHardwareMACs() {
  uint8_t mac_sta[6];
  uint8_t mac_ap[6];
  esp_read_mac(mac_sta, ESP_MAC_WIFI_STA);
  esp_read_mac(mac_ap, ESP_MAC_WIFI_SOFTAP);

  Serial.println("\n--- Hardware eFuse MAC Addresses ---");
  Serial.printf("  Station (STA) MAC: %02X:%02X:%02X:%02X:%02X:%02X\n",
    mac_sta[0], mac_sta[1], mac_sta[2], mac_sta[3], mac_sta[4], mac_sta[5]);
  Serial.printf("  SoftAP  (AP)  MAC: %02X:%02X:%02X:%02X:%02X:%02X\n",
    mac_ap[0], mac_ap[1], mac_ap[2], mac_ap[3], mac_ap[4], mac_ap[5]);
  Serial.println("------------------------------------\n");
}

void setup() {
  Serial.begin(115200);
#if ARDUINO_USB_CDC_ON_BOOT
  Serial.setTxTimeoutMs(0);
#endif
  delay(500);

  Serial.println("\n==============================================");
  Serial.println("         bpad 185 Wi-Fi Diagnostic");
  Serial.println("==============================================");

  // 1. Read hardware eFuse MAC addresses
  printHardwareMACs();

  // 2. Set Dual AP+STA Mode & Boost Transmit Power to Maximum (+19.5 dBm)
  WiFi.mode(WIFI_AP_STA);
  WiFi.setTxPower(WIFI_POWER_19_5dBm);
  delay(100);

  // 3. Station Mode Scan Test (Receiver RF check)
  Serial.println("[1/2] Scanning 2.4 GHz Channels...");
  int n = WiFi.scanNetworks();
  Serial.printf("[SCAN] Found %d network(s):\n", n);

  if (n == 0) {
    Serial.println("  >>> WARNING: No networks found! Check antenna connection. <<<");
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

  // 4. Start SoftAP on Channel 6 (Open network for instant phone discovery)
  Serial.println("\n[2/2] Starting SoftAP Broadcast...");
  const char* apSSID = "bpad185_Setup";

  // Channel 6, visible (ssid_hidden=0), open authentication
  bool apSuccess = WiFi.softAP(apSSID, NULL, 6, 0, 4);

  if (apSuccess) {
    IPAddress ip = WiFi.softAPIP();
    Serial.printf("[AP] >>> SUCCESS: SoftAP Broadcast is LIVE! <<<\n");
    Serial.printf("     SSID:     %s  (Open / No Password)\n", apSSID);
    Serial.printf("     Channel:  6\n");
    Serial.printf("     IP Addr:  %s\n", ip.toString().c_str());
    Serial.printf("     AP MAC:   %s\n", WiFi.softAPmacAddress().c_str());
    Serial.println("\nCheck your phone's Wi-Fi list now for 'bpad185_Setup'.");
  } else {
    Serial.println("[AP] >>> FAILED to start SoftAP! <<<");
  }

  Serial.println("\n(To test connecting to your home router, type your password in Serial Monitor)\n");
}

void loop() {
  static uint32_t lastCheck = 0;
  if (millis() - lastCheck >= 3000) {
    lastCheck = millis();
    int clients = WiFi.softAPgetStationNum();
    if (clients > 0) {
      Serial.printf("[AP Monitor] Phone/Client connected! Count: %d\n", clients);
    }
  }

  // Allow connecting to home router via Serial command: "connect <SSID> <PASS>"
  if (Serial.available()) {
    String cmd = Serial.readStringUntil('\n');
    cmd.trim();
    if (cmd.startsWith("connect ")) {
      int firstSpace = cmd.indexOf(' ', 8);
      if (firstSpace > 8) {
        String ssid = cmd.substring(8, firstSpace);
        String pass = cmd.substring(firstSpace + 1);
        Serial.printf("\n[STA] Connecting to %s ...\n", ssid.c_str());
        WiFi.begin(ssid.c_str(), pass.c_str());
        int tries = 0;
        while (WiFi.status() != WL_CONNECTED && tries < 20) {
          delay(500);
          Serial.print(".");
          tries++;
        }
        if (WiFi.status() == WL_CONNECTED) {
          Serial.printf("\n[STA] CONNECTED! IP: %s  RSSI: %d dBm\n",
            WiFi.localIP().toString().c_str(), WiFi.RSSI());
        } else {
          Serial.println("\n[STA] Connection failed.");
        }
      }
    }
  }
}
