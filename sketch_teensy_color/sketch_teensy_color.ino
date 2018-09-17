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

// For external "light break" input. Note: analog pin
#define THRESHOLD_ANALOG_PIN A0

// Threshold limits of an obstacle on light break detector
int lowThreshold = 100; // 150;
int highThreshold = 200; // 500;

// TEST TEST TEST: uncomment the following not wait for start/stop signal
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


// Change this to match inner or outer teensy
enum teensyPos {
  TeensyInner = 1,
  TeensyOuter
};
teensyPos teensyId = TeensyInner;


// buffer to hold sensor color values
uint16_t firstSenseVals[AS726x_NUM_CHANNELS];
uint16_t secondSenseVals[AS726x_NUM_CHANNELS];
uint16_t thirdSenseVals[AS726x_NUM_CHANNELS];

// color calculator variables
int totSpectrum[3], meanSpectrum[3], numVariance[3], stdDevSpectrum[3];

// State machine for loop processing
enum sensingStates {
  WAIT_START,
  INITIATE_ONE_SHOT,
  WAIT_END,
  SEND_REPORT
};

// jumpstart state machine
int readSensorState = WAIT_START;

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
  dialPin12 = 11
};

// value order or color sensors (changes per sample calculated)
int orderedColors[3][6] = {
  {AS726x_VIOLET, AS726x_BLUE, AS726x_GREEN, AS726x_YELLOW, AS726x_ORANGE, AS726x_RED},
  {AS726x_VIOLET, AS726x_BLUE, AS726x_GREEN, AS726x_YELLOW, AS726x_ORANGE, AS726x_RED},
  {AS726x_VIOLET, AS726x_BLUE, AS726x_GREEN, AS726x_YELLOW, AS726x_ORANGE, AS726x_RED}
};

// Data ready flags for each sensor for state machine sequencing
bool firstReady, secondReady, thirdReady;
const char *lastColor[3];
int lastNote[3];
int lastChan[3];


// calculate some basic statistics about the current color sample
// The std dev value is also used to filter out white/black/grey colors
void calcSampleStats(uint16_t *senseVals, int sensorId) {
  if (sensorId > 3) {
    Serial.println("*** Oops, calc on invalid sensor id");
  }
  int useId = sensorId - 1;
  totSpectrum[useId] = 0;
  for (int idx = 0; idx < 6; idx++) {
    totSpectrum[useId] += senseVals[orderedColors[useId][idx]];
  }
  meanSpectrum[useId] = totSpectrum[useId] / 6;
  numVariance[useId] = 0;
  for (int idx = 0; idx < 6; idx++) {
    numVariance[useId] += sq(meanSpectrum[useId] - senseVals[orderedColors[useId][idx]]);
  }
  stdDevSpectrum[useId] = sqrt(numVariance[useId] / 5); // 5 not 6 cause it's a sample

  // sort colors by value
  for (int idx = 0; idx < 5; idx++) {
    for (int jdx = idx + 1; jdx < 6; jdx++ ) {
      if (senseVals[orderedColors[useId][idx]] < senseVals[orderedColors[useId][jdx]]) {
        int tmpColor = orderedColors[useId][idx];
        orderedColors[useId][idx] = orderedColors[useId][jdx];
        orderedColors[useId][jdx] = tmpColor;
      }
    }
  }
}


// Report is printed to serial monitor
void printJsonVals(const char *colorInd, int sensorId, uint16_t senseVals[6]) {
  if (sensorId > 3) {
    Serial.println("*** Oops, calc on invalid sensor id");
  }
  int useId = sensorId - 1;
  int *orderColors = orderedColors[useId];
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
  int useChan = 0;
  printJsonVals(colorInd, sensorId, senseVals);
  if (sensorId > 3) {
    Serial.print("Oops, invalid sensorId");
    return;
  }
  int useId = sensorId - 1;
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
  lastColor[useId] = colorInd;
  lastNote[useId] = useNote;
  if (useChan < unknownChan)
    lastChan[useId] = color2Channel[useChan];
  else
    lastChan[useId] = unknownChan;
  if (useNote > 0) {
    usbMIDI.sendNoteOn(lastNote[useId], 99, lastChan[useId]);
  } else if (useId == 0) {
    // usbMIDI.sendNoteOn(39, 99, 1); // debug break detect
  }
}

void sendMidiOff(int sensorId) {
  if (sensorId > 3) {
    Serial.print("Oops, invalid sensor id");
    return;
  }
  int useId = sensorId - 1;
  lastColor[useId] = "";
  if (lastNote[useId] > 0) {
    usbMIDI.sendNoteOff(lastNote[useId], 0, lastChan[useId]);
  }
}


// Calculate the matching color (note: tuned for our sample swatches)
void calcColorMatch(int sensorId, uint16_t senseVals[6]) {
  if (sensorId > 3) {
    Serial.print("Oops, invalid sensor id");
    return;
  }
  int useId = sensorId - 1; 
  if (stdDevSpectrum[useId] < 6) {
    // guess at a gray or black color
#ifndef AUTOTRIGGER
    sendMidiOn(".", 0, sensorId, senseVals);
#endif
    return;
  }

  int *orderColors = orderedColors[useId];

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
          // Serial.println(millis()); // ???
        } else {
          sendMidiOn("Violet", violetMidi, sensorId, senseVals);
        }
      } else if (orderColors[1] == AS726x_GREEN) {
        // sendMidiOn("Cyan", 74, sensorId, senseVals);
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
         // Serial.println(millis());
      } else if (orderColors[1] == AS726x_RED && orderColors[2] == AS726x_YELLOW) {
        sendMidiOn("Red", redMidi, sensorId, senseVals);
      } else if (orderColors[1] == AS726x_YELLOW) {
        if (senseVals[AS726x_GREEN] > senseVals[AS726x_RED]) {
          sendMidiOn("Yellow", yellowMidi, sensorId, senseVals);
      } else {
          sendMidiOn(".", 0, sensorId, senseVals);
        }
      } else {
        sendMidiOn(".", 0, sensorId, senseVals);
      }
      break;
      break;
    case AS726x_RED:
      sendMidiOn("Red", redMidi, sensorId, senseVals);
      break;
   }
}

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

char t2tBuff[80];
int t2tIdx = 0;

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
        calcSampleStats(firstSenseVals, 1);
        calcColorMatch(1, firstSenseVals);
#ifndef SINGLE_SENSOR_ONLY
        calcSampleStats(secondSenseVals, 2);
        calcColorMatch(2, secondSenseVals);
        calcSampleStats(thirdSenseVals, 3);
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

#ifndef NO_CHANGE_CHANNEL
      // read the color to channel switches here
      int dialCode_a = digitalRead(dialPin1) + digitalRead(dialPin2) * 2 + digitalRead(dialPin3) * 4 + digitalRead(dialPin4) * 8;
      int dialCode_b = digitalRead(dialPin5) + digitalRead(dialPin6) * 2 + digitalRead(dialPin7) * 4 + digitalRead(dialPin8) * 8;
      int dialCode_c = digitalRead(dialPin9) + digitalRead(dialPin10) * 2 + digitalRead(dialPin11) * 4 + digitalRead(dialPin12) * 8;

      if (teensyId == TeensyInner) {
        // responsible for dials for Violet, Blue, Green
        switch(dialCode_a){
          case 1: color2Channel[violetChan] = 2; break;
          case 2: color2Channel[violetChan] = 3; break;
          case 4: color2Channel[violetChan] = 4; break;
          case 8: color2Channel[violetChan] = 5; break;
          default:
            color2Channel[violetChan] = 1;
        }
        switch(dialCode_b){
          case 1: color2Channel[blueChan] = 2; break;
          case 2: color2Channel[blueChan] = 3; break;
          case 4: color2Channel[blueChan] = 4; break;
          case 8: color2Channel[blueChan] = 5; break;
          default:
            color2Channel[blueChan] = 1;
        }
        switch(dialCode_c){
          case 1: color2Channel[greenChan] = 2; break;
          case 2: color2Channel[greenChan] = 3; break;
          case 4: color2Channel[greenChan] = 4; break;
          case 8: color2Channel[greenChan] = 5; break;
          default:
            color2Channel[greenChan] = 1;
        }
      } else {
        // responsible for dials for Yellow, Red, Magenta
        switch(dialCode_a){
          case 1: color2Channel[yellowChan] = 2; break;
          case 2: color2Channel[yellowChan] = 3; break;
          case 4: color2Channel[yellowChan] = 4; break;
          case 8: color2Channel[yellowChan] = 5; break;
          default:
            color2Channel[yellowChan] = 1;
        }
        switch(dialCode_b){
          case 1: color2Channel[redChan] = 2; break;
          case 2: color2Channel[redChan] = 3; break;
          case 4: color2Channel[redChan] = 4; break;
          case 8: color2Channel[redChan] = 5; break;
          default:
            color2Channel[redChan] = 1;
        }
        switch(dialCode_c){
          case 1: color2Channel[magentaChan] = 2; break;
          case 2: color2Channel[magentaChan] = 3; break;
          case 4: color2Channel[magentaChan] = 4; break;
          case 8: color2Channel[magentaChan] = 5; break;
          default:
            color2Channel[magentaChan] = 1;
        }
      }

      // if any channel changes occur, send to other teensy
      for (int idx = 0; idx < sizeof(lastChannels)/sizeof(uint8_t); idx++) {
        if (color2Channel[idx] != lastChannels[idx]) {
          Serial1.print('{');
          Serial1.print('C'); Serial1.print(idx);
          Serial1.print(':'); Serial1.print(color2Channel[idx]);
          Serial1.println('}');
        }
        lastChannels[idx] = color2Channel[idx];
      }
#endif // NO_CHANGE_CHANNEL
    }
  }

#ifndef NO_CHANGE_CHANNEL
  // inter-teensy comm Rx is valid in any state
  if (Serial1.available()) {
    // note: inter-teensy comm RX processed 1 char at a time
    char chNow = Serial1.read();
    if (t2tIdx < sizeof(t2tBuff)/sizeof(*t2tBuff)) {
      t2tBuff[t2tIdx++] = chNow;
    }
    if (t2tBuff[0] != '{') {
      // something got messed up. Clear out the buffer.
      t2tIdx = 0;
    } else if (chNow == '}') {
      // make sure we have the min length to recognize a packet
      if (t2tIdx > 4) {
        // Assume t2tBuff[0] == '{'
        // Assume t2tBuff[1] == 'C'
        // Assume t2tBuff[2] is an integer and not ascii
        int colorIdx = t2tBuff[2];
        // Assume t2tBuff[3] == ':'
        int newChan = t2tBuff[3];
        // Assume t2tBuff[4] == '}'
        
        if (colorIdx < sizeof(color2Channel)/sizeof(*color2Channel)) {
          color2Channel[colorIdx] = newChan;
          lastChannels[colorIdx] = newChan;
        }
      }
      // reset buffer
      t2tIdx = 0;
    }
  }
#endif // NO_CHANGE_CHANNEL
}
