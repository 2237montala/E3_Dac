#include <Arduino.h>
#include <Adafruit_NeoPixel.h>
#include <TM1637Display.h>
#include <SD.h>

int engineRPMPin = A0; //Engine rpm pin from lm2907
int secondRPMPin = A1; //Secondard rpm pin from lm2907
int engineRPM = 0; //Value for engine rpm
int secondRPM = 0; //Value for secondary rpm
#define reduction 3 //Gear box rpm reduction
const float wheelCircum = 4.712; //in feet

#define rpmArrayLen 100
int rpmArray[rpmArrayLen]; //Array of rpms to be saved
byte collectionCounter = 0;

//Each dial has 24 leds but to make it look like a dial we only use 18
byte numLED = 18;
byte mphDialPin = 6;
byte rpmDialPin = 7;
//Adafruit_NeoPixel mphDial = Adafruit_NeoPixel(numLED, mphDialPin, NEO_GRBW + NEO_KHZ800);
//Adafruit_NeoPixel rpmDial = Adafruit_NeoPixel(numLED, rpmDialPin, NEO_GRBW + NEO_KHZ800);

//SD Card
File dataFile;
String fileName;
String fileHeader = "Sample_";
const byte chipSelect = 53;

//Loop settings
unsigned long previousMillis = 0;
const int interval = 10; //in millis

//Other Settings
const byte recordSwitch = 3;
bool stop = 0;
bool startUp = 1;

int getRPM(int pin, int samples);
void writeData(int arrayLength, String fileName);
String generateFileName(String fh,int cs, bool skipSDInit);

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

  //Neopixel start up code
  //mphDial.setBrightness(25);
  //mphDial.begin();
  //mphDial.show(); // Initialize all pixels to 'off'
  //rpmDial.setBrightness(25);
  //rpmDial.begin();
  //rpmDial.show();


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

    if(currentTime - previousMillis >= interval)
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

      //int halfShaftRPM = (secondRPM / reduction);

      //Calculate Values
      //float mph = (wheelCircum * halfShaftRPM)/88; //Unrounded mph
      //mph = float(long(mph*100)/100); //Rounded to 2 decimal places


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
