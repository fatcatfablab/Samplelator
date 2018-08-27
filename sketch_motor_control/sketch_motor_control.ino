/*
 * Samplelator speed control.
*/

#include "DualVNH5019MotorShield.h"

DualVNH5019MotorShield motors;

#define MAXSPEED_MOTOR 400    // change this value to the highest speed you want to give the motor controller

#define SPEED_UNIT (MAXSPEED_MOTOR / 1024) // rotary pot read values are 0 to 1023

int speedPin = A4;    // select the input pin for the potentiometer
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
  motors.init(); 
}

int blinkInterval = 0;
int blinkState = 0;
int rampSpeed = 0;
int matchedSpeed = 0;


unsigned char got1Fault = 0;
unsigned char got2Fault = 0;

void delayIfFault() {
  unsigned char check1 = motors.getM1Fault();
  unsigned char check2 = motors.getM2Fault();
  if (check1 && !got1Fault) {
    Serial.println("Motor 1 fault");
  }
  if (check2 && !got2Fault) {
    Serial.println("Motor 2 fault");
  }
  if (got1Fault && !check1) {
    Serial.println("Motor 1 clear");
  }
  if (got2Fault && !check2) {
    Serial.println("Motor 2 clear");
  }
  if (check1 || check2) {
    // sleep 60secs if a motor has a fault
    for (int idx = 0; idx < 60; idx++)
      delay(1000);
  } else {
    // delay between motor speed commands
    delay(2);
  }
  got1Fault = check1;
  got2Fault = check2;
}


void loop() {
  blinkInterval++;
  if (blinkInterval > 10000) {
    blinkInterval = 0;
    blinkState = !blinkState;
    if (blinkState)
      digitalWrite(ledPin, HIGH);
    else
      digitalWrite(ledPin, LOW);
  }

  if (got1Fault || got2Fault) {
    delayIfFault();
    return;
  }

  int wantSpeed = analogRead(speedPin);
  if (abs(rampSpeed - wantSpeed) < 8) {
    // reduce chatter
    if (matchedSpeed < 1000) {
      matchedSpeed++;
      return;
    }
    matchedSpeed = 0;
    rampSpeed = wantSpeed;
  } else if (rampSpeed < wantSpeed) {
    rampSpeed += (wantSpeed - rampSpeed) / 2;
  } else if (rampSpeed > wantSpeed) {
    rampSpeed -= (rampSpeed - wantSpeed) / 2;
  }
  int motorSpeed = rampSpeed * SPEED_UNIT;
  motors.setSpeeds(motorSpeed, motorSpeed);
  Serial.print("M1 current: ");
  Serial.print(motors.getM1CurrentMilliamps());
  Serial.print(" === ");
  Serial.print("M2 current: ");
  Serial.print(motors.getM2CurrentMilliamps());
  Serial.println();
  delayIfFault();
}
