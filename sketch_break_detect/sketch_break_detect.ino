int pd=A0;                  //Photodiode to analog pin
int ledPin=13;                   //piezo buzzer to digital pin 13
int sensorPin=0;                 //Readings from sensor to analog pin 0
int low_limit=150;             //Threshold range of an obstacle
int high_limit=200;          //Threshold range of no obstacle
int val;

// State machine for loop processing
enum sensingStates {
  WAIT_START,
  INITIATE_ONE_SHOT,
  WAIT_END,
  SEND_REPORT
};

// jumpstart state machine
int readSensorState = WAIT_START;

void setup()
{
  // pinMode(sensorPin,INPUT);
  pinMode(ledPin,OUTPUT);
  digitalWrite(ledPin,LOW);      //set the buzzer in off mode (initial condition)
  Serial.begin(115200);          //setting serial monitor at a default baund rate of 9600
  delay(100);
  Serial.println("Waiting for start");
}
 
void loop()
{

  if (readSensorState == WAIT_START)
  {
    val = analogRead(sensorPin);

    // debounce light break signal
    if(val > high_limit)
    {
        readSensorState = INITIATE_ONE_SHOT;
        // digitalWrite(ledPin,LOW);     // Buzzer will be in ON state
        // Serial.println("\tbuzz");
        // last_val = val;
     } else {
      Serial.print("Wait Start : "); Serial.println(val);
     }
  }

  
  if (readSensorState == INITIATE_ONE_SHOT)
  {
    /* code to start the AMS measurement here */
    readSensorState = SEND_REPORT;
  }


  if (readSensorState == SEND_REPORT)
  {
    /* code to report + play the note here */
    readSensorState = WAIT_END;
  }


  if (readSensorState == WAIT_END)
  {
    val = analogRead(sensorPin);

    // debounce line break signal
    if(val < low_limit)
    {
      readSensorState = WAIT_START;
      // Serial.println("\tpin");
      // digitalWrite(ledPin,HIGH);      //Buzzer will be in OFF state
      Serial.println("");
    } else {
      Serial.print("Wait End :"); Serial.println(val);
    }
  }
} 
