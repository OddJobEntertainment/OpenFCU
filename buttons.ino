
//-------------------------------------------
// Utilities for buttons input and debouncing
//-------------------------------------------

// NOTE: the button id is the 0 based index of the button in the array of buttons data

int getButtonPin(const int buttonId)
{
  return buttons[buttonId].pin;
}

bool buttonIsPressed(const int buttonId)
{
  return buttons[buttonId].lastValidStatus == BUTTON_PRESSED;
}

unsigned long buttonPressDuration(const int buttonId)
{
  ButtonData &bd = buttons[buttonId];
  
  return bd.lastValidStatus == BUTTON_PRESSED ? millis() - bd.startTimeMs : 0;
}

void readButtonWithDebouncing(const int buttonId)
{
  ButtonData &bd = buttons[buttonId];

  unsigned long timeNow = millis();
  int statusNow = digitalRead(bd.pin);

  if (statusNow != bd.buttonStatus)
  {
    bd.startTimeMs = timeNow;
    bd.buttonStatus = statusNow;
  }

  if (timeNow - bd.startTimeMs > buttonsDebouncingTimeMs)
    bd.lastValidStatus = bd.buttonStatus;
}

bool thereIsANewPress(const int buttonId)
{
  ButtonData &bd = buttons[buttonId];

  if (bd.lastValidStatus != BUTTON_PRESSED)
  {
    bd.precStatusIsPressed = false;
    return false;
  }

  // the button is pressed

  if (bd.precStatusIsPressed)
    // the last time was pressed too, isn't a new press
    return false;

  // is a new press

  bd.precStatusIsPressed = true;
  return true;
}

// call this function every loop() cycle to keep all buttons data updated
void readAllButtons()
{
  for (int i = 0; i < numButtons; i++)
  {
    readButtonWithDebouncing(i);
  }
}
