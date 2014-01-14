/* 
  HexWeb firmware for HexBright FLEX   
*/

#include <math.h>
#include <Wire.h>
#include <EEPROM.h>

// Settings
#define MAX_MODES               32
#define EEPROM_COUNT            0
#define EEPROM_MODE(x)          (sizeof(Mode)*(x)+1)
#define EEPROM_TEXT             EEPROM_MODE(MAX_MODES)
#define MAX_TEXT                (512 - EEPROM_TEXT)
#define BUF_SIZE                10
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
  word period;
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

byte nModes = 0;
Mode conf[MAX_MODES];

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
  
  // Initialize 'off' mode configuration
  conf[0].action = 'Z';
  conf[0].bright = 0;
  conf[0].condPush.to = 1;
  conf[0].condRel.to  = 0xFF;
  conf[0].condHold.to = 0xFF;
  conf[0].condIdle.to = 0xFF;

  Serial.println(F("Powered up!"));
  
  if (readConfig())
  {  
    Serial.print(F("Restored configuration with "));
    Serial.print(nModes);
    Serial.println(F(" modes."));
  }
  else
  {
    Serial.println(F("No configuration found in EEPROM."));
    Serial.print(F("Loading default configuration.  "));
    nModes = 1;
    conf[1].action = 'O';
    conf[1].bright = 100;
    conf[1].condPush.to = 0;
    conf[1].condRel.to  = 0xFF;
    conf[1].condHold.to = 0xFF;
    conf[1].condIdle.to = 0xFF;
    writeConfig();
  }
  
  Serial.println(F("Configure me at http://OminousHum.com/hexweb/hexweb.html"));
  Serial.println(F("then paste in a new configuration code here at any time."));
  Serial.println();
}

void loop()
{
  static byte mode = 0;  // The actual current mode
  static byte led  = 0;  // Mode to use settings from
  static boolean btnDown = false;
  static unsigned long lastDazzle = 0;
  static unsigned long lastChange = 0;
  static unsigned long lastButton = 0;

  unsigned long time = millis();
  
  // Do whatever this mode does
  switch (conf[led].action)
  {
  case 'F':
  {
    unsigned long window = (conf[led].period * conf[led].duty) / 100;
    if (window == 0) window = 1;
    digitalWrite(DPIN_DRV_EN, (time%conf[led].period) < window);
    break;
  }
  case 'D':
    if (time-lastDazzle < conf[led].period) break;
    lastDazzle = time;
    digitalWrite(DPIN_DRV_EN, random(100) < conf[led].duty);
    break;
  }
  
  // Check for mode changes
  byte newMode = mode;
  boolean newBtnDown = digitalRead(DPIN_RLED_SW);
  if (conf[mode].condPush.to != 0xFF && !btnDown && newBtnDown)
    newMode = conf[mode].condPush.to;
  if (conf[mode].condRel.to  != 0xFF && btnDown && !newBtnDown)
    newMode = conf[mode].condRel.to;
  if (conf[mode].condHold.to != 0xFF && btnDown && newBtnDown &&
      (time-lastButton) > conf[mode].condHold.time)
    newMode = conf[mode].condHold.to;
  if (conf[mode].condIdle.to != 0xFF &&
      (time-lastChange) > conf[mode].condIdle.time)
    newMode = conf[mode].condIdle.to;
  
  // Do the mode transitions
  if (newMode != mode)
  {
    Serial.print(F("Changing to mode "));
    Serial.print(newMode);
    Serial.print(F(" with action "));
    Serial.write(conf[newMode].action);
    Serial.println(F("."));
    
    switch (conf[newMode].action)
    {
    case 'Z':  // Off
      pinMode(DPIN_PWR, OUTPUT);
      digitalWrite(DPIN_PWR, LOW);
      digitalWrite(DPIN_DRV_MODE, LOW);
      analogWrite(DPIN_DRV_EN, LOW);
      led = newMode;
      break;
    case 'O':  // On
    case 'F':  // Flash
    case 'D':  // Dazzle
      pinMode(DPIN_PWR, OUTPUT);
      digitalWrite(DPIN_PWR, HIGH);
      digitalWrite(DPIN_DRV_MODE, (conf[newMode].bright & 0x80) ? HIGH : LOW);
      analogWrite(DPIN_DRV_EN, conf[newMode].bright << 1);
      led = newMode;
      break;
    }
    
    mode = newMode;
    lastChange = time;
  }
  
  // Handle button state changes
  if (newBtnDown != btnDown)
  {
    lastButton = time;
    btnDown = newBtnDown;
    delay(50);  // Debounce
  }
}

boolean readConfig()
{
  char *pModeBytes = (char*)&conf[1];

  // First check the magic value
  if (EEPROM.read(0) != 42)
    return false;  

  nModes = EEPROM.read(1);  
  for (int i = 0; i < sizeof(Mode)*nModes; i++)
    pModeBytes[i] = EEPROM.read(i+2);
}

void writeConfig()
{
  char *pModeBytes = (char*)&conf[1];

  Serial.print(F("Writing to EEPROM..."));

  EEPROM.write(0, 42);  // Magic, indicates EEPROM is initialized for this.
  EEPROM.write(1, nModes);
  for (int i = 0; i < sizeof(Mode)*nModes; i++)
    EEPROM.write(i+2, pModeBytes[i]);

  Serial.println(F(" done!"));
}

void serialEvent()
{
  if (Serial.peek() == '[')
  {
    if (readNewConfiguration())
    {
      Serial.print(F("That looks like a valid configuration.  "));
      writeConfig();
    }
    else
    {
      Serial.println(F("Restoring previous configuration."));
      readConfig();
    }
  }
  else
    Serial.read();  // Discard
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

byte tryRead(char *buf, word len)
{
  if (Serial.readBytes(buf, len) < len)
  {
    Serial.println(F("ERROR: Timed out waiting for input."));
    return 0;
  }
  return 1;
}

boolean readNewConfiguration()
{
  /* 
   *  Read in, parse, validate, and store configuration strings
   *  Example:   [01:O19p02i04000A,O64p03i04000A,OE4p01i04000A,Up00]
   *  
   *  Syntax:    '[' byte:version ':' (mode ',')* mode ']'
   *  mode:      (unchanged | on | flash | dazzle) condition*
   *  unchanged: 'U'
   *  on:        'O' byte:bright
   *  flash:     'F' byte:bright word:period byte:duty
   *  dazzle:    'D' byte:bright word:period byte:duty
   *  condition: (push | release | hold | idle)
   *  push:      'p' byte:to
   *  release:   'r' byte:to
   *  hold:      'h' byte:to word:time
   *  idle:      'i' byte:to word:time
   */
  
  static char buf[BUF_SIZE];

  // Ignore everything until we see the start char
  if (Serial.read() != '[')
    return false;  

  if (!tryRead(buf, 3)) return false;
  if (buf[2] != ':')
  {
    Serial.println(F("ERROR: Problem parsing input."));
    return false;
  }

  word ver;
  if (!parseHex(buf, 2, &ver)) return false;
  if (ver != VERSION)
  {
    Serial.println(F("ERROR: Configuration site version doesn't match this firmware."));
    Serial.print(F("Firmware version: "));
    Serial.print(VERSION);
    Serial.print(F("  Configuration site version: "));
    Serial.println(ver);
    return false;
  }
  
  // Reading in modes
  byte totalModes = 0;
  byte largestTo  = 0;
  while (true)
  {
    Mode *newMode = &conf[totalModes+1];
    word parsed1, parsed2, parsed3;
    
    if (totalModes >= MAX_MODES)
    {
      Serial.println(F("ERROR: Too many modes!"));
      return false;
    }
    
    if (!tryRead(buf, 1)) return false;
    switch (buf[0])
    {
    case 'U':  // Unchanged
      newMode->action = 'U';
      newMode->bright = 0;
      newMode->period = 0;
      newMode->duty   = 0;
      break;
    case 'O':  // On
      if (!tryRead(buf, 2)) return false;
      if (!parseHex(buf, 2, &parsed1)) return false;
      newMode->action = 'O';
      newMode->bright = parsed1;
      newMode->period = 0;
      newMode->duty   = 0;
      break;
    case 'F':  // Flash
      if (!tryRead(buf, 8)) return false;
      if (!parseHex(buf,   2, &parsed1)) return false;
      if (!parseHex(buf+2, 4, &parsed2)) return false;
      if (!parseHex(buf+6, 2, &parsed3)) return false;
      newMode->action = 'F';
      newMode->bright = parsed1;
      newMode->period = parsed2;
      newMode->duty   = parsed3;
      break;
    case 'D':  // Dazzle
      if (!tryRead(buf, 8)) return false;
      if (!parseHex(buf,   2, &parsed1)) return false;
      if (!parseHex(buf+2, 4, &parsed2)) return false;
      if (!parseHex(buf+6, 2, &parsed3)) return false;
      newMode->action = 'D';
      newMode->bright = parsed1;
      newMode->period = parsed2;
      newMode->duty   = parsed3;
      break;
    default:
      Serial.println(F("ERROR: Invalid mode in input!"));
      return false;
    }

    // Invalidate conditions in struct
    newMode->condPush.to = 0xFF;
    newMode->condRel.to  = 0xFF;
    newMode->condHold.to = 0xFF;
    newMode->condIdle.to = 0xFF;
    
    // Read in conditions, if any
    if (!tryRead(buf, 1)) return false;
    while (buf[0] != ']' && buf[0] != ',')
    {
      switch (buf[0])
      {
      case 'p':
        if (!tryRead(buf, 2)) return false;
        if (!parseHex(buf, 2, &parsed1)) return false;
        if (parsed1 > largestTo) largestTo = parsed1;
        newMode->condPush.to = parsed1;
        break;
      case 'r':
        if (!tryRead(buf, 2)) return false;
        if (!parseHex(buf, 2, &parsed1)) return false;
        if (parsed1 > largestTo) largestTo = parsed1;
        newMode->condRel.to = parsed1;
        break;
      case 'h':
        if (!tryRead(buf, 6)) return false;
        if (!parseHex(buf,   2, &parsed1)) return false;
        if (!parseHex(buf+2, 4, &parsed2)) return false;
        if (parsed1 > largestTo) largestTo = parsed1;
        newMode->condHold.to   = parsed1;
        newMode->condHold.time = parsed2;
        break;
      case 'i':
        if (!tryRead(buf, 6)) return false;
        if (!parseHex(buf,   2, &parsed1)) return false;
        if (!parseHex(buf+2, 4, &parsed2)) return false;
        if (parsed1 > largestTo) largestTo = parsed1;
        newMode->condIdle.to   = parsed1;
        newMode->condIdle.time = parsed2;
        break;
      default:
        Serial.println(F("ERROR: Invalid condition in input!"));
        return false;
      }
      if (!tryRead(buf, 1)) return false;
    }
    
    totalModes++;

    if (buf[0] == ']') break;      // We're all done
    if (buf[0] == ',') continue;   // Done with this mode
  }
  
  // Did we see an invalid 'to' reference in any of the conditions?
  if (largestTo > totalModes)
  {
    Serial.print(F("ERROR: A condition referenced a mode that was not defined!"));
    return false;
  }
    
  nModes = totalModes;
  return true;
}
