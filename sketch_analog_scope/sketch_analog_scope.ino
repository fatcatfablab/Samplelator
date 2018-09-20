/*
*  Analog scope
*  
*  Code is based on example "AnalogInput" by David Cuartielles.
*  Modification is to show analog values as printed horizontal characters per loop inteval.
*  The values are 0 to 63 characters, with zero printed as '.' and other values are '*'.
*  The sensor read value is constrained with 63 by treating the read value as a 0 to 255
*  value, and shifting it right by 3, i.e., dividing by 8,  to limit the range from 0 to 63.
*/

#define PRINTRAW

int sensorPin = A0;    // select the input pin for the potentiometer
int ledPin = 13;      // select the pin for the LED
  // Pin 13: Arduino has an LED connected on pin 13
  // Pin 11: Teensy 2.0 has the LED on pin 11
  // Pin  6: Teensy++ 2.0 has the LED on pin 6
  // Pin 13: Teensy 3.0 has the LED on pin 13
int sensorValue = 0;  // variable to store the value coming from the sensor

void setup() {
  // declare the ledPin as an OUTPUT:
  pinMode(ledPin, OUTPUT);
  Serial.begin(115200);  
}

const char *signalChars[65] = {
  ".", "*", "**", "***", "****", // 0 - 4
  "*****", "******", "*******", "********", "*********", // 5 - 9
  "**********", // 10
  "***********",
  "************",
  "*************",
  "**************",
  "***************", // 15
  "****************",
  "*****************",
  "******************",
  "*******************",
  "********************", // 20
  "*********************",
  "**********************",
  "***********************",
  "************************",
  "*************************", // 25
  "**************************",
  "***************************",
  "****************************",
  "*****************************",
  "******************************", // 30
  "*******************************",
  "********************************",
  "*********************************",
  "**********************************",
  "***********************************", // 35
  "************************************",
  "*************************************",
  "**************************************",
  "***************************************",
  "****************************************", // 40
  "*****************************************",
  "******************************************",
  "*******************************************",
  "********************************************",
  "*********************************************", // 45
  "**********************************************",
  "***********************************************",
  "************************************************",
  "*************************************************",
  "**************************************************", // 50
  "***************************************************",
  "****************************************************",
  "*****************************************************",
  "******************************************************",
  "*******************************************************", // 55
  "********************************************************",
  "*********************************************************",
  "**********************************************************",
  "***********************************************************",
  "************************************************************", // 60
  "*************************************************************",
  "**************************************************************",
  "***************************************************************",
  "****************************************************************", // 64
 };
  
int prevReads[4] = { 0, 0, 0, 0 };
int blinkInterval = 0;
int blinkState = 0;

void loop() {
  // read the value from the sensor:
  sensorValue = analogRead(sensorPin);

  blinkInterval++;
  if (blinkInterval > 10000) {
    blinkInterval = 0;
    blinkState = !blinkState;
    if (blinkState)
      digitalWrite(ledPin, HIGH);
    else
      digitalWrite(ledPin, LOW);
  }

#ifdef PRINTRAW
  if (abs(prevReads[0] - sensorValue) > 2)
    Serial.println(sensorValue);
  prevReads[0] = sensorValue;
#else // PRINTRAW
  unsigned int displayValue = (sensorValue >> 4) & 0x3F;
  if (displayValue == 0) {
    int sumPrevs = 0;
    for (int idx = 0; idx < 4; idx++) {
      sumPrevs += prevReads[idx];
    }
    if (sumPrevs > 0)
      Serial.println(signalChars[displayValue]);
    // else don't keep printing out zeroes
  } else {
    Serial.println(signalChars[displayValue]);
  }

  for (int idx = 0; idx < 3; idx++)
    prevReads[3 - idx] = prevReads[2 - idx];
  prevReads[0] = displayValue;
#endif // PRINTRAW
}
