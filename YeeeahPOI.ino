// SPDX-FileCopyrightText: 2019 Phillip Burgess for Adafruit Industries
//
// SPDX-License-Identifier: MIT

/*------------------------------------------------------------------------
  POV IR Supernova Poi sketch.  Uses the following Adafruit parts
  (X2 for two poi):
  - Teensy 3.2 (required - NOT compatible w/AVR-based boards)
  - 2200 mAh Lithium Ion Battery https://www.adafruit.com/product/1781
  - LiPoly Backpack https://www.adafruit.com/product/2124
  - 144 LED/m DotStar strip (#2328 or #2329)
    (ONE METER is enough for TWO poi)
  - Infrared Sensor: https://www.adafruit.com/product/157
  - Mini Remote Control: https://www.adafruit.com/product/389
    (only one remote is required for multiple poi)
  Needs Adafruit_DotStar library: github.com/adafruit/Adafruit_DotStar
  Also, uses version of IRremote library from the Teensyduino installer,
  the stock IRremote lib will NOT work here!
  This is based on the LED poi code (also included in the repository),
  but AVR-specific code has been stripped out for brevity, since these
  mega-poi pretty much require a Teensy 3.X.
  Adafruit invests time and resources providing this open source code,
  please support Adafruit and open-source hardware by purchasing
  products from Adafruit!
  Written by Phil Burgess / Paint Your Dragon for Adafruit Industries.
  MIT license, all text above must be included in any redistribution.
  See 'COPYING' file for additional notes.
  ------------------------------------------------------------------------*/

#include <Arduino.h>
#include <Adafruit_DotStar.h>
// #include <avr/power.h>
// #include <avr/sleep.h>
#include <SPI.h>

// Adafruit Bluetooth Low Energy Libraries
#include <BLEDevice.h>
#include <BLEServer.h>
#include <BLEUtils.h>
#include <BLE2902.h>

typedef uint16_t line_t;

// BLUETOOTH VARIABLES -----------------------------------------------------
// I don't understand these yet, it's just copied for a sample sketch


BLECharacteristic *pCharacteristic;
bool deviceConnected = false;
float txValue = 0;
const byte readPin = 4;    // analog pin G32
const byte LEDpin = 13;      // LED on pin G2
bool convert = false;
bool comReceived = false;
bool colorUpdate = false;
String moduleSelect = "";

String rxString = "";
String colorVal = "";
std::string rxValue;       // rxValue gathers input data
unsigned long nextPing = 0;
unsigned long lastPing = 0;


//  POI BLADE VARIABLES ----------------------------------------------------

const byte buttonPin = 12;
unsigned long buttonPress = 0; // to check if the momentary switch has been pressed
unsigned long buttonTimer = 0; // to track the time at which the momentary switch is pressed
unsigned long buttonHold = 0; // to track how long the switch has been held
bool bladePower = false; // to determine if LEDs should be powered up or not
bool powerToggle = false;  // debounce variable to prevent other actions if the blade has just been turned on or off
bool stillPressed = false; // debounce variable to determine if momentary switch is still pressed or is a new press
bool bladeMode = true;  // sets blade in either POI mode (true) or monocolor mode (false);


// LIGHT BLADE VARIABLES ---------------------------------------------------

int NUM_LEDS = 102;

#define PULSE_ALLOW 1       // blade pulsation (1 - allow, 0 - disallow)
#define PULSE_AMPL 20       // pulse amplitude
#define PULSE_DELAY 30      // delay between pulses

unsigned long PULSE_timer;
float k = 0.2;
uint8_t redVal = 0;
uint8_t greenVal = 0;
uint8_t blueVal = 255;
byte redOffset, greenOffset, blueOffset;
int PULSEOffset;


// CONFIGURABLE STUFF ------------------------------------------------------

#include "graphics.h" // Graphics data is contained in this header file.
// It's generated using the 'convert.py' Python script.  Various image
// formats are supported, trading off color fidelity for PROGMEM space.
// Handles 1-, 4- and 8-bit-per-pixel palette-based images, plus 24-bit
// truecolor.  1- and 4-bit palettes can be altered in RAM while running
// to provide additional colors, but be mindful of peak & average current
// draw if you do that!  Power limiting is normally done in convert.py
// (keeps this code relatively small & fast).

// Ideally you use hardware SPI as it's much faster, though limited to
// specific pins.  If you really need to bitbang DotStar data & clock on
// different pins, optionally define those here:
//#define LED_DATA_PIN  0
//#define LED_CLOCK_PIN 1

// Empty and full thresholds (millivolts) used for battery level display:
#define BATT_MIN_MV 3350 // Some headroom over battery cutoff near 2.9V
#define BATT_MAX_MV 4000 // And little below fresh-charged battery near 4.1V

boolean autoCycle = false; // Set to true to cycle images by default
uint32_t CYCLE_TIME = 12; // Time, in seconds, between auto-cycle images

#if defined(LED_DATA_PIN) && defined(LED_CLOCK_PIN)
// Older DotStar LEDs use GBR order.  If colors are wrong, edit here.
Adafruit_DotStar strip = Adafruit_DotStar(NUM_LEDS,
                         LED_DATA_PIN, LED_CLOCK_PIN, DOTSTAR_BGR);
#else
Adafruit_DotStar strip = Adafruit_DotStar(NUM_LEDS, DOTSTAR_BGR);
#endif


// UART service UUID data
#define SERVICE_UUID           "6E400004-B5A3-F393-E0A9-E50E24DCCA9E"
#define CHARACTERISTIC_UUID_RX "6E400005-B5A3-F393-E0A9-E50E24DCCA9E"
#define CHARACTERISTIC_UUID_TX "6E400006-B5A3-F393-E0A9-E50E24DCCA9E"

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
      if ((millis() - lastPing) >= 10) {   // non-interrupting delay to prevent bluetooth stack from going into congestion if too many packets are sent
        String rxValue = pCharacteristic->getValue();
        moduleSelect = rxValue[1];
        Serial.print("moduleSelect: ");
        Serial.println(moduleSelect);
        if (moduleSelect == "B") {
          convert = true;
          Serial.println(" ");
          Serial.print("Received data: ");
          rxString = "";
          for (int i = 0; i < rxValue.length(); i++) {
            Serial.print(rxValue[i]);
            rxString = rxString + rxValue[i]; // build string from received data
          }
        }
        else if (moduleSelect == "C") {
          for (int i = 0; i < rxValue.length(); i++) {
            Serial.print(rxValue[i]);
          }
          colorUpdate = true;
          redVal = rxValue[2];

          Serial.print("Red: ");
          Serial.println(redVal);
          greenVal = rxValue[3];

          Serial.print("Green: ");
          Serial.println(greenVal);
          blueVal = rxValue[4];

          Serial.print("Blue: ");
          Serial.println(blueVal);
        }
        lastPing = millis();
      }
    }
};


void     imageInit(void),
         buttonInterrupt(void);
uint16_t readVoltage(void);


// GLOBAL STATE STUFF ------------------------------------------------------

uint32_t lastImageTime = 0L, // Time of last image change
         lastLineTime  = 0L;
uint8_t  imageNumber   = 0,  // Current image being displayed
         imageType,          // Image type: PALETTE[1,4,8] or TRUECOLOR
         *imagePalette,       // -> palette data in PROGMEM
         *imagePixels,        // -> pixel data in PROGMEM
         palette[16][3];     // RAM-based color table for 1- or 4-bit images
line_t   imageLines,         // Number of lines in active image
         imageLine;          // Current line number in image
// volatile uint16_t irCode = BTN_NONE; // Last valid IR code received

const uint8_t PROGMEM brightness[] = { 15, 31, 63, 127, 255 };
uint8_t bLevel = 1; // index into brightness[] -- 0 = dimmest, 4 = full brightness (was defaulting to 4/255 = full)

// Microseconds per line for various speed settings
const uint16_t PROGMEM lineTable[] = { // 375 * 2^(n/3)
  1000000L /  375, // 375 lines/sec = slowest
  1000000L /  472,
  1000000L /  595,
  1000000L /  750, // 750 lines/sec = mid
  1000000L /  945,
  1000000L / 1191,
  1000000L / 1500  // 1500 lines/sec = fastest
};
uint8_t  lineIntervalIndex = 3;
uint16_t lineInterval      = 1000000L / 750;


// POIBlade Subroutines ----------------------------------------------------

void imageInit() { // Initialize global image state for current imageNumber
  imageType    = images[imageNumber].type;
  imageLines   = images[imageNumber].lines;
  imageLine    = 0;
  imagePalette = (uint8_t *)images[imageNumber].palette;
  imagePixels  = (uint8_t *)images[imageNumber].pixels;
  // 1- and 4-bit images have their color palette loaded into RAM both for
  // faster access and to allow dynamic color changing.  Not done w/8-bit
  // because that would require inordinate RAM (328P could handle it, but
  // I'd rather keep the RAM free for other features in the future).
  if (imageType == PALETTE1)      memcpy_P(palette, imagePalette,  2 * 3);
  else if (imageType == PALETTE4) memcpy_P(palette, imagePalette, 16 * 3);
  lastImageTime = millis(); // Save time of image init for next auto-cycle
}

void nextImage(void) {
  if (++imageNumber >= NUM_IMAGES) imageNumber = 0;
  imageInit();
}

void prevImage(void) {
  imageNumber = imageNumber ? imageNumber - 1 : NUM_IMAGES - 1;
  imageInit();
}

// LightBlade Subroutines -------------------------------------------------

void light_up() {
  for (char i = 0; i <= (NUM_LEDS - 1); i++) {
    strip.setPixelColor(i, redVal, greenVal, blueVal);
    strip.show();
    //    delay(5);
  }
}

void light_down() {
  for (char i = 0; i <= (NUM_LEDS - 1); i++) {
    strip.setPixelColor((NUM_LEDS - 1 - i), 0, 0, 0);
    strip.show();
    //    delay(5);
  }
}

void changeColor() {
  moduleSelect = "";
  if (bladeMode == false) {
    for (char i = 0; i <= (NUM_LEDS - 1); i++) {
      strip.setPixelColor(i, redVal, greenVal, blueVal);
    }
    if (bladePower == true) {
      strip.show();
    }
  }
  colorUpdate = false;
}

void randomPULSE() {
  if (PULSE_ALLOW && bladePower && (millis() - PULSE_timer > PULSE_DELAY)) {
    PULSE_timer = millis();
    PULSEOffset = PULSEOffset * k + random(-PULSE_AMPL, PULSE_AMPL) * (1 - k);
    PULSEOffset = constrain(PULSEOffset, -15, 5);
    redOffset = constrain(redVal + PULSEOffset, 0, 255);
    greenOffset = constrain(greenVal + PULSEOffset, 0, 255);
    blueOffset = constrain(blueVal + PULSEOffset, 0, 255);
    for (char i = 0; i <= (NUM_LEDS - 1); i++) {
      strip.setPixelColor (i, redOffset, greenOffset, blueOffset);
    }
    strip.show();
  }
}


// GLOBAL SUBROUTINES -----------------------------------------------------


void buttonInterrupt() {
  buttonPress = digitalRead(buttonPin);
  if (buttonPress == LOW) {
    if (stillPressed == false) {
      stillPressed = true;
      buttonTimer = millis();
    }
    else {
      if (powerToggle == false) {
        buttonHold = millis();
        Serial.println(buttonHold);
        Serial.println(buttonTimer);
        if (buttonHold - buttonTimer > 2000) {
          bladePower = !bladePower;
          Serial.println("Power Toggled");
          powerToggle = true;
          if (bladePower == true) {
            Serial.println("Blade on");
            if (bladeMode == true) {
              imageInit();   // Initialize pointers for default image
            }
            else {
              light_up();
            }
          }
          else {
            Serial.println("Blade off");
            if (bladeMode == true) {
              strip.clear();
              strip.show();
            }
            else {
              light_down();
            }
          }
        }
      }
    }
  }
  else {
    if (bladePower == true && powerToggle == false && stillPressed == true && bladeMode == true) {
      Serial.println("Next Image!");
      nextImage();
    }
    stillPressed = false;
    powerToggle = false;
  }
}

// BLUETOOTH SUBROUTINES ---------------------------------------------------------

void convertControlPad() {

  // Control Pad button interpreter
  convert = false;
  moduleSelect = "";
  Serial.print("rxString:");
  Serial.println(rxString);
  if (rxString == "!B11:") Serial.println("************ Start Action 1");
  else if (rxString == "!B219") Serial.println("********** Start Action 2");
  else if (rxString == "!B318") Serial.println("********** Start Action 3");
  else if (rxString == "!B417") Serial.println("********** Start Action 4");
  else if (rxString == "!B516") {
    Serial.println("Blade Mode Toggled");
    bladeMode = !bladeMode;
    if (bladePower == true) {
      strip.clear();
      strip.show();
      if (bladeMode == true) {
        imageInit();
      }
      else {
        light_up();
      }
    }
  }
  else if (rxString == "!B615") Serial.println("********** Start Action DOWN");
  else if (rxString == "!B714") {
    if (bladeMode == true && bladePower == true) {
      Serial.println("Previous Image");
      prevImage();
    }
  }
  else if (rxString == "!B813") {
    if (bladeMode == true && bladePower == true) {
      Serial.println("Next Image");
      nextImage();
    }
  }
  else if (rxString == "!B10;") Serial.println("********** Stop Action 1");
  else if (rxString == "!B20:") Serial.println("********** Stop Action 2");
  else if (rxString == "!B309") Serial.println("********** Stop Action 3");
  else if (rxString == "!B408") Serial.println("********** Stop Action 4");
  else if (rxString == "!B507") {
    Serial.println("Release BladeMode Button");

  }
  else if (rxString == "!B606") Serial.println("********** Stop Action DOWN");
  else if (rxString == "!B705") {
    Serial.println("Release Prev Image Button");

  }
  else if (rxString == "!B804") {
    Serial.println("Release Next Image Button");

  }
  rxString = "";

}

void ble_setup() {

  BLEDevice::init("YeeeahPOI-Stu"); // give the BLE device a name

  BLEServer *pServer = BLEDevice::createServer(); // create BLE server
  pServer->setCallbacks(new MyServerCallbacks());

  // Create the BLE Service
  BLEService *pService = pServer->createService(SERVICE_UUID);

  // Create a BLE Characteristic
  pCharacteristic = pService->createCharacteristic(
                      CHARACTERISTIC_UUID_TX,
                      BLECharacteristic::PROPERTY_NOTIFY);
  pCharacteristic->addDescriptor(new BLE2902());
  BLECharacteristic *pCharacteristic = pService->createCharacteristic(
                                         CHARACTERISTIC_UUID_RX,
                                         BLECharacteristic::PROPERTY_WRITE);
  pCharacteristic->setCallbacks(new MyCallbacks());

  pService->start(); // start the service

  pServer->getAdvertising()->start(); // start advertising
  Serial.println("Waiting a client connection to notify...");
  Serial.println(" ");
}


void ble_loop() {

  if (deviceConnected) {
    //  txValue = analogRead(readPin);        // pick up value on readPin
    //  char txString[8];                     // convert the value to a char array
    //  dtostrf(txValue, 1, 2, txString); // float_val, min_width, decimal_digits, char_buffer
    //  pCharacteristic->setValue(&txValue, 1); // to send the integer value
    //  pCharacteristic->setValue("Hello!");    // to send a test message
    //  pCharacteristic->setValue(txString);  // prepare to send array
    //  if (digitalRead(buttonPin) == LOW) {     // send when button is pressed
    //    pCharacteristic->notify();          // send the value to the app!
    //    pCharacteristic->setValue(" ");     // send a space
    //    pCharacteristic->notify();
    //      digitalWrite(LEDpin, HIGH);         // switch on the LED
    //    Serial.println(" ");
    //    Serial.print("*** Peripheral sends: ");
    //    Serial.print(txString);             // report value on serial line
    //    Serial.println(" ***");
    //  }
    //    else digitalWrite(LEDpin, LOW);
    if (convert == true) convertControlPad();
    if (colorUpdate == true) changeColor();
  }
}

// disconnecting
//    if (!deviceConnected && oldDeviceConnected) {
//      nextPing = millis();
//      if ((nextPing - lastPing) >= 500) {  // non-interrupting delay to give the bluetooth stack the chance to get things ready
//        pServer->startAdvertising(); // restart advertising
//        Serial.println("start advertising");
//        oldDeviceConnected = deviceConnected;
//      }
//    }
// connecting
//    if (deviceConnected && !oldDeviceConnected) {
// do stuff here on connecting
//        oldDeviceConnected = deviceConnected;
//    }
//}



void setup() {
  Serial.begin(115200);
  pinMode(buttonPin, INPUT_PULLUP);
  //  pinMode(LEDpin, OUTPUT);
  strip.begin(); // Allocate DotStar buffer, init SPI
  strip.setBrightness(pgm_read_byte(&brightness[bLevel])); // scale overall strip brightness down (DotStar global brightness scaler)
  strip.clear(); // Make sure strip is clear
  strip.show();  // before measuring battery

  ble_setup();  // start Bluetooth service
}



// MAIN LOOP ---------------------------------------------------------------

void loop() {
  uint32_t t = millis(); // Current time, milliseconds
  ble_loop();
  buttonInterrupt();
  if (bladePower == true) {
    if (bladeMode == true) {
      if (autoCycle) {
        if ((t - lastImageTime) >= (CYCLE_TIME * 1000L)) nextImage();
        // CPU clocks vary slightly; multiple poi won't stay in perfect sync.
        // Keep this in mind when using auto-cycle mode, you may want to cull
        // the image selection to avoid unintentional regrettable combinations.
      }

      // Transfer one scanline from pixel data to LED strip:

      switch (imageType) {

        case PALETTE1: { // 1-bit (2 color) palette-based image
            uint8_t  pixelNum = 0, byteNum, bitNum, pixels, idx,
                     *ptr = (uint8_t *)&imagePixels[imageLine * NUM_LEDS / 8];
            for (byteNum = NUM_LEDS / 8; byteNum--; ) { // Always padded to next byte
              pixels = *ptr++;                       // 8 pixels of data (pixel 0 = LSB)
              for (bitNum = 8; bitNum--; pixels >>= 1) {
                idx = pixels & 1; // Color table index for pixel (0 or 1)
                strip.setPixelColor(pixelNum++,
                                    palette[idx][0], palette[idx][1], palette[idx][2]);
              }
            }
            break;
          }

        case PALETTE4: { // 4-bit (16 color) palette-based image
            uint8_t  pixelNum, p1, p2,
                     *ptr = (uint8_t *)&imagePixels[imageLine * NUM_LEDS / 2];
            for (pixelNum = 0; pixelNum < NUM_LEDS; ) {
              p2  = *ptr++;  // Data for two pixels...
              p1  = p2 >> 4; // Shift down 4 bits for first pixel
              p2 &= 0x0F;    // Mask out low 4 bits for second pixel
              strip.setPixelColor(pixelNum++,
                                  palette[p1][0], palette[p1][1], palette[p1][2]);
              strip.setPixelColor(pixelNum++,
                                  palette[p2][0], palette[p2][1], palette[p2][2]);
            }
            break;
          }

        case PALETTE8: { // 8-bit (256 color) PROGMEM-palette-based image
            uint16_t  o;
            uint8_t   pixelNum,
                      *ptr = (uint8_t *)&imagePixels[imageLine * NUM_LEDS];
            for (pixelNum = 0; pixelNum < NUM_LEDS; pixelNum++) {
              o = *ptr++ * 3; // Offset into imagePalette
              strip.setPixelColor(pixelNum,
                                  imagePalette[o],
                                  imagePalette[o + 1],
                                  imagePalette[o + 2]);
            }
            break;
          }

        case TRUECOLOR: { // 24-bit ('truecolor') image (no palette)
            uint8_t  pixelNum, r, g, b,
                     *ptr = (uint8_t *)&imagePixels[imageLine * NUM_LEDS * 3];
            for (pixelNum = 0; pixelNum < NUM_LEDS; pixelNum++) {
              r = *ptr++;
              g = *ptr++;
              b = *ptr++;
              strip.setPixelColor(pixelNum, r, g, b);
            }
            break;
          }
      }

      if (++imageLine >= imageLines) imageLine = 0; // Next scanline, wrap around

      strip.show(); // Refresh LEDs
      lastLineTime = t;
    }
    else {
      delay(10);
      randomPULSE();
    }
  }
}
