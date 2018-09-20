#define outputEmpties true
#define dumpReadings false

#define interruptor A0
#define sensorPort Serial5

#define violet 0
#define blue 1
#define green 2
#define yellow 3
#define orange 4
#define red 5

float readings[] = {0, 0, 0, 0, 0, 0};

char colorNames[][7] = {
  "violet",
  "blue",
  "green",
  "yellow",
  "orange",
  "red"
};

void setup() {
  // put your setup code here, to run once:
  sensorPort.begin(115200);
  Serial.begin(115200);

  delay(1000);

  Serial.println("Color sensor test");

  delay(100);

  if (runCommand("AT")) Serial.println("Detected sensor");
  else Serial.println("Didn't detect sensor!!");

  if (runCommand("ATTCSMD=3") &&
      runCommand("ATINTTIME=1") &&
      runCommand("ATGAIN=3") &&
      runCommand("ATLEDC=00010001") &&
      runCommand("ATLED1=100"))
    Serial.println("Configured successfully");
  else Serial.println("Configuration failed");

  while (sensorPort.available()) sensorPort.read();
}

void loop() {
  // put your main code here, to run repeatedly:

  int interruptorReading = analogRead(interruptor);

  if (interruptorReading < 300 || interruptorReading > 500) return;

  if (runCommand("ATTCSMD=3", false)) {
    delay(30);

    sensorPort.println("ATCDATA");

    float lowerLimit = 150;
    float upperLimit = 1200;

    bool tooLow = true;
    bool tooHigh = false;

    float highestReading = 0;
    float secondHighestReading = 0;
    byte highestIndex = 0xFF;
    byte secondHighestIndex = 0xFF;

    for (byte i = 0; i < 6; i++) {
      float reading = sensorPort.parseFloat();
      readings[i] = reading;

      if (reading > lowerLimit) tooLow = false;

      if (reading > upperLimit) tooHigh = true;
      else if (reading > highestReading) {
        secondHighestReading = highestReading;
        highestReading = reading;

        secondHighestIndex = highestIndex;
        highestIndex = i;
      }
      else if (reading > secondHighestReading) {
        secondHighestReading = reading;
        secondHighestIndex = i;
      }
    }

    if (dumpReadings) {
      for (int i = 0; i < 6; i++) {
        Serial.print(colorNames[i]);
        Serial.print(": ");
        Serial.print(readings[i]);
        Serial.print(' ');
      }
      Serial.println();
    }

    if (readings[orange] == 0 && readings[red] == 0) Serial.println("Reading invalid - some channels unread");
    else if (tooLow) {
      if (outputEmpties) Serial.println("Empty - Low");
    }
    else if (tooHigh) {
      if (outputEmpties) Serial.println("Empty - High");
    }
    else if (highestIndex < 6 && secondHighestIndex < 6) {
      Serial.print(colorNames[highestIndex]);
      Serial.print('/');
      Serial.println(colorNames[secondHighestIndex]);
    }
    else {
      if (outputEmpties) {
        Serial.print("Empty - ");
        Serial.print(highestIndex);
        Serial.print('/');
        Serial.print(secondHighestIndex);
        Serial.println("?");
      }
    }
  }
}

bool runCommand(char * command, bool verbose) {
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

bool runCommand(char * command) {
  return runCommand(command, true);
}

