/*

This file contains the code that manages the UI for the settings display and modifications.

=============================
How the UI works
=============================

---------------------
Screen and resolution
---------------------

The UI will use an SH1106 OLED with a 128x64 resolution; 

the user can change the display to an SSD1306, commenting out the define USING_SH1106_OLED at line 110;
the user can change the resolution to 128x32, uncommenting the define USING_OLED_HEIGHT_32 at line 126.

The OLED is normally inactive, it will display the UI "Main" mode (see below) when the user press the joystick select button.

The UI can be in 2 modes, "Main" and "Modification"

In the following, "inverted row" means white background and black text color.

-------------
UI Main mode
-------------

Because all settings does not fit on the screen, they are divided in "pages"

The top row of the OLED display is inverted and shows "Settings" followed by the current page number and the total number of pages.

The following rows show the settings names and values, one for row.

For the boolean settings the printed value is "ON" or "OFF". 

The current selected setting is displayed on an inverted row.

In this mode, the joystick buttons work as follows:

a) up

moves the current setting selection one row up. If the current selected setting is the first one of the page, goes back to the preceding setting page.

b) down

moves the current setting selection one row down. If the current selected setting is the last one of the page, goes to the next setting page.

c) left

exits the UI discarding all the settings modifications; before exiting the OLED displays the message "Settings:" on one row and "DISCARDED" on the next one.

d) right

exits the UI saving the updated settings values to the EEPROM; before exiting the OLED displays the message "Settings:" on one row and "SAVED" on the next row; 
if none of the settings has been modified in the current session, the OLED displays the message "Settings:" on one row and "NOT MODIFIED" on the next row.

e) select

goes to the  "Modification" UI mode for the current selected setting.

--------------------
UI Modification mode
--------------------

In the following, a long press means pressing a button for more than a certain duration; the current requested duration is 2 seconds, it can be modified
modifying the variable minDurationForLongPressMs at line 244

The top row of the OLED display is inverted and shows the setting name. The row below displays the setting value.

In this mode, the joystick buttons work as follows:

a) up

If the value is a number, one short press increments it by 1; a long press repeats the increment with a certain step until the button is released
or the maximum value for the setting is reached; 

- the long press increment step for the delays is 10, it can be changed modifying the define SETTING_DELAY_LONG_PRESS_DELTA at line 174;
- the maximum value for the delays is 2000, it can be changed modifying the define SETTING_DELAY_VALUE_MAX at line 173;

- the long press increment step for the shot numbers is 5, it can be changed modifying the define SETTING_NUM_SHOT_LONG_PRESS_DELTA at line 178;
- the maximum value for the shot numbers is 1000, it can be changed modifying the define SETTING_NUM_SHOT_VALUE_MAX at line 177;

If the value is a boolean, each press toggles the value.

b) down

If the value is a number, one short press decrements it by 1; a long press repeats the decrement with a certain step until the button is released
or the minimum value for the setting is reached; 

- the decrement steps are the same as the up button.
- the minimum value for the delays is 1, it can be changed modifying the define SETTING_DELAY_VALUE_MIN at line 172;
- the minimum value for the shot numbers is 1, it can be changed modifying the define SETTING_NUM_SHOT_VALUE_MIN at line 176;

If the value is a boolean, each press toggles the value.

c) left

goes back to the "Main" UI mode discarding the setting modification

d) right

goes back to the "Main" UI mode keeping the setting modification

e) select

unused in this mode

*/

#define USING_SH1106_OLED

#include <SPI.h>
#include <Wire.h>
#include <Adafruit_GFX.h>

#ifdef USING_SH1106_OLED
  #include <Adafruit_SH110X.h>
#else
  #include <Adafruit_SSD1306.h>
#endif

//==========================
// Variables and defines
//==========================

//#define USING_OLED_HEIGHT_32

#define SCREEN_WIDTH 128 // OLED display width, in pixels

#ifdef USING_OLED_HEIGHT_32
  #define SCREEN_HEIGHT 32 // OLED display height, in pixels
#else
  #define SCREEN_HEIGHT 64
#endif  

#define SCREEN_ADDRESS 0x3C // on some OLED this address can be wrong, if the OLED does not work we should use the I2C scanner

#ifdef USING_SH1106_OLED
  #define BLACK_COLOR SH110X_BLACK
  #define WHITE_COLOR SH110X_WHITE
#else
  #define BLACK_COLOR SSD1306_BLACK
  #define WHITE_COLOR SSD1306_WHITE
#endif

#define NUM_PIXELS_PER_COL 6
#define NUM_PIXELS_PER_ROW 8

#define DISPLAY_NUM_COLS (SCREEN_WIDTH / NUM_PIXELS_PER_COL)
#define DISPLAY_NUM_ROWS (SCREEN_HEIGHT / NUM_PIXELS_PER_ROW)

// menu

#define MENU_MODE_DISPLAY_MENU_PAGE 0
#define MENU_MODE_MODIFY_SETTING    1 

#define ID_SETTING_SHOT_DELAY_MS           0
#define ID_SETTING_BURST_DELAY_MS          1
#define ID_SETTING_DWELL_MS                2 
#define ID_SETTING_TRIGGER_DEBOUNCING_MS   3
#define ID_SETTING_NUM_SHOTS_PER_BURST     4
#define ID_SETTING_SHOT_LIMIT              5 
#define ID_SETTING_FULL_AUTO_BURST         6
#define ID_SETTING_BINARY_TRIGGER          7
#define ID_SETTING_FORCE_RELOAD            8
#define ID_SETTING_MUZZLE_FLASH            9
#define ID_SETTINGS_INVERTED_FIRE_SELECTOR 10

#define SETTING_TYPE_BOOL 1
#define SETTING_TYPE_UINT 2

#define SETTING_DELAY_VALUE_MIN 1
#define SETTING_DELAY_VALUE_MAX 2000
#define SETTING_DELAY_LONG_PRESS_DELTA 10

#define SETTING_NUM_SHOT_VALUE_MIN 1
#define SETTING_NUM_SHOT_VALUE_MAX 1000
#define SETTING_NUM_SHOT_LONG_PRESS_DELTA 5

// menu layout depending on OLED pixel height

#ifdef USING_OLED_HEIGHT_32

  #define NUM_MAX_SETTINGS_PER_MENU_PAGE 3

  #define DISPLAY_SETTINGS_MENU_FIRST_ROW   1
  #define DISPLAY_SETTINGS_MODIFICATION_ROW 1

  #define MENU_EXIT_MESSAGE_FIRST_ROW 1

#else

  #define NUM_MAX_SETTINGS_PER_MENU_PAGE 6

  #define DISPLAY_SETTINGS_MENU_FIRST_ROW 2
  #define DISPLAY_SETTINGS_MODIFICATION_ROW 2

  #define MENU_EXIT_MESSAGE_FIRST_ROW 1

#endif  

#define DISPLAY_SETTINGS_MENU_FIRST_COL 0 

#define MENU_EXIT_MESSAGE_COL           0

// menu names

// NB: at text size 1, a SSD1306 display 128 pixel width can show approx 21 column
const char shotDelayMsName[]          PROGMEM = "Shot delay";
const char burstDelayMsName[]         PROGMEM = "Burst delay";
const char dwellMsName[]              PROGMEM = "Dwell";
const char triggerDebouncingMsName[]  PROGMEM = "Trg. debounce";
const char numShotsPerBurstName[]     PROGMEM = "Burst shots";
const char shotLimitName[]            PROGMEM = "Shot limit";
const char fullAutoBurstName[]        PROGMEM = "Auto burst";
const char binaryTriggerName[]        PROGMEM = "Bin. trigger";
const char forceReloadName[]          PROGMEM = "Force reload";
const char muzzleFlashName[]          PROGMEM = "Muzzle flash";
const char invertedFireSelectorName[] PROGMEM = "Inv. selector";

const char *const settingNames[] PROGMEM = {shotDelayMsName, burstDelayMsName, dwellMsName, triggerDebouncingMsName, numShotsPerBurstName, shotLimitName, 
                                            fullAutoBurstName, binaryTriggerName, forceReloadName, muzzleFlashName, invertedFireSelectorName};

#define MAX_SETTING_NAME_LEN 22

char nameBuffer[MAX_SETTING_NAME_LEN + 1];

// other menu variables

const int numTotSettings = 11;
const int numMenuPages = numTotSettings % NUM_MAX_SETTINGS_PER_MENU_PAGE ? (numTotSettings / NUM_MAX_SETTINGS_PER_MENU_PAGE) + 1 : numTotSettings / NUM_MAX_SETTINGS_PER_MENU_PAGE;

int menuMode;

int currMenuPage;
int currMenuPageNumSettings;
int currMenuCursorPos;
int currMenuPageFirstSettingId;

// variables for long press cursor up/down
unsigned long currPressStartTimeMs;
int currPressButtonId;

const unsigned long minDurationForLongPressMs = 2000;
const unsigned long longPressIntervalBetweenIncDecMs = 100;

unsigned long lastModificationTimeMs;

// settings
Settings tempSettings;
SettingValue settingValue;

// display object

#ifdef USING_SH1106_OLED
  Adafruit_SH1106G oledDisplay = Adafruit_SH1106G(SCREEN_WIDTH, SCREEN_HEIGHT, &Wire, -1);
#else
  Adafruit_SSD1306 oledDisplay(SCREEN_WIDTH, SCREEN_HEIGHT, &Wire, -1);
#endif

//==============================
// Utility
//==============================

int mapDisplayColToPixelXPos(const int displayCol)
{
  return displayCol * NUM_PIXELS_PER_COL;
}

int mapDisplayRowToPixelYPos(const int displayRow)
{
  return displayRow * NUM_PIXELS_PER_ROW;
}

int mapMenuRowToDisplayRow(const int menuRow)
{
  return DISPLAY_SETTINGS_MENU_FIRST_ROW + menuRow;
}

int mapMenuRowToSettingId(const int menuRow)
{
  int settingId = currMenuPageFirstSettingId + menuRow;

  return settingId < numTotSettings ? settingId : -1;
}

int evalMenuPageNumSettingsByFirstSettingId(const int firstSettingId)
{
  int remainingSettings = max( (numTotSettings - firstSettingId), 0 ); 

  return min( NUM_MAX_SETTINGS_PER_MENU_PAGE, remainingSettings );
}

int evalMenuPageNumSettings(const int page)
{
  int firstSettingId = page * NUM_MAX_SETTINGS_PER_MENU_PAGE;

  return evalMenuPageNumSettingsByFirstSettingId(firstSettingId);
}

//==============================
// OLED low-level functions
//==============================

void oledDisplayInit()
{
  delay(250);

#ifdef USING_SH1106_OLED

  oledDisplay.begin(SCREEN_ADDRESS, true); // Address 0x3C default

#else

  if (!oledDisplay.begin(SSD1306_SWITCHCAPVCC, SCREEN_ADDRESS)) 
  {
    Serial.println(F("SSD1306 allocation failed"));
    for(;;); // Don't proceed, loop forever
  }

#endif

  oledDisplay.display(); // the Adafruit library display the Adafruit logo set in library initialization
  delay(2000); // Pause for 2 seconds  

  oledDisplay.setTextSize(1);             // Normal 1:1 pixel scale
  oledDisplay.setTextWrap(false); // text that does not fit in the current row is clipped

  setTextMode(false); // no inverted

  oledDisplay.clearDisplay();
  oledDisplay.display();   
}

void oledDisplayShutdown()
{
  oledDisplay.clearDisplay();
  oledDisplay.display();
}

void setTextMode(const bool inverted)
{
  if (inverted)
  {
    oledDisplay.setTextColor(BLACK_COLOR, WHITE_COLOR); // Draw 'inverse' text
  }

  else
  {
    oledDisplay.setTextColor(WHITE_COLOR, BLACK_COLOR); // white on black text 
  }  
}

void clearDisplayRow(const int displayRow)
{
  oledDisplay.setCursor(0, mapDisplayRowToPixelYPos(displayRow));

  for (int i=0; i<DISPLAY_NUM_COLS; i++)
    oledDisplay.write(' ');
}

//----------------------------
// Display single setting data
//----------------------------

int displaySettingName(const int settingId, const int col, const int row, const bool printColon=true)
{
  strcpy_P(nameBuffer, (char *)pgm_read_word(&(settingNames[settingId])));  

  int numCols = strlen(nameBuffer);

  oledDisplay.setCursor(mapDisplayColToPixelXPos(col), mapDisplayRowToPixelYPos(row));

  oledDisplay.print(nameBuffer);

  if (printColon)
  {
    oledDisplay.print(F(": "));
    numCols += 2;
  }

  return col + numCols;
}

void displaySettingUIntValue(const unsigned int val, const int col, const int row, const bool clearRow)
{
  if (clearRow)
    clearDisplayRow(row);

  oledDisplay.setCursor(mapDisplayColToPixelXPos(col), mapDisplayRowToPixelYPos(row));

  oledDisplay.print(val);
}

void displaySettingBoolValue(const bool val, const int col, const int row, const bool clearRow)
{
  if (clearRow)
    clearDisplayRow(row);

  oledDisplay.setCursor(mapDisplayColToPixelXPos(col), mapDisplayRowToPixelYPos(row));

  oledDisplay.print(val ? "ON" : "OFF");
}

void displaySettingValue(SettingValue &settingValue, const int col, const int row, const bool clearRow)
{
  switch(settingValue.valueType)
  {
    case SETTING_TYPE_BOOL:
      displaySettingBoolValue(settingValue.currBoolValue, col, row, clearRow);
      return;

    case SETTING_TYPE_UINT:
      displaySettingUIntValue(settingValue.currUIntValue, col, row, clearRow);
      return;
  }
}

void displaySettingToMainMenu(const int settingId, const int menuRow, const bool inverted, const bool clearRow)
{
  int currCol = DISPLAY_SETTINGS_MENU_FIRST_COL;
  int currRow = mapMenuRowToDisplayRow(menuRow);

  setTextMode(inverted);

  if (clearRow)
    clearDisplayRow(currRow);

  switch(settingId)
  {
    case ID_SETTING_SHOT_DELAY_MS:
      currCol = displaySettingName(settingId, currCol, currRow);
      displaySettingUIntValue(tempSettings.shotDelayMs, currCol, currRow, false);
      break;

    case ID_SETTING_BURST_DELAY_MS:
      currCol = displaySettingName(settingId, currCol, currRow);
      displaySettingUIntValue(tempSettings.burstDelayMs, currCol, currRow, false);
      break;

    case ID_SETTING_DWELL_MS:
      currCol = displaySettingName(settingId, currCol, currRow);
      displaySettingUIntValue(tempSettings.dwellMs, currCol, currRow, false);
      break;

    case ID_SETTING_TRIGGER_DEBOUNCING_MS:
      currCol = displaySettingName(settingId, currCol, currRow);
      displaySettingUIntValue(tempSettings.triggerDebouncingMs, currCol, currRow, false);
      break;

    case ID_SETTING_NUM_SHOTS_PER_BURST:
      currCol = displaySettingName(settingId, currCol, currRow);
      displaySettingUIntValue(tempSettings.numShotsPerBurst, currCol, currRow, false);
      break;

    case ID_SETTING_SHOT_LIMIT:
      currCol = displaySettingName(settingId, currCol, currRow);
      displaySettingUIntValue(tempSettings.shotLimit, currCol, currRow, false);
      break;

    case ID_SETTING_FULL_AUTO_BURST:
      currCol = displaySettingName(settingId, currCol, currRow);
      displaySettingBoolValue(tempSettings.fullAutoBurst, currCol, currRow, false);
      break;

    case ID_SETTING_BINARY_TRIGGER:
      currCol = displaySettingName(settingId, currCol, currRow);
      displaySettingBoolValue(tempSettings.binaryTrigger, currCol, currRow, false);
      break;

    case ID_SETTING_FORCE_RELOAD:
      currCol = displaySettingName(settingId, currCol, currRow);
      displaySettingBoolValue(tempSettings.forceReload, currCol, currRow, false);
      break;

    case ID_SETTING_MUZZLE_FLASH:
      currCol = displaySettingName(settingId, currCol, currRow);
      displaySettingBoolValue(tempSettings.muzzleFlash, currCol, currRow, false);
      break;

    case ID_SETTINGS_INVERTED_FIRE_SELECTOR:
      currCol = displaySettingName(settingId, currCol, currRow);
      displaySettingBoolValue(tempSettings.invertedFireSelector, currCol, currRow, false);
      break;
  }  
}

//================================
// settingValue struct management
//================================

void initSettingValueBool(bool *valuePtr)
{
  settingValue.valueType = SETTING_TYPE_BOOL;
  settingValue.boolValuePtr = valuePtr;
  settingValue.currBoolValue = *valuePtr;
}

void initSettingValueUInt(unsigned int *valuePtr, const unsigned int minVal, const unsigned int maxVal, const unsigned int longPressDelta)
{
  settingValue.valueType = SETTING_TYPE_UINT;
  settingValue.uintValuePtr = valuePtr;
  settingValue.currUIntValue = *valuePtr;
  settingValue.uintValueMin = minVal;
  settingValue.uintValueMax = maxVal;
  settingValue.longPressDelta = longPressDelta;
} 

void commitSetting()
{
  switch(settingValue.valueType)
  {
    case SETTING_TYPE_BOOL:
      *(settingValue.boolValuePtr) = settingValue.currBoolValue;
      return;

    case SETTING_TYPE_UINT:
      *(settingValue.uintValuePtr) = settingValue.currUIntValue;
      return;
  }
}

void getSettingValue(const int settingId)
{
  switch(settingId)
  {
    case ID_SETTING_SHOT_DELAY_MS:
      initSettingValueUInt(&tempSettings.shotDelayMs, SETTING_DELAY_VALUE_MIN, SETTING_DELAY_VALUE_MAX, SETTING_DELAY_LONG_PRESS_DELTA); 
      return;

    case ID_SETTING_BURST_DELAY_MS:
      initSettingValueUInt(&tempSettings.burstDelayMs, SETTING_DELAY_VALUE_MIN, SETTING_DELAY_VALUE_MAX, SETTING_DELAY_LONG_PRESS_DELTA); 
      return;

    case ID_SETTING_DWELL_MS:
      initSettingValueUInt(&tempSettings.dwellMs, SETTING_DELAY_VALUE_MIN, SETTING_DELAY_VALUE_MAX, SETTING_DELAY_LONG_PRESS_DELTA); 
      return;

    case ID_SETTING_TRIGGER_DEBOUNCING_MS:
      initSettingValueUInt(&tempSettings.triggerDebouncingMs, SETTING_DELAY_VALUE_MIN, SETTING_DELAY_VALUE_MAX, SETTING_DELAY_LONG_PRESS_DELTA); 
      return;

    case ID_SETTING_NUM_SHOTS_PER_BURST:
      initSettingValueUInt(&tempSettings.numShotsPerBurst, SETTING_NUM_SHOT_VALUE_MIN, SETTING_NUM_SHOT_VALUE_MAX, SETTING_NUM_SHOT_LONG_PRESS_DELTA); 
      return;

    case ID_SETTING_SHOT_LIMIT:
      initSettingValueUInt(&tempSettings.shotLimit, SETTING_NUM_SHOT_VALUE_MIN, SETTING_NUM_SHOT_VALUE_MAX, SETTING_NUM_SHOT_LONG_PRESS_DELTA); 
      return;

    case ID_SETTING_FULL_AUTO_BURST:
      initSettingValueBool(&tempSettings.fullAutoBurst); 
      return;

    case ID_SETTING_BINARY_TRIGGER:
      initSettingValueBool(&tempSettings.binaryTrigger); 
      return;

    case ID_SETTING_FORCE_RELOAD:
      initSettingValueBool(&tempSettings.forceReload); 
      return;

    case ID_SETTING_MUZZLE_FLASH:
      initSettingValueBool(&tempSettings.muzzleFlash); 
      return;

    case ID_SETTINGS_INVERTED_FIRE_SELECTOR:
      initSettingValueBool(&tempSettings.invertedFireSelector); 
      return;
  }  
}

//================================
// Mode menu page management
//================================

void displayMenuPage(const int page, const int menuRow)
{
  if (page >= numMenuPages)
    // sanity check
    return;    

  oledDisplay.clearDisplay();

  currMenuPage = page;

  // display menu header

  setTextMode(true); // Draw 'inverse' text

  clearDisplayRow(0);

  oledDisplay.setCursor(0,0);
  oledDisplay.print(F("Settings ("));
  oledDisplay.print(page + 1);
  oledDisplay.print(F("/"));
  oledDisplay.print(numMenuPages);
  oledDisplay.print(F(")"));

  setTextMode(false);

  currMenuPageFirstSettingId = page * NUM_MAX_SETTINGS_PER_MENU_PAGE;

  currMenuPageNumSettings = evalMenuPageNumSettingsByFirstSettingId(currMenuPageFirstSettingId);

  currMenuCursorPos = -1;

  int currSettingId;

  for (int i=0; i<currMenuPageNumSettings; i++)
  {
    currSettingId = mapMenuRowToSettingId(i);

    if (currSettingId == -1)
      break;

    displaySettingToMainMenu(currSettingId, i, false, false);
  }

  moveMenuCursorTo(menuRow);

  oledDisplay.display();
}

//--------------------
// Cursor management 
//--------------------

void moveMenuCursorTo(const int menuRow)
{
  if (menuRow == currMenuCursorPos)
    return;

  if (mapMenuRowToSettingId(menuRow) == -1)
    return;

  if (currMenuCursorPos != -1)
  {
    // remove the cursor from the last position
    displaySettingToMainMenu(mapMenuRowToSettingId(currMenuCursorPos), currMenuCursorPos, false, true);
  }

  // set the cursor to the new position
  currMenuCursorPos = menuRow;
  displaySettingToMainMenu(mapMenuRowToSettingId(currMenuCursorPos), currMenuCursorPos, true, true);
  oledDisplay.display();
}

void moveMenuCursorUp()
{
  if (currMenuCursorPos > 0)
  {
    moveMenuCursorTo(currMenuCursorPos - 1);
  }

  else if (currMenuPage > 0)
  {
    int precPage = currMenuPage - 1;

    displayMenuPage(precPage, evalMenuPageNumSettings(precPage) - 1);
  }
}

void moveMenuCursorDown()
{
  if (currMenuCursorPos < currMenuPageNumSettings - 1)
  {
    moveMenuCursorTo(currMenuCursorPos + 1);
  }

  else if (currMenuPage < numMenuPages - 1)
    displayMenuPage(currMenuPage + 1, 0);
}

int updateMenuPage()
{
  if (thereIsANewPress(ID_BUTTON_JOY_UP))
  {
    moveMenuCursorUp();
    return MENU_CONTINUE;
  }

  if (thereIsANewPress(ID_BUTTON_JOY_DOWN))
  {
    moveMenuCursorDown();
    return MENU_CONTINUE;
  }

  if (thereIsANewPress(ID_BUTTON_JOY_LEFT))
  {
    oledDisplayShutdown();    
    return MENU_DISCARD_AND_EXIT;
  }

  if (thereIsANewPress(ID_BUTTON_JOY_RIGHT))
  {
    oledDisplayShutdown();
    return MENU_SAVE_AND_EXIT;
  }

  if (thereIsANewPress(ID_BUTTON_JOY_SELECT))
  {
    menuMode = MENU_MODE_MODIFY_SETTING;

    displayPageForSettingModify();
    return MENU_CONTINUE;
  }

  // no buttons new press
  return MENU_CONTINUE;
}

//===============================
// Mode modify setting management
//===============================

void displayPageForSettingModify()
{
  int settingId = mapMenuRowToSettingId(currMenuCursorPos);

  getSettingValue(settingId);

  oledDisplay.clearDisplay();

  // display header

  setTextMode(true); // Draw 'inverse' text

  clearDisplayRow(0);

  oledDisplay.setCursor(0,0);
  displaySettingName(settingId, 0, 0, false);  

  // display setting value

  setTextMode(false);

  displaySettingValue(settingValue, 0, DISPLAY_SETTINGS_MODIFICATION_ROW, false);

  oledDisplay.display();

  currPressButtonId = ID_BUTTON_INVALID;
}

//-----------------------------------
// Bool value modification management
//-----------------------------------

void updateMenuSettingBool()
{
  if (thereIsANewPress(ID_BUTTON_JOY_UP))
  {
    settingValue.currBoolValue = !settingValue.currBoolValue;
    displaySettingBoolValue(settingValue.currBoolValue, 0, DISPLAY_SETTINGS_MODIFICATION_ROW, true);
    oledDisplay.display();
    return;
  }

  if (thereIsANewPress(ID_BUTTON_JOY_DOWN))
  {
    settingValue.currBoolValue = !settingValue.currBoolValue;
    displaySettingBoolValue(settingValue.currBoolValue, 0, DISPLAY_SETTINGS_MODIFICATION_ROW, true);
    oledDisplay.display();
    return;
  }
}

//-----------------------------------
// UInt value modification management
//-----------------------------------

void incDecAndDisplaySettingValue(const unsigned int delta, const bool doIncrement)
{
  if (doIncrement)
  {
    if (settingValue.currUIntValue < settingValue.uintValueMax)
    {
      settingValue.currUIntValue = settingValue.currUIntValue < settingValue.uintValueMax - delta ? settingValue.currUIntValue + delta : settingValue.uintValueMax;
      displaySettingUIntValue(settingValue.currUIntValue, 0, DISPLAY_SETTINGS_MODIFICATION_ROW, true);
      oledDisplay.display();
    }

    return;
  }

  if (settingValue.currUIntValue > settingValue.uintValueMin)
  {
    settingValue.currUIntValue = settingValue.currUIntValue > settingValue.uintValueMin + delta ? settingValue.currUIntValue - delta : settingValue.uintValueMin;
    displaySettingUIntValue(settingValue.currUIntValue, 0, DISPLAY_SETTINGS_MODIFICATION_ROW, true);
    oledDisplay.display();
  }
}

// manage short and long presses of the cursor up/down buttons for the setting modify mode
bool manageCursorUpDown(const int buttonId, const bool doIncrement)
{
  if (thereIsANewPress(buttonId))
  {
    currPressStartTimeMs = millis();
    lastModificationTimeMs = currPressStartTimeMs;
    currPressButtonId = buttonId;
    incDecAndDisplaySettingValue(1, doIncrement);
    return true;
  }

  // not a new press

  if (!buttonIsPressed(buttonId))
  {
    if (currPressButtonId == buttonId)
      // the current pressed button has been released
      currPressButtonId = ID_BUTTON_INVALID;

    return false;
  }

  // button is pressed and is not a new press

  if (currPressButtonId != buttonId) 
  {
    // what happened here?
    currPressButtonId = ID_BUTTON_INVALID;
    return true;
  }

  unsigned long timeNowMs = millis();

  if (timeNowMs - currPressStartTimeMs >= minDurationForLongPressMs)
  {
    if (timeNowMs - lastModificationTimeMs >= longPressIntervalBetweenIncDecMs)
    {
      lastModificationTimeMs = timeNowMs;
      incDecAndDisplaySettingValue(settingValue.longPressDelta, doIncrement);
      return true;
    }
  }

  return true;
}

void updateMenuSettingUInt()
{
  if (manageCursorUpDown(ID_BUTTON_JOY_UP, true))
  {
    return;
  }

  if (manageCursorUpDown(ID_BUTTON_JOY_DOWN, false))
  {
    return;
  }
}

void updateMenuSetting()
{
  if (thereIsANewPress(ID_BUTTON_JOY_LEFT))
  {
    // discard setting modification

    menuMode = MENU_MODE_DISPLAY_MENU_PAGE;

    displayMenuPage(currMenuPage, currMenuCursorPos);

    return;
  }

  if (thereIsANewPress(ID_BUTTON_JOY_RIGHT))
  {
    // save setting modification
    commitSetting();

    menuMode = MENU_MODE_DISPLAY_MENU_PAGE;

    displayMenuPage(currMenuPage, currMenuCursorPos);

    return;
  }

  switch(settingValue.valueType)
  {
    case SETTING_TYPE_UINT:
      updateMenuSettingUInt();
      return;

    case SETTING_TYPE_BOOL:
      updateMenuSettingBool();
      return;
  }
}

//=============================
// Top level menu interface
//=============================

bool commitSettings(Settings &settings)
{
  if ( (settings.binaryTrigger == tempSettings.binaryTrigger) &&
       (settings.forceReload == tempSettings.forceReload) &&
       (settings.fullAutoBurst == tempSettings.fullAutoBurst) &&
       (settings.invertedFireSelector == tempSettings.invertedFireSelector) &&
       (settings.muzzleFlash == tempSettings.muzzleFlash) &&
       (settings.burstDelayMs == tempSettings.burstDelayMs) &&
       (settings.shotDelayMs == tempSettings.shotDelayMs) &&
       (settings.dwellMs == tempSettings.dwellMs) &&
       (settings.triggerDebouncingMs == tempSettings.triggerDebouncingMs) &&
       (settings.numShotsPerBurst == tempSettings.numShotsPerBurst) &&
       (settings.shotLimit == tempSettings.shotLimit) )
  {
    // settings not modified
    return false;
  }

  settings = tempSettings;
  return true;     
}

void displayMenuExitMessage(const int msgId, const unsigned long delayMs)
{
  oledDisplay.clearDisplay();

  setTextMode(false);

  oledDisplay.setCursor(mapDisplayColToPixelXPos(MENU_EXIT_MESSAGE_COL), mapDisplayRowToPixelYPos(MENU_EXIT_MESSAGE_FIRST_ROW));
  oledDisplay.print(F("Settings:"));
  oledDisplay.setCursor(mapDisplayColToPixelXPos(MENU_EXIT_MESSAGE_COL), mapDisplayRowToPixelYPos(MENU_EXIT_MESSAGE_FIRST_ROW + 1));

  switch(msgId)
  {
    case ID_MENU_EXIT_MESSAGE_NOT_MODIFIED:
      oledDisplay.print(F("NOT MODIFIED"));
      break;

    case ID_MENU_EXIT_MESSAGE_DISCARDED:
      oledDisplay.print(F("DISCARDED"));
      break;

    case ID_MENU_EXIT_MESSAGE_SAVED:
      oledDisplay.print(F("SAVED TO EEPROM"));
      break;
  }

  oledDisplay.display();

  delay(delayMs);

  oledDisplayShutdown();
}

void displayFirstMenuPage(Settings &settings)
{
  tempSettings = settings;

  menuMode = MENU_MODE_DISPLAY_MENU_PAGE;

  displayMenuPage(0, 0);
}

int updateMenu()
{
  switch(menuMode)
  {
    case MENU_MODE_DISPLAY_MENU_PAGE:
      return updateMenuPage();

    case MENU_MODE_MODIFY_SETTING:
      updateMenuSetting();

      return MENU_CONTINUE;        
  }

  return MENU_CONTINUE;
}
