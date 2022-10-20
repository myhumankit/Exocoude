/*
   Exocoude / Fabrikarium 2022 / 18-20 oct 2022
   Interface de contrôle et BLE server (BLE = Bluetooth Low Energy) 
   3 commandes par interrupteur flexible fixé sur un doigt pour définir le sens et l'orientation d'un mouvement.
   Le sens (haut,bas,gauche,droite) est transmis en BLE à une 2e carte qui commande les moteurs, 
     un interrupteur relié à cette 2e carte active ou non le mouvement 

   arduino IDE v1.8.5
     + lib. OneButton v2.0.4 de Matthias Hertel (https://github.com/mathertel/OneButton)
     + lib  ESP32 C++ Utility Classes (BLE) by Neil Kolban (http://www.neilkolban.com/esp32/docs/cpp_utils/html/index.html)

   v. 002 : proto sur arduino nano avec son et pullup externe
   v. 003 : modif pour pullup interne
   v. 004 : integration avec BLE server

   Une seule donnée est transmise sur 2 caractères quand un changement est détecté :
   1er - direction - U : haut, R : droite, D : bas, L : gauche
   2eme - moteur - 1 : marche, 0 : arrêt

   mouvement    sens        direction

   false        false       U           haut
   false        true        D           bas
   true         false       L           gauche
   true         true        R           droite
*/

#include <BLEDevice.h>
#include <BLEUtils.h>
#include <BLEServer.h>
#include "OneButton.h"

#define DEBUG true             // Infos de debug sur la console série

// See the following for generating UUIDs:
// https://www.uuidgenerator.net/
#define SERVICE_UUID        "0589d19b-410b-4242-a0f6-65e0094bf080"
#define CHARACTERISTIC_UUID "f4c50a2e-3c7d-4948-b964-33966da1dfe2"

#define BROCHE_BOUTON1     4    // interrupteur flexible
#define BROCHE_BOUTON2     5    // bouton de test, simule le flag actif/inactif
#define BROCHE_HP          2    // buzzer pour un retour audio sur l'interrupteur flexible

BLEServer *pServer;
BLEService *pService;
BLECharacteristic *pCharacteristic;

char strCharacteristic[4] = "UI0";  // donnees transmises par BLE

OneButton bouton1(BROCHE_BOUTON1, true, true); // broche, logique (true = active low), pullup interne ?
OneButton bouton2(BROCHE_BOUTON2, true, true); // broche, logique (true = active low), pullup interne ?

// Variables qui conservent les modes
// Par défaut mode haut/bas, direction haut
// Quand on bascule en mode rotation, par défaut à gauche
boolean mode_onoff     = false ; // allumé / éteint
boolean mode_mouvement = false ; // false : haut/bas, true : gauche/droite
boolean mode_sens      = false ; // false : gauche/haut, true : droite/bas
boolean mode_marche    = false ; // false : arrêt, true : marche

// Donnée à transmettre
boolean changed = false;         // transmettre uniquement si changement

static void handleClick1() {
  if (mode_onoff) {
    changed = true;
    if (DEBUG) Serial.println("clic (changement de sens)");

    mode_sens = !mode_sens;           // inverser le sens 
    sonClic(1);
  }
}

static void handleDoubleClick1() {
  if (mode_onoff) {
    changed = true;
    if (DEBUG) Serial.println("double clic (changement de type de mouvement");

    mode_mouvement = !mode_mouvement;  // changer de mouvement (haut/bas OU gauche/droite)

    // Remettre la valeur de sens à défaut
    mode_sens = false;
    sonClic(2);
  }
}

static void handleLongPressStop1() {
  changed = true;
  if (DEBUG) Serial.println("long press stop 1");
  mode_onoff = !mode_onoff;

  if (mode_onoff) {
    if (DEBUG)  Serial.println("systeme actif");
    sonOn();
  } else {
    if (DEBUG) Serial.println("systeme en arret");
    // Remettre toutes les valeurs par défaut
    mode_mouvement = false;
    mode_sens = false;
    mode_marche = false;
    sonOff();
  }
}

static void handleLongPressStart2() {
  if (mode_onoff) {
    changed = true;
    if (DEBUG) Serial.println("press (active)");

    mode_marche = true;
    sonClic(1);
  }
}

static void handleLongPressStop2() {
  if (mode_onoff) {
    changed = true;
    if (DEBUG) Serial.println("release (inactive)");

    mode_marche = false;
    sonClic(1);
  }
}

void sonClic(int repet) {
  for (int i = 0; i < repet; i++) {
    tone(BROCHE_HP, 1760*repet, 150);
    delay(100);
  }
}

void sonOn() {
  for (int i = 2000; i < 6000; i += 200) {
    tone(BROCHE_HP, i, 20);
  }
}

void sonOff() {
  for (int i = 6000; i > 2000; i -= 200) {
    tone(BROCHE_HP, i, 20);
  }
}


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

  // Buzzer
  pinMode(BROCHE_HP, OUTPUT);

  // Evènements attachés au boutons
  bouton1.attachClick(handleClick1);
  bouton1.attachDoubleClick(handleDoubleClick1);
  bouton1.attachLongPressStop(handleLongPressStop1);
  bouton2.attachLongPressStart(handleLongPressStart2);
  bouton2.attachLongPressStop(handleLongPressStop2);

  // Réglages des durées
  bouton1.setDebounceTicks(50);  // Period of time in which to ignore additional level changes.
  bouton1.setClickTicks(500);    // Timeout used to distinguish single clicks from double clicks.
  bouton1.setPressTicks(3000);   // Duration to hold a button to trigger a long press.
  bouton2.setDebounceTicks(50);  // Period of time in which to ignore additional level changes.
  bouton2.setClickTicks(500);    // Timeout used to distinguish single clicks from double clicks.
  bouton2.setPressTicks(1000);   // Duration to hold a button to trigger a long press.

  Serial.println("Ready!");
}

void loop() {
  bouton1.tick();
  bouton2.tick();

  if (changed) {
    if (mode_onoff) {
      strCharacteristic[2] = '1';

      // Chercher la direction définie
      if (!mode_mouvement && !mode_sens) strCharacteristic[0] = 'U' ;  // haut
      if (!mode_mouvement && mode_sens)  strCharacteristic[0] = 'D' ;  // bas
      if (mode_mouvement && !mode_sens)  strCharacteristic[0] = 'L' ;  // gauche
      if (mode_mouvement && mode_sens)   strCharacteristic[0] = 'R' ;  // droite
      if (mode_marche)                   strCharacteristic[1] = 'A' ;  // marche
      else                               strCharacteristic[1] = 'I' ;  // arrêt
    } else {
      strCharacteristic[0] = 'U';
      strCharacteristic[1] = 'I';
      strCharacteristic[2] = '0';
    }
    
    if (DEBUG) {
      Serial.print("Mouvement update: "); // debug
      Serial.println(strCharacteristic);
    }

    pCharacteristic->setValue(strCharacteristic);
    pCharacteristic->notify();

    changed = false;
  }

}
