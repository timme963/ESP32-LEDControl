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
#define NUM_LEDS 60
#define DATA_PIN 22

CRGB leds[NUM_LEDS];
int maxLEDs = 15;
int bright = 100;

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

void brightness(int newbright) {
  LEDS.setBrightness(newbright);
  bright = newbright;
  FastLED.show();
}

void color(String color) {
  int color1 = color.substring(0,3).toInt();
  int color2 = color.substring(4,7).toInt();
  int color3 = color.substring(8).toInt();
  for (int i = 0; i < maxLEDs; i++) {
    leds[i] = CRGB(color1, color2, color3);
    FastLED.show();
  }
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

        Serial.println();

        // Do stuff based on the command received from the app
        if (command.startsWith("num")) {
          num_LEDs(command.substring(3).toInt());
        }

        if (command.startsWith("bright")) {
          brightness(command.substring(6).toInt());
        }

        if (command.equals("on")) {
          LEDS.setBrightness(bright);
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
  if (deviceConnected) {
    // Fabricate some arbitrary junk for now...
    //txValue = analogRead(readPin) / 3.456; // This could be an actual sensor reading!

    // Let's convert the value to a char array:
    //char txString[8]; // make sure this is big enuffz
    //dtostrf(txValue, 1, 2, txString); // float_val, min_width, digits_after_decimal, char_buffer
    
//    pCharacteristic->setValue(&txValue, 1); // To send the integer value
//    pCharacteristic->setValue("Hello!"); // Sending a test message
    //pCharacteristic->setValue(txString);
    
    //pCharacteristic->notify(); // Send the value to the app!
    //Serial.print("*** Sent Value: ");
    //Serial.print(txString);
    //Serial.println(" ***");

    // You can add the rxValue checks down here instead
    // if you set "rxValue" as a global var at the top!
    // Note you will have to delete "std::string" declaration
    // of "rxValue" in the callback function.
//    if (rxValue.find("A") != -1) { 
//      Serial.println("Turning ON!");
//      digitalWrite(LED, HIGH);
//    }
//    else if (rxValue.find("B") != -1) {
//      Serial.println("Turning OFF!");
//      digitalWrite(LED, LOW);
//    }
  }
  delay(1000);
}
