/*
 * Samplelator speed control, using Arduino Mega (Adjusted PWN FREQ!!).
*/

#include "DualVNH5019MotorShield.h"

// FIXME: for single motor, not sure about both PWMs becoming the same, i.e., pin 9
DualVNH5019MotorShield motor(2, 7, 9, 6, A0, 2, 7, 9, 12, A1);
// DualVNH5019MotorShield motor(2, 4, 9, 6, A0, 7, 8, 10, 12, A1); // default values
// DualVNH5019MotorShield motor; // default

#define MINMOTOR_VALUE 0
#define MAXMOTOR_VALUE 400
#define MINPOT_VALUE 0
#define MAXPOT_VALUE 1023

// change this value to the highest speed you want to give the motor controller
#define MAXSPEED_MOTOR MAX_MOTOR_VALUE

// change this value to the amount of speed changes per loop()
#define MAXDELTA_SPEED 5

// Change this value to larger values to see blink for faster processors
// #define MAXBLINK_INTERVAL 10000    // For Teensy 3.6
#define MAXBLINK_INTERVAL 10    // For Arduino Uno

int speedPin = A4;    // select the input pin for the potentiometer
int ledPin = 13;      // select the pin for the LED
  // Pin 13: Arduino has an LED connected on pin 13
  // Pin 11: Teensy 2.0 has the LED on pin 11
  // Pin  6: Teensy++ 2.0 has the LED on pin 6
  // Pin 13: Teensy 3.0 has the LED on pin 13
int blinkInterval = 0;
int blinkState = 0;

int rampSpeed = 0;
int matchedGoal = 0;
unsigned char got1Fault = 0;
unsigned char got2Fault = 0;


void setup() {
  TCCR2B = (TCCR2B & 0b11111000) | 0x01;
 
   // declare the ledPin as an OUTPUT:
  pinMode(ledPin, OUTPUT);
  Serial.begin(115200); 
  motor.init(); 
}


// Report any motor faults and delay till fault disappears
// Note: even though we modified the board as a single motor driver, the docs
//       says to monitor both half-bridge status to determine fault status
void delayIfFault() {
  unsigned char check1 = motor.getM1Fault();
  unsigned char check2 = motor.getM2Fault();
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
  if (blinkInterval > MAXBLINK_INTERVAL) {
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

  unsigned int speedChange;
  unsigned int wantSpeed = analogRead(speedPin);
  if (abs(rampSpeed - wantSpeed) < MAXDELTA_SPEED) {
    // reduce chatter
    if (matchedGoal < 1000) {
      matchedGoal++;
      return;
    }
    matchedGoal = 0;
    rampSpeed = wantSpeed;
  } else if (rampSpeed < wantSpeed) {
    speedChange = (wantSpeed - rampSpeed) / 2;
    speedChange = (MAXDELTA_SPEED < speedChange) ? MAXDELTA_SPEED : speedChange;
    rampSpeed += speedChange;
  } else if (rampSpeed > wantSpeed) {
    speedChange = (rampSpeed - wantSpeed) / 2;
    speedChange = (MAXDELTA_SPEED < speedChange) ? MAXDELTA_SPEED : speedChange;
    if (rampSpeed > wantSpeed * 2) {
      // apply some brake if significantly slowing down
      motor.setM1Brake(MAXDELTA_SPEED);
    }
    rampSpeed -= speedChange;
  }
 
  int motorSpeed;

  if (rampSpeed < MINPOT_VALUE) {
    motorSpeed = MINMOTOR_VALUE;
  } else if (rampSpeed > MAXPOT_VALUE) {
    motorSpeed = MAXMOTOR_VALUE;
  } else {
    motorSpeed = map(rampSpeed, MINPOT_VALUE, MAXPOT_VALUE, MINMOTOR_VALUE, MAXMOTOR_VALUE);
  }
  //  Serial.print("motorSpeed="); Serial.println(motorSpeed);
  if (motorSpeed == 0) {
    // apply brakes
    motor.setM1Brake(MAXDELTA_SPEED);
  } else {
    motor.setM1Speed(motorSpeed); // Note: we only send commands to M1, but actually control both
  }

  delayIfFault();
}
