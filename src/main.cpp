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
//Actual value is 9.48 but decimals are slow in math
const int reduction = 9.48 * 100; //Gear box rpm reduction

//Actual value is 4.712 but decimals are slow in math
const int wheelCircum = 4.712 * 1000; //in feet

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
unsigned long previousMillis = 0;
const int recordInerval = 10; //in millis
const int ledInerval = 30; //in millis

//Other Settings
const byte recordSwitch = 3;
bool stop = 0;
bool startUp = 1;
const byte leftBut = 8;
const byte rightBut = 7;

int getRPM(int pin, int samples);
void writeData(int arrayLength, String fileName);
String generateFileName(String fh,int cs, bool skipSDInit);
int updateLED(int numLED, int maxLEDNum);

void setup() {
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
  FastLED.setBrightness(225* 0.4);

  sevSeg = Adafruit_7segment();

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
      }
    if(currentTime - previousMillis >= recordInerval)
    {
      //Collect data

      //In theory both rpm collections should be 3 ms @ 10 samples each
      //with 50 microsecond delay between samples
      engineRPM = getRPM(engineRPMPin,10);
      secondRPM = getRPM(secondRPMPin,10);

      rpmArray[collectionCounter] = engineRPM;
      rpmArray[collectionCounter+rpmArrayLen/2] = secondRPM;
      collectionCounter++;

      if(collectionCounter > rpmArrayLen/2)
      {
        //After 100 cycles it should be about 1 second per write
        //Save data to sd card
        writeData(rpmArrayLen/2,fileName);
        collectionCounter = 0;
        Serial.println("Data Saved");
      }
    }
    else if(currentTime - previousMillis >= ledInerval)
    {
      //Calculate Values extra 1000 is for wheelCircum
      int mph = (wheelCircum * (secondRPM/reduction/100))/88*1000; //Unrounded mph

      //Update the led rings
      int numLEDtoLight = map8(engineRPM, 0, 18);
      //Update the leds and set the max led to a new value
      engLEDMax += updateLED(numLEDtoLight,engLEDMax);

      numLEDtoLight = map8(mph, 0, 18);
      //Update the leds and set the max led to a new value
      mphLEDMax += updateLED(numLEDtoLight,mphLEDMax);

      //Display the leds
      FastLED.show();

      //Update seven segment Display
      sevSeg.print(1000, DEC);
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
    delayMicroseconds(50);
    avgRPM += analogRead(pin);
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

int updateLED(int numLED, int maxLEDNum)
{
  int diff = maxLEDNum - numLED;
  if(diff > 0)
  {
    //Need to turn off leds
    for(int i = numLED; i < maxLEDNum; i++)
    {
      leds[i] = 0;
    }
  }
  else
  {
    //Need to add more on leds
    for(int i = maxLEDNum; i < numLED; i++)
    {
      leds[i] = 0xFF44DD;
    }
  }
  return diff;
}
