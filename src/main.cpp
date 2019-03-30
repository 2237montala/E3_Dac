#include <Arduino.h>
//#define FASTLED_ALLOW_INTERRUPTS 0
#include <FastLED.h>
#include <SD.h>
#include <Wire.h> // Enable this line if using Arduino Uno, Mega, etc.
#include <Adafruit_GFX.h>
#include "Adafruit_LEDBackpack.h"

#define DEBUG 1

int engineRPMPin = A0; //Engine rpm pin from lm2907
int secondRPMPin = A1; //Secondard rpm pin from lm2907
int engineRPM = 0; //Value for engine rpm
int secondRPM = 0; //Value for secondary rpm
int mph = 18;
const float reduction = 9.48; //Gear box rpm reduction

const float wheelCircum = 4.712; //in feet


#define rpmArrayLen 100
int rpmArray[rpmArrayLen]; //Array of rpms to be saved
byte collectionCounter = 0;
bool writingData = false;

//Each dial has 24 leds but to make it look like a dial we only use 18
#define NUM_LEDS 48
#define DATA_PIN 6
CRGB leds[NUM_LEDS];
int engLEDMax = 0;
int mphLEDMax = 0;

//4 Digit 7 Segment Display
Adafruit_7segment sevSeg;

//SD Card
File dataFile;
String fileName;
const String fileHeader = "Sample_";
const byte chipSelect = 53;

//Loop settings
unsigned long prevMillisRec = 0;
unsigned long prevMillisLED = 0;
const int recordInerval = 10; //in millis
const int ledInerval = 30; //in millis
unsigned long startTime = 0;

//Other Settings
const byte recordSwitch = 2;
bool stop = 0;
bool startUp = 1;
const byte leftBut = 3;
const byte rightBut = 4;
byte laps = 0;
byte displayMode = 0;
bool resetLap = false;
bool buttonLeftPressed = false;
bool buttonRightPressed = false;

int getRPM(int pin, int samples);
void writeData(int arrayLength, String fileName);
String generateFileName(String fh,int cs, bool skipSDInit);
int updateMPHLED(int start, int numLED, int maxLED, int color);
int map(int x, int in_min, int in_max, int out_min, int out_max);
int updateRPMLED(int start, int numLED, int maxLED, int color);
int milliToMinSec(long milli);
void displayTime(long currTime);
int checkButtons(int currDisplayMode,int buttonOne, int buttonTwo);
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
  pinMode(LED_BUILTIN,OUTPUT);
  pinMode(leftBut,INPUT);
  pinMode(rightBut,INPUT);
  pinMode(12,OUTPUT);

  digitalWrite(LED_BUILTIN, LOW);

  //Check if sd card is present
  fileName = generateFileName(fileHeader,chipSelect,false);
  if(fileName.compareTo("") == 0)
  {
    //Card error
    Serial.println("SD Error");
    while(1)
    {
      digitalWrite(LED_BUILTIN, HIGH);
      delay(1000);
      digitalWrite(LED_BUILTIN,LOW);
      delay(1000);
    }
  }

  FastLED.addLeds<NEOPIXEL, DATA_PIN>(leds, NUM_LEDS);
  FastLED.setBrightness(225* .05);
  leds[23].red = 255;
  leds[47].red = 255;
  FastLED.show();

  sevSeg = Adafruit_7segment();
  sevSeg.begin(0x70);
  sevSeg.setBrightness(16*.5);
  sevSeg.print(0000, DEC);
  sevSeg.writeDisplay();
}

void loop() {
  if(digitalRead(recordSwitch))
  {
    unsigned long currentTime = millis();
    if(stop == 1)
      {
        digitalWrite(LED_BUILTIN,HIGH);
        stop = 0;
        //Serial.println(stop);
        startTime = currentTime;
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

      rpmArray[collectionCounter] = engineRPM;
      rpmArray[collectionCounter+rpmArrayLen/2] = secondRPM;
      collectionCounter++;

      if(collectionCounter > rpmArrayLen/2)
      {
        //After 50 cycles it should be about 0.5 seconds before a write
        //Save data to sd card
        //writeData(rpmArrayLen/2,fileName);
        collectionCounter = 0;
        writingData = true;
      }
      else if(writingData)
      {
        writingData = false;
      }
    }
    if(!writingData && currentTime - prevMillisLED >= ledInerval)
    {
      digitalWrite(12,HIGH);
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
          displayTime(milliToMinSec(currentTime-startTime));
          sevSeg.drawColon(true);
          break;
        case 1:
          sevSeg.drawColon(false);
          sevSeg.print(laps,DEC);
          break;
        case 2:
          //driver time
          sevSeg.print(0000,DEC);
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
      digitalWrite(12,LOW);
    }
  }
  else if(!stop && !digitalRead(recordSwitch))
  {
    //Serial.println("Not recording");
    stop = 1;
    //Serial.println(stop);
    digitalWrite(LED_BUILTIN,LOW);
    fileName = generateFileName(fileHeader,chipSelect,true);
    updateMPHLED(0, 0, mphLEDMax, 1);
    updateRPMLED(6, 0, engLEDMax, 1);
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

void writeData(int arrayLength, String fileName)
{
  dataFile = SD.open(fileName, FILE_WRITE);
  for(int i = 0; i < arrayLength/2; i++)
  {
    dataFile.println(String(rpmArray[i]) + "," + String(rpmArray[(i) + rpmArrayLen/2]));
  }
  dataFile.close();
}

String generateFileName(String fh,int cs, bool skipSDInit)
{
  int fileHeaderCount = 0;
  String fn;
  bool sdReady;

  //Run look if start up is false or the SD library hasn't been
  //initalized yet
  if(!skipSDInit)
    sdReady = SD.begin(cs);
  else
    sdReady = true;

  if(sdReady)
  {
    //SD card is present
    //Check for other files and create new one
    while(SD.exists(fh + String(fileHeaderCount)+".txt"))
    {
      //Keep incrementing the file name until you reach the end
      fileHeaderCount++;
    }
     fn = fh + String(fileHeaderCount) + ".txt";
    Serial.println(fn);
  }
  else
  {
    //No sd card. Notify user
    Serial.println("No SD");
    fn =  "";
  }
  return fn;
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
            //leds[i].g = 255;
            //leds[i].r = 255;
            leds[i] = CRGB::Yellow;
          }
          else
          {
            //leds[i].g = 255;
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

int  milliToMinSec(long milli)
{
  int sec = (milli/1000 % 60);
  int min = (milli/60000) * 100;
  //Serial.println(min + sec);
  return min + sec;
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

int checkButtons(int currDisplayMode,int buttonOne, int buttonTwo)
{
  bool buttonOneState = digitalRead(buttonOne);
  bool buttonTwoState = digitalRead(buttonTwo);
  if(!resetLap && buttonOneState && buttonTwoState)
  {
    //Add lap and reset sev seg timer
    laps++;
    startTime = millis();
    resetLap = true;
    #ifdef DEBUG
      Serial.println("lap reset");
    #endif
  }
  else if(resetLap && !buttonOneState && !buttonTwoState)
  {
    resetLap = false;
  }
  else if(!buttonLeftPressed && buttonOneState)
  {
    //Move display left
    buttonLeftPressed = true;
    if(currDisplayMode != 0)
      currDisplayMode -= 1;

    #ifdef DEBUG
      Serial.println(currDisplayMode);
    #endif
  }
  else if(!buttonRightPressed && buttonTwoState)
  {
    //Move display right
    buttonRightPressed = true;
    if(currDisplayMode != 5)
      currDisplayMode += 1;

    #ifdef DEBUG
      Serial.println(currDisplayMode);
    #endif
  }
  else
  {
    if(!buttonTwoState)
    {
      buttonRightPressed = false;
    }
    if(!buttonOneState)
    {
      buttonLeftPressed = false;
    }
  }


  return currDisplayMode;
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
