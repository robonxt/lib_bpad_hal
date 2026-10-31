// =============================================================================
//  wifi_test.ino — bpad_hal Wi-Fi subsystem diagnostic
// =============================================================================
//  Clean Wi-Fi test designed for ESP32 Arduino Core 3.x / ESP-IDF 5.x.
//  - Starts SoftAP directly on boot (no scan interference or baseband shutdown)
//  - Captures and prints all internal Wi-Fi hardware events in real time
//  - Provides HTTP response on port 80 for phone captive portal / connectivity
//  - Allows scanning or connecting to home router via Serial commands
// =============================================================================

#include <Arduino.h>
#include <WiFi.h>
#include <NetworkServer.h>
#include <NetworkClient.h>
#include <esp_mac.h>
#include <bpad_hal.h>

const bpadConfig hw = bpadConfig::defaultBoard();

NetworkServer server(80);

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

void registerWiFiEvents() {
  WiFi.onEvent([](WiFiEvent_t event, WiFiEventInfo_t info) {
    switch (event) {
      case ARDUINO_EVENT_WIFI_READY:
        Serial.println("[EVT] Wi-Fi baseband hardware READY");
        break;
      case ARDUINO_EVENT_WIFI_AP_START:
        Serial.println("[EVT] SoftAP Beaconing Started!");
        break;
      case ARDUINO_EVENT_WIFI_AP_STOP:
        Serial.println("[EVT] SoftAP Stopped");
        break;
      case ARDUINO_EVENT_WIFI_AP_STACONNECTED:
        Serial.printf("[EVT] Client connected to SoftAP! Client MAC: %02X:%02X:%02X:%02X:%02X:%02X  AID: %d\n",
          info.wifi_ap_staconnected.mac[0], info.wifi_ap_staconnected.mac[1],
          info.wifi_ap_staconnected.mac[2], info.wifi_ap_staconnected.mac[3],
          info.wifi_ap_staconnected.mac[4], info.wifi_ap_staconnected.mac[5],
          info.wifi_ap_staconnected.aid);
        break;
      case ARDUINO_EVENT_WIFI_AP_STADISCONNECTED:
        Serial.println("[EVT] Client disconnected from SoftAP");
        break;
      case ARDUINO_EVENT_WIFI_STA_START:
        Serial.println("[EVT] STA station interface started");
        break;
      case ARDUINO_EVENT_WIFI_STA_CONNECTED:
        Serial.println("[EVT] >>> STA CONNECTED to Router! Authenticating & requesting IP... <<<");
        break;
      case ARDUINO_EVENT_WIFI_STA_GOT_IP:
        Serial.printf("[EVT] >>> STA GOT IP: %s (Gateway: %s) <<<\n",
          IPAddress(info.got_ip.ip_info.ip.addr).toString().c_str(),
          IPAddress(info.got_ip.ip_info.gw.addr).toString().c_str());
        break;
      case ARDUINO_EVENT_WIFI_STA_DISCONNECTED:
        Serial.printf("[EVT] STA Disconnected. Reason Code: %d\n", info.wifi_sta_disconnected.reason);
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
  Serial.println("         bpad 185 Wi-Fi Diagnostic");
  Serial.println("==============================================");

  // 1. Read hardware eFuses
  printHardwareMACs();

  // 2. Attach real-time event listener
  registerWiFiEvents();

  // 3. Start SoftAP directly (clean baseband boot, no preliminary scan)
  Serial.println("[AP] Launching SoftAP broadcast...");
  const char* apSSID = "bpad185_Setup";
  const char* apPass = "12345678";

  WiFi.mode(WIFI_AP);
  bool apSuccess = WiFi.softAP(apSSID, apPass);

  if (apSuccess) {
    IPAddress myIP = WiFi.softAPIP();
    Serial.printf("[AP] >>> SUCCESS: SoftAP Broadcast is LIVE! <<<\n");
    Serial.printf("     SSID:     %s\n", apSSID);
    Serial.printf("     Password: %s\n", apPass);
    Serial.printf("     Channel:  %d\n", WiFi.channel());
    Serial.printf("     IP Addr:  %s\n", myIP.toString().c_str());
    Serial.printf("     AP MAC:   %s\n", WiFi.softAPmacAddress().c_str());
    
    server.begin();
    Serial.println("[AP] HTTP Server listening on port 80 (Captive Portal ready)");
    Serial.println("\n>>> Open Wi-Fi on your phone and connect to 'bpad185_Setup' (PW: 12345678) <<<");
  } else {
    Serial.println("[AP] >>> FAILED to create SoftAP! <<<");
  }

  Serial.println("\n--- Serial Commands Available ---");
  Serial.println("  1. Type 'scan' to scan nearby 2.4 GHz channels");
  Serial.println("  2. Type 'connect <SSID> <PASSWORD>' to test home router connection\n");
}

void loop() {
  // Handle HTTP client requests from connected phones
  NetworkClient client = server.accept();
  if (client) {
    String currentLine = "";
    while (client.connected()) {
      if (client.available()) {
        char c = client.read();
        if (c == '\n') {
          if (currentLine.length() == 0) {
            client.println("HTTP/1.1 200 OK");
            client.println("Content-type:text/html");
            client.println("Connection: close");
            client.println();
            client.println("<!DOCTYPE html><html><body style='font-family:sans-serif;text-align:center;padding:40px;background:#111;color:#fff;'>");
            client.println("<h1>bpad 185 Wi-Fi Verified!</h1>");
            client.println("<p>Communication between phone and bpad 185 is fully functional.</p>");
            client.println("</body></html>");
            client.println();
            break;
          } else {
            currentLine = "";
          }
        } else if (c != '\r') {
          currentLine += c;
        }
      }
    }
    client.stop();
  }

  // Serial commands handler
  if (Serial.available()) {
    String cmd = Serial.readStringUntil('\n');
    cmd.trim();

    if (cmd == "scan") {
      Serial.println("\n[SCAN] Switching to dual mode and scanning 2.4 GHz channels...");
      WiFi.mode(WIFI_AP_STA);
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
    } else if (cmd.startsWith("connect ")) {
      int firstSpace = cmd.indexOf(' ', 8);
      if (firstSpace > 8) {
        String ssid = cmd.substring(8, firstSpace);
        String pass = cmd.substring(firstSpace + 1);
        Serial.printf("\n[STA] Connecting to '%s' ...\n", ssid.c_str());
        WiFi.mode(WIFI_AP_STA);
        WiFi.begin(ssid.c_str(), pass.c_str());
      } else {
        Serial.println("[STA] Format: connect <SSID> <PASSWORD>");
      }
    }
  }

  delay(10);
}
