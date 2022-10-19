/*
    Based on Neil Kolban example for IDF: https://github.com/nkolban/esp32-snippets/blob/master/cpp_utils/tests/BLE%20Tests/SampleServer.cpp
    Ported to Arduino ESP32 by Evandro Copercini
    updates by chegewara
*/

#include <BLEDevice.h>
#include <BLEUtils.h>
#include <BLEServer.h>

// See the following for generating UUIDs:
// https://www.uuidgenerator.net/

#define SERVICE_UUID        "0589d19b-410b-4242-a0f6-65e0094bf080"
#define CHARACTERISTIC_UUID "f4c50a2e-3c7d-4948-b964-33966da1dfe2"

BLEServer *pServer;
BLEService *pService;
BLECharacteristic *pCharacteristic;

char strCharacteristic[3] = "D0";

void setup() {
  Serial.begin(115200);
  Serial.println("Starting BLE");

  BLEDevice::init("Exocoude Capteurs");
  pServer = BLEDevice::createServer();
  pService = pServer->createService(SERVICE_UUID);
  pCharacteristic = pService->createCharacteristic(
                      CHARACTERISTIC_UUID,
                      BLECharacteristic::PROPERTY_READ |
                      BLECharacteristic::PROPERTY_NOTIFY
                    );

  pCharacteristic->setValue(strCharacteristic);
  pService->start();

  // BLEAdvertising *pAdvertising = pServer->getAdvertising();  // this still is working for backward compatibility
  BLEAdvertising *pAdvertising = BLEDevice::getAdvertising();
  pAdvertising->addServiceUUID(SERVICE_UUID);
  pAdvertising->setScanResponse(true);
  pAdvertising->setMinPreferred(0x06);  // functions that help with iPhone connections issue
  pAdvertising->setMinPreferred(0x12);
  BLEDevice::startAdvertising();

  Serial.println("Ready!");
}

void loop() {
  // put your main code here, to run repeatedly:
  delay(2000);
  strCharacteristic[0] = 'U';
  pCharacteristic->setValue(strCharacteristic);
  pCharacteristic->notify();
  delay(2000);
  strCharacteristic[0] = 'L';
  strCharacteristic[1] = '1';
  pCharacteristic->setValue(strCharacteristic);
  pCharacteristic->notify();
}