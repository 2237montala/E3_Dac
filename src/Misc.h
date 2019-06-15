class Misc
{
public:
  Misc();
  static int milliToMinSec(long milli);
  static int milliToHourMin(long milli);
  static int calculateTrueEngineRPM(int analogPinValue);
  static int calculateTrueSecondaryRPM(int analogPinValue);
};
