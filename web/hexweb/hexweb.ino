/* 
  HexWeb firmware for HexBright FLEX   
*/

#include <math.h>
#include <Wire.h>
#include <EEPROM.h>

// Settings
#define MAX_MODES               32
#define BUF_SIZE                10
#define VERSION                 1
#define OVERTEMP                340
// Constants
#define ACC_ADDRESS             0x4C
#define ACC_REG_TILT            3
#define ACC_REG_INTS            6
#define ACC_REG_MODE            7
// Pin assignments
#define DPIN_RLED_SW            2
#define DPIN_GLED               5
#define DPIN_PWR                8
#define DPIN_DRV_MODE           9
#define DPIN_DRV_EN             10
#define DPIN_ACC_INT            3
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
  } condTap;
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
byte mode = 0;  // The actual current mode
byte led  = 0;  // Mode to use settings from


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
  pinMode(DPIN_ACC_INT,  INPUT);
  digitalWrite(DPIN_DRV_MODE, LOW);
  digitalWrite(DPIN_DRV_EN,   LOW);
  digitalWrite(DPIN_ACC_INT,  HIGH);
  
  // Be less verbose if the button is down.  This is a good clue
  // that we're being powered up without USB and need to handle
  // that button as quickly as possible.
  boolean verbose = !digitalRead(DPIN_RLED_SW);
  
  // Initialize serial busses
  Serial.begin(9600);
  Wire.begin();
  
  // Configure accelerometer
  byte config[] PROGMEM = {
    ACC_REG_INTS,  // First register (see next line)
    0xE4,  // Interrupts: shakes, taps
    0x00,  // Mode: not enabled yet
    0x00,  // Sample rate: 120 Hz
    0x0F,  // Tap threshold
    0x10   // Tap debounce samples
  };
  Wire.beginTransmission(ACC_ADDRESS);
  Wire.write(config, sizeof(config));
  Wire.endTransmission();

  // Enable accelerometer
  byte enable[] PROGMEM = {ACC_REG_MODE, 0x01};  // Mode: active!
  Wire.beginTransmission(ACC_ADDRESS);
  Wire.write(enable, sizeof(enable));
  Wire.endTransmission();
  
  // Initialize an immutable 'off' mode configuration
  conf[0].action = 'Z';
  conf[0].bright = 0;
  conf[0].condPush.to = 1;
  conf[0].condRel.to  = 0xFF;
  conf[0].condTap.to  = 0xFF;
  conf[0].condHold.to = 0xFF;
  conf[0].condIdle.to = 0xFF;

  if (verbose)
    Serial.println(F("Powered up!"));
  
  if (readConfig())
  {  
    if (verbose)
    {
      Serial.print(F("Restored configuration with "));
      Serial.print(nModes);
      Serial.println(F(" modes."));
    }
  }
  else
  {
    if (verbose)
    {
      Serial.println(F("No configuration found in EEPROM."));
      Serial.print(F("Loading default configuration.  "));
    }
    nModes = 1;
    conf[1].action = 'O';
    conf[1].bright = 0x7F;
    conf[1].condPush.to = 0;
    conf[1].condRel.to  = 0xFF;
    conf[1].condTap.to  = 0xFF;
    conf[1].condHold.to = 0xFF;
    conf[1].condIdle.to = 0xFF;
    writeConfig();
  }

  if (verbose)
  {
    Serial.println(F("Configure me at http://OminousHum.com/hexweb/"));
    Serial.println(F("then paste in a new configuration code here at any time."));
    Serial.println();
  }
}

void powerOff()
{
  pinMode(DPIN_PWR, OUTPUT);
  digitalWrite(DPIN_PWR, LOW);
  digitalWrite(DPIN_DRV_MODE, LOW);
  analogWrite(DPIN_DRV_EN, LOW);
  mode = led = 0;
}

void loop()
{
  static boolean btnDown = false;
  static unsigned long lastDazzle, lastChange, lastButton, lastTemp, lastAccel;
  
  unsigned long time = millis();
  
    
  // Check if the accelerometer wants to interrupt
  boolean tapped = false, shaked = false;
  if (!digitalRead(DPIN_ACC_INT))
  {
    Wire.beginTransmission(ACC_ADDRESS);
    Wire.write(ACC_REG_TILT);
    Wire.endTransmission(false);       // End, but do not stop!
    Wire.requestFrom(ACC_ADDRESS, 1);  // This one stops.
    byte tilt = Wire.read();
    
    if (time-lastAccel > 500)
    {
      lastAccel = time;
  
      tapped = !!(tilt & 0x20);
      shaked = !!(tilt & 0x80);
  
      if (tapped) Serial.println(F("Tap!"));
      if (shaked) Serial.println(F("Shake!"));
    }
  }
  
  // Check the state of the charge controller
  int chargeState = analogRead(APIN_CHARGE);
  if (chargeState < 128)  // Low - charging
  {
    digitalWrite(DPIN_GLED, (time&0x0100)?LOW:HIGH);
  }
  else if (chargeState > 768) // High - charged
  {
    digitalWrite(DPIN_GLED, HIGH);
  }
  else // Hi-Z - shutdown
  {
    digitalWrite(DPIN_GLED, (time&0x03FC)?LOW:HIGH);   
  }

  // Check the temperature sensor
  if (time-lastTemp > 1000)
  {
    lastTemp = time;
    int temperature = analogRead(APIN_TEMP);
    if (temperature > OVERTEMP)
    {
      Serial.println(F("Overheat shutdown!"));
      for (int i = 0; i < 6; i++)
      {
        digitalWrite(DPIN_DRV_MODE, LOW);
        delay(100);
        digitalWrite(DPIN_DRV_MODE, HIGH);
        delay(100);
      }
      digitalWrite(DPIN_DRV_MODE, LOW);
      powerOff();
    }
  }

  // Do whatever this mode does
  switch (conf[led].action)
  {
  case 'F':
  {
    unsigned long window = (conf[led].period * conf[led].duty) >> 7;
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
  if (conf[mode].condTap.to  != 0xFF && tapped)
    newMode = conf[mode].condTap.to;
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
      powerOff();
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

  // First check for the magic value
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

  EEPROM.write(0, 42);  // Magic, indicates EEPROM is initialized
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
      powerOff();  // Back to mode 0
    }
    else
    {
      Serial.println(F("Invalid configuration, restoring previous."));
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
   *  tap:       't' byte:to
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
    newMode->condTap.to  = 0xFF;
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
      case 't':
        if (!tryRead(buf, 2)) return false;
        if (!parseHex(buf, 2, &parsed1)) return false;
        if (parsed1 > largestTo) largestTo = parsed1;
        newMode->condTap.to = parsed1;
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
