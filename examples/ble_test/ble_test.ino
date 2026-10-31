// =============================================================================
//  ble_test.ino — bpad_hal Bluetooth Low Energy (BLE) subsystem test
// =============================================================================
//  Tests BLE 5.0 advertising and server connection on bpad 185.
//  Broadcasts as "bpad185" with custom service UUID 0x1850.
//  Use any BLE scanner app (e.g. nRF Connect on iOS / Android) to connect.
// =============================================================================

#include <Arduino.h>
#include <BLEDevice.h>
#include <BLEUtils.h>
#include <BLEServer.h>
#include <BLE2902.h>
#include <bpad_hal.h>

#define SERVICE_UUID        "4fafc201-1fb5-459e-8fcc-c5c9c331914b"
#define CHARACTERISTIC_UUID "beb5483e-36e1-4688-b7f5-ea07361b26a8"

const bpadConfig hw = bpadConfig::defaultBoard();

BLEServer* pServer = nullptr;
BLECharacteristic* pCharacteristic = nullptr;
bool deviceConnected = false;
bool oldDeviceConnected = false;
uint32_t counter = 0;

class MyServerCallbacks : public BLEServerCallbacks {
  void onConnect(BLEServer* pServer) override {
    deviceConnected = true;
    Serial.println("[BLE] Client connected!");
  }

  void onDisconnect(BLEServer* pServer) override {
    deviceConnected = false;
    Serial.println("[BLE] Client disconnected. Restarting advertising...");
  }
};

void setup() {
  Serial.begin(115200);
#if ARDUINO_USB_CDC_ON_BOOT
  Serial.setTxTimeoutMs(0);
#endif
  delay(500);

  Serial.println("\n==============================================");
  Serial.println("         bpad 185 BLE Diagnostic");
  Serial.println("==============================================");
  Serial.printf("Board: %s\n", hw.name);

  // Initialize BLE Device
  Serial.println("[BLE] Initializing BLE stack...");
  BLEDevice::init("bpad185");

  // Create BLE Server
  pServer = BLEDevice::createServer();
  pServer->setCallbacks(new MyServerCallbacks());

  // Create Service
  BLEService* pService = pServer->createService(SERVICE_UUID);

  // Create Characteristic with Read, Write, and Notify
  pCharacteristic = pService->createCharacteristic(
                      CHARACTERISTIC_UUID,
                      BLECharacteristic::PROPERTY_READ   |
                      BLECharacteristic::PROPERTY_WRITE  |
                      BLECharacteristic::PROPERTY_NOTIFY
                    );

  pCharacteristic->addDescriptor(new BLE2902());
  pCharacteristic->setValue("Hello from bpad 185!");

  // Start Service
  pService->start();

  // Start Advertising
  BLEAdvertising* pAdvertising = BLEDevice::getAdvertising();
  pAdvertising->addServiceUUID(SERVICE_UUID);
  pAdvertising->setScanResponse(true);
  pAdvertising->setMinPreferred(0x06); // functions that help with iPhone connections
  pAdvertising->setMinPreferred(0x12);

  BLEDevice::startAdvertising();

  Serial.println("[BLE] Advertising active as \"bpad185\"!");
  Serial.println("[BLE] Open your phone's Bluetooth or nRF Connect app to verify discovery.\n");
}

void loop() {
  // If connected, send a notification every 2 seconds
  if (deviceConnected) {
    char payload[32];
    snprintf(payload, sizeof(payload), "Heartbeat #%u", counter++);
    pCharacteristic->setValue(payload);
    pCharacteristic->notify();
    Serial.printf("[BLE Notify] Sent: %s\n", payload);
    delay(2000);
  }

  // Handle reconnect advertising
  if (!deviceConnected && oldDeviceConnected) {
    delay(500);
    pServer->startAdvertising();
    oldDeviceConnected = deviceConnected;
  }
  if (deviceConnected && !oldDeviceConnected) {
    oldDeviceConnected = deviceConnected;
  }

  delay(20);
}
