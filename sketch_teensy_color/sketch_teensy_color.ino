/***************************************************************************
 * This is based on the example from Adafruit AS726x library on Teensy 3.6.
 * This version **does not** use neopixels as those need 5 volts.
 * This uses the IR "line break" detector analog input.
 * The color detection algorithm is tuned for our sample swatches.
 ***************************************************************************/
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

//Threshold range of an obstacle on light break detector
int pdiodeThreshold = 200;

// Minimum color channel value to consider above noise
#define CHAN_THRESH 6

// TEST TEST TEST: uncomment the following not wait for start/stop signal
// #define AUTOTRIGGER

// TEST TEST TEST: uncomment one of the following to display sort by strongest color value
// #define ORDER_BY_VALUE
#define SHORT_COLOR_REPORT

// TEST TEST TEST: uncomment for single sensor testing
#define SINGLE_SENSOR_ONLY

// buffer to hold sensor color values
// uint16_t senseVals[AS726x_NUM_CHANNELS];
uint16_t firstSenseVals[AS726x_NUM_CHANNELS];
uint16_t secondSenseVals[AS726x_NUM_CHANNELS];
uint16_t thirdSenseVals[AS726x_NUM_CHANNELS];

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
int readSensorState = WAIT_END;


// value order or color sensors (changes per sample calculated)
int orderColors[6] = {
  AS726x_VIOLET, AS726x_BLUE, AS726x_GREEN, AS726x_YELLOW, AS726x_ORANGE, AS726x_RED
};

// calculate some basic statistics about the current color sample
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

#ifdef SHORT_COLOR_REPORT
  // Just report the color
  Serial.print("{ id:"); Serial.print(sensorId);
  Serial.print(", x:"); Serial.print(colorInd);
  Serial.println(" }");
#endif  // SHORT_COLOR_REPORT
}

int blah = 0;

void sendMidiOn(const char *colorInd, int useNote, int sensorId, uint16_t senseVals[6]) {
  printJsonVals(colorInd, sensorId, senseVals);
  // FIXME: use color
  lastNote[sensorId - 1] = useNote;
  if (useNote > 0)
    usbMIDI.sendNoteOn(lastNote[sensorId - 1], 99, sensorId);
  blah++;
  if (blah > 7)
    blah = 0;
  if (sensorId < 1) {
    Serial.print("Oops, sensorId is less than 1");
    return;
  }
  lastColor[sensorId - 1] = colorInd;
}

void sendMidiOff(int sensorId, uint16_t senseVals[6]) {
  // FIXME: use color
  if (lastNote[sensorId - 1] > 0)
    usbMIDI.sendNoteOff(lastNote[sensorId - 1], 0, sensorId);
  if (sensorId < 1) {
    Serial.print("Oops, last sensor Id was less than 1");
    return;
  }
  lastColor[sensorId - 1] = "";
}



// Calculate the matching color (note: tuned for our sample swatches)
void calcColorMatch(int sensorId, uint16_t senseVals[6]) {
  if (stdDevSpectrum < 3) {
    // guess at a gray or black color
#ifndef AUTOTRIGGER
    sendMidiOn(".", 0, sensorId, senseVals);
#endif
    return;
  }
   
  switch (orderColors[0]) {
    case AS726x_VIOLET:
      if (orderColors[1] == AS726x_BLUE) {
        sendMidiOn("Blue", 62, sensorId, senseVals);
      } else if (orderColors[1] == AS726x_ORANGE) {
        sendMidiOn("Violet", 60, sensorId, senseVals);
      } else if (orderColors[1] == AS726x_GREEN) {
        sendMidiOn("Blue", 62, sensorId, senseVals);
      } else if (senseVals[AS726x_BLUE] > senseVals[AS726x_ORANGE]) {
        // sendMidiOn("blue", 61, sensorId, senseVals);
        sendMidiOn(".", 0, sensorId, senseVals);
      } else {
        // sendMidiOn("violet", 59, sensorId, senseVals);
        sendMidiOn(".", 0, sensorId, senseVals);
      }
      break;
    case AS726x_BLUE:
      if (orderColors[1] == AS726x_VIOLET) {
        sendMidiOn("Blue", 62, sensorId, senseVals);
      } else if (orderColors[1] == AS726x_GREEN) {
        sendMidiOn("green", 63, sensorId, senseVals);
        // sendMidiOn(".", 0, sensorId, senseVals);
      } else {
        // sendMidiOn("blue", 61, sensorId, senseVals);
        sendMidiOn(".", 0, sensorId, senseVals);
      }
      break;
    case AS726x_GREEN:
      if (orderColors[1] == AS726x_YELLOW) {
        if (senseVals[AS726x_ORANGE] > senseVals[AS726x_BLUE]) {
          // sendMidiOn("lime", 50, sensorId, senseVals); 
          sendMidiOn(".", 0, sensorId, senseVals);
        } else {
          sendMidiOn("Green", 64, sensorId, senseVals);
        }
      } else if (orderColors[1] == AS726x_BLUE) {
        sendMidiOn("green", 63, sensorId, senseVals);
        // sendMidiOn(".", 0, sensorId, senseVals);
      }
      break;
    case AS726x_YELLOW:
      if (senseVals[AS726x_YELLOW] - senseVals[orderColors[1]] < 4) {
        if (senseVals[AS726x_YELLOW] - senseVals[orderColors[2]] < 4) {
          // sendMidiOn("Beige", 72, sensorId, senseVals); 
          sendMidiOn(".", 0, sensorId, senseVals);
        } else {
          // sendMidiOn("yellow", 65, sensorId, senseVals);
          sendMidiOn(".", 0, sensorId, senseVals);
        }
      } else {
        sendMidiOn("Yellow", 66, sensorId, senseVals);
      }
      break;
    case AS726x_ORANGE:
      if (orderColors[1] == AS726x_YELLOW) {
        if (senseVals[AS726x_GREEN] > senseVals[AS726x_RED]) {
          sendMidiOn("Yellow", 65, sensorId, senseVals);
        } else {
          sendMidiOn("Orange", 68, sensorId, senseVals);
        }
      } else if (orderColors[1] == AS726x_RED) {
        sendMidiOn("Red", 70, sensorId, senseVals);
      } else {
        sendMidiOn("orange", 67, sensorId, senseVals);
      }
      break;
    case AS726x_RED:
      sendMidiOn("Red", 72, sensorId, senseVals);
      break;
   }
}

void setup() {
  // Set up serial monitor
  Serial.begin(115200);
  while(!Serial);

  // initialize digital pin LED_BUILTIN as an output indicator
  // pinMode(LED_BUILTIN, OUTPUT);
  
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
  // thirdAms.setGain(GAIN_16X);
  thirdAms.setIntegrationTime(2); // This would be N * 5.6ms since filling both banks
  thirdAms.setConversionType(ONE_SHOT); // doing one-shot mode

  for(; !secondAms.begin(&Wire1);) {
    Serial.println("could not connect to second sensor! Please check your wiring.");
    delay(30000);
  }
  secondAms.setGain(GAIN_64X); // require high gain for short integration times
  // secondAms.setGain(GAIN_16X);
  secondAms.setIntegrationTime(2); // This would be N * 5.6ms since filling both banks
  secondAms.setConversionType(ONE_SHOT); // doing one-shot mode
#endif // SINGLE_SENSOR_ONLY

  for(; !firstAms.begin(&Wire);) {
    Serial.println("could not connect to first sensor! Please check your wiring.");
    delay(30000);
  }
  firstAms.setGain(GAIN_64X); // require high gain for short integration times
  // firstAms.setGain(GAIN_16X);
  firstAms.setIntegrationTime(2); // This would be N * 5.6ms since filling both banks
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
  if (readSensorState == WAIT_START) {
#ifndef AUTOTRIGGER
    // if under threshold, keep looking
    if (analogRead(THRESHOLD_ANALOG_PIN) < pdiodeThreshold) {
      return;
    }
#endif
    firstReady = false;
#ifndef SINGLE_SENSOR_ONLY
    secondReady = false;
    thirdReady = false;
#else
    secondReady = true;
    thirdReady = true;
#endif // SINGLE_SENSOR_ONLY
    readSensorState = INITIATE_ONE_SHOT;
    // Serial.println("start");
  } // fall thru!
   
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
    // Serial.println("wait end...");
  } // fall thru!

  if (readSensorState == SEND_REPORT) {
    // generate report after sensor ready to be read
    if ( !firstReady ) {
      firstReady = firstAms.dataReady();
      if (firstReady) {
        firstAms.readRawValues(firstSenseVals);
        // firstAms.drvOff();
      }
    }
#ifndef SINGLE_SENSOR_ONLY
    if ( !secondReady ) {
      secondReady = secondAms.dataReady();
      if (secondReady) {
        secondAms.readRawValues(secondSenseVals);
        // secondAms.drvOff();
      }
    }
    if ( !thirdReady ) {
      thirdReady = thirdAms.dataReady();
      if (thirdReady) {
        thirdAms.readRawValues(thirdSenseVals);
        // thirdAms.drvOff();
      }
    }
#endif // SINGLE_SENSOR_ONLY
    if (firstReady && secondReady && thirdReady) {
        sendMidiOff(1, firstSenseVals);
#ifndef SINGLE_SENSOR_ONLY
        sendMidiOff(2, secondSenseVals);
        sendMidiOff(3, thirdSenseVals);
#endif // SINGLE_SENSOR_ONLY
        calcSampleStats(firstSenseVals);
        calcColorMatch(1, firstSenseVals);
#ifndef SINGLE_SENSOR_ONLY
        calcSampleStats(secondSenseVals);
        calcColorMatch(2, secondSenseVals);
        calcSampleStats(thirdSenseVals);
        calcColorMatch(3, thirdSenseVals);
#endif // SINGLE_SENSOR_ONLY
        readSensorState = WAIT_END;
    } // FIXME:may need a timeout counter here
  }
  
  if (readSensorState == WAIT_END) {
    // if over threshold, keep looking
#ifndef AUTOTRIGGER
    if (analogRead(THRESHOLD_ANALOG_PIN) > pdiodeThreshold) {
      return;
    }
#endif // AUTOTRIGGER
    readSensorState = WAIT_START;
    // Serial.println("report now"); // fall thru!
  } // fall thru!
}