/***************************************************************************
 * This is based on the example from Adafruit AS726x library on Teensy 3.6.
 * This version **does not** use neopixels as those need 5 volts.
 * This uses the IR "line break" detector analog input.
 * The color detection algorithm is tuned for our sample swatches.
 ***************************************************************************/
enum midiMap {
 blueMidi = 36,
 greenMidi = 40,
 redMidi = 42,
  magentaMidi = 46,
 violetMidi = 48,
 yellowMidi = 50
}; 


#include <Wire.h>
#include <Adafruit_AS726x.h>

// This is the AMS7262 sensor
// Note: I2C address is fixed at 0x49 using standard Wire (SCL/SDA) pins for communications.
// Adafruit_AS726x ams;
Adafruit_AS726x firstAms, secondAms, thirdAms;

// Data ready flags for each sensor for state machine sequencing
bool firstReady, secondReady, thirdReady;
const char *lastColor[3];
int lastNote[3];

// For external "light break" input. Note: analog pin!
#define THRESHOLD_ANALOG_PIN 0

// Threshold limits of an obstacle on light break detector
int lowThreshold = 150;
int highThreshold = 500;

// TEST TEST TEST: uncomment the following not wait for start/stop signal
// #define AUTOTRIGGER

// TEST TEST TEST: uncomment one of the following for display to serial options
#define ORDER_BY_VALUE
// #define RGB_REPORT
// #define SHORT_COLOR_REPORT

// TEST TEST TEST: uncomment for single sensor testing
#define SINGLE_SENSOR_ONLY

// buffer to hold sensor color values
uint16_t firstSenseVals[AS726x_NUM_CHANNELS];
uint16_t secondSenseVals[AS726x_NUM_CHANNELS];
uint16_t thirdSenseVals[AS726x_NUM_CHANNELS];

// color calculator variables
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


// value order or color sensors (changes per sample calculated)
int orderColors[6] = {
  AS726x_VIOLET, AS726x_BLUE, AS726x_GREEN, AS726x_YELLOW, AS726x_ORANGE, AS726x_RED
};

// calculate some basic statistics about the current color sample
// The std dev value is also used to filter out white/black/grey colors
void calcSampleStats(uint16_t *senseVals) {
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
void printJsonVals(const char *colorInd, int sensorId, uint16_t senseVals[6]) {
#ifdef ORDER_BY_VALUE
// NOTE: requires stats to have been run
  Serial.print("{ id:"); Serial.print(sensorId);
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
#endif // ORDER_BY_VALUE

#ifdef RGB_REPORT
  uint16_t totRgb = senseVals[AS726x_RED] + senseVals[AS726x_GREEN] + senseVals[AS726x_BLUE];
  totRgb = totRgb == 0 ? 1 : totRgb;
  uint8_t neoRed = 255 * senseVals[AS726x_RED] / totRgb;
  uint8_t neoGreen = 255 * senseVals[AS726x_GREEN] / totRgb;
  uint8_t neoBlue = 255 * senseVals[AS726x_BLUE] / totRgb;
 
  Serial.print("{ id:"); Serial.print(sensorId);
  Serial.print(", r:"); Serial.print(neoRed);
  Serial.print(", g:"); Serial.print(neoGreen);
  Serial.print(", b:"); Serial.print(neoBlue);
  Serial.print(", x:"); Serial.print(colorInd);
  Serial.println(" }");
#endif

#ifdef SHORT_COLOR_REPORT
  // Just report the color
  Serial.print("{ id:"); Serial.print(sensorId);
  Serial.print(", x:"); Serial.print(colorInd);
  Serial.println(" }");
#endif  // SHORT_COLOR_REPORT
}


void sendMidiOn(const char *colorInd, int useNote, int sensorId, uint16_t senseVals[6]) {
  printJsonVals(colorInd, sensorId, senseVals);
  if (sensorId < 1) {
    Serial.print("Oops, sensorId is less than 1");
    return;
  }
  lastColor[sensorId - 1] = colorInd;
  lastNote[sensorId - 1] = useNote;
  if (useNote > 0)
    usbMIDI.sendNoteOn(lastNote[sensorId - 1], 99, sensorId);
}

void sendMidiOff(int sensorId) {
  if (sensorId < 1) {
    Serial.print("Oops, last sensor Id was less than 1");
    return;
  }
  lastColor[sensorId - 1] = "";
  if (lastNote[sensorId - 1] > 0)
    usbMIDI.sendNoteOff(lastNote[sensorId - 1], 0, sensorId);
}


// Calculate the matching color (note: tuned for our sample swatches)
void calcColorMatch(int sensorId, uint16_t senseVals[6]) {
  if (stdDevSpectrum < 6) {
    // guess at a gray or black color
#ifndef AUTOTRIGGER
    sendMidiOn(".", 0, sensorId, senseVals);
#endif
    return;
  }
   if (senseVals[orderColors[0]] < 75) {
    sendMidiOn(".", 0, sensorId, senseVals);
    return; 
   }
    
  switch (orderColors[0]) {
    case AS726x_VIOLET:
      if (orderColors[1] == AS726x_BLUE) {
        sendMidiOn("Blue", blueMidi, sensorId, senseVals);
      } else if (orderColors[1] == AS726x_ORANGE) {
        if (senseVals[AS726x_YELLOW] < senseVals[AS726x_RED]) {
          sendMidiOn("Magenta", magentaMidi, sensorId, senseVals);
          Serial.println(millis());
        } else {
          sendMidiOn("Violet", violetMidi, sensorId, senseVals);
        }
      } else if (orderColors[1] == AS726x_GREEN) {
       //  sendMidiOn("Cyan", 74, sensorId, senseVals);
         sendMidiOn("Blue", blueMidi, sensorId, senseVals);
      } else {
        sendMidiOn(".", 0, sensorId, senseVals);
      }
      break;
    case AS726x_BLUE:
      if (orderColors[1] == AS726x_VIOLET) {
        sendMidiOn("Blue", blueMidi, sensorId, senseVals);
      } else {
        sendMidiOn(".", 0, sensorId, senseVals);
      }
      break;
    case AS726x_GREEN:
      if (orderColors[1] == AS726x_YELLOW && orderColors[2] == AS726x_ORANGE) {
        sendMidiOn(".", 0, sensorId, senseVals);
      } else if (orderColors[1] == AS726x_YELLOW && orderColors[2] == AS726x_VIOLET) {
        sendMidiOn(".", 0, sensorId, senseVals);
      } else if (orderColors[1] == AS726x_ORANGE) {
        sendMidiOn(".", 0, sensorId, senseVals);
      } else {
        sendMidiOn("Green", greenMidi, sensorId, senseVals);
      }
      break;
    case AS726x_YELLOW:
      if (orderColors[2] == AS726x_VIOLET || orderColors[2] == AS726x_BLUE || 
          orderColors[2] == AS726x_RED) {
            sendMidiOn(".", 0, sensorId, senseVals);
      } else {
         sendMidiOn(".", 0, sensorId, senseVals);
      }
      break;
    case AS726x_ORANGE:
      if (orderColors[1] == AS726x_GREEN && orderColors[2] == AS726x_YELLOW) { 
          sendMidiOn("Yellow", yellowMidi, sensorId, senseVals);
      } else if (orderColors[1] == AS726x_VIOLET) {    
         sendMidiOn("Magenta", magentaMidi, sensorId, senseVals);
         Serial.println(millis());
      } else if (orderColors[1] == AS726x_RED && orderColors[2] == AS726x_YELLOW) {
        sendMidiOn("Red", redMidi, sensorId, senseVals);
      } else if (orderColors[1] == AS726x_YELLOW) {
        if (senseVals[AS726x_GREEN] > senseVals[AS726x_RED]) {
          sendMidiOn("Yellow", yellowMidi, sensorId, senseVals);
      } else {
          // sendMidiOn("Orange", 68, sensorId, senseVals);
          sendMidiOn(".", 0, sensorId, senseVals);
        }
      } else {
        sendMidiOn(".", 0, sensorId, senseVals);
        // sendMidiOn("orange", 67, sensorId, senseVals);
      }
      break;
    case AS726x_RED:
      sendMidiOn("Red", redMidi, sensorId, senseVals);
      break;
   }
}

void setup() {
  // Set up serial monitor
  Serial.begin(115200);
//  while(!Serial);

  // initialize digital pin LED_BUILTIN as an output indicator
  pinMode(LED_BUILTIN, OUTPUT);
  
  // Wire1 is CLK=19, DATA=18
  // Wire2 is CLCK=37, DATA=38
  // Wire3 is CLK=3, DATA=4

  //begin and make sure we can talk to the sensors
#ifndef SINGLE_SENSOR_ONLY  
  for(; !thirdAms.begin(&Wire2);) {
    Serial.println("could not connect to third sensor! Please check your wiring.");
    delay(30000);
  }
  thirdAms.setGain(GAIN_64X); // require high gain for short integration times
  thirdAms.setIntegrationTime(2); // This would be N * 5.6ms since filling both banks
  thirdAms.setConversionType(ONE_SHOT); // doing one-shot mode

  for(; !secondAms.begin(&Wire1);) {
    Serial.println("could not connect to second sensor! Please check your wiring.");
    delay(30000);
  }
  secondAms.setGain(GAIN_64X); // require high gain for short integration times
  secondAms.setIntegrationTime(1); // This would be N * 5.6ms since filling both banks
  secondAms.setConversionType(ONE_SHOT); // doing one-shot mode
#endif // SINGLE_SENSOR_ONLY

  for(; !firstAms.begin(&Wire);) {
    Serial.println("could not connect to first sensor! Please check your wiring.");
    delay(30000);
  }
  firstAms.setGain(GAIN_64X); // require high gain for short integration times
  firstAms.setIntegrationTime(1); // This would be N * 5.6ms since filling both banks
  firstAms.setConversionType(ONE_SHOT); // doing one-shot mode

  firstAms.drvOn();
#ifndef SINGLE_SENSOR_ONLY
  secondAms.drvOn();
  thirdAms.drvOn();  
  delay(1000);
#endif //SINGLE_SENSOR_ONLY

  Serial.println("Waiting for start sample...");  
}

void loop() {
  int triggerValue;
  
  if (readSensorState == WAIT_START) {
#ifndef AUTOTRIGGER
    triggerValue = analogRead(THRESHOLD_ANALOG_PIN);
#else
    triggerValue = lowThreshold - 1;
#endif
    // if under threshold, keep looking
    if (triggerValue < lowThreshold) {
      firstReady = false;
      sendMidiOff(1);
#ifndef SINGLE_SENSOR_ONLY
      secondReady = false;
      thirdReady = false;
      sendMidiOff(2);
      sendMidiOff(3);
#else
      secondReady = true;
      thirdReady = true;
#endif // SINGLE_SENSOR_ONLY
      readSensorState = INITIATE_ONE_SHOT;
      digitalWrite(LED_BUILTIN, LOW);
    }
  }
   
  if (readSensorState == INITIATE_ONE_SHOT) {
    // firstAms.drvOn();
    firstAms.startMeasurement(); // initiate one shot color sensing
#ifndef SINGLE_SENSOR_ONLY
    // secondAms.drvOn();
    secondAms.startMeasurement();
    // thirdAms.drvOn();
    thirdAms.startMeasurement();
#endif // SINGLE_SENSOR_ONLY
    for(;usbMIDI.read();) {
      // do nothing with incomming usb messages
    }
    readSensorState = SEND_REPORT;
  }

  if (readSensorState == SEND_REPORT) {
    // generate report after sensor ready to be read
    if ( !firstReady ) {
      firstReady = firstAms.dataReady();
      if (firstReady) {
        firstAms.readRawValues(firstSenseVals);
      }
    } else {
      Serial.println("Miss!!");
    }
#ifndef SINGLE_SENSOR_ONLY
    if ( !secondReady ) {
      secondReady = secondAms.dataReady();
      if (secondReady) {
        secondAms.readRawValues(secondSenseVals);
      }
    }
    if ( !thirdReady ) {
      thirdReady = thirdAms.dataReady();
      if (thirdReady) {
        thirdAms.readRawValues(thirdSenseVals);
      }
    }
#endif // SINGLE_SENSOR_ONLY

    if (firstReady && secondReady && thirdReady) {
        calcSampleStats(firstSenseVals);
        calcColorMatch(1, firstSenseVals);
#ifndef SINGLE_SENSOR_ONLY
        calcSampleStats(secondSenseVals);
        calcColorMatch(2, secondSenseVals);
        calcSampleStats(thirdSenseVals);
        calcColorMatch(3, thirdSenseVals);
#endif // SINGLE_SENSOR_ONLY
        readSensorState = WAIT_END;
    }
  }
  
  if (readSensorState == WAIT_END) {
#ifndef AUTOTRIGGER
    triggerValue = analogRead(THRESHOLD_ANALOG_PIN);
#else
    triggerValue = highThreshold + 1;
#endif
    if (triggerValue > highThreshold) {
      readSensorState = WAIT_START;
      digitalWrite(LED_BUILTIN, HIGH);
    }
  }
}
