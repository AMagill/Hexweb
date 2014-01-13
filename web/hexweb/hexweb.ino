/* 
  HexWeb firmware for HexBright FLEX   
*/

#include <math.h>
#include <Wire.h>
#include <EEPROM.h>

// Settings
#define BUF_SIZE                10
#define MAX_MODES               32
#define MODE_OFFSET(x)          (sizeof(Mode)*(x)+1)
#define VERSION                 1
#define OVERTEMP                340
// Pin assignments
#define DPIN_RLED_SW            2
#define DPIN_GLED               5
#define DPIN_PWR                8
#define DPIN_DRV_MODE           9
#define DPIN_DRV_EN             10
#define APIN_TEMP               0
#define APIN_CHARGE             3

struct Mode {
  byte action;
  byte bright;
  word freq;
  byte duty;
  struct {
    byte to;
  } condPush;
  struct {
    byte to;
  } condRel;
  struct {
    byte to;
    word time;
  } condHold;
  struct {
    byte to;
    word time;
  } condIdle;
};

void setup()
{
  // We just powered on!  That means either we got plugged 
  // into USB, or the user is pressing the power button.
  pinMode(DPIN_PWR,      INPUT);
  digitalWrite(DPIN_PWR, LOW);

  // Initialize GPIO
  pinMode(DPIN_RLED_SW,  INPUT);
  pinMode(DPIN_GLED,     OUTPUT);
  pinMode(DPIN_DRV_MODE, OUTPUT);
  pinMode(DPIN_DRV_EN,   OUTPUT);
  digitalWrite(DPIN_DRV_MODE, LOW);
  digitalWrite(DPIN_DRV_EN,   LOW);
  
  // Initialize serial busses
  Serial.begin(9600);
  Wire.begin();

  Serial.println(F("Powered up!"));
  Serial.println(F("Configure me at http://OminousHum.com/hexweb/hexweb.html"));
  Serial.println(F("then paste in a new configuration code here at any time."));
  Serial.println();
}

void loop()
{

}

byte parseHex(char *str, byte len, word *result)
{
  *result = 0;
  for (int i = 0; i < len; i++)
  {
    *result <<= 4;
    if (str[i] >= '0' && str[i] <= '9')
      *result += str[i] - '0';
    else if (str[i] >= 'A' && str[i] <= 'F')
      *result += str[i] - 'A' + 10;
    else if (str[i] >= 'a' && str[i] <= 'f')
      *result += str[i] - 'a' + 10;
    else
    {
      Serial.println(F("ERROR: Problem parsing number in input."));
      return 0;
    }
  }
  return 1;
}

byte tryRead(char *buf, int len)
{
  if (Serial.readBytes(buf, len) < len)
  {
    Serial.println(F("ERROR: Timed out waiting for input."));
    return 0;
  }
  return 1;
}

void serialEvent()
{
  /* 
   *  Read in, parse, validate, and store configuration strings
   *  Example:   [01:O19p02i04000A,O64p03i04000A,OE4p01i04000A,Up00]
   *  
   *  Syntax:    '[' byte:version ':' (mode ',')* mode ']'
   *  mode:      (unchanged | on | flash | dazzle) condition*
   *  unchanged: 'U'
   *  on:        'O' byte:bright
   *  flash:     'F' byte:bright word:frequency byte:duty
   *  dazzle:    'D' byte:bright word:frequency byte:duty
   *  condition: (push | release | hold | idle)
   *  push:      'p' byte:to
   *  release:   'u' byte:to
   *  hold:      'h' byte:to word:time
   *  idle:      'i' byte:to word:time
   */
  
  static char buf[BUF_SIZE];

  // Ignore everything until we see the start char
  if (Serial.read() != '[')
    return;  

  if (!tryRead(buf, 3)) return;
  if (buf[2] != ':')
  {
    Serial.println(F("ERROR: Problem parsing input."));
    return;
  }

  word ver;
  if (!parseHex(buf, 2, &ver)) return;
  if (ver != VERSION)
  {
    Serial.println(F("ERROR: Configuration site version doesn't match this firmware."));
    Serial.print(F("Firmware version: "));
    Serial.print(VERSION);
    Serial.print(F("  Configuration site version: "));
    Serial.println(ver);
    return;
  }
  
  // This is the point of no return.
  // Invalidate the current configuration.
  EEPROM.write(0, 0);  // Zero modes.

  // Reading in modes
  byte totalModes = 0;
  byte largestTo  = 0;
  while (true)
  {
    Mode newMode;
    word parsed1, parsed2, parsed3;
    
    if (!tryRead(buf, 1)) return;
    switch (buf[0])
    {
    case 'U':  // Unchanged
      newMode.action = 'U';
      newMode.bright = 0;
      newMode.freq   = 0;
      newMode.duty   = 0;
      break;
    case 'O':  // On
      if (!tryRead(buf, 2)) return;
      if (!parseHex(buf, 2, &parsed1)) return;
      newMode.action = 'O';
      newMode.bright = parsed1;
      newMode.freq   = 0;
      newMode.duty   = 0;
      break;
    case 'F':  // Flash
      if (!tryRead(buf, 8)) return;
      if (!parseHex(buf,   2, &parsed1)) return;
      if (!parseHex(buf+2, 4, &parsed2)) return;
      if (!parseHex(buf+6, 2, &parsed3)) return;
      newMode.action = 'F';
      newMode.bright = parsed1;
      newMode.freq   = parsed2;
      newMode.duty   = parsed3;
      break;
    case 'D':  // Dazzle
      if (!tryRead(buf, 8)) return;
      if (!parseHex(buf,   2, &parsed1)) return;
      if (!parseHex(buf+2, 4, &parsed2)) return;
      if (!parseHex(buf+6, 2, &parsed3)) return;
      newMode.action = 'D';
      newMode.bright = parsed1;
      newMode.freq   = parsed2;
      newMode.duty   = parsed3;
      break;
    default:
      Serial.println(F("ERROR: Invalid mode in input!"));
      return;
    }

    // Invalidate conditions in struct
    newMode.condPush.to = 0xFF;
    newMode.condRel.to  = 0xFF;
    newMode.condHold.to = 0xFF;
    newMode.condIdle.to = 0xFF;
    
    // Read in conditions, if any
    if (!tryRead(buf, 1)) return;
    while (buf[0] != ']' && buf[0] != ',')
    {
      switch (buf[0])
      {
      case 'p':
        if (!tryRead(buf, 2)) return;
        if (!parseHex(buf, 2, &parsed1)) return;
        if (parsed1 > largestTo) largestTo = parsed1;
        newMode.condPush.to = parsed1;
        break;
      case 'r':
        if (!tryRead(buf, 2)) return;
        if (!parseHex(buf, 2, &parsed1)) return;
        if (parsed1 > largestTo) largestTo = parsed1;
        newMode.condRel.to = parsed1;
        break;
      case 'h':
        if (!tryRead(buf, 6)) return;
        if (!parseHex(buf,   2, &parsed1)) return;
        if (!parseHex(buf+2, 4, &parsed2)) return;
        if (parsed1 > largestTo) largestTo = parsed1;
        newMode.condHold.to   = parsed1;
        newMode.condHold.time = parsed2;
        break;
      case 'i':
        if (!tryRead(buf, 6)) return;
        if (!parseHex(buf,   2, &parsed1)) return;
        if (!parseHex(buf+2, 4, &parsed2)) return;
        if (parsed1 > largestTo) largestTo = parsed1;
        newMode.condIdle.to   = parsed1;
        newMode.condIdle.time = parsed2;
        break;
      default:
        Serial.println(F("ERROR: Invalid condition in input!"));
        return;
      }
      if (!tryRead(buf, 1)) return;
    }
    
    // Lets write this new mode to the EEPROM
    char *pModeBytes = (char*)&newMode;
    for (int i = 0; i < sizeof(Mode); i++)
      EEPROM.write(MODE_OFFSET(totalModes)+i, pModeBytes[i]);
    totalModes++;
    
    if (buf[0] == ']') break;      // We're all done
    if (buf[0] == ',') continue;   // Done with this mode
  }
  
  // Did we see an invalid 'to' reference in any of the conditions?
  if (largestTo > totalModes)
  {
    Serial.print(F("ERROR: A condition referenced a mode that was not defined!"));
    return;
  }
  
  // At this point we must have successfully parsed the entire string.
  // Validate the configuration by writing out the mode count.
  Serial.print(F("Successfully configured with "));
  Serial.print(totalModes);
  Serial.println(F(" modes."));
  EEPROM.write(0, totalModes);
}
