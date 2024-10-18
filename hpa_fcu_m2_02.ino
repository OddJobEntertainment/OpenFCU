/*

  The program implements an HPA FCU as a state machine with 6 states:

  1) IDLE
  2) SEMI_AUTO
  3) FULL_AUTO
  4) BINARY_TRIGGER_TAIL
  5) RELOAD
  6) MENU

  The code is divided among 3 files:

  - the main file (this one), that manages the state machine
  - the "menu.ino" that manages the UI for the settings display and modifications,
  - the "buttons.ino" that manages the input and debouncing of the buttons

  Below the description of the operations of each state

  -----------
  Status IDLE
  -----------

  The program checks the trigger, the fire selector and the joystick select button until one of the following happens:

  a) if the trigger is pressed and the fire selector is pressed the program goes to the FULL_AUTO status

  b) if the trigger is pressed and the fireSelector is not pressed the program goes to the SEMI_AUTO status

  c) if the trigger is not pressed and if the joystick select is pressed the program goes to the MENU status


  ----------------
  Status FULL_AUTO
  ----------------

  The program does shot continuously while the trigger is pressed and the fire selector is pressed;

  when one or both of the trigger and the fire selector are released, the program goes back to the status IDLE.

  If the shot count reaches shotLimit and forceReload is true, goes to the status RELOAD


  ----------------------
  Status SEMI_AUTO
  ----------------------

  a) the program fires one burst of burstCount shots;

  then

  b) if the fire selector is pressed, goes back to the status IDLE

  c) if the trigger is still pressed and fullAutoBurst is true waits burstDelayMs and then goes to point c)

  d) if the trigger is still pressed and fullAutoBurst is false does nothing until the trigger is released

  e) if the trigger is released, if binaryTrigger is true goes to the BINARY_TRIGGER_TAIL status, otherwise goes to the IDLE status

  If the shot count reaches shotLimit and forceReload is true, goes to the status RELOAD


  --------------------------
  Status BINARY_TRIGGER_TAIL
  --------------------------

  The program fires one burst of burstCount shots and then goes back to the status IDLE

  If the shot count reaches shotLimit and forceReload is true, goes to the status RELOAD


  ----------------------
  Status RELOAD
  ----------------------

  The program waits for a reload sequence (magazine pin goes HIGH and then LOW), then goes to the status IDLE


  ---------------------
  Status MENU
  ---------------------

  Manages display, modifications and storage to the EEPROM of the settings, as detailed in the section "How the UI works" in the file "menu.ino"

*/

#include <EEPROM.h>

//==========================
// Variables and defines
//==========================

// uncomment the define below to see debug messages on the serial monitor
//#define DEBUG_ENABLED

// uncomment the define below to apply the binaryTrigger setting to the full_auto fire mode
//#define APPLY_BINARY_TRIGGER_TO_FULL_AUTO

// uncomment the define below to apply the binaryTrigger setting to the semi-auto fire mode even when fullAutoBurst is true
//#define APPLY_BINARY_TRIGGER_TO_FULL_AUTO_BURST

#define BUTTON_PRESSED LOW
#define BUTTON_RELEASED HIGH

#define SOLENOID_ON  HIGH
#define SOLENOID_OFF LOW

#define STATUS_IDLE                0
#define STATUS_SEMI_AUTO           1
#define STATUS_FULL_AUTO           2
#define STATUS_BINARY_TRIGGER_TAIL 3
#define STATUS_RELOAD              4
#define STATUS_MENU                5

#define RELOAD_WAITING_FOR_MAGAZINE_OUT 0
#define RELOAD_WAITING_FOR_MAGAZINE_IN  1

#define MENU_CONTINUE         0
#define MENU_SAVE_AND_EXIT    1
#define MENU_DISCARD_AND_EXIT 2

#define ID_MENU_EXIT_MESSAGE_NOT_MODIFIED 0
#define ID_MENU_EXIT_MESSAGE_DISCARDED    1
#define ID_MENU_EXIT_MESSAGE_SAVED        2

#define MENU_EXIT_MESSAGE_DURATION_MS 2000

//----------------
// Pin assignments
//----------------

// input pins

const uint8_t triggerPin      = 3;
const uint8_t fireSelectorPin = 4;
const uint8_t magazinePin     = 5;
// joystick pins
const uint8_t joyUpPin        = 8;
const uint8_t joyLeftPin      = 9;
const uint8_t joySelectPin    = 7;
const uint8_t joyRightPin     = 11;
const uint8_t joyDownPin      = 10;

// output pins
const uint8_t solenoidPin     = 2;
const uint8_t muzzlePin       = 6;

//----------------------------
// Buttons data
//----------------------------

typedef struct tagButtonData
{
  int pin;
  unsigned long startTimeMs; // starting time of current status
  int buttonStatus; // current button status
  int lastValidStatus; // last valid status, i.e. status after debouncing

  bool precStatusIsPressed; // if true, last time we checked button status it was pressed, otherwise it was released

} ButtonData;

ButtonData buttons[] =
{
  // add buttons here
  //  {buttonPin, 0, BUTTON_RELEASED, BUTTON_RELEASED, false}
  {fireSelectorPin, 0, BUTTON_RELEASED, BUTTON_RELEASED, false},
  {magazinePin, 0, BUTTON_RELEASED, BUTTON_RELEASED, false},
  {joyUpPin, 0, BUTTON_RELEASED, BUTTON_RELEASED, false},
  {joyLeftPin, 0, BUTTON_RELEASED, BUTTON_RELEASED, false},
  {joySelectPin, 0, BUTTON_RELEASED, BUTTON_RELEASED, false},
  {joyRightPin, 0, BUTTON_RELEASED, BUTTON_RELEASED, false},
  {joyDownPin, 0, BUTTON_RELEASED, BUTTON_RELEASED, false}
};

const int numButtons = sizeof(buttons) / sizeof(ButtonData);

unsigned long buttonsDebouncingTimeMs = 50; // 50 millisecs debouncing

#define ID_BUTTON_FIRE_SELECTOR 0
#define ID_BUTTON_MAGAZINE      1
#define ID_BUTTON_JOY_UP        2
#define ID_BUTTON_JOY_LEFT      3
#define ID_BUTTON_JOY_SELECT    4
#define ID_BUTTON_JOY_RIGHT     5
#define ID_BUTTON_JOY_DOWN      6

#define ID_BUTTON_INVALID      -1

//-------------
// Settings
//-------------

// used only by menu functions
typedef struct tagSettingValue
{
  int valueType;

  union
  {
    struct
    {
      bool *boolValuePtr;
      bool currBoolValue;
    };

    struct
    {
      unsigned int *uintValuePtr; // reference to the actual value to be modified
      unsigned int currUIntValue; // current value
      unsigned int uintValueMin; // min value for the setting
      unsigned int uintValueMax; // max value for the setting
      unsigned int longPressDelta; // delta value for long press inc dec
    };
  };

} SettingValue;

typedef struct tagSettings
{
  unsigned int shotDelayMs  = 200;   //the time between each individual shot, must be set high enough to allow the magazine to feed properly into the hop-up unit
  unsigned int burstDelayMs = 400;   //used for destiny 2 style full-auto burst where bursts will continue firing whilst the trigger is held
  unsigned int dwellMs      = 200;   //time that the solenoid is open, should be tuned to be open long enough for the BB to exit the barrel

  unsigned int triggerDebouncingMs = 50; // 50 millisecs debouncing

  unsigned int numShotsPerBurst = 3;     //value indicates how many shots are fired while in "semi-auto"; set to 1 for true semi
  unsigned int shotLimit        = 30;    //used to set a limit on how many shots can be fired before a reload is required

  bool fullAutoBurst = false;    //allows bursts to continue firing if true after burstDelay
  bool binaryTrigger = false;    //when true makes the pull activate the firing sequence and the release
  bool forceReload   = true;    //when true forces reloads to be done
  bool muzzleFlash   = false;    //to be used for simulating muzzle flash
  bool invertedFireSelector = false; // each fireSelector press will change the fire mode in sequence: safe -> semi auto -> full auto -> safe and so on;
} Settings;

Settings settings;

const char *magicEepromPrefix = "EV01"; // this string must be changed if the settings format change

//----------------
// other variables
//----------------

unsigned int shotCount;     // counts how many shots have been fired, only if forceReload is true; when countLimit is reached, restarts from 0 after magazine reload

unsigned int currBurstShotCount;
int reloadStatus;

int status;

//========================
// Utility
//========================

//---------------------
// EEPROM read/write
//---------------------

// return true if the EEPROM contains valid settings
bool isEepromValid()
{
  int l = strlen(magicEepromPrefix);

  for (int i = 0; i < l; i++)
    if (EEPROM.read(i) != magicEepromPrefix[i])
      return false;

  return true;
}

void saveSettingsToEeprom()
{
#ifdef DEBUG_ENABLED
  Serial.println(F("Saving settings to EEPROM"));
#endif

  int l = strlen(magicEepromPrefix);

  for (int i = 0; i < l; i++)
    EEPROM.update(i, magicEepromPrefix[i]);

  EEPROM.put(l, settings);
}

bool readSettingsFromEeprom()
{
  if (!isEepromValid())
  {
#ifdef DEBUG_ENABLED
    Serial.println(F("No valid settings in the EEPROM"));
#endif

    return false;
  }

#ifdef DEBUG_ENABLED
  Serial.println(F("Reading settings from EEPROM"));
#endif

  EEPROM.get(strlen(magicEepromPrefix), settings);
  return true;
}

//---------------------
// Buttons
//---------------------

bool isTriggerPressed()
{
  return digitalRead(triggerPin) == BUTTON_PRESSED;
}

bool isMagazineButtonPressed()
{
  return buttonIsPressed(ID_BUTTON_MAGAZINE);
}

bool isFireSelectorInAutoPosition()
{
  bool isFireSelectorPressed = buttonIsPressed(ID_BUTTON_FIRE_SELECTOR);

  return settings.invertedFireSelector ? !isFireSelectorPressed : isFireSelectorPressed;
}

bool thereIsANewJoySelectPress()
{
  return thereIsANewPress(ID_BUTTON_JOY_SELECT);
}

//--------------------
// Shot low level
//--------------------

// return false if forceReload is true and the shot limit is reached
bool doOneShot()
{
  if (settings.forceReload)
  {
    if (shotCount >= settings.shotLimit)
    {

#ifdef DEBUG_ENABLED
      Serial.print(F("Shot count "));
      Serial.print(shotCount);
      Serial.println(F(". Reload forced."));
#endif

      return false;
    }

    shotCount++;
  }
   if(settings.muzzleFlash){ 
    digitalWrite(solenoidPin, SOLENOID_ON);
    digitalWrite(muzzlePin, HIGH);
    delay(settings.dwellMs);
    digitalWrite(solenoidPin, SOLENOID_OFF);
    digitalWrite(muzzlePin, LOW);
    delay(settings.shotDelayMs);

  return true;
   }
   else{
    digitalWrite(solenoidPin, SOLENOID_ON);
    delay(settings.dwellMs);
    digitalWrite(solenoidPin, SOLENOID_OFF);
    delay(settings.shotDelayMs);

  return true;
    }
}

// return true when the burst is done, otherwise false
bool doNextShotAndGoToStatusReloadIfShotError(const unsigned int numShotsRequired)
{
  if (currBurstShotCount < numShotsRequired)
  {
    if (!doOneShot())
    {
      // forceReload true and shotCount == shotLimit
      transitionToStatusReload();
      return false;
    }

    currBurstShotCount++;

    return false;
  }

  return true;
}

//=========================================
// Binary trigger tail status management
// Does the burst after the trigger release
//=========================================

void transitionToStatusBinaryTriggerTail()
{
#ifdef DEBUG_ENABLED
  Serial.println(F("Entering status BINARY_TRIGGER_TAIL"));
#endif

  currBurstShotCount = 0;

  status = STATUS_BINARY_TRIGGER_TAIL;
}

void manageStatusBinaryTriggerTail()
{
  if (!doNextShotAndGoToStatusReloadIfShotError(settings.numShotsPerBurst))
    return;

  // burst done without shot error, back to status idle
  transitionToStatusIdle();
}

//=====================================
// Semi auto status management
//=====================================

void transitionToStatusSemiAuto()
{
#ifdef DEBUG_ENABLED
  Serial.println(F("Entering status SEMI_AUTO"));
#endif

  currBurstShotCount = 0;

  status = STATUS_SEMI_AUTO;
}

void manageStatusSemiAuto()
{
  if (!doNextShotAndGoToStatusReloadIfShotError(settings.numShotsPerBurst))
    return;

  // burst done without shot error

  if (isFireSelectorInAutoPosition())
  {
    // fire selector position changed, goes back to idle
    transitionToStatusIdle();
    return;
  }

  if (isTriggerPressed())
  {
    if (!settings.fullAutoBurst)
      // burst done, trigger still pressed, no fullAutoBurst, does nothing
      return;

    // burst done, trigger still pressed, fullAutoBurst true, prepare for the next burst
    currBurstShotCount = 0;

    delay(settings.burstDelayMs);

    return;
  }

  // trigger released

#ifndef APPLY_BINARY_TRIGGER_TO_FULL_AUTO_BURST
  if (settings.fullAutoBurst)
  {
    // does not check binaryTrigger, always back to status idle when the trigger is released
    delay(settings.triggerDebouncingMs);
    transitionToStatusIdle();
    return;
  }
#endif

  if (settings.binaryTrigger)
  {
    // does the binary trigger tail burst
    transitionToStatusBinaryTriggerTail();
    return;
  }

  // no binaryTrigger, back to status idle when the trigger is released
  delay(settings.triggerDebouncingMs);
  transitionToStatusIdle();
}

//=====================================
// Full auto status management
//=====================================

void transitionToStatusFullAuto()
{
#ifdef DEBUG_ENABLED
  Serial.println(F("Entering status FULL_AUTO"));
#endif

  status = STATUS_FULL_AUTO;
}

void manageStatusFullAuto()
{
  if (!isFireSelectorInAutoPosition())
  {
    // fire selector changed, goes back to idle
    transitionToStatusIdle();
    return;
  }

  if (!doOneShot())
  {
    // forceReload true and shotCount == shotLimit
    transitionToStatusReload();
    return;
  }

  if (!isTriggerPressed())
  {

#ifdef APPLY_BINARY_TRIGGER_TO_FULL_AUTO
    if (settings.binaryTrigger)
    {
      transitionToStatusBinaryTriggerTail();
      return;
    }
#endif

    // binaryTrigger false or does not check binaryTrigger in full-auto, back to status idle when the trigger is released
    delay(settings.triggerDebouncingMs);
    transitionToStatusIdle();

    return;
  }
}

//=====================================
// Reload management
//=====================================

void transitionToStatusReload()
{
#ifdef DEBUG_ENABLED
  Serial.println(F("Entering status Reload: waiting for magazine out"));
#endif

  reloadStatus = RELOAD_WAITING_FOR_MAGAZINE_OUT;

  status = STATUS_RELOAD;
}

void manageStatusReload()
{
  switch (reloadStatus)
  {
    case RELOAD_WAITING_FOR_MAGAZINE_OUT:
      if (!isMagazineButtonPressed())
      {
#ifdef DEBUG_ENABLED
        Serial.println("Reload: waiting for magazine in.");
#endif

        reloadStatus = RELOAD_WAITING_FOR_MAGAZINE_IN;
      }

      return;

    case RELOAD_WAITING_FOR_MAGAZINE_IN:
      if (isMagazineButtonPressed())
      {
        // reload done, goes back to idle
#ifdef DEBUG_ENABLED
        Serial.println("Reload: done.");
#endif

        shotCount = 0;

        transitionToStatusIdle();
      }

      return;

    default:
      // internal error, should never be here
#ifdef DEBUG_ENABLED
      Serial.println(F("Internal error: invalid reload status"));
#endif

      return;
  }
}

//=====================================
// Menu status management
//=====================================

void saveNewSettingsToEepromAndDisplayExitMessage()
{
  if (!commitSettings(settings))
  {
    // commitSettings return false if the old settings have not been modified, inform the user and exit

    displayMenuExitMessage(ID_MENU_EXIT_MESSAGE_NOT_MODIFIED, MENU_EXIT_MESSAGE_DURATION_MS);
    return;
  }

  // old settings modified, save the new settings to EEPROM

  saveSettingsToEeprom();
  displayMenuExitMessage(ID_MENU_EXIT_MESSAGE_SAVED, MENU_EXIT_MESSAGE_DURATION_MS);
}

void transitionToStatusMenu()
{
#ifdef DEBUG_ENABLED
  Serial.println(F("Entering status MENU"));
#endif

  displayFirstMenuPage(settings);

  status = STATUS_MENU;
}

void manageStatusMenu()
{
  switch (updateMenu())
  {
    case MENU_SAVE_AND_EXIT:
      saveNewSettingsToEepromAndDisplayExitMessage();

      transitionToStatusIdle();
      return;

    case MENU_DISCARD_AND_EXIT:
      displayMenuExitMessage(ID_MENU_EXIT_MESSAGE_DISCARDED, MENU_EXIT_MESSAGE_DURATION_MS);

      transitionToStatusIdle();
      return;
  }

  // updateMenu return MENU_CONTINUE, stay in the menu status
}

//=====================================
// Idle status management
//=====================================

void transitionToStatusIdle()
{
#ifdef DEBUG_ENABLED
  Serial.println(F("Entering status IDLE"));
#endif

  status = STATUS_IDLE;
}

void manageStatusIdle()
{
  if (isTriggerPressed())
  {
    if (isFireSelectorInAutoPosition())
      transitionToStatusFullAuto();

    else
      transitionToStatusSemiAuto();

    return;
  }

  // no trigger pressed, check the joy select

  if (thereIsANewJoySelectPress())
  {
    // goes to menu status
    transitionToStatusMenu();
    return;
  }
}

//=============================
// setup() and loop()
//=============================

void setup()
{
  Serial.begin(9600);

  pinMode(triggerPin,      INPUT_PULLUP);
  pinMode(joyUpPin,        INPUT_PULLUP);
  pinMode(joyDownPin,      INPUT_PULLUP);
  pinMode(joyLeftPin,      INPUT_PULLUP);
  pinMode(joySelectPin,    INPUT_PULLUP);
  pinMode(joyRightPin,     INPUT_PULLUP);
  pinMode(fireSelectorPin, INPUT_PULLUP);
  pinMode(magazinePin,     INPUT_PULLUP);
  pinMode(solenoidPin,     OUTPUT);
  pinMode(muzzlePin,       OUTPUT);

  shotCount = 0;

  oledDisplayInit();

  if (!readSettingsFromEeprom())
    // no valid settings stored in the Eeprom, initialize the Eeprom with the default settings
    saveSettingsToEeprom();

  transitionToStatusIdle();
}

void loop()
{
  readAllButtons();

  switch (status)
  {
    case STATUS_IDLE:
      manageStatusIdle();
      break;

    case STATUS_FULL_AUTO:
      manageStatusFullAuto();
      break;

    case STATUS_SEMI_AUTO:
      manageStatusSemiAuto();
      break;

    case STATUS_BINARY_TRIGGER_TAIL:
      manageStatusBinaryTriggerTail();
      break;

    case STATUS_RELOAD:
      manageStatusReload();
      break;

    case STATUS_MENU:
      manageStatusMenu();
      break;
  }
}
