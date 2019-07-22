#include <Arduino.h>
#include <FastLED.h>
#include <SPI.h>
#include "SdFat.h"
#include <Wire.h> // Enable this line if using Arduino Uno, Mega, etc.
#include <Adafruit_GFX.h>
#include "Adafruit_LEDBackpack.h"
#include "Misc.h"

#define DEBUG 0

int engineRPMPin = A1; //Engine rpm pin from lm2907
int secondRPMPin = A0; //Secondard rpm pin from lm2907
int engineRPM = 0; //Value for engine rpm
int secondRPM = 0; //Value for secondary rpm
const float reduction = 9.48; //Gear box rpm reduction
const int wheelDia = 23; //in inches
int rpmToMphFactor = 0; //Factor to multiply rpm value to get mph.
//This number needs to be divided by 10 later
int mph = 0;

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
const int recordInerval = 10; //in millis
const int ledInerval = 30; //in millis
unsigned long lapTime = 0;
unsigned long driverTime = 0;

//Other Settings
const byte recordSwitch = 6;
bool stop = false;
const byte leftBut = 8;
const byte rightBut = 7;
const byte redLED = 5;
const byte grnLED = 4;
byte laps = 0;
byte displayMode = 0;
bool resetLap = false;

bool butLeftPressed = false;
bool butRightPressed = false;
bool buttonActive = false;
bool longPressActive = false;
unsigned long buttonHeldTimer = 500;
unsigned long buttonHeldLength = 0;

//Sets how long the recording will be from pressing the steering wheel buttons
const long recordMenuLen = 15*1000;
long recordMenuRecAmt = 0; //How long it has been recording
bool recordMenuOpt = false;
bool recording = false;

int getRPM(int pin, int samples);
void writeData(int arrayLength);
void generateFileName(int cs, bool skipSDInit);
void writeHeader();
int updateMPHLED(int start, int numLED, int maxLED, int color);
int map(int x, int in_min, int in_max, int out_min, int out_max);
int updateRPMLED(int start, int numLED, int maxLED, int color);
void displayTime(long currTime);
int checkButtons(int currDisplayMode,int butLeft, int butRight);

void setup() {
  Serial.begin(115200);

  rpmToMphFactor = (60*wheelDia*3.14)/63;
  //analogReference(EXTERNAL);
  //Initialize values in array to -1
  memset(rpmArray,0,rpmArrayLen);

  pinMode(recordSwitch, INPUT);
  pinMode(leftBut,INPUT);
  pinMode(rightBut,INPUT);
  pinMode(redLED,OUTPUT);
  pinMode(grnLED,OUTPUT);
  //pinMode(2,OUTPUT);

  digitalWrite(redLED, HIGH);
  digitalWrite(grnLED, LOW);

  //CFastLED::addLeds<NEOPIXEL,DATA_PIN>(leds,NUM_LEDS);
  FastLED.addLeds<NEOPIXEL, DATA_PIN>(leds, NUM_LEDS);
  FastLED.setBrightness(255* .65);
  //leds[18].red = 255;
  //leds[40].red = 255;
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
    //Serial.println("SD Error");
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
        for(int i = 0; i < 4;i++)
        {
          leds[7+i] = CRGB::Red;
          FastLED.show();
        }
      }
    }
  }
}

void loop() {
  unsigned long currentTime = millis();

  if(digitalRead(recordSwitch) || recordMenuOpt)
  {
    if(stop)
      {
        #ifdef DEBUG
          //Serial.println("Recording");
        #endif

        for(int i = 0; i < 4;i++)
        {
          leds[25+i] = CRGB::Red;
        }
        FastLED.show();

        dataFile.open(fileName, O_WRONLY | O_CREAT | O_EXCL);
        writeHeader();
        digitalWrite(grnLED, LOW);
        digitalWrite(redLED,HIGH);
        stop = false;
        lapTime = currentTime;
        driverTime = lapTime;
        recording = true;

        if(recordMenuOpt && !digitalRead(recordSwitch))
        {
          //The recording command was from the steering wheel
          recordMenuRecAmt = millis();
        }
      }
  }
  else if(!stop && ((!digitalRead(recordSwitch) || recordMenuOpt)
      || (currentTime - recordMenuRecAmt > recordMenuLen)))
    {
      //To stop recording the record switch has to be off or the steering wheel
      //buttons had to pressed or the recording amount has been reached for the steering wheel
      #ifdef DEBUG
        //Serial.println("Not Recording");
      #endif

      stop = true;
      recording = false;
      dataFile.close();

      //Create a visual indicator
      for(int i = 0; i < 4;i++)
      {
        leds[25+i] = CRGB::Green;
      }
      FastLED.show();

      digitalWrite(redLED, LOW);
      digitalWrite(grnLED,HIGH);
      generateFileName(chipSelect,true);
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

      if(collectionCounter > (rpmArrayLen/2)-1)
      {
        //After 50 cycles it should be about 0.5 seconds before a write
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
    prevMillisLED = currentTime;
    //Calculate mph by dividing the secondary rpm by the gear reduction
    //the multiplying it by the conversion factor then dividing by 1000
    //The 1000 is to make the factor a integer as integer math is fast on the
    //arduino that float math
    mph = ((Misc::calculateTrueSecondaryRPM(secondRPM)/reduction)*rpmToMphFactor)/1000;
    //Serial.println(mph);

    //Update the led rings
    int numLEDtoLight = map(engineRPM,0,655,0,18);
    //Update the leds and set the max led to a new value
    engLEDMax -= updateRPMLED(6,numLEDtoLight,engLEDMax,1);

    numLEDtoLight = map(mph,0,40,0,18);
    //Update the leds and set the max led to a new value
    mphLEDMax -= updateMPHLED(0,numLEDtoLight,mphLEDMax,1);

    //Update seven segment Display
    displayMode = checkButtons(displayMode,leftBut,rightBut);
    switch(displayMode)
    {
      case 0:
        //displayTime(milliToMinSec(currentTime-lapTime));
        displayTime(Misc::milliToMinSec(currentTime-lapTime));
        sevSeg.drawColon(true);
        break;
      case 1:
        sevSeg.drawColon(false);
        sevSeg.print(laps,DEC);
        break;
      case 2:
        //driver time
        displayTime(Misc::milliToHourMin(currentTime-driverTime));
        sevSeg.drawColon(true);
        break;
      case 3:
        //engineRPM
        sevSeg.print(Misc::calculateTrueEngineRPM(engineRPM),DEC);
        break;
      case 4:
        //secondary rpm
        sevSeg.print(Misc::calculateTrueSecondaryRPM(secondRPM),DEC);
        break;
      case 5:
        //mph
        sevSeg.print(mph,DEC);
        break;
      case 6:
                              //GFEDCBA
        sevSeg.drawColon(false);
        sevSeg.writeDigitRaw(0,B0110001); //R
        sevSeg.writeDigitRaw(1,B1111001); //E
        sevSeg.writeDigitRaw(3,B0111001); //C
        sevSeg.writeDigitRaw(4,B0000000);
        break;
    }
    sevSeg.writeDisplay();
  }

  if(stop && Serial.available()>0)
  {
    //If the DAQ isn't currently recording data and if there is
    //any serial data then check if its the send data command
    char data = Serial.read();
    if(data == 't') //t for trasnfer
    {
      //Serial.begin(115200);
      Serial.println("Ready");
      //Send the contents of the sd card
      //Open base directory
      //See if a file exists
      //If it does the print every line to Serial
      //Add a delim value and repeat
      SdFile root;
      SdFile file;

      if(root.open("/"))
      {
        while(file.openNext(&root,O_RDONLY))
        {
          //While there are still files in the queue
          char fName[13];
          char line[25];
          file.getName(fName,13);
          if(fName[0]=='d' && fName[1] == 'a')
          {
            Serial.println(fName);
            while(file.fgets(line,sizeof(line))>0)
            {
              Serial.print(line);
            }
            Serial.println("/");
          }
          file.close();
          //
        }
        Serial.println("//");
      }
      //Flash leds for error
    }
    else if(data == 'd')
    {
      Serial.println("Deleting");

      SdFile root;
      SdFile file;
      //Delete the files on the sd card
      if(root.open("/"))
      {
        while(file.openNext(&root,O_RDONLY))
        {
            char fName[13];
            file.getName(fName,13);
            file.close();
            sd.remove(fName);
          }
        }
      }
    Serial.println("Done");
    Serial.flush();
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
  for(int i = 0; i < arrayLength; i++)
  {
    dataFile.print(rpmArray[i]);
    dataFile.write(',');
    dataFile.print(rpmArray[arrayLength + i]);
    dataFile.println();
  }
  writeCount++;
  //This writes data to the card and updates and internal variables
  //This is the same as closing and opening the file but faster
  if (writeCount == 3)
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
        //Serial.println("SD cannot be initalized");
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
          //Serial.println("Can't create file name");
        #endif
      }
    }
    #ifdef DEBUG
      //Serial.println(fileName);
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

void displayTime(long currTime)
{
  for(int i = 0;i < 5; i++)
  {
    //Index 2 is the colon so we skip it
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

  if(butLeftState == true)
  {
    //If left button is pressed change its state
    if(buttonActive == false)
    {
      buttonActive = true;
      buttonHeldLength = millis();
      //Serial.println("Left Pressed");
    }
    butLeftPressed = true;
    //Serial.println("Left Pressed");
  }

  if(butRightState == true)
  {
    //If right button is pressed change its state
    if(buttonActive == false)
    {
      buttonActive = true;
      buttonHeldLength = millis();
      //Serial.println("Right button pressd");
    }
    butRightPressed=true;
  }

  if((buttonActive == true && millis() - buttonHeldTimer > buttonHeldLength)
      && longPressActive == false)
  {
    //If any button is pressed and the button held timer is greater than the
    //held button length. Enable the long press
    longPressActive = true;

    //Now do the long press action
    if((currDisplayMode >= 0 && currDisplayMode <= 2) && butLeftState && butRightState)
    {
      //Add lap and reset sev seg timer
      laps++;
      lapTime = millis();
      resetLap = true;
      #ifdef DEBUG
        //Serial.println("lap reset");
      #endif

      if(currDisplayMode == 2)
      {
        //In driver time
        driverTime = millis();
        //Don't want to increase laps if the driver time reset
        laps--;
      }
    }
    else if(currDisplayMode == 6 && butLeftState && butRightState)
    {
      //If on the record menu and holding both buttons down, start recording
      recordMenuOpt = !recordMenuOpt;
    }
  }

  if(buttonActive == true && (butLeftState == false || butRightState == false))
  {
    //If a button was pressed in the previous loop but now none are pressed
    //Then disable the long press and change the button state vars
    if(longPressActive == true)
    {
      longPressActive = false;
    }
    else
    {
      //Do you short press action
      if(butLeftState)
      {
        //Move display left
        //butLeftPressed = true;
        if(currDisplayMode != 0)
          currDisplayMode -= 1;

        #ifdef DEBUG
          //Serial.println(currDisplayMode);
        #endif
      }
      else if(butRightState)
      {
        //Move display right
        //butRightPressed = true;
        if(currDisplayMode != 6)
          currDisplayMode += 1;

        #ifdef DEBUG
          //Serial.println(currDisplayMode);
        #endif
      }
    }

    // if(butLeftState) butLeftPressed = false;
    // if(butRightState) butRightPressed = false;
    buttonActive = false;
    butLeftPressed = false;
    butRightPressed = false;
  }
  return currDisplayMode;
}
