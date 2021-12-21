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

BLECharacteristic *pCharacteristic;
bool deviceConnected = false;
float txValue = 0;

//std::string rxValue; // Could also make this a global var to access it in loop()

// See the following for generating UUIDs:
// https://www.uuidgenerator.net/

#define SERVICE_UUID           "6E400001-B5A3-F393-E0A9-E50E24DCCA9E" // UART service UUID
#define CHARACTERISTIC_UUID_RX "6E400002-B5A3-F393-E0A9-E50E24DCCA9E"
#define CHARACTERISTIC_UUID_TX "6E400003-B5A3-F393-E0A9-E50E24DCCA9E"
#define NUM_LEDS 60 //maximal 60 LEDs
#define DATA_PIN 22
#define COOLING  55
#define SPARKING 120

CRGB leds[NUM_LEDS];
int maxLEDs = 15;
int color1;
int color2;
int color3;
String effect;
String effectCommand;
bool gReverseDirection = false;
unsigned long startMillis;
unsigned long currentMillis;
const unsigned long period = 20;

void num_LEDs(int num) {
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
  color1 = color.substring(0,3).toInt();
  color2 = color.substring(4,7).toInt();
  color3 = color.substring(8).toInt();
  for (int i = 0; i < maxLEDs; i++) {
    leds[i] = CRGB(color1, color2, color3);
  }
  FastLED.show();
}

void wake(String time) {
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
  currentMillis = millis();
  if (currentMillis - startMillis >= intervall.toInt()) {
    for (int i = 0; i < maxLEDs; i++) {
      leds[i] = CRGB::Black;
    }
    FastLED.show();
    delay(100);
  }
  //delay(intervall.toInt());
  if (currentMillis - startMillis >= (intervall.toInt()*2)) {
    startMillis = currentMillis;
    for (int i = 0; i < maxLEDs; i++) {
      Serial.println(i);
      leds[i] = CRGB(color1, color2, color3);
    }
    FastLED.show();
    delay(100);
  }

  //delay(intervall.toInt());
}

void fadeall() {
  for(int i = 0; i < maxLEDs; i++) {
    leds[i].nscale8(250);
  }
}

void colorChange() {
  static uint8_t hue = 0;
  for (int i = 0; i < maxLEDs; i++) {
		leds[i] = CHSV(hue++, 255, 255);
    delay(10);
  }
  
  FastLED.show();
}

void cylon() {
  static uint8_t hue = 0;
  for (int i = 0; i < maxLEDs; i++) {
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
        }

        if (command.startsWith("num")) {
          num_LEDs(command.substring(3).toInt());
        }

        if (command.equals("on")) {
          LEDS.setBrightness(255);
          FastLED.show();
        }

        if (command.equals("off")) {
          LEDS.setBrightness(0);
          FastLED.show();
        }

        if (command.startsWith("c")) {
          color(command.substring(1));
        }

        if (command.startsWith("wakeup")) {
          wake(command.substring(6));
        }

        if (command.startsWith("gosleep")) {
          sleep(command.substring(7));
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

void loop() {
  if (effect != "") {
    if (effect == "blink") {
      blink(effectCommand);
    }

    if (effect == "Color") {
      colorChange();
    }

    if (effect == "cylon") {
      cylon();
    }

    if (effect == "firee") {
      fire();
    }
  }
}
