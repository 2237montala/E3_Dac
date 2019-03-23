#include <Arduino.h>
#include <FastLED.h>
#include <SD.h>
#include <Wire.h> // Enable this line if using Arduino Uno, Mega, etc.
#include <Adafruit_GFX.h>
#include "Adafruit_LEDBackpack.h"

int engineRPMPin = A0; //Engine rpm pin from lm2907
int secondRPMPin = A1; //Secondard rpm pin from lm2907
int engineRPM = 0; //Value for engine rpm
int secondRPM = 0; //Value for secondary rpm
const int reduction = 9.48; //Gear box rpm reduction

const int wheelCircum = 4.712; //in feet

#define rpmArrayLen 100
int rpmArray[rpmArrayLen]; //Array of rpms to be saved
byte collectionCounter = 0;

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
String fileHeader = "Sample_";
const byte chipSelect = 53;

//Loop settings
unsigned long prevMillisRec = 0;
unsigned long prevMillisLED = 0;
const int recordInerval = 10; //in millis
const int ledInerval = 30; //in millis
unsigned long startTime = 0;

//Other Settings
const byte recordSwitch = 3;
bool stop = 0;
bool startUp = 1;
const byte leftBut = 8;
const byte rightBut = 7;

int getRPM(int pin, int samples);
void writeData(int arrayLength, String fileName);
String generateFileName(String fh,int cs, bool skipSDInit);
int updateMPHLED(int start, int numLED, int maxLED, int color);
int map(int x, int in_min, int in_max, int out_min, int out_max);
int updateRPMLED(int start, int numLED, int maxLED, int color);
int milliToMinSec(long milli);

void setup() {
  //analogReference(EXTERNAL);


  //Initialize values in array to -1
  for(int i = 0; i < rpmArrayLen; i++)
  {
    rpmArray[i] = -1;
  }
  Serial.begin(9600);
  pinMode(recordSwitch, INPUT_PULLUP);
  pinMode(LED_BUILTIN,OUTPUT);

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
  FastLED.setBrightness(225* 0.1);
  leds[1].red = 255;
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
        Serial.println(stop);
        startTime = currentTime;
      }
    if(currentTime - prevMillisRec >= recordInerval)
    {
      //Collect data
      prevMillisRec = currentTime;
      //In theory both rpm collections should be 3 ms @ 10 samples each
      //with 50 microsecond delay between samples
      engineRPM = getRPM(engineRPMPin,10);
      secondRPM = getRPM(secondRPMPin,10);

      rpmArray[collectionCounter] = engineRPM;
      rpmArray[collectionCounter+rpmArrayLen/2] = secondRPM;
      collectionCounter++;



      if(collectionCounter > rpmArrayLen/2)
      {

        //After 50 cycles it should be about 0.5 seconds before a write
        //Save data to sd card
        //writeData(rpmArrayLen/2,fileName);
        collectionCounter = 0;
        Serial.println("Data Saved");
        Serial.println(rpmArray[1]);
        Serial.println(rpmArray[51]);
        //Serial.println(currentTime);
      }
    }
    if(currentTime - prevMillisLED >= ledInerval)
    {
      prevMillisLED = currentTime;


      //Serial.println("Updating display");
      //Calculate Values extra 1000 is for wheelCircum
      //int mph = (wheelCircum * (secondRPM/reduction))/88; //Unrounded mph
      //Serial.println(mph);

      //Update the led rings
      int numLEDtoLight = map(engineRPM,0,675,0,18);
      //Update the leds and set the max led to a new value
      engLEDMax += updateRPMLED(6,numLEDtoLight,engLEDMax,1);

      numLEDtoLight = map(secondRPM,0,675,0,18);
      //Update the leds and set the max led to a new value
      mphLEDMax += updateMPHLED(0,numLEDtoLight,mphLEDMax,1);

      //Update seven segment Display
      int currTime = milliToMinSec(currentTime-startTime);

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

      sevSeg.drawColon(true);
      sevSeg.writeDisplay();
    }
  }
  else if(!stop && !digitalRead(recordSwitch))
  {
    Serial.println("Not recording");
    stop = 1;
    Serial.println(stop);
    digitalWrite(LED_BUILTIN,LOW);
    fileName = generateFileName(fileHeader,chipSelect,true);
  }
}

int getRPM(int pin, int samples)
{
  int avgRPM = 0;
  for(int i = 0; i < samples; i++)
  {
    delayMicroseconds(50);
    avgRPM += analogRead(pin);
  }
  return avgRPM/samples;
}

void writeData(int arrayLength, String fileName)
{
  for(int i = 0; i < arrayLength/2; i++)
  {
    dataFile = SD.open(fileName, FILE_WRITE);
    dataFile.println(String(rpmArray[i]) + " " + String(rpmArray[(i) + rpmArrayLen/2]));
    dataFile.close();
  }
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
  for(int i = start; i < 24; i++)
  {
    leds[i] = CRGB::Black;
  }

  if(color == 1)
  {
    for(int i = 0;i< numLED;i++)
    {
      if(i < 6)
      {
        leds[5-i].g = 255;
      }
      else if(i > 5 && i < 10)
      {
        leds[24+5-i].g = 255;
      }
      else if(i > 9 && i < 15)
      {
        leds[24+5-i].g = 255;
        leds[24+5-i].r = 255;
      }
      else if(i > 14 && i < 18)
      {
        leds[24+5-i].r = 255;
      }
    }

    // for(int i = 12; i < numLED+12;i++)
    // {
    //   if(i >= 12 && i < 15)
    //   {
    //     leds[i].r=225;
    //   }
    //   else if(i > 14 && i < 20)
    //   {
    //     leds[i].g=255;
    //     leds[i].r=255;
    //   }
    //   else if(i > 19 && i < 24)
    //   {
    //     leds[i].g = 255;
    //   }
    //   else if(i > 23)
    //   {
    //     leds[(24-i)*-1].g = 255;
    //   }
    // }
  }
  FastLED.show();
  return 0;
}

int updateRPMLED(int start, int numLED, int maxLED, int color)
{
    start += 24;
    for(int i = start; i < start+24; i++)
    {
      leds[i] = CRGB::Black;
    }

    if(color == 1)
    {
      for(int i = start; i < start + numLED; i++)
      {
        if(i >= start+15)
        {
          leds[i].r = 255;
        }
        else if(i > start+9 && i < start+15)
        {
          leds[i].g = 255;
          leds[i].r = 255;
        }
        else
        {
          leds[i].g = 255;
        }
      }
    }
    FastLED.show();
    return 0;
  }

int map(int x, int in_min, int in_max, int out_min, int out_max) {
  return (x - in_min) * (out_max - out_min) / (in_max - in_min) + out_min;
}

int  milliToMinSec(long milli)
{
  int sec = (milli/1000 % 60);
  int min = (milli/60000) * 100;
  //Serial.println(min + sec);
  return min + sec;
}
