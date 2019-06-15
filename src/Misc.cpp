#include "Misc.h"

Misc::Misc(){}

int Misc::milliToMinSec(long milli)
{
  int sec = (milli/1000 % 60);
  int min = (milli/60000) * 100;
  return min + sec;
}

int Misc::milliToHourMin(long milli)
{
  int min = (milli/60000);
  int hr  = (min/60);
  return hr+min;
}

int Misc::calculateTrueEngineRPM(int analogPinValue)
{
  //The conversion factor is 6.218
  //long rpm = (analogPinValue * 6218) / 1000;
  int rpm = (analogPinValue)* 6.218;
  #ifdef DEBUG
    //Serial.println(rpm);
  #endif
  return rpm;
}

int Misc::calculateTrueSecondaryRPM(int analogPinValue)
{
  //The conversion factor is 14.49
  //long rpm = (analogPinValue * 6218) / 1000;
  int rpm = (analogPinValue)* 14.49;
  #ifdef DEBUG
    //Serial.println(rpm);
  #endif
  return rpm;
}
