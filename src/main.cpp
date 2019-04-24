#include <Arduino.h>
//#define FASTLED_ALLOW_INTERRUPTS 0
#include <FastLED.h>
#include <SPI.h>
#include "SdFat.h"
#include <Wire.h> // Enable this line if using Arduino Uno, Mega, etc.
#include <Adafruit_GFX.h>
#include "Adafruit_LEDBackpack.h"

#define DEBUG 1

int engineRPMPin = A1; //Engine rpm pin from lm2907
int secondRPMPin = A0; //Secondard rpm pin from lm2907
int engineRPM = 0; //Value for engine rpm
int secondRPM = 0; //Value for secondary rpm
int mph = 18;
const float reduction = 9.48; //Gear box rpm reduction
const float wheelCircum = 23; //in inches


#define rpmArrayLen 100
int rpmArray[rpmArrayLen]; //Array of rpms to be saved
byte collectionCounter = 0;
byte writeCount = 0;
bool writingData = false;

//Each dial has 24 leds but to make it look like a dial we only use 18
#define NUM_LEDS 48
#define DATA_PIN 9
CRGB leds[NUM_LEDS];
int engLEDMax = 0;
int mphLEDMax = 0;

//4 Digit 7 Segment Display
Adafruit_7segment sevSeg;

//SD Card
SdFat sd;
SdFile dataFile;
char fileName[13] = "data000.csv";
const uint8_t fileNameSize = 4;
const byte chipSelect = 10;
bool sdError = false;

//Loop settings
unsigned long prevMillisRec = 0;
unsigned long prevMillisLED = 0;
const int recordInerval = 15; //in millis
const int ledInerval = 30; //in millis
unsigned long lapTime = 0;
unsigned long driverTime = 0;

//Other Settings
const byte recordSwitch = 6;
bool stop = false;
//bool startUp = true;
const byte leftBut = 7;
const byte rightBut = 8;
const byte redLED = 5;
const byte grnLED = 4;
byte laps = 0;
byte displayMode = 0;
bool resetLap = false;
bool butLeftPressed = false;
bool butRightPressed = false;
unsigned int butLeftHoldLen = 5000;
unsigned int butRightHoldLen = 5000;
bool recordMenuOpt = false;
bool recording = false;

int getRPM(int pin, int samples);
void writeData(int arrayLength);
void generateFileName(int cs, bool skipSDInit);
void writeHeader();
int updateMPHLED(int start, int numLED, int maxLED, int color);
int map(int x, int in_min, int in_max, int out_min, int out_max);
int updateRPMLED(int start, int numLED, int maxLED, int color);
int milliToMinSec(long milli);
int milliToHourMin(long milli);
void displayTime(long currTime);
int checkButtons(int currDisplayMode,int butLeft, int butRight);
bool buttonHeld(int buttonHeldLength, int buttonHeldLengthTrigger);
int calculateTrueEngineRPM(int analogPinValue);
int calculateTrueSeconardRPM(int analogPinValue);

void setup() {
  //analogReference(EXTERNAL);
  //Initialize values in array to -1
  for(int i = 0; i < rpmArrayLen; i++)
  {
    rpmArray[i] = -1;
  }
  #ifdef DEBUG
    Serial.begin(9600);
  #endif

  pinMode(recordSwitch, INPUT);
  pinMode(leftBut,INPUT);
  pinMode(rightBut,INPUT);
  pinMode(redLED,OUTPUT);
  pinMode(grnLED,OUTPUT);
  pinMode(2,OUTPUT);

  digitalWrite(redLED, HIGH);
  digitalWrite(grnLED, LOW);

  FastLED.addLeds<NEOPIXEL, DATA_PIN>(leds, NUM_LEDS);
  FastLED.setBrightness(225* .25);
  leds[18].red = 255;
  leds[40].red = 255;
  FastLED.show();

  sevSeg = Adafruit_7segment();
  sevSeg.begin(0x70);
  sevSeg.setBrightness(16*1);
  sevSeg.print(0000, DEC);
  sevSeg.writeDisplay();

  //Check if sd card is present
  generateFileName(chipSelect,false);
  if(sdError)
  {
    //Card error
    Serial.println("SD Error");
    digitalWrite(grnLED,HIGH);
    bool runWithSD = true;
    while(runWithSD)
    {
      //If there is an error alert the user
      for(int i = 0; i < NUM_LEDS; i++)
        leds[i] = CRGB::Red;
      FastLED.show();
      digitalWrite(redLED, LOW);
      delay(1000);
      for(int i = 0; i < NUM_LEDS; i++)
        leds[i] = CRGB::Black;
      FastLED.show();
      digitalWrite(redLED,HIGH);
      delay(1000);

      if(digitalRead(leftBut) && digitalRead(rightBut))
      {
        //If both buttons are pressed then run program without sd saving
        runWithSD = false;

        //Create visual indicator that data will not be saved
        for(int i = 0; i < 6;i++)
        {
          leds[18+i] = CRGB::Red;
          leds[32+i] = CRGB::Red;
        }
} } } }

void loop() {
  unsigned long currentTime = millis();

  if(digitalRead(recordSwitch) || recordMenuOpt)
  {
    if(stop)
      {
        #ifdef DEBUG
          Serial.println("Recording");
        #endif

        dataFile.open(fileName, O_WRONLY | O_CREAT | O_EXCL);
        writeHeader();
        digitalWrite(grnLED, HIGH);
        digitalWrite(redLED,LOW);
        stop = false;
        lapTime = currentTime;
        driverTime = lapTime;
        recording = true;
      }
    else if(!stop && (!digitalRead(recordSwitch) || recordMenuOpt))
      {
        #ifdef DEBUG
          Serial.println("Not Recording");
        #endif

        stop = true;
        recording = false;
        dataFile.close();

        digitalWrite(redLED, HIGH);
        digitalWrite(grnLED,LOW);
        generateFileName(chipSelect,true);
        //updateMPHLED(0, 0, mphLEDMax, 1);
        //updateRPMLED(6, 0, engLEDMax, 1);
        //sevSeg.print(0000,DEC);
        //sevSeg.writeDisplay();
      }
  }

  if(currentTime - prevMillisRec >= recordInerval)
  {
    //Collect data
    prevMillisRec = currentTime;
    //Takes 2.2ms to record 20 samples
    engineRPM = getRPM(engineRPMPin,10);
    secondRPM = getRPM(secondRPMPin,10);

    #ifdef DEBUG
      //Serial.println(engineRPM);
      //Serial.println(secondRPM);
    #endif

    if(recording)
    {
      rpmArray[collectionCounter] = engineRPM;
      rpmArray[collectionCounter+rpmArrayLen/2] = secondRPM;
      collectionCounter++;

      if(collectionCounter > rpmArrayLen/2)
      {
        //After 50 cycles it should be about 0.75 seconds before a write
        //Save data to sd card
        digitalWrite(2, HIGH);
        writeData(rpmArrayLen/2);
        collectionCounter = 0;
        writingData = true;
        digitalWrite(2,LOW);
      }
      else if(writingData)
      {
        writingData = false;
      }
    }
  }

  if(!writingData && currentTime - prevMillisLED >= ledInerval)
  {
    //digitalWrite(2,HIGH);
    prevMillisLED = currentTime;
    //Serial.println("Updating display");
    //Calculate Values extra 1000 is for wheelCircum
    //mph = (wheelCircum * (secondRPM/reduction))/88; //Unrounded mph
    //Serial.println(mph);

    //Update the led rings
    int numLEDtoLight = map(engineRPM,0,655,0,18);
    //Update the leds and set the max led to a new value
    engLEDMax -= updateRPMLED(6,numLEDtoLight,engLEDMax,1);

    numLEDtoLight = map(secondRPM,0,655,0,18);
    //Update the leds and set the max led to a new value
    mphLEDMax -= updateMPHLED(0,numLEDtoLight,mphLEDMax,1);

    //Update seven segment Display
    displayMode = checkButtons(displayMode,leftBut,rightBut);
    switch(displayMode)
    {
      case 0:
        displayTime(milliToMinSec(currentTime-lapTime));
        sevSeg.drawColon(true);
        break;
      case 1:
        sevSeg.drawColon(false);
        sevSeg.print(laps,DEC);
        break;
      case 2:
        //driver time
        displayTime(milliToHourMin(currentTime-driverTime));
        sevSeg.drawColon(true);
        break;
      case 3:
        //engineRPM
        sevSeg.print(calculateTrueEngineRPM(engineRPM),DEC);
        break;
      case 4:
        //secondary rpm
        sevSeg.print(calculateTrueSeconardRPM(secondRPM),DEC);
        break;
      case 5:
        //mph
        sevSeg.print(mph,DEC);
        break;
    }
    sevSeg.writeDisplay();
    //digitalWrite(2,LOW);
  }
}

int getRPM(int pin, int samples)
{
  int avgRPM = 0;
  for(int i = 0; i < samples; i++)
  {
    //delayMicroseconds(50);
    avgRPM += analogRead(pin);
  }
  return avgRPM/samples;
}

void writeData(int arrayLength)
{
  //Serial.println("Writing data");
  for(int i = 0; i < arrayLength/2; i++)
  {
    dataFile.print(rpmArray[i]);
    dataFile.write(',');
    dataFile.print(rpmArray[rpmArrayLen/2]);
    dataFile.println();
  }
  writeCount++;
  //This writes data to the card and updates and internal variables
  //This is the same as closing and opening the file but faster
  if (writeCount == 1)
    dataFile.sync();
    writeCount = 0;
}

void writeHeader() {
  dataFile.print(F("Engine RPM"));
  dataFile.print(F(",Secondary RPM"));
  dataFile.println();
}

void generateFileName(int cs, bool skipSDInit)
{
  //Run look if start up is false or the SD library hasn't been
  //initalized yet
  if(!skipSDInit)
  {
    if(!sd.begin(cs,SD_SCK_MHZ(50)))
    {
      #ifdef DEBUG
        Serial.println("SD cannot be initalized");
      #endif
      sdError = true;
    }
  }
  else
  {
    //SD card is present
    //Check for other files and create new one
    while(sd.exists(fileName))
    {
      if (fileName[fileNameSize + 2] != '9')
      {
        fileName[fileNameSize + 2]++;
      }
      else if (fileName[fileNameSize + 1] != '9')
      {
        fileName[fileNameSize + 2] = '0';
        fileName[fileNameSize + 1]++;
      }
      else if (fileName[fileNameSize] != '9')
      {
        fileName[fileNameSize + 1] = '0';
        fileName[fileNameSize]++;
      }
       else
      {
        #ifdef DEBUG
          Serial.println("Can't create file name");
        #endif
      }
    }
    #ifdef DEBUG
      Serial.println(fileName);
    #endif
  }
}

int updateMPHLED(int start, int numLED, int maxLED, int color)
{
  int diff = maxLED - numLED;

  if(diff > 0)
  {
    for(int i = numLED; i < maxLED; i++)
    {
      if(i < 6)
      {
        leds[5-i] = CRGB::Black;
      }
      else if(i > 5)
        leds[24+5-i] = CRGB::Black;
    }
  }
  else
  {
    if(color == 1)
    {
      for(int i = 0;i< numLED;i++)
      {
        if(i < 6)
        {
          //leds[5-i].g = 255;
          leds[5-i] = CRGB::Green;
        }
        else if(i > 5 && i < 11)
        {
          //leds[24+5-i].g = 255;
          leds[24+5-i] = CRGB::Green;
        }
        else if(i > 10 && i < 15)
        {
          //leds[24+5-i].g = 255;
          //leds[24+5-i].r = 255;
          leds[24+5-i] = CRGB::Yellow;
        }
        else if(i > 14 && i < 18)
        {
          //leds[24+5-i].r = 255;
          leds[24+5-i] = CRGB::Red;
        }
      }
    }
  }
  FastLED.show();
  return diff;
}

int updateRPMLED(int start, int numLED, int maxLED, int color)
{
    int diff = maxLED - numLED;
    start += 24;
    if(diff > 0)
    {
      //Turn off leds
      for(int i = maxLED; i > numLED; i--)
      {
        leds[i+start-1] = CRGB::Black;
      }
    }
    else
    {
      //Turn on leds
      if(color == 1)
      {
        for(int i = start; i < start + numLED; i++)
        {
          if(i >= start+15)
          {
            leds[i] = CRGB::Red;
          }
          else if(i > start+10 && i < start+15)
          {
            leds[i] = CRGB::Yellow;
          }
          else
          {
            leds[i] = CRGB::Green;
          }
        }
      }
    }
    FastLED.show();
    return diff;
  }

int map(int x, int in_min, int in_max, int out_min, int out_max)
{
  return (x - in_min) * (out_max - out_min) / (in_max - in_min) + out_min;
}

int milliToMinSec(long milli)
{
  int sec = (milli/1000 % 60);
  int min = (milli/60000) * 100;
  return min + sec;
}

int milliToHourMin(long milli)
{
  int min = (milli/60000) * 100;
  int hr  = (min/60);
  return hr+min;
}

void displayTime(long currTime)
{
  for(int i = 0;i < 5; i++)
  {
    if(i < 2)
    {
      sevSeg.writeDigitNum(4-i,currTime % 10);
      currTime = currTime / 10;
    }

    if(i >= 3)
    {
      sevSeg.writeDigitNum(4-i,currTime % 10);
      currTime = currTime / 10;
    }
  }
}

int checkButtons(int currDisplayMode,int butLeft, int butRight)
{
  bool butLeftState = digitalRead(butLeft);
  bool butRightState = digitalRead(butRight);
  //If the buttons are being held then increment the time they have been
  if(butLeftState)
    butLeftHoldLen += ledInerval;
  else
    butLeftHoldLen = 0;

  if(butRightState)
    butRightHoldLen += ledInerval;
  else
    butRightHoldLen=0;

  if(!resetLap && butLeftState && butRightState)
  {
    //Add lap and reset sev seg timer
    laps++;
    lapTime = millis();
    resetLap = true;
    #ifdef DEBUG
      Serial.println("lap reset");
    #endif

    if(currDisplayMode == 2)
    {
      //In driver time
      driverTime = millis();
    }
  }
  else if(resetLap && !butLeftState && !butRightState)
  {
    resetLap = false;
  }
  else if(!butLeftPressed && butLeftState)
  {
    //Move display left
    butLeftPressed = true;
    if(currDisplayMode != 0)
      currDisplayMode -= 1;

    #ifdef DEBUG
      Serial.println(currDisplayMode);
    #endif
  }
  else if(!butRightPressed && butRightState)
  {
    //Move display right
    butRightPressed = true;
    if(currDisplayMode != 5)
      currDisplayMode += 1;

    #ifdef DEBUG
      Serial.println(currDisplayMode);
    #endif
  }
  else
  {
    if(!butRightState)
    {
      butRightPressed = false;
    }
    if(!butLeftState)
    {
      butLeftPressed = false;
    }
  }
  return currDisplayMode;
}

bool buttonHeld(int buttonHeldLength, int buttonHeldLengthTrigger)
{
  #ifdef DEBUG
    Serial.println("Button held");
  #endif
  return buttonHeldLength >= buttonHeldLengthTrigger;
}

int calculateTrueEngineRPM(int analogPinValue)
{
  //The conversion factor is 6.218
  //long rpm = (analogPinValue * 6218) / 1000;
  int rpm = (analogPinValue)* 6.218;
  #ifdef DEBUG
    //Serial.println(rpm);
  #endif
  return rpm;
}

int calculateTrueSeconardRPM(int analogPinValue)
{
  //The conversion factor is 14.49
  //long rpm = (analogPinValue * 6218) / 1000;
  int rpm = (analogPinValue)* 14.49;
  #ifdef DEBUG
    //Serial.println(rpm);
  #endif
  return rpm;
}
