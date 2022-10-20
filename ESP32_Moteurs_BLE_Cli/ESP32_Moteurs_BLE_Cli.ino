/*
   Exocoude / Fabrikarium 2022 / 18-20 oct 2022
   Controle moteur et BLE client

   arduino IDE v2.0.0
     + lib  ESP32 C++ Utility Classes (BLE) by Neil Kolban (http://www.neilkolban.com/esp32/docs/cpp_utils/html/index.html)
*/

#include "BLEDevice.h"
//#include "BLEScan.h"
#include "OneButton.h"

#define DEBUG true             // Infos de debug sur la console série

#define USE_ILS         1
#define BROCHE_ILS1     4
#define BROCHE_ILS2     5

// The motor control relays are inactive at H (5V)
// Drive a L (0V) to the corresponding pin (relay control input) to turn
// in that direction
#define BROCHE_MOT_U    16
#define BROCHE_MOT_D    17
#define BROCHE_MOT_L    18
#define BROCHE_MOT_R    19

// The remote service we wish to connect to.
static BLEUUID    serviceUUID("0589d19b-410b-4242-a0f6-65e0094bf080");
// The characteristic of the remote service we are interested in.
static BLEUUID    charUUID("f4c50a2e-3c7d-4948-b964-33966da1dfe2");

static boolean doConnect = false;
static boolean connected = false;
static boolean doScan = false;
static BLERemoteCharacteristic* pRemoteCharacteristic;
static BLEAdvertisedDevice* myDevice;

char strCharacteristic[4] = "UI0";

OneButton ils1(BROCHE_ILS1, true, true); // ils, logique (true = active low), pullup interne
OneButton ils2(BROCHE_ILS2, true, true); // ils, logique (true = active low), pullup interne

// Variables qui conservent les modes
// Par défaut mode haut/bas, direction haut
// Quand on bascule en mode rotation, par défaut à gauche
boolean mode_onoff     = false ; // allumé / éteint
boolean mode_mouvement = false ; // false : haut/bas, true : gauche/droite
boolean mode_sens      = false ; // false : gauche/haut, true : droite/bas
boolean mode_marche    = false ; // false : arrêt, true : marche
boolean changed        = false;   // detection de changement

static void handleLongPressStart1() {
  changed = true;
  if (DEBUG) Serial.println("press1 (active)");

  strCharacteristic[1] = 'A';
  mode_marche = true;
}

static void handleLongPressStop1() {
  changed = true;
  if (DEBUG) Serial.println("release1 (inactive)");

  strCharacteristic[1] = 'I';
  mode_marche = false;
}

static void handleLongPressStart2() {
  changed = true;
  if (DEBUG) Serial.println("press2 (active)");
}

static void handleLongPressStop2() {
  changed = true;
  if (DEBUG) Serial.println("release2 (inactive)");
}

static void notifyCallback(
  BLERemoteCharacteristic* pBLERemoteCharacteristic,
  uint8_t* pData,
  size_t length,
  bool isNotify) {
    changed = true;
    strCharacteristic[0] = pData[0];
    if (USE_ILS == 0) strCharacteristic[1] = pData[1];
    strCharacteristic[2] = pData[2]; 
     
    if (DEBUG) {   
      //Serial.print("Notify callback for characteristic ");
      //Serial.print(pBLERemoteCharacteristic->getUUID().toString().c_str());
      //Serial.print(" of data length ");
      //Serial.println(length);
      //Serial.print("data: ");
      //Serial.println((char*)pData);
      Serial.print("Notify callback: ");
      Serial.println(strCharacteristic);
    }
}

class MyClientCallback : public BLEClientCallbacks {
  void onConnect(BLEClient* pclient) {
  }

  void onDisconnect(BLEClient* pclient) {
    connected = false;
    Serial.println("onDisconnect");
  }
};

bool connectToServer() {
    Serial.print("Forming a connection to ");
    Serial.println(myDevice->getAddress().toString().c_str());
    
    BLEClient*  pClient  = BLEDevice::createClient();
    Serial.println(" - Created client");

    pClient->setClientCallbacks(new MyClientCallback());

    // Connect to the remove BLE Server.
    pClient->connect(myDevice);  // if you pass BLEAdvertisedDevice instead of address, it will be recognized type of peer device address (public or private)
    Serial.println(" - Connected to server");
    pClient->setMTU(517); //set client to request maximum MTU from server (default is 23 otherwise)
  
    // Obtain a reference to the service we are after in the remote BLE server.
    BLERemoteService* pRemoteService = pClient->getService(serviceUUID);
    if (pRemoteService == nullptr) {
      Serial.print("Failed to find our service UUID: ");
      Serial.println(serviceUUID.toString().c_str());
      pClient->disconnect();
      return false;
    }
    Serial.println(" - Found our service");

    // Obtain a reference to the characteristic in the service of the remote BLE server.
    pRemoteCharacteristic = pRemoteService->getCharacteristic(charUUID);
    if (pRemoteCharacteristic == nullptr) {
      Serial.print("Failed to find our characteristic UUID: ");
      Serial.println(charUUID.toString().c_str());
      pClient->disconnect();
      return false;
    }
    Serial.println(" - Found our characteristic");

    // Read the value of the characteristic.
    if(pRemoteCharacteristic->canRead()) {
      std::string value = pRemoteCharacteristic->readValue();
      Serial.print("The characteristic value was: ");
      Serial.println(value.c_str());
    }

    if(pRemoteCharacteristic->canNotify())
      pRemoteCharacteristic->registerForNotify(notifyCallback);

    connected = true;
    return true;
}
/**
 * Scan for BLE servers and find the first one that advertises the service we are looking for.
 */
class MyAdvertisedDeviceCallbacks: public BLEAdvertisedDeviceCallbacks {
 /**
   * Called for each advertising BLE server.
   */
  void onResult(BLEAdvertisedDevice advertisedDevice) {
    Serial.print("BLE Advertised Device found: ");
    Serial.println(advertisedDevice.toString().c_str());
    
    // We have found a device, let us now see if it contains the service we are looking for.
    if (advertisedDevice.haveServiceUUID() && advertisedDevice.isAdvertisingService(serviceUUID)) {
      BLEDevice::getScan()->stop();
      myDevice = new BLEAdvertisedDevice(advertisedDevice);
      doConnect = true;
      doScan = true;
      Serial.println("FOUND!");
    } // Found our server
  } // onResult
}; // MyAdvertisedDeviceCallbacks


void setup() {
  Serial.begin(115200);
  Serial.println("Starting BLE Client...");
  BLEDevice::init("");

  // Retrieve a Scanner and set the callback we want to use to be informed when we
  // have detected a new device.  Specify that we want active scanning and start the
  // scan to run for 5 seconds.
  BLEScan* pBLEScan = BLEDevice::getScan();
  pBLEScan->setAdvertisedDeviceCallbacks(new MyAdvertisedDeviceCallbacks());
  pBLEScan->setInterval(1349);
  pBLEScan->setWindow(449);
  pBLEScan->setActiveScan(true);
  pBLEScan->start(5, false);

  // Evènements attachés aux ils
  ils1.attachLongPressStart(handleLongPressStart1);
  ils1.attachLongPressStop(handleLongPressStop1);
  ils2.attachLongPressStart(handleLongPressStart2);
  ils2.attachLongPressStop(handleLongPressStop2);

  // Réglages des durées
  ils1.setDebounceTicks(50);  // Period of time in which to ignore additional level changes.
  ils1.setClickTicks(500);    // Timeout used to distinguish single clicks from double clicks.
  ils1.setPressTicks(500);    // Duration to hold a button to trigger a long press.
  ils2.setDebounceTicks(50);  // Period of time in which to ignore additional level changes.
  ils2.setClickTicks(500);    // Timeout used to distinguish single clicks from double clicks.
  ils2.setPressTicks(500);    // Duration to hold a button to trigger a long press.

  // As a precaution pre-drive H to all relay control signals to inactivate motors ?
  digitalWrite(BROCHE_MOT_U, HIGH);
  digitalWrite(BROCHE_MOT_D, HIGH);
  digitalWrite(BROCHE_MOT_L, HIGH);
  digitalWrite(BROCHE_MOT_R, HIGH);

  // Configure relay control signals as digital outputs
  pinMode(BROCHE_MOT_U, OUTPUT);
  pinMode(BROCHE_MOT_D, OUTPUT);
  pinMode(BROCHE_MOT_L, OUTPUT);
  pinMode(BROCHE_MOT_R, OUTPUT);

  // Drive H to all relay control signals to inactivate motors
  digitalWrite(BROCHE_MOT_U, HIGH);
  digitalWrite(BROCHE_MOT_D, HIGH);
  digitalWrite(BROCHE_MOT_L, HIGH);
  digitalWrite(BROCHE_MOT_R, HIGH);
} // End of setup.


// This is the Arduino main loop function.
void loop() {
  ils1.tick();
  //ils2.tick();

  // If the flag "doConnect" is true then we have scanned for and found the desired
  // BLE Server with which we wish to connect.  Now we connect to it.  Once we are 
  // connected we set the connected flag to be true.
  if (doConnect == true) {
    if (connectToServer()) {
      Serial.println("We are now connected to the BLE Server.");
    } else {
      Serial.println("We have failed to connect to the server; there is nothin more we will do.");
    }
    doConnect = false;
  }

  if (changed) {
    if ( strCharacteristic[1] == 'A' && strCharacteristic[2] == '1') {
      if ( strCharacteristic[0] == 'U' ) {
         if (DEBUG) {        
           Serial.println("Motor UD : UP");
           Serial.println("Motor LR : OFF");
         }
         digitalWrite(BROCHE_MOT_U, LOW);
         digitalWrite(BROCHE_MOT_D, HIGH);
         digitalWrite(BROCHE_MOT_L, HIGH);
         digitalWrite(BROCHE_MOT_R, HIGH);
      } else if ( strCharacteristic[0] == 'D' ) {
         if (DEBUG) {        
           Serial.println("Motor UD : DOWN");
           Serial.println("Motor LR : OFF");
         }
         digitalWrite(BROCHE_MOT_U, HIGH);
         digitalWrite(BROCHE_MOT_D, LOW);
         digitalWrite(BROCHE_MOT_L, HIGH);
         digitalWrite(BROCHE_MOT_R, HIGH);
      } else if ( strCharacteristic[0] == 'L' ) {
         if (DEBUG) {                
           Serial.println("Motor UD : OFF");
           Serial.println("Motor LR : LEFT");
         }
         digitalWrite(BROCHE_MOT_U, HIGH);
         digitalWrite(BROCHE_MOT_D, HIGH);
         digitalWrite(BROCHE_MOT_L, LOW);
         digitalWrite(BROCHE_MOT_R, HIGH);
      } else if ( strCharacteristic[0] == 'R' ) {
         if (DEBUG) { 
           Serial.println("Motor UD : OFF");
           Serial.println("Motor LR : RIGHT");
         }
         digitalWrite(BROCHE_MOT_U, HIGH);
         digitalWrite(BROCHE_MOT_D, HIGH);
         digitalWrite(BROCHE_MOT_L, HIGH);
         digitalWrite(BROCHE_MOT_R, LOW);
      } 
    } else {
      if (DEBUG) { 
        Serial.println("Motor UD : OFF");
        Serial.println("Motor LR : OFF");
      }
      digitalWrite(BROCHE_MOT_U, HIGH);
      digitalWrite(BROCHE_MOT_D, HIGH);
      digitalWrite(BROCHE_MOT_L, HIGH);
      digitalWrite(BROCHE_MOT_R, HIGH);
    }
  }

  // If we are connected to a peer BLE Server, update the characteristic each time we are reached
  // with the current time since boot.
  if (connected) {
    //String newValue = "Time since boot: " + String(millis()/1000);
    //Serial.println(newValue);
  } else if(doScan){
    BLEDevice::getScan()->start(0);  // this is just example to start scan after disconnect, most likely there is better way to do it in arduino
  }
  
  delay(100); // Delay between loops.
} // End of loop
