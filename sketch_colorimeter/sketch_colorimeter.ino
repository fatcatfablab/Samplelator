/***************************************************************************
 * This is based on the example from Adafruit AS726x library.
 * We use an array of 6 neopixels to show the history of valid color values (above noise).
 * There is also a IR "line break" detector analog input.
 * The color detection algorithm is tuned for our sample swatches.
 ***************************************************************************/
#include <Adafruit_AS726x.h>
#include <Wire.h>
#include <Adafruit_NeoPixel.h>

// Parameter 1 = number of pixels in strip
// Parameter 2 = pin number (most are valid)
// Parameter 3 = pixel type flags, add together as needed:
//   NEO_KHZ800  800 KHz bitstream (most NeoPixel products w/WS2812 LEDs)
//   NEO_KHZ400  400 KHz (classic 'v1' (not v2) FLORA pixels, WS2811 drivers)
//   NEO_GRB     Pixels are wired for GRB bitstream (most NeoPixel products)
//   NEO_RGB     Pixels are wired for RGB bitstream (v1 FLORA pixels, not v2)
// Adafruit_NeoPixel strip = Adafruit_NeoPixel(60, PIN, NEO_GRB + NEO_KHZ800);
#define NEOPIX_PIN 2
#define NEOPIX_ARRAY 6
Adafruit_NeoPixel strip = Adafruit_NeoPixel(NEOPIX_ARRAY, NEOPIX_PIN, NEO_GRB + NEO_KHZ800);

// neopixels "color history"
uint32_t neop[NEOPIX_ARRAY];

// This is the AMS7262 sensor
// Note: I2C address is fixed at 0x49 using standard Wire (SCL/SDA) pins for communications.
Adafruit_AS726x ams;

// For external "light break" input. Note: analog pin!
#define THRESHOLD_ANALOG_PIN 0

//Threshold range of an obstacle on light break detector
int pdiodeThreshold = 200;

// Minimum color channel value to consider above noise
#define CHAN_THRESH 6

// TEST TEST TEST: uncomment the following not wait for start/stop signal
#define AUTOTRIGGER

// TEST TEST TEST: uncomment to display sort by strongest color value
#define ORDER_BY_VALUE

// buffer to hold sensor color values
uint16_t senseVals[AS726x_NUM_CHANNELS];

// color calculator variables
uint16_t totRgb;
uint8_t neoRed, neoGreen, neoBlue;
int totSpectrum, meanSpectrum, numVariance, stdDevSpectrum;

// State machine for loop processing
enum sensingStates {
  WAIT_START,
  INITIATE_ONE_SHOT,
  WAIT_END,
  SEND_REPORT
};

// jumpstart state machine
int readSensorState = WAIT_START;

// current color for neopixel position 0
uint32_t npixNow;

// value order or color sensors (changes per sample calculated)
int orderColors[6] = {
  AS726x_VIOLET, AS726x_BLUE, AS726x_GREEN, AS726x_YELLOW, AS726x_ORANGE, AS726x_RED
};

// calculate some basic statistics about the current color sample
void calcSampleStats() {
  totSpectrum = 0;
  for (int idx = 0; idx < 6; idx++) {
    totSpectrum += senseVals[orderColors[idx]];
  }
  meanSpectrum = totSpectrum / 6;
  numVariance = 0;
  for (int idx = 0; idx < 6; idx++) {
    numVariance += sq(meanSpectrum - senseVals[orderColors[idx]]);
  }
  stdDevSpectrum = sqrt(numVariance / 5); // 5 not 6 cause it's a sample

  // sort colors by value
  for (int idx = 0; idx < 5; idx++) {
    for (int jdx = idx + 1; jdx < 6; jdx++ ) {
      if (senseVals[orderColors[idx]] < senseVals[orderColors[jdx]]) {
        int tmpColor = orderColors[idx];
        orderColors[idx] = orderColors[jdx];
        orderColors[jdx] = tmpColor;
      }
    }
  }
}


// Report is printed to serial monitor
void printJsonVals(char *colorInd) {
#ifdef ORDER_BY_VALUE
// NOTE: requires stats to have been run
  Serial.print("{ z:3");
  for (int idx = 0; idx < 6; idx++) {
    switch(orderColors[idx]) {
      case AS726x_VIOLET:
        Serial.print(", v:"); Serial.print(senseVals[AS726x_VIOLET]);
        break;
      case AS726x_BLUE:
        Serial.print(", b:"); Serial.print(senseVals[AS726x_BLUE]);
        break;
      case AS726x_GREEN:
        Serial.print(", g:"); Serial.print(senseVals[AS726x_GREEN]);
        break;
      case AS726x_YELLOW:
        Serial.print(", y:"); Serial.print(senseVals[AS726x_YELLOW]);
        break;
      case AS726x_ORANGE:
        Serial.print(", o:"); Serial.print(senseVals[AS726x_ORANGE]);
        break;
      case AS726x_RED:
        Serial.print(", r:"); Serial.print(senseVals[AS726x_RED]);
        break;
      default:
        break;
    }
  }
  Serial.print(", x:"); Serial.print(colorInd);
  Serial.println(" }");
#else
  // Just report the color
  Serial.print("{ z:2");
  Serial.print(", x:"); Serial.print(colorInd);
  Serial.println(" }");
#endif

}

// Update neopixes using only the Red, Green, Blue components
void updateNeopixels() {    
  totRgb = senseVals[AS726x_RED] + senseVals[AS726x_GREEN] + senseVals[AS726x_BLUE];
  neoRed = 255 * senseVals[AS726x_RED] / totRgb;
  neoGreen = 255 * senseVals[AS726x_GREEN] / totRgb;
  neoBlue = 255 * senseVals[AS726x_BLUE] / totRgb;

  npixNow = strip.Color(neoRed, neoGreen, neoBlue);
   
  strip.setPixelColor(0, npixNow);
  
  for (int idx = 0; idx < 5; idx++) {
     neop[5 - idx] = neop[4 - idx];
  }
  neop[0] = npixNow;
  for (int idx = 0; idx < 6; idx++) {
    strip.setPixelColor(idx, neop[idx]);
  }
  strip.show();
}

// Calculate the matching color (note: tuned for our sample swatches)
void calcColorMatch() {
  if (stdDevSpectrum < 3) {
    // guess at a gray or black color
#ifndef AUTOTRIGGER
    printJsonVals(".");
#endif
    return;
  }
   
  switch (orderColors[0]) {
    case AS726x_VIOLET:
      if (orderColors[1] == AS726x_BLUE) {
        printJsonVals("Blue");
      } else if (orderColors[1] == AS726x_ORANGE) {
        printJsonVals("Violet");
      } else if (orderColors[1] == AS726x_GREEN) {
        printJsonVals("Blue");
      } else if (senseVals[AS726x_BLUE] > senseVals[AS726x_ORANGE]) {
        printJsonVals("blue");
      } else {
        printJsonVals("violet");
      }
      break;
    case AS726x_BLUE:
      if (orderColors[1] == AS726x_VIOLET) {
        printJsonVals("Blue");
      } else if (orderColors[1] == AS726x_GREEN) {
        printJsonVals("green");
      } else {
        printJsonVals("blue");
      }
      break;
    case AS726x_GREEN:
      if (orderColors[1] == AS726x_YELLOW) {
        if (senseVals[AS726x_ORANGE] > senseVals[AS726x_BLUE]) {
          printJsonVals("lime"); 
        } else {
          printJsonVals("Green");
        }
      } else {
        printJsonVals("green");
      }
      break;
    case AS726x_YELLOW:
      if (senseVals[AS726x_YELLOW] - senseVals[orderColors[1]] < 4) {
        if (senseVals[AS726x_YELLOW] - senseVals[orderColors[2]] < 4) {
          printJsonVals("Beige"); 
        } else {
          printJsonVals("yellow");
        }
      } else {
        printJsonVals("Yellow");
      }
      break;
    case AS726x_ORANGE:
      if (orderColors[1] == AS726x_YELLOW) {
        if (senseVals[AS726x_GREEN] > senseVals[AS726x_RED]) {
          printJsonVals("yellow");
        } else {
          printJsonVals("Orange");
        }
      } else if (orderColors[1] == AS726x_RED) {
        printJsonVals("Red");
      } else {
        printJsonVals("orange");
      }
      break;
    case AS726x_RED:
      printJsonVals("Red");
      break;
   }
}


void setup() {
  // Set up serial monitor
  Serial.begin(115200);
  while(!Serial);
  Serial.print("Configuring...");
  // Set up neopixels
  strip.begin();
  // Start with reference colors
  strip.setPixelColor(0, strip.Color(255, 0, 0)); // Red
  strip.setPixelColor(1, strip.Color(0, 255, 0)); // Green
  strip.setPixelColor(2, strip.Color(0, 0, 255)); // Blue
  strip.setPixelColor(3, strip.Color(255, 255, 0)); // Yellow
  strip.setPixelColor(4, strip.Color(255, 64, 0)); // Orange
  strip.setPixelColor(5, strip.Color(255, 0, 255)); // Violet
  strip.show();

  // initialize digital pin LED_BUILTIN as an output indicator
  // pinMode(LED_BUILTIN, OUTPUT);

  //begin and make sure we can talk to the sensor
  for(; !ams.begin();) {
    Serial.println("could not connect to sensor! Please check your wiring.");
    delay(30000);
  }
  ams.setGain(GAIN_64X); // require high gain for short integration times
  // ams.setGain(GAIN_16X);
  ams.setIntegrationTime(2); // This would be N * 5.6ms since filling both banks
  ams.setConversionType(ONE_SHOT); // doing one-shot mode
  ams.drvOn(); // built in sensor LED light on
  
  delay(1000);

  // OK, clear out reference colors and start the show
  for (int idx = 0; idx < 6; idx++) {
    strip.setPixelColor(idx, strip.Color(0,0,0));
  }
  strip.show();

  Serial.println("Waiting for start sample...");  
}


void loop() {
  if (readSensorState == WAIT_START) {
#ifndef AUTOTRIGGER
    // if under threshold, keep looking
    if (analogRead(THRESHOLD_ANALOG_PIN) < pdiodeThreshold) {
      return;
    }
#endif
    readSensorState = INITIATE_ONE_SHOT;
    // Serial.println("start");
  } // fall thru!
   
  if (readSensorState == INITIATE_ONE_SHOT) {
    ams.startMeasurement(); // initiate one shot color sensing
    readSensorState = WAIT_END;
    // Serial.println("wait end...");
  } // fall thru!
  
  if (readSensorState == WAIT_END) {
    // if over threshold, keep looking
#ifndef AUTOTRIGGER
    if (analogRead(THRESHOLD_ANALOG_PIN) > pdiodeThreshold) {
      return;
    }
#endif
    readSensorState = SEND_REPORT;
    // Serial.println("report now"); // fall thru!
  } // fall thru!


  if (readSensorState == SEND_REPORT) {
    // generate report after sensor ready to be read
    if (ams.dataReady()) {
      // Read the actual color sensor values
      ams.readRawValues(senseVals);
      // Calculate the color and report
      calcSampleStats();
      calcColorMatch();
      updateNeopixels();
      readSensorState = WAIT_START;
    } // FIXME:may need a timeout counter here
  } 
}
