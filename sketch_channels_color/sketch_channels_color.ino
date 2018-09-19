/***************************************************************************
 * This is based on the example from Adafruit AS726x library on Teensy 3.6.
 * This version **does not** use neopixels as those need 5 volts.
 * This uses the IR "line break" detector analog input.
 * The color detection algorithm is tuned for our sample swatches.
 ***************************************************************************/
#include <Wire.h>
#include <Adafruit_AS726x.h>

// The pin for the external "light break" input. Note: analog pin
#define THRESHOLD_ANALOG_PIN A0

// Threshold limits of an obstacle on light break detector
int lowThreshold = 300; // 150;  // May change with timer peg and sensor relocation
int highThreshold = 500; // 500;

// TEST TEST TEST: uncomment the following to not wait for start/stop signal
// #define AUTOTRIGGER

// TEST TEST TEST: uncomment to disable color to channel changes
#define NO_CHANNEL_CHANGES

// TEST TEST TEST: uncomment one of the following for display to serial options
// #define ORDER_BY_VALUE
// #define RGB_REPORT
#define SHORT_COLOR_REPORT

// TEST TEST TEST: uncomment for single sensor testing
// #define SINGLE_SENSOR_ONLY

// Modify this to set the integration time (must be > 1)
#define UNIT_INTEGRATE 1

int defaultOrder[AS726x_NUM_CHANNELS] = {
  AS726x_VIOLET, AS726x_BLUE, AS726x_GREEN, AS726x_YELLOW, AS726x_ORANGE, AS726x_RED
};


// color to note assignment
int blueMidi = 36;
int greenMidi = 40;
int redMidi = 42;
int magentaMidi = 46;
int violetMidi = 48;
int yellowMidi = 50;

enum colorChannelIndex {
  violetChan = 0,
  blueChan,
  greenChan,
  yellowChan,
  redChan,
  magentaChan,
  unknownChan = 16
};

// change which channel the color gets sent to. Currently it all goes to midi channel 1
uint8_t color2Channel[] = {
  1, 1, 1, 1, 1, 1
};

uint8_t lastChannels[] = {
  1, 1, 1, 1, 1, 1
};

class ColorSensor : public Adafruit_AS726x {
 protected:
  static int allSensorIds;
  
 public:	// basically, this class is more like a struct+typedef
  int sensorId;
  uint16_t senseVals[AS726x_NUM_CHANNELS];
  int orderedColors[AS726x_NUM_CHANNELS];
  int totSpectrum;
  int meanSpectrum;
  int numVariance;
  int stdDevSpectrum;
  bool isReady;
  bool wasRead;
  const char *lastColor;
  int lastNote;
  int lastChan;

  ColorSensor(void);
  // ~ColorSensor(void);

  void readColorRegs(void);
  void calcSampleStats(void);
  void printReport(const char *);
  void sendMidiOn(const char *colorInd, int useNote);
  void sendMidiOff(void);
  void calcColorMatch(void);
};

int ColorSensor::allSensorIds = 1;

ColorSensor::ColorSensor() {
  sensorId = ColorSensor::allSensorIds++;
  for (int idx = 0; idx < AS726x_NUM_CHANNELS; idx++)
    senseVals[idx] = 0;
  for (int idx = 0; idx < AS726x_NUM_CHANNELS; idx++)
    orderedColors[idx] = defaultOrder[idx];
  totSpectrum = 0;
  meanSpectrum = 0;
  numVariance = 0;
  stdDevSpectrum = 0;
  isReady = false;
  wasRead = false;
  lastColor = "";
  lastNote = 0;
  lastChan = 1;
}

void ColorSensor::readColorRegs(void) {
  readRawValues(senseVals, 6);
}

// calculate some basic statistics about the current color sample
// The std dev value is also used to filter out white/black/grey colors
void ColorSensor::calcSampleStats() {
  totSpectrum = 0;
  for (int idx = 0; idx < AS726x_NUM_CHANNELS; idx++) {
    totSpectrum += senseVals[idx];
  }
  meanSpectrum = totSpectrum / AS726x_NUM_CHANNELS;
  numVariance = 0;
  for (int idx = 0; idx < 6; idx++) {
    numVariance += sq(meanSpectrum - senseVals[idx]);
  }
  stdDevSpectrum = sqrt(numVariance / (AS726x_NUM_CHANNELS - 1)); // 5 not 6 cause it's a sample

  // sort colors by value
  for (int idx = 0; idx < 5; idx++) {
    for (int jdx = idx + 1; jdx < 6; jdx++ ) {
      if (senseVals[orderedColors[idx]] < senseVals[orderedColors[jdx]]) {
        int tmpColor = orderedColors[idx];
        orderedColors[idx] = orderedColors[jdx];
        orderedColors[jdx] = tmpColor;
      }
    }
  }
}


// Report is printed to serial monitor
void ColorSensor::printReport(const char *colorInd) {
#ifdef ORDER_BY_VALUE
  Serial.print("{ id:"); Serial.print(sensorId);
  for (int idx = 0; idx < AS726x_NUM_CHANNELS; idx++) {
    switch(orderedColors[idx]) {
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

void ColorSensor::sendMidiOn(const char *colorInd, int useNote) {
  printReport(colorInd);
  int useChan;
  // shortcut map the color to the configured channel
  switch (*colorInd) {
    case 'V': useChan = violetChan; break;
    case 'B': useChan = blueChan; break;
    case 'G': useChan = greenChan; break;
    case 'Y': useChan = yellowChan; break;
    case 'R': useChan = redChan; break;
    case 'M': useChan = magentaChan; break;
    default: useChan = unknownChan; useNote = 0; break;
  }
  lastColor = colorInd;
  lastNote = useNote;
  if (useChan < unknownChan)
    lastChan = color2Channel[useChan];
  else
    lastChan = unknownChan;
  if (useNote > 0) {
    usbMIDI.sendNoteOn(lastNote, 99, lastChan);
  } else if (useNote == 0) {
    // usbMIDI.sendNoteOn(39, 99, 1); // debug break detect
  }
}

void ColorSensor::sendMidiOff(void) {
  lastColor = "";
  if (lastNote > 0) {
    usbMIDI.sendNoteOff(lastNote, 0, lastChan);
  }
}


// Calculate the matching color (note: tuned for our sample swatches)
void ColorSensor::calcColorMatch(void) {
  if (stdDevSpectrum < 6) {
    // guess at a gray or black color
#ifndef AUTOTRIGGER
    sendMidiOn(".", 0);
#endif
    return;
  }

  // if the peak color is small, it's not a playable color
  if (senseVals[orderedColors[0]] < 75) {
    sendMidiOn(".", 0);
    return; 
  }
      
  switch (orderedColors[0]) {
    case AS726x_VIOLET:
      if (orderedColors[1] == AS726x_BLUE) {
        sendMidiOn("Blue", blueMidi);
      } else if (orderedColors[1] == AS726x_ORANGE) {
        if (senseVals[AS726x_YELLOW] < senseVals[AS726x_RED]) {
          sendMidiOn("Magenta", magentaMidi);
          // Serial.println(millis()); // ???
        } else {
          sendMidiOn("Violet", violetMidi);
        }
      } else if (orderedColors[1] == AS726x_GREEN) {
        // sendMidiOn("Cyan", 74);
        sendMidiOn("Blue", blueMidi);
      } else {
        sendMidiOn(".", 0);
      }
      break;
    case AS726x_BLUE:
      if (orderedColors[1] == AS726x_VIOLET) {
        sendMidiOn("Blue", blueMidi);
      } else {
        sendMidiOn(".", 0);
      }
      break;
    case AS726x_GREEN:
      if (orderedColors[1] == AS726x_YELLOW && orderedColors[2] == AS726x_ORANGE) {
        sendMidiOn(".", 0);
      } else if (orderedColors[1] == AS726x_YELLOW && orderedColors[2] == AS726x_VIOLET) {
        sendMidiOn(".", 0);
      } else if (orderedColors[1] == AS726x_ORANGE) {
        sendMidiOn(".", 0);
      } else {
        sendMidiOn("Green", greenMidi);
      }
      break;
    case AS726x_YELLOW:
      if (orderedColors[2] == AS726x_VIOLET || orderedColors[2] == AS726x_BLUE || 
          orderedColors[2] == AS726x_RED) {
            sendMidiOn(".", 0);
      } else {
         sendMidiOn(".", 0);
      }
      break;
    case AS726x_ORANGE:
      if (orderedColors[1] == AS726x_GREEN && orderedColors[2] == AS726x_YELLOW) { 
          sendMidiOn("Yellow", yellowMidi);
      } else if (orderedColors[1] == AS726x_VIOLET) {    
         sendMidiOn("Magenta", magentaMidi);
         // Serial.println(millis());
      } else if (orderedColors[1] == AS726x_RED && orderedColors[2] == AS726x_YELLOW) {
        sendMidiOn("Red", redMidi);
      } else if (orderedColors[1] == AS726x_YELLOW) {
        if (senseVals[AS726x_GREEN] > senseVals[AS726x_RED]) {
          sendMidiOn("Yellow", yellowMidi);
      } else {
          sendMidiOn(".", 0);
        }
      } else {
        sendMidiOn(".", 0);
      }
      break;
    case AS726x_RED:
      sendMidiOn("Red", redMidi);
      break;
   }
}


ColorSensor firstAms, secondAms, thirdAms;


// State machine for loop processing
enum sensingStates {
  WAIT_START,
  INITIATE_ONE_SHOT,
  WAIT_END,
  SEND_REPORT
};

// jumpstart state machine
int readSensorState = WAIT_START;

// pins to read channel selector dials
enum dialPins {
  dialPin1 = 0,
  dialPin2 = 1,
  dialPin3 = 2,
  dialPin4 = 3,
  dialPin5 = 4,
  dialPin6 = 5,
  dialPin7 = 6,
  dialPin8 = 7,
  dialPin9 = 8,
  dialPin10 = 9,
  dialPin11 = 10,
  dialPin12 = 11,
  dialPin13 = 12,
  dialPin14 = 13,
  dialPin15 = 14,
  dialPin16 = 15,
  dialPin17 = 16,
  dialPin18 = 17
};


void setup() {
  // Set up serial monitor
  Serial.begin(115200);
  // while(!Serial);

  // Set up inter-teensy board comm for color to channel mapping
  Serial1.begin(9600);

  // initialize digital pin LED_BUILTIN as an output indicator
  pinMode(LED_BUILTIN, OUTPUT);

#ifndef NO_CHANGE_CHANNEL
  pinMode(dialPin1, INPUT);
  pinMode(dialPin2, INPUT);
  pinMode(dialPin3, INPUT);
  pinMode(dialPin4, INPUT);
  pinMode(dialPin5, INPUT);
  pinMode(dialPin6, INPUT);
  pinMode(dialPin7, INPUT);
  pinMode(dialPin8, INPUT);
  pinMode(dialPin9, INPUT);
  pinMode(dialPin10, INPUT);
  pinMode(dialPin11, INPUT);
  pinMode(dialPin12, INPUT);
#endif // NO_CHANGE_CHANNEL
  
  // Wire1 is i2c CLK=19, DATA=18
  // Wire2 is i2c CLCK=37, DATA=38
  // Wire3 is i2c CLK=3, DATA=4

Serial.println("Init third");
  //begin and make sure we can talk to the sensors
#ifndef SINGLE_SENSOR_ONLY  
  for(; !thirdAms.begin(&Wire2);) {
    Serial.println("could not connect to third sensor! Please check your wiring.");
    delay(30000);
  }
  thirdAms.setGain(GAIN_64X); // require high gain for short integration times
  thirdAms.setIntegrationTime(UNIT_INTEGRATE); // This would be N * 5.6ms since filling both banks
  thirdAms.setConversionType(ONE_SHOT); // doing one-shot mode
  thirdAms.drvOn();  

Serial.println("Init second");

  for(; !secondAms.begin(&Wire1);) {
    Serial.println("could not connect to second sensor! Please check your wiring.");
    delay(30000);
  }
  secondAms.setGain(GAIN_64X); // require high gain for short integration times
  secondAms.setIntegrationTime(UNIT_INTEGRATE); // This would be N * 5.6ms since filling both banks
  secondAms.setConversionType(ONE_SHOT); // doing one-shot mode
  secondAms.drvOn();
#endif // SINGLE_SENSOR_ONLY
Serial.println("Init first");
  for(; !firstAms.begin(&Wire);) {
    Serial.println("could not connect to first sensor! Please check your wiring.");
    delay(30000);
  }
  firstAms.setGain(GAIN_64X); // require high gain for short integration times
  firstAms.setIntegrationTime(UNIT_INTEGRATE); // This would be N * 5.6ms since filling both banks
  firstAms.setConversionType(ONE_SHOT); // doing one-shot mode
  firstAms.drvOn();

  Serial.println("Waiting for start sample...");  
}



void loop() {
  int triggerValue;

  // Serial.println(readSensorState);
  if (readSensorState == WAIT_START) {
#ifndef AUTOTRIGGER
    triggerValue = analogRead(THRESHOLD_ANALOG_PIN);
#else
    triggerValue = lowThreshold - 1;
#endif
    // if under threshold, keep looking
    if (triggerValue > highThreshold) {
      firstAms.isReady = false;
      firstAms.wasRead = false;
      firstAms.sendMidiOff();
#ifndef SINGLE_SENSOR_ONLY
      secondAms.isReady = false;
      secondAms.wasRead = false;
      thirdAms.isReady = false;
      thirdAms.wasRead = false;
      secondAms.sendMidiOff();
      thirdAms.sendMidiOff();
#else
      secondAms.isReady = true;
      thirdAms.isReady = true;
#endif // SINGLE_SENSOR_ONLY
      readSensorState = INITIATE_ONE_SHOT;
      digitalWrite(LED_BUILTIN, HIGH);
    } else {
      // Serial.print("Wait Start : "); Serial.println(triggerValue);
    }
  }
   
  if (readSensorState == INITIATE_ONE_SHOT) {
    firstAms.startMeasurement(); // initiate one shot color sensing
#ifndef SINGLE_SENSOR_ONLY
    secondAms.startMeasurement();
    thirdAms.startMeasurement();
#endif // SINGLE_SENSOR_ONLY
    readSensorState = SEND_REPORT;
  }

  if (readSensorState == SEND_REPORT) {
    // generate report after sensor ready to be read
    if ( !firstAms.isReady ) {
      firstAms.isReady = firstAms.dataReady();
      if (firstAms.isReady) {
        firstAms.readColorRegs();
        firstAms.wasRead = true;
      }
    }
#ifndef SINGLE_SENSOR_ONLY
    if ( !secondAms.isReady ) {
      secondAms.isReady = secondAms.dataReady();
      if (secondAms.isReady) {
        secondAms.readColorRegs();
        secondAms.wasRead = true;
      }
    }
    if ( !thirdAms.isReady ) {
      thirdAms.isReady = thirdAms.dataReady();
      if (thirdAms.isReady) {
        thirdAms.readColorRegs();
        thirdAms.wasRead = true;
      }
    }
#endif // SINGLE_SENSOR_ONLY

    if (firstAms.isReady && secondAms.isReady && thirdAms.isReady) {
        firstAms.calcSampleStats();
        firstAms.calcColorMatch();
#ifndef SINGLE_SENSOR_ONLY
        secondAms.calcSampleStats();
        secondAms.calcColorMatch();
        thirdAms.calcSampleStats();
        thirdAms.calcColorMatch();
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
    if (triggerValue < lowThreshold) {
      readSensorState = WAIT_START;
      digitalWrite(LED_BUILTIN, LOW);

#ifndef NO_CHANGE_CHANNEL
      // read the color to channel switches here
      int dialCode_v = digitalRead(dialPin1) + digitalRead(dialPin2) * 2 + digitalRead(dialPin3) * 4;
      int dialCode_b = digitalRead(dialPin4) + digitalRead(dialPin5) * 2 + digitalRead(dialPin6) * 4;
      int dialCode_g = digitalRead(dialPin7) + digitalRead(dialPin8) * 2 + digitalRead(dialPin9) * 4;
      int dialCode_y = digitalRead(dialPin10) + digitalRead(dialPin11) * 2 + digitalRead(dialPin12) * 4;
      int dialCode_r = digitalRead(dialPin13) + digitalRead(dialPin14) * 2 + digitalRead(dialPin15) * 4;
      int dialCode_m = digitalRead(dialPin16) + digitalRead(dialPin17) * 2 + digitalRead(dialPin18) * 4;

      switch(dialCode_v){
        case 1: color2Channel[violetChan] = 2; break;
        case 2: color2Channel[violetChan] = 3; break;
        case 4: color2Channel[violetChan] = 4; break;
        default:
          color2Channel[violetChan] = 1;
      }
      switch(dialCode_b){
        case 1: color2Channel[blueChan] = 2; break;
        case 2: color2Channel[blueChan] = 3; break;
        case 4: color2Channel[blueChan] = 4; break;
        default:
          color2Channel[blueChan] = 1;
      }
      switch(dialCode_g){
        case 1: color2Channel[greenChan] = 2; break;
        case 2: color2Channel[greenChan] = 3; break;
        case 4: color2Channel[greenChan] = 4; break;
        default:
          color2Channel[greenChan] = 1;
      }
      switch(dialCode_y){
        case 1: color2Channel[yellowChan] = 2; break;
        case 2: color2Channel[yellowChan] = 3; break;
        case 4: color2Channel[yellowChan] = 4; break;
        default:
          color2Channel[yellowChan] = 1;
      }
      switch(dialCode_r){
        case 1: color2Channel[redChan] = 2; break;
        case 2: color2Channel[redChan] = 3; break;
        case 4: color2Channel[redChan] = 4; break;
        default:
          color2Channel[redChan] = 1;
      }
      switch(dialCode_m){
        case 1: color2Channel[magentaChan] = 2; break;
        case 2: color2Channel[magentaChan] = 3; break;
        case 4: color2Channel[magentaChan] = 4; break;
        default:
          color2Channel[magentaChan] = 1;
      }
#endif // NO_CHANGE_CHANNEL
    } else {
      // Serial.print("Wait End : "); Serial.println(triggerValue);
    }
  }
}
