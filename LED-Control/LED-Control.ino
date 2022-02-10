/*
    Video: https://www.youtube.com/watch?v=oCMOYS71NIU
    Based on Neil Kolban example for IDF: https://github.com/nkolban/esp32-snippets/blob/master/cpp_utils/tests/BLE%20Tests/SampleNotify.cpp
    Ported to Arduino ESP32 by Evandro Copercini

   Create a BLE server that, once we receive a connection, will send periodic notifications.
   The service advertises itself as: 6E400001-B5A3-F393-E0A9-E50E24DCCA9E
   Has a characteristic of: 6E400002-B5A3-F393-E0A9-E50E24DCCA9E - used for receiving data with "WRITE" 
   Has a characteristic of: 6E400003-B5A3-F393-E0A9-E50E24DCCA9E - used to send data with  "NOTIFY"

   The design of creating the BLE server is:
   1. Create a BLE Server
   2. Create a BLE Service
   3. Create a BLE Characteristic on the Service
   4. Create a BLE Descriptor on the characteristic
   5. Start the service.
   6. Start advertising.

   In this example rxValue is the data received (only accessible inside that function).
   And txValue is the data to be sent, in this example just a byte incremented every second. 
*/
#include <FastLED.h>
#include "BluetoothSerial.h"
#include <BLEDevice.h>
#include <BLEServer.h>
#include <BLEUtils.h>
#include <BLE2902.h>
#include <string>
#include <Ticker.h>
#include <BLEAdvertisedDevice.h>
#include <BLEAddress.h>

BLECharacteristic *pCharacteristic;
bool deviceConnected = false;
float txValue = 0;

//std::string rxValue; // Could also make this a global var to access it in loop()

// See the following for generating UUIDs:
// https://www.uuidgenerator.net/

#define SERVICE_UUID           "6E400001-B5A3-F393-E0A9-E50E24DCCA9E" // UART service UUID
#define CHARACTERISTIC_UUID_RX "6E400002-B5A3-F393-E0A9-E50E24DCCA9E"
#define CHARACTERISTIC_UUID_TX "6E400003-B5A3-F393-E0A9-E50E24DCCA9E"
#define NUM_LEDS 150 //maximal 150 LEDs bei mehr bitte hier anpassen, hier 150 wegen Stromversorgung
#define DATA_PIN 22
#define COOLING  55
#define SPARKING 120

CRGB leds[NUM_LEDS];
int maxLEDs = 15; // bei reboot sind 15 LEDs als standard gesetzt
int color1 = 255;
int color2 = 000;
int color3 = 255;
int brightness = 255;
String effect = "";
String effectCommand;
std::string mName = "";
static BLEAddress *pServerAddress;
BLEScan* pBLEScan ;
int Verzoegerung = 5;
int VerzoegerungZaeler = 0;
bool gReverseDirection = false;
Ticker Tic;
unsigned long startMillis;
unsigned long currentMillis;
const unsigned long period = 20;

void num_LEDs(int num) {
  // sets the number of controllable leds
  if (num < maxLEDs) {
    for (int i = num; i <= maxLEDs; i++) {
    leds[i] = CRGB::Black;
    }
  } else {
    for (int i = (num-1); i >= maxLEDs; i--) {
    leds[i] = CRGB::White;//When add LEDs this extra LEDs are with White color
    }
  }
  FastLED.show();
  maxLEDs = num;
}

void color(String color) {
  // sets the color of all leds
  color1 = color.substring(0,3).toInt();
  color2 = color.substring(4,7).toInt();
  color3 = color.substring(8).toInt();
  for (int i = 0; i < maxLEDs; i++) {
    leds[i] = CRGB(color1, color2, color3);
  }
  FastLED.show();
}

void wake(String time) {
  // at variable time the leds are get brighter to maximum
  LEDS.setBrightness(0);
  for (int i = 0; i < maxLEDs; i++) {
    leds[i] = CRGB::White;
    FastLED.show();
  }
  while (LEDS.getBrightness() < 255) {
    if (LEDS.getBrightness() == 250) {
      LEDS.setBrightness(LEDS.getBrightness() + 5);
    } else {
      LEDS.setBrightness(LEDS.getBrightness() + 10);
    }
    FastLED.show();
    delay(time.toInt());
  }
}

void sleep(String time) {
  // at variable time the leds are getting darker to off
  while (LEDS.getBrightness() > 0) {
    if (LEDS.getBrightness() - 10 >= 0) {
      LEDS.setBrightness(LEDS.getBrightness() - 10);
    } else {
      LEDS.setBrightness(0);
    }
    FastLED.show();
    delay(time.toInt());
  }
}

void blink(String intervall) {
  // leds are blinking
  currentMillis = millis();
  if (currentMillis - startMillis >= intervall.toInt()) {
    for (int i = 0; i < maxLEDs; i++) {
      leds[i] = CRGB::Black;
    }
    FastLED.show();
    delay(100);
  }
  if (currentMillis - startMillis >= (intervall.toInt()*2)) {
    startMillis = currentMillis;
    for (int i = 0; i < maxLEDs; i++) {
      leds[i] = CRGB(color1, color2, color3);
    }
    FastLED.show();
    delay(100);
  }
}

void fadeall() {
  for(int i = 0; i < maxLEDs; i++) {
    leds[i].nscale8(250);
  }
}

void colorChange() {
  // the color of the leds is changeing
  static uint8_t hue = 0;
  for (int i = 0; i < maxLEDs; i++) {
		leds[i] = CHSV(hue++, 255, 255);
    delay(10);
    FastLED.show();
  }
  //FastLED.show();
}

void cylon() {
  //https://wokwi.com/arduino/libraries/FastLED
  // leds are shown in loop with changing color 
  static uint8_t hue = 0;
  for (int i = 0; i < maxLEDs; i++) {
		// Set the i'th led to red
		leds[i] = CHSV(hue++, 255, 255);
		FastLED.show();
		// now that we've shown the leds, reset the i'th led to black
		leds[i] = CRGB::Black;
		fadeall();
		// Wait a little bit before we loop around and do it again
		delay(10);
	}
	// Now go in the other direction.
	for (int i = (maxLEDs)-1; i >= 0; i--) {
		// Set the i'th led to red
		leds[i] = CHSV(hue++, 255, 255);
		// Show the leds
		FastLED.show();
		// now that we've shown the leds, reset the i'th led to black
		leds[i] = CRGB::Black;
		fadeall();
		// Wait a little bit before we loop around and do it again
		delay(10);
  }
}

void fire() {
  //https://wokwi.com/arduino/libraries/FastLED
  // leds are shown fire effects
  static byte heat[NUM_LEDS];
  // Step 1.  Cool down every cell a little
  for (int i = 0; i < maxLEDs; i++) {
    heat[i] = qsub8(heat[i],  random8(0, ((COOLING * 10) / maxLEDs) + 2));
  }
  // Step 2.  Heat from each cell drifts 'up' and diffuses a little
  for (int k= maxLEDs - 1; k >= 2; k--) {
    heat[k] = (heat[k - 1] + heat[k - 2] + heat[k - 2] ) / 3;
  }
  // Step 3.  Randomly ignite new 'sparks' of heat near the bottom
  if (random8() < SPARKING) {
    int y = random8(7);
    heat[y] = qadd8(heat[y], random8(16,160));
  }
  // Step 4.  Map from heat cells to LED colors
  for (int j = 0; j < maxLEDs; j++) {
    CRGB color = HeatColor(heat[j]);
    int pixelnumber;
    if (gReverseDirection) {
      pixelnumber = (maxLEDs-1) - j;
    } else {
      pixelnumber = j;
    }
    leds[pixelnumber] = color;
  }
  FastLED.show();
  delay(16);
}

//Example Code for ESP32 Advertising
class MyAdvertisedDeviceCallbacks: public BLEAdvertisedDeviceCallbacks {
    void onResult(BLEAdvertisedDevice advertisedDevice) // passiert wenn BLE Device ( beacon ) gefunden wurde
    {
      Serial.println(advertisedDevice.getAddress().toString().c_str()); // beacon Adresse anzeigen
      if (advertisedDevice.getServiceDataUUID().toString().c_str() == mName)
      {
        Serial.println(" Ueberwachte Adresse");                         // wenn überwache Adresse gefunden wurde
        LEDS.setBrightness(255);
        FastLED.show();
        VerzoegerungZaeler = 0;
        advertisedDevice.getScan()->stop();                           // Scanvorgang beenden
      }
    }
};

void SekundenTic() {
  VerzoegerungZaeler++;  // Sekundenzähler
  if (VerzoegerungZaeler >= Verzoegerung && effect == "beacon") {
    LEDS.setBrightness(0);
    FastLED.show();
  }
}

String fillNull(int number) {
  // fügt führende nullen ein
    String newNumber = "";
    newNumber += number;
    while (newNumber.length() < 3) {
      newNumber = "0" + newNumber;
    }
    return newNumber;
}


class MyServerCallbacks: public BLEServerCallbacks {
    void onConnect(BLEServer* pServer) {
      deviceConnected = true;     
    };

    void onDisconnect(BLEServer* pServer) {
      deviceConnected = false;
    }
};

class MyCallbacks: public BLECharacteristicCallbacks {
    void onWrite(BLECharacteristic *pCharacteristic) {
      std::string rxValue = pCharacteristic->getValue();
     
      String command = "";
      if (rxValue.length() > 0) {
        Serial.println("*********");
        Serial.print("Received Value: ");

        for (int i = 0; i < rxValue.length(); i++) {
          Serial.print(rxValue[i]);
          command += rxValue[i];
        }
        
        command.trim();

        // Do stuff based on the command received from the app
        if (command.startsWith("e")) {
          effect = command.substring(1,6);
        } else {
          effect = "";
          delay(10);
        }

        if (command.startsWith("num")) {
          num_LEDs(command.substring(3).toInt());
        }

        else if (command.equals("on")) {
          LEDS.setBrightness(255);
          FastLED.show();
          brightness = 255;
          pCharacteristic->setValue("b255");
          pCharacteristic->notify();
        }

        else if (command.equals("off")) {
          LEDS.setBrightness(0);
          FastLED.show();
          brightness = 0;
          pCharacteristic->setValue("b0");
          pCharacteristic->notify();
        }

        else if (command.startsWith("c")) {
          // stes color and sends feedback to app
          color(command.substring(1));
          String c = "c";
          String data = c + fillNull(color1);
          String data1 = data + fillNull(color2);
          String data2 = data1 + fillNull(color3);
          std::string data3 = data2.c_str();
          pCharacteristic->setValue(data3);
          pCharacteristic->notify();
        }

        else if (command.startsWith("wakeup")) {
          wake(command.substring(6));
        }

        else if (command.startsWith("gosleep")) {
          sleep(command.substring(7));
        }

        else if (command.equals("beacon+ein")) {
          // starts scanning for the device wich sends this signal
          pServerAddress = new BLEAddress(mName.c_str());
          BLEDevice::init("");
          pBLEScan = BLEDevice::getScan();
          pBLEScan->setAdvertisedDeviceCallbacks(new MyAdvertisedDeviceCallbacks());
          Tic.attach(1,SekundenTic);
          effect = "beacon";
        }

        else if (command.equals("beacon+aus")) {
          effect = "";
        }

        if (command.startsWith("m")) {
          // receive device data and send saved data
          if (brightness == 255) {
          pCharacteristic->setValue("b255");
          } else {
            pCharacteristic->setValue("b0");
          }
          pCharacteristic->notify();
          delay(10);
          String name = command.substring(1);
          mName = name.c_str();
          String c = "c";
          String data = c + fillNull(color1);
          String data1 = data + fillNull(color2);
          String data2 = data1 + fillNull(color3);
          std::string data3 = data2.c_str();
          pCharacteristic->setValue(data3);
          pCharacteristic->notify();
        }

        if (command.startsWith("eblink")) {
          effectCommand = command.substring(6);
        }

        Serial.println();
        Serial.println("*********");
      }
    }
};

void setup() {
  Serial.begin(115200);

  // Create the BLE Device
  BLEDevice::init("MYESP32"); // Give it a name

  // Create the BLE Server
  BLEServer *pServer = BLEDevice::createServer();
  pServer->setCallbacks(new MyServerCallbacks());
  
  // Create the BLE Service
  BLEService *pService = pServer->createService(SERVICE_UUID);

  // Create a BLE Characteristic
  pCharacteristic = pService->createCharacteristic(
                      CHARACTERISTIC_UUID_TX,
                      BLECharacteristic::PROPERTY_NOTIFY
                    );
                      
  pCharacteristic->addDescriptor(new BLE2902());

  BLECharacteristic *pCharacteristic = pService->createCharacteristic(
                                         CHARACTERISTIC_UUID_RX,
                                         BLECharacteristic::PROPERTY_WRITE
                                       );

  pCharacteristic->setCallbacks(new MyCallbacks());

  // Start the service
  pService->start();

  // Start advertising
  pServer->getAdvertising()->start();
  Serial.println("Waiting a client connection to notify...");

  FastLED.addLeds<WS2812B, DATA_PIN, GRB>(leds, NUM_LEDS);  // GRB ordering is typical
}

//loop for restart effects when they done for repeat
void loop() {
  if (effect == "beacon") {
    currentMillis = millis();
    if (currentMillis - startMillis >= 2000) {
      pBLEScan->start(15);
    }
  }
  
  if (effect == "blink") {
    blink(effectCommand);
  }

  else if (effect == "Color") {
    colorChange();
  }

  else if (effect == "cylon") {
    cylon();
  }

  else if (effect == "firee") {
    fire();
  }
}
