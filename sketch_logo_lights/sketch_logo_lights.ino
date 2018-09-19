// This is based off of the code from
// https://github.com/fatcatfablab/Space-Invaders-Shooting-Gallery/blob/master/hardware/HotShotLogoBoard/HotShotLogoBoard.ino
// We've randomized the commands for the Samplelator logo display

#include <Adafruit_DotStar.h>


// Because conditional #includes don't work w/Arduino sketches...
#include <SPI.h>         // COMMENT OUT THIS LINE FOR GEMMA OR TRINKET
//#include <avr/power.h> // ENABLE THIS LINE FOR GEMMA OR TRINKET
#include <math.h>

#define NUMPIXELS 153 // Number of LEDs in strip

// Here's how to control the LEDs from any two pins:
#define DATAPIN    4
#define CLOCKPIN   5
Adafruit_DotStar strip = Adafruit_DotStar(NUMPIXELS, DATAPIN, CLOCKPIN);

#define NUM_ROWS 17
#define NUM_COLS 9

// Artisinal handcrafted asymetric matrix

int ledMatrix[NUM_ROWS][NUM_COLS] = {
  {  0,   30,   31,   63,  64,  97,  98,  130,  131  },
  {  1,   29,  32,   62,  65,  96,   99,  129,  132   },
  {  2,   28,  33,   61,  66,  95,  100,  128,  133   },
  {  3,   27,  34,   60,  67,  94,  102,  127,  134   },
  {  4,   26,   35,  59,  68,  93,  103,  126,  135  },
  {  5,   25,   36,  58,  69,  92,  104,  125,  136  },
  {  6,   24,  37,  57,   70,  91,  105,  124,  137 },
  {  7,   23,  38,  56,   71,  90,  106,  123,  138  },
  {  8,   22,  39,  55,   72,  89,  107,  122,  139  },
  {  9,   21,  40,  54,   73,  88,  108,  121,  140  },
  {  10,   20,  41,  53,  74,  87, 109,  120,  141 },
  {  11,   19,  42,  52,  75,  86, 110,  119, 142 },
  {  12,   18,  43,  51,  76,  85, 111,  118, 143  },
  {  13,   17,  44,  50,  77,  84, 112, 117, 144 },
  {  14,  16,   45,  49,  78,  83, 113, 116, 145 },
  {  152,  15,  46,  48,  79,  82, 114, 115, 152},
  {  152,  152,  152,  47,  80,  81, 152, 152, 152  } 
};

int wheelData[8][9] = {
  { 67, 68, 69, 70, 71, 72, 73, 74, 75 }, 
  { 2, 27, 35, 58, 71, 89, 108, 120, 142 },
  { 7, 23, 38, 56, 71, 90, 106, 123, 138 },
  { 12, 19, 41, 54, 71, 91, 104, 126, 134 },
  { 67, 68, 69, 70, 71, 72, 73, 74, 75 },
  { 2, 27, 35, 58, 71, 89, 108, 120, 142 },
  { 7, 23, 38, 56, 71, 90, 106, 123, 138},
  { 12, 19, 41, 54, 71, 91, 104, 126, 134 }
};


void setup() {
  Serial.begin(9600);

  strip.begin(); // Initialize pins for output
  strip.show();  // Turn all LEDs off ASAP
  strip.setBrightness(255);
  delay(3000);
  
  flash(5);
  
  Serial.println(ledMatrix[0][1]);
  randomSeed(millis() % 1023);
}


void loop() {
  bool gotCmd = false;
  int rndCmd = random(7);

  if (Serial.available() > 0) {
    rndCmd = Serial.read() - '0';
    Serial.print("Override cmd : ");
    Serial.println(rndCmd);
    gotCmd = true;
  }

  switch(rndCmd) {
    case 0:
      off();
      break;
    case 1:
      sleep();
      break;
    case 2:
      noise();
      break;
    case 3:
      flash(11);
      break;
    case 4:
      konami(1);
      break;
    case 5:
      diagTL();
      break;
    case 6:
      wheels();
      break;
    default:
      gotCmd = false;
      break;
  }

  if (gotCmd) {
    Serial.println("Nap time...");
    delay(32367);
    gotCmd = false;
  }
}


void wipe( int b, int t, char* d ) {
  if (d == "u") {
    for (int x = 16; x >= 0; x--) {
      for (int y = 8; y >=0; y--) {
        strip.setPixelColor(ledMatrix[x][y], b);
      }
      strip.show();
      delay(t);
    }
  }

  if (d == "d") {
    for (int x = 0; x <=16; x++) {
      for (int y = 0; y <= 8; y++) {
        strip.setPixelColor(ledMatrix[x][y], b);
      }
      strip.show();
     delay(t);
    }
  }

  if (d == "r") {
    for (int y = 0; y <= 8; y++) {
      for(int x = 0; x <= 16; x++) {
        strip.setPixelColor(ledMatrix[x][y], b);
      }
      strip.show();
      delay(t);
    }
  }

  if (d == "l") {
    for (int y = 8; y >= 0; y--) {
      for (int x = 0; x <= 16; x++) {
        strip.setPixelColor(ledMatrix[x][y], b);
      }
      strip.show();
      delay(t);
    }
  }
}


void wipes( int t ) {
  Serial.println("wipes");
  wipe(255 ,t, "d");
  wipe(0, t, "d");
}


void wheels() {
  Serial.println("wheels...");
  int rndWheels = random(1, 20);
  while( rndWheels-- > 0 ) {
    for (int jdx = 0; jdx < 8; jdx++) {
      for (int idx = 0; idx < 9; idx++) {
        strip.setPixelColor(wheelData[jdx][idx], 255);
      }
      strip.show();
      delay(200);
    }
  }
}


void noise() {
  int rndNoise = random(1,20);
  while( rndNoise-- > 0 ) {
    for (int x = 0; x <= 150; x++) {
      int rawval = analogRead(A0);
      int b = map (rawval, 0, 5, 0, 255);
      strip.setPixelColor(x, b);
    }
    strip.show();
  }
}


void flash( int t) {
  Serial.println("flash...");
  int h = 0;
  while ( h <= t-1) {
    for (int x = 0; x <= 149 ; x++) {
      strip.setPixelColor(x, 255);
    }
    strip.show();
    delay(100);
    for (int x = 0; x <= 149 ; x++) {
      strip.setPixelColor(x, 0);
    }
    strip.show();
    delay(100);
    h++;
  }
}


void sleep() {
  Serial.println("sleep...");
  int rndSleep = random(1,20);
  while( rndSleep-- > 0 ) {
    float b = (exp(sin(millis() / 2000.0 * PI)) - 0.36787944) * 90.0;
    for (uint16_t i = 0; i < strip.numPixels(); i++) {
      strip.setPixelColor(i, b);
    }
    strip.show();
  }
}


void konami ( int t) {
  Serial.println("konami...");
  wipe(255 ,t, "u");
  wipe(0, t, "u");
  wipe(255 ,t, "u");
  wipe(0, t, "u");
  wipe(255 ,t, "d");
  wipe(0, t, "d");
  wipe(255 ,t, "d");
  wipe(0, t, "f");
  wipe(255 , t,"l");
  wipe(0,t, "l");
  wipe(255 , t,"r");
  wipe(0,t, "r");
  wipe(255 , t,"l");
  wipe(0,t, "l");
  wipe(255 , t,"r");
  wipe(0,t, "r");
}


void diagTL() {
  Serial.println("diagTL");
  int i, j, t;
  for ( t = 0; t<NUM_ROWS+NUM_COLS; ++t) {
    for( i=t, j=0; i>=0 ; --i, ++j) {
      if( (i<NUM_ROWS) && (j<NUM_COLS) ) { 
        strip.setPixelColor(ledMatrix[i][j], 255);
      }
    } 
    strip.show();  
    delay(2);    
  }
  for ( t = 0; t<NUM_ROWS+NUM_COLS; ++t) {
    for( i=t, j=0; i>=0 ; --i, ++j) {
      if( (i<NUM_ROWS) && (j<NUM_COLS) ) { 
        strip.setPixelColor(ledMatrix[i][j], 0);
      }
    } 
    strip.show();  
    delay(2);    
  }
}


void off() { 
  Serial.println("off...");
  for ( int u =0; u <= 152;u++) {
    strip.setPixelColor(u, 0);
  }
  strip.show();
  
}
