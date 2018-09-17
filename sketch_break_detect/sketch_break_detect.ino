int pd=A0;                      //Photodiode to analog pin

int buzz=13;                   //piezo buzzer to digital pin 13
int senRead=0;                 //Readings from sensor to analog pin 0
int low_limit=50;             //Threshold range of an obstacle
int high_limit=500;          //Threshold range of no obstacle
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
  pinMode(pd,OUTPUT);
  pinMode(buzz,OUTPUT);
  digitalWrite(pd,HIGH);       //supply 5 volts to photodiode
  digitalWrite(buzz,LOW);      //set the buzzer in off mode (initial condition)
  Serial.begin(9600);          //setting serial monitor at a default baund rate of 9600
  delay(100);
  Serial.println("Waiting for start");
}
 
void loop()
{
  if (readSensorState == WAIT_START)
  {
    val = analogRead(senRead);

    // debounce light break signal
    if(val < low_limit)
    {
        readSensorState = INITIATE_ONE_SHOT;
        digitalWrite(buzz,LOW);     // Buzzer will be in ON state
        Serial.println("\tbuzz");
        // last_val = val;
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
    val = analogRead(senRead);

    // debounce line break signal
    if(val > high_limit)
    {
      readSensorState = WAIT_START;
      Serial.println("\tpin");
      digitalWrite(buzz,HIGH);      //Buzzer will be in OFF state
      Serial.println("");
    }
  }
} 
