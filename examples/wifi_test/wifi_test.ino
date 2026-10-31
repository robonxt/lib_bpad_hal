// =============================================================================
//  wifi_test.ino — bpad_hal Pure Station Mode Diagnostic
// =============================================================================
//  Focuses 100% of RF silicon on connecting to an existing router (STA mode).
//  - No SoftAP running to lock channels or divide radio synthesizer time.
//  - Boosts TX power to maximum (+19.5 dBm).
//  - Translates disconnection event codes directly into plain English.
// =============================================================================

#include <Arduino.h>
#include <WiFi.h>
#include <esp_mac.h>
#include <bpad_hal.h>

const bpadConfig hw = bpadConfig::defaultBoard();

const char* wifiReasonToString(uint8_t reason) {
  switch (reason) {
    case 1:   return "UNSPECIFIED";
    case 2:   return "AUTH_EXPIRE (Auth frame timed out / weak signal)";
    case 3:   return "AUTH_LEAVE";
    case 4:   return "ASSOC_EXPIRE / DISASSOC_INACTIVITY";
    case 13:  return "INVALID_IE";
    case 14:  return "MIC_FAILURE";
    case 15:  return "4WAY_HANDSHAKE_TIMEOUT (Key exchange dropped)";
    case 200: return "BEACON_TIMEOUT (Lost router beacon)";
    case 201: return "NO_AP_FOUND (SSID not found on 2.4 GHz)";
    case 202: return "AUTH_FAIL (Password rejected by router)";
    case 203: return "ASSOC_FAIL";
    case 204: return "HANDSHAKE_TIMEOUT";
    case 205: return "CONNECTION_FAIL";
    default:  return "UNKNOWN_CODE";
  }
}

void printHardwareMACs() {
  uint8_t mac_sta[6];
  esp_read_mac(mac_sta, ESP_MAC_WIFI_STA);

  Serial.println("\n--- Hardware eFuse MAC Address ---");
  Serial.printf("  Station (STA) MAC: %02X:%02X:%02X:%02X:%02X:%02X\n",
    mac_sta[0], mac_sta[1], mac_sta[2], mac_sta[3], mac_sta[4], mac_sta[5]);
  Serial.println("----------------------------------\n");
}

void registerWiFiEvents() {
  WiFi.onEvent([](WiFiEvent_t event, WiFiEventInfo_t info) {
    switch (event) {
      case ARDUINO_EVENT_WIFI_READY:
        Serial.println("[EVT] Wi-Fi hardware READY");
        break;
      case ARDUINO_EVENT_WIFI_STA_START:
        Serial.println("[EVT] STA station interface started");
        WiFi.setSleep(false); // Disable modem sleep to prevent dropped handshake frames
        WiFi.setTxPower(WIFI_POWER_8_5dBm); // Prevent ceramic antenna RF saturation
        Serial.println("[EVT] Sleep disabled. TX Power calibrated to +8.5 dBm.");
        break;
      case ARDUINO_EVENT_WIFI_STA_CONNECTED:
        Serial.println("[EVT] >>> STEP 1: ASSOCIATED WITH ROUTER! Handshaking... <<<");
        break;
      case ARDUINO_EVENT_WIFI_STA_GOT_IP:
        Serial.println("\n==============================================");
        Serial.println("   >>> SUCCESS: CONNECTED TO WI-FI NETWORK! <<<");
        Serial.println("==============================================");
        Serial.printf("  IP Address: %s\n", WiFi.localIP().toString().c_str());
        Serial.printf("  Gateway:    %s\n", WiFi.gatewayIP().toString().c_str());
        Serial.printf("  Subnet:     %s\n", WiFi.subnetMask().toString().c_str());
        Serial.printf("  RSSI:       %d dBm\n", WiFi.RSSI());
        Serial.println("==============================================\n");
        break;
      case ARDUINO_EVENT_WIFI_STA_DISCONNECTED:
        Serial.printf("[EVT] Disconnected! Code %d -> %s\n",
          info.wifi_sta_disconnected.reason,
          wifiReasonToString(info.wifi_sta_disconnected.reason));
        break;
      default:
        break;
    }
  });
}

void setup() {
  Serial.begin(115200);
#if ARDUINO_USB_CDC_ON_BOOT
  Serial.setTxTimeoutMs(0);
#endif
  delay(500);

  Serial.println("\n==============================================");
  Serial.println("     bpad 185 Pure Station (STA) Diagnostic");
  Serial.println("==============================================");

  printHardwareMACs();
  registerWiFiEvents();

  // Pure Station Mode — 100% radio dedicated to STA
  WiFi.mode(WIFI_STA);
  delay(100);

  // Initial scan to display available 2.4 GHz channels & signal strength
  Serial.println("[SCAN] Scanning 2.4 GHz channels...");
  int n = WiFi.scanNetworks();
  Serial.printf("[SCAN] Found %d network(s):\n", n);
  for (int i = 0; i < n; ++i) {
    Serial.printf("  %2d: [%-4d dBm] %-32s (Ch: %2d, Enc: %s)\n",
      i + 1,
      WiFi.RSSI(i),
      WiFi.SSID(i).c_str(),
      WiFi.channel(i),
      (WiFi.encryptionType(i) == WIFI_AUTH_OPEN) ? "Open" : "Secured");
  }
  WiFi.scanDelete();

  Serial.println("\n>>> Ready! Type in Serial Monitor: connect <SSID> <PASSWORD> <<<");
  Serial.println("Example: connect JesusIsTheLord mysecretpass\n");
}

void loop() {
  if (Serial.available()) {
    String cmd = Serial.readStringUntil('\n');
    cmd.trim();

    if (cmd == "scan") {
      Serial.println("\n[SCAN] Scanning 2.4 GHz channels...");
      int n = WiFi.scanNetworks();
      Serial.printf("[SCAN] Found %d network(s):\n", n);
      for (int i = 0; i < n; ++i) {
        Serial.printf("  %2d: [%-4d dBm] %-32s (Ch: %2d, Enc: %s)\n",
          i + 1,
          WiFi.RSSI(i),
          WiFi.SSID(i).c_str(),
          WiFi.channel(i),
          (WiFi.encryptionType(i) == WIFI_AUTH_OPEN) ? "Open" : "Secured");
      }
      WiFi.scanDelete();
    } else if (cmd.startsWith("power ")) {
      String p = cmd.substring(6);
      p.trim();
      int dbm = p.toInt();
      wifi_power_t pw = WIFI_POWER_8_5dBm;
      if (dbm <= 5) pw = WIFI_POWER_5dBm;
      else if (dbm <= 8) pw = WIFI_POWER_8_5dBm;
      else if (dbm <= 11) pw = WIFI_POWER_11dBm;
      else if (dbm <= 15) pw = WIFI_POWER_15dBm;
      else pw = WIFI_POWER_19_5dBm;
      WiFi.setTxPower(pw);
      Serial.printf("[PWR] Transmit power set to level: %d dBm\n", dbm);
    } else if (cmd.startsWith("connect ")) {
      int firstSpace = cmd.indexOf(' ', 8);
      if (firstSpace > 8) {
        String ssid = cmd.substring(8, firstSpace);
        String pass = cmd.substring(firstSpace + 1);
        Serial.printf("\n[STA] Connecting to '%s' (TX Power: +8.5 dBm, Sleep: OFF)...\n", ssid.c_str());
        WiFi.setSleep(false);
        WiFi.begin(ssid.c_str(), pass.c_str());
      } else {
        Serial.println("[STA] Format: connect <SSID> <PASSWORD>");
      }
    }
  }

  delay(20);
}
