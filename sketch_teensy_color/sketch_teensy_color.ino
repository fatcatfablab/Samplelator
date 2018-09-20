/***************************************************************************
   This is based on the example from Adafruit AS726x library on Teensy 3.6.
   This version **does not** use neopixels as those need 5 volts.
   This uses the IR "line break" detector analog input.
   The color detection algorithm is tuned for our sample swatches.
 ***************************************************************************/

/*
   Current limited to 12.5mA?
   Integration time
*/

// The pin for the external "light break" input. Note: analog pin
#define THRESHOLD_ANALOG_PIN A16

// Threshold limits of an obstacle on light break detector
int lowThreshold = 300; // 150;  // May change with timer peg and sensor relocation
int highThreshold = 500; // 500;

// TEST TEST TEST: uncomment the following to not wait for start/stop signal
#define AUTOTRIGGER false

// TEST TEST TEST: uncomment to disable color to channel changes
#define NO_CHANNEL_CHANGES true

// TEST TEST TEST: uncomment one of the following for display to serial options
#define ORDER_BY_VALUE false
#define RGB_REPORT false
#define SHORT_COLOR_REPORT true

// TEST TEST TEST: uncomment for single sensor testing
#define SINGLE_SENSOR_ONLY false

// Modify this to set the integration time (must be > 1)
#define UNIT_INTEGRATE 1

#define AS726x_NUM_CHANNELS 6
#define AS726x_VIOLET 0
#define AS726x_BLUE 1
#define AS726x_GREEN 2
#define AS726x_YELLOW 3
#define AS726x_ORANGE 4
#define AS726x_RED 5

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

class ColorSensor {
  protected:
    static int allSensorIds;
    HardwareSerial & sensorPort;
    bool runCommand(char * command, bool verbose);
    bool runCommand(char * command);

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

    ColorSensor(HardwareSerial & port);
    // ~ColorSensor(void);

    bool begin(void);
    void readColorRegs(void);
    void calcSampleStats(void);
    void printReport(const char *);
    void sendMidiOn(const char *colorInd, int useNote);
    void sendMidiOff(void);
    void calcColorMatch(void);
    void startMeasurement(void);
};

int ColorSensor::allSensorIds = 1;

ColorSensor::ColorSensor(HardwareSerial & port) : sensorPort(port) {

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

bool ColorSensor::runCommand(char * command, bool verbose) {
  for (byte i = 0; i < 3; i++) {
    sensorPort.println(command);

    if (sensorPort.find("OK\n")) {
      if (verbose) {
        Serial.print("Ran command ");
        Serial.println(command);
      }
      return true;
    }

    if (i != 2) {
      if (verbose) {
        Serial.print("Command ");
        Serial.print(command);
        Serial.println(" failed, retrying");
      }
      delay(5);
    }
    else if (verbose) {
      Serial.println("Command failed");
    }
  }

  return false;
}

bool ColorSensor::runCommand(char * command) {
  return runCommand(command, true);
}

bool ColorSensor::begin() {
  sensorPort.begin(115200);

  if (runCommand("AT")) Serial.println("Detected sensor");
  else Serial.println("Didn't detect sensor!!");

  if (runCommand("ATTCSMD=3") && // doing one-shot mode
      runCommand("ATINTTIME=1") && // This would be N * 5.6ms since filling both banks
      runCommand("ATGAIN=3") && // require high gain for short integration times
      runCommand("ATLEDC=00010001") && // Set LED current to 50mA for flashlight and 4mA for indicator
      runCommand("ATLED1=100")) { // Turn on flashlight
    Serial.println("Configured successfully");
    return true;
  }
  else {
    Serial.println("Configuration failed");
    return false;
  }
}

void ColorSensor::readColorRegs(void) {
  sensorPort.println("ATDATA");

  for (byte i = 0; i < 6; i++) {
    int reading = sensorPort.parseInt();
    senseVals[i] = reading;
  }
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
  if (ORDER_BY_VALUE) {
    Serial.print("{ id:"); Serial.print(sensorId);
    for (int idx = 0; idx < AS726x_NUM_CHANNELS; idx++) {
      switch (orderedColors[idx]) {
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
  } // ORDER_BY_VALUE

  if (RGB_REPORT) {
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
  }

  if (SHORT_COLOR_REPORT) {
    // Just report the color
    Serial.print("{ id:"); Serial.print(sensorId);
    Serial.print(", x:"); Serial.print(colorInd);
    Serial.println(" }");
  }  // SHORT_COLOR_REPORT
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
    if (!AUTOTRIGGER) {
      sendMidiOn(".", 0);
    }
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

void ColorSensor::startMeasurement() {
  runCommand("ATTCSMD=3", false);
}

ColorSensor ams[] = {
  ColorSensor(Serial1),
  ColorSensor(Serial2),
  ColorSensor(Serial3),
  ColorSensor(Serial4),
  ColorSensor(Serial5),
  ColorSensor(Serial6)
};

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
  dialPin1 = 28,
  dialPin2 = 29,
  dialPin3 = 30,
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

  delay(1000);

  Serial.println("Sanity");

  // while(!Serial);

  // initialize digital pin LED_BUILTIN as an output indicator
  pinMode(LED_BUILTIN, OUTPUT);

  if (!NO_CHANNEL_CHANGES) {
    pinMode(dialPin1, INPUT_PULLUP);
    pinMode(dialPin2, INPUT_PULLUP);
    pinMode(dialPin3, INPUT_PULLUP);
    pinMode(dialPin4, INPUT_PULLUP);
    pinMode(dialPin5, INPUT_PULLUP);
    pinMode(dialPin6, INPUT_PULLUP);
    pinMode(dialPin7, INPUT_PULLUP);
    pinMode(dialPin8, INPUT_PULLUP);
    pinMode(dialPin9, INPUT_PULLUP);
    pinMode(dialPin10, INPUT_PULLUP);
    pinMode(dialPin11, INPUT_PULLUP);
    pinMode(dialPin12, INPUT_PULLUP);
  } // NO_CHANGE_CHANNEL

  //begin and make sure we can talk to the sensors
  if (!SINGLE_SENSOR_ONLY) {
    for (int i = 1; i < 6; i++) {
      Serial.print("Init AMS #");
      Serial.println(i);

      for (; !ams[i].begin();) {
        Serial.print("could not connect to sensor #");
        Serial.print(i);
        Serial.println("! Please check your wiring.");
        delay(30000);
      }
    }
  } // SINGLE_SENSOR_ONLY
  Serial.println("Init AMS #0");
  for (; !ams[0].begin();) {
    Serial.println("could not connect to first sensor! Please check your wiring.");
    delay(30000);
  }

  Serial.println("Waiting for start sample...");
}



void loop() {
  int triggerValue;

  // Serial.println(readSensorState);
  if (readSensorState == WAIT_START) {
    if (!AUTOTRIGGER) {
      triggerValue = analogRead(THRESHOLD_ANALOG_PIN);
    } else {
      triggerValue = highThreshold + 1;
    }
    // if under threshold, keep looking
    if (triggerValue > highThreshold) {
      ams[0].isReady = false;
      ams[0].wasRead = false;
      ams[0].sendMidiOff();
      if (!SINGLE_SENSOR_ONLY) {
        for (int i = 1; i < 6; i++) {
          ams[i].isReady = false;
          ams[i].wasRead = false;
          ams[i].sendMidiOff();
        }
      } else {
        for (int i = 1; i < 6; i++) {
          ams[i].isReady = true;
          ams[i].wasRead = true;
        }
      } // SINGLE_SENSOR_ONLY
      readSensorState = INITIATE_ONE_SHOT;
      digitalWrite(LED_BUILTIN, HIGH);
    } else {
      // Serial.print("Wait Start : "); Serial.println(triggerValue);
    }
  }

  if (readSensorState == INITIATE_ONE_SHOT) {
    ams[0].startMeasurement(); // initiate one shot color sensing
    if (!SINGLE_SENSOR_ONLY) {
      for (int i = 1; i < 6; i++) {
        ams[i].startMeasurement();
      }
    } // SINGLE_SENSOR_ONLY
    delay(30); // Hardcoded delay - no dataReady() available for UART
    readSensorState = SEND_REPORT;
  }

  if (readSensorState == SEND_REPORT) {
    ams[0].readColorRegs();
    if (!SINGLE_SENSOR_ONLY) {
      for (int i = 1; i < 6; i++) {
        ams[i].readColorRegs();
      }
    }

    ams[0].calcSampleStats();
    ams[0].calcColorMatch();
    if (!SINGLE_SENSOR_ONLY) {
      for (int i = 1; i < 6; i++) {
        ams[i].calcSampleStats();
        ams[i].calcColorMatch();
      }
    } // SINGLE_SENSOR_ONLY
    readSensorState = WAIT_END;
  }

  if (readSensorState == WAIT_END) {
    if (!AUTOTRIGGER) {
      triggerValue = analogRead(THRESHOLD_ANALOG_PIN);
    } else {
      triggerValue = lowThreshold - 1;
    }
    if (triggerValue < lowThreshold) {
      readSensorState = WAIT_START;
      digitalWrite(LED_BUILTIN, LOW);

      if (!NO_CHANNEL_CHANGES) {
        // read the color to channel switches here
        int dialCode_v = digitalRead(dialPin1) + digitalRead(dialPin2) * 2 + digitalRead(dialPin3) * 4;
        int dialCode_b = digitalRead(dialPin4) + digitalRead(dialPin5) * 2 + digitalRead(dialPin6) * 4;
        int dialCode_g = digitalRead(dialPin7) + digitalRead(dialPin8) * 2 + digitalRead(dialPin9) * 4;
        int dialCode_y = digitalRead(dialPin10) + digitalRead(dialPin11) * 2 + digitalRead(dialPin12) * 4;
        int dialCode_r = digitalRead(dialPin13) + digitalRead(dialPin14) * 2 + digitalRead(dialPin15) * 4;
        int dialCode_m = digitalRead(dialPin16) + digitalRead(dialPin17) * 2 + digitalRead(dialPin18) * 4;

        switch (dialCode_v) {
          case 1: color2Channel[violetChan] = 2; break;
          case 2: color2Channel[violetChan] = 3; break;
          case 4: color2Channel[violetChan] = 4; break;
          default:
            color2Channel[violetChan] = 1;
        }
        switch (dialCode_b) {
          case 1: color2Channel[blueChan] = 2; break;
          case 2: color2Channel[blueChan] = 3; break;
          case 4: color2Channel[blueChan] = 4; break;
          default:
            color2Channel[blueChan] = 1;
        }
        switch (dialCode_g) {
          case 1: color2Channel[greenChan] = 2; break;
          case 2: color2Channel[greenChan] = 3; break;
          case 4: color2Channel[greenChan] = 4; break;
          default:
            color2Channel[greenChan] = 1;
        }
        switch (dialCode_y) {
          case 1: color2Channel[yellowChan] = 2; break;
          case 2: color2Channel[yellowChan] = 3; break;
          case 4: color2Channel[yellowChan] = 4; break;
          default:
            color2Channel[yellowChan] = 1;
        }
        switch (dialCode_r) {
          case 1: color2Channel[redChan] = 2; break;
          case 2: color2Channel[redChan] = 3; break;
          case 4: color2Channel[redChan] = 4; break;
          default:
            color2Channel[redChan] = 1;
        }
        switch (dialCode_m) {
          case 1: color2Channel[magentaChan] = 2; break;
          case 2: color2Channel[magentaChan] = 3; break;
          case 4: color2Channel[magentaChan] = 4; break;
          default:
            color2Channel[magentaChan] = 1;
        }
      } // NO_CHANGE_CHANNEL
    } else {
      // Serial.print("Wait End : "); Serial.println(triggerValue);
    }
  }
}
