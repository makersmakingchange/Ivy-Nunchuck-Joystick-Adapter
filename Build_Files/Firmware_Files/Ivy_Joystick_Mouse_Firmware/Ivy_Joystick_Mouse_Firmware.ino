/*
  File: OpenAT_Ivy_Joystick_Mouse.ino
  Software: USB gamepad and mouse funtionality for a Nunchuck Controller.
  Developed by: Makers Making Change
  Version: (27 September 2023)
  License: GPL v3

  Copyright (C) 2023 Neil Squire Society
  This program is free software: you can redistribute it and/or modify it under the terms of
  the GNU General Public License as published by the Free Software Foundation,
  either version 3 of the License, or (at your option) any later version.
  This program is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY;
  without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
  See the GNU General Public License for more details.
  You should have received a copy of the GNU General Public License along with this program.
  If not, see <http://www.gnu.org/licenses/>
*/
// This firmware enables a Nunchuck controller to be used as a USB HID Mouse and/or USB HID Gamepad. The Nunchuck Controller is connected
// via an Adafruit Wii Nunchuck Breakout Board through I2C to an Adafruit SAMD21 Qt Py development board.

#include "XACGamepad.h" // Definition of USB HID Gamepad compatible with Xbox Adaptive Controller
#include "OpenAT_Joystick_Response.h" // Non-linear mouse cursor speed response
#include <FlashStorage.h> // Non-volatile memory for storing settings
#include <TinyUSB_Mouse_and_Keyboard.h> // Convenience library for TinyUSB HID Mouse.
#include <WiiChuck.h> //Nunchuck communication
#include <Adafruit_NeoPixel.h> //Lights on QtPy


#define MODE_MOUSE 1
#define MODE_GAMEPAD 0

#define USB_DEBUG  0 // Set this to 0 for best performance.

#define UPDATE_INTERVAL   5 // TBD Update interval for perfoming HID actions (in milliseconds)
#define DEFAULT_DEBOUNCING_TIME 5

#define JOYSTICK_DEFAULT_DEADZONE_LEVEL      0              //Joystick deadzone
#define JOYSTICK_MIN_DEADZONE_LEVEL          0
#define JOYSTICK_MAX_DEADZONE_LEVEL          10
#define JOYSTICK_MAX_DEADZONE_VALUE          64             //Out of 127

#define JOYSTICK_MAX_VALUE                   127

#define JOYSTICK_NUNCHUCK_X_MIN 0
#define JOYSTICK_NUNCHUCK_X_MAX 255
#define JOYSTICK_NUNCHUCK_Y_MIN 0
#define JOYSTICK_NUNCHUCK_Y_MAX 255

#define SLOW_SCROLL_DELAY                    200            //Minimum time, in ms, between each slow scroll action (number of slow scroll actions defined below)
#define FAST_SCROLL_DELAY                    60             //Minimum time, in ms, between each fast scroll action (higher delay = slower scroll speed)
#define SLOW_SCROLL_NUM                      10             //Number of times to scroll at the slow scroll rate

#define LONG_PRESS_MILLIS                    2000

//#define JOYSTICK_REACTION_TIME             30             //Minimum time between each action in ms
#define SWITCH_REACTION_TIME                 100            //Minimum time between each switch action in ms
#define STARTUP_DELAY_TIME                   5000           //Time to wait on startup
#define FLASH_DELAY_TIME                     5              //Time to wait after reading/writing flash memory

#define MOUSE_MAX_XY                         6            // Mouse pixels per update at max deflection (max "speed")

//Define model number and version number
#define JOYSTICK_MODEL                        1
#define JOYSTICK_VERSION                      2


void resetJoystick(void); //forward declaration of reset function

XACGamepad gamepad;         //Starts an instance of the USB gamepad object
Accessory nunchuck;         //instance of nunchuck controller from WiiChuck library

// Declare variables for settings. Default values are written to flash memory on first run, then read back on
// subsequent power cycles.
int isConfigured = 0;
int modelNumber = 3;  // 1 - Forest Hub, 2 - Ivy Nunchuck Adapter, Enabled Controller Mini, 3 - 
int versionNumber = 1;
int deadzoneLevel = 1;
int operatingMode = MODE_MOUSE;   // 1 = Mouse mode, 0 = Joystick Mode


// Declarge variables to store in flash
FlashStorage(isConfiguredFlash, int);
FlashStorage(modelNumberFlash, int);
FlashStorage(versionNumberFlash, int);
FlashStorage(deadzoneLevelFlash, int);
FlashStorage(operatingModeFlash, int);

long lastInteractionUpdate;
long ZPressStartMillis = 0;

// Declare joystick input variables
int inputX;
int inputY;
int outputX;
int outputY;
int responseX = 0;
int responseY = 0;

//Declare switch state variables for each switch
int switchAState;           // Mouse mode = left click
int switchBState;           // Mouse mode = middle click
int switchCState;           // Mouse mode = right click
int switchJoyState;         // Mouse mode = left click              // Switch in the joystick (press down)

// Previous status of switches
int switchAPrevState = HIGH;
int switchBPrevState = HIGH;
int switchCPrevState = HIGH;
int switchJoyPrevState = HIGH;


int currentDeadzoneValue;

bool settingsEnabled = false;

//***API FUNCTIONS***// - DO NOT CHANGE
typedef void (*FunctionPointer)(bool, bool, String);

//Type definition for API function pointer

typedef struct {                                  //Type definition for API function list
  String endpoint;                                //Unique two character end point
  String code;                                    //Unique one character command code
  String parameter;                               //Parameter that is passed to function
  FunctionPointer function;                       //API function pointer
} _functionList;

//Switch structure
typedef struct {
  uint8_t switchNumber;
  String switchButtonName;
  uint8_t switchButtonNumber;
} switchStruct;


// Declare individual API functions with command, parameter, and corresponding function
_functionList getModelNumberFunction =            {"MN", "0", "0", &getModelNumber};
_functionList getVersionNumberFunction =          {"VN", "0", "0", &getVersionNumber};
_functionList getJoystickDeadZoneFunction =       {"DZ", "0", "0", &getJoystickDeadZone};
_functionList setJoystickDeadZoneFunction =       {"DZ", "1", "",  &setJoystickDeadZone};

// Declare array of API functions
_functionList apiFunction[5] = {
  getModelNumberFunction,
  getVersionNumberFunction,
  getJoystickDeadZoneFunction,
  setJoystickDeadZoneFunction
};

//Switch properties
const switchStruct switchProperty[] {
  {1, "A", 1},
  {2, "B", 2},
  {3, "C", 3},
  {4, "Joy", 4}
};

Adafruit_NeoPixel pixels(1, PIN_NEOPIXEL);  // Create a pixel strand with 1 pixel on QtPy

//***MICROCONTROLLER AND PERIPHERAL CONFIGURATION***//
// Function   : setup
//
// Description: This function handles the initialization of variables, pins, methods, libraries. This function only runs once at powerup or reset.
//
// Parameters :  void
//
// Return     : void
//*********************************//
void setup() {
  pixels.begin(); // Initiate pixel on microcontroller
  pixels.setPixelColor(0, pixels.Color(255, 0, 0)); // Turn LED red to start
  pixels.show();

  if (USB_DEBUG) {
    Serial.begin(115200);
  } else {
    //Serial.end();
  } // endif

  // Read Memory
  readMemory();
  delay(FLASH_DELAY_TIME);

 // Begin HID gamepad or mouse, depending on mode selection
  switch (operatingMode) {
    case MODE_MOUSE:
      Mouse.begin();
      break;
    case MODE_GAMEPAD:
      gamepad.begin();
      break;
    default:
      break;
  }

  // Initialize Joystick
  initJoystick();
  
  if (USB_DEBUG) {
    while (!Serial) {
      delay(1);
    }
    Serial.println("USB Debug mode on. Change flag in code for best performance.");
  }
  
  checkSetupMode(); // Check to see if operating mode change

  delay(STARTUP_DELAY_TIME); //startup delay to faciliate reprogramming

  // Turn on indicator light, depending on mode selection
  switch(operatingMode) {
    case MODE_MOUSE:
      pixels.setPixelColor(0, pixels.Color(255, 255, 0)); // Turn LED yellow
      pixels.show();
      break;
    case MODE_GAMEPAD:
      pixels.setPixelColor(0, pixels.Color(0, 0, 255)); // Turn LED blue
      pixels.show();
      break;
  }

  lastInteractionUpdate = millis();  // get first timestamp

}

void loop() {

  settingsEnabled = serialSettings(settingsEnabled); //Check to see if setting option is enabled

  if ((millis()-lastInteractionUpdate) >= UPDATE_INTERVAL) {
    lastInteractionUpdate = millis();  // get timestamp
    readJoystick();
    readSwitch();

    joystickActions();

    switch(operatingMode){
      case MODE_MOUSE:      // Perform Mouse HID buttons
        mouseSwitches();
        break;
      case MODE_GAMEPAD:    // Update Gamepad HID buttons
        joystickSwitches();
    }
  }
  delay(1);
}

//*********************************//
// Joystick Functions
//*********************************//

//***READ Memory FUNCTION***//
// Function   : readMemory
//
// Description: This function reads values from Memory. The first time it is called, it writes default values.
//
// Parameters : void
//
// Return     : void
//****************************************//
void readMemory() {
  //Check if it's first time running the code
  isConfigured = isConfiguredFlash.read();
  delay(FLASH_DELAY_TIME);

  if (isConfigured == 0) { //Define default settings if it's first time running the code

    //Write default settings to flash storage
    modelNumberFlash.write(modelNumber);
    versionNumberFlash.write(versionNumber);
    deadzoneLevelFlash.write(deadzoneLevel);
    operatingModeFlash.write(operatingMode);

    //Mark as configured.
    isConfiguredFlash.write(1);
    delay(FLASH_DELAY_TIME);

  } else {
    //Load settings from flash storage
    modelNumber = modelNumberFlash.read();
    versionNumber = versionNumberFlash.read();
    deadzoneLevel = deadzoneLevelFlash.read();
    operatingMode = operatingModeFlash.read();
    delay(FLASH_DELAY_TIME);
  }

  //Serial print settings
  Serial.print("Model Number: ");
  Serial.println(modelNumber);

  Serial.print("Version Number: ");
  Serial.println(versionNumber);

  Serial.print("Operating Mode: ");
  Serial.println(operatingMode);

  Serial.print("Deadzone Level: ");
  Serial.println(deadzoneLevel);
}


//*********************************//
// Joystick Functions
//*********************************//

//***INITIALIZE JOYSTICK FUNCTION***//
// Function   : initJoystick
//
// Description: This function initializes joystick as input.
//
// Parameters : void
//
// Return     : void
//****************************************//
void initJoystick()
{
  getJoystickDeadZone(true, false); //Get joystick deadzone stored in memory
  nunchuck.begin();

  if (USB_DEBUG) {
    Serial.println("Deadzone set. Nunchuck initialized.");
  }
}

//***READ JOYSTICK FUNCTION**//
// Function   : readJoystick
//
// Description: This function reads the current raw values of potentiometers, checks if values exceed deadband, and calculates
//              joystick movements. Outputs true if joystick should be moved.
//
// Parameters :  Void
//
// Return     : Void
//*********************************//

void readJoystick() {

  nunchuck.readData(); // Get updated data from Numchuck
  inputX = nunchuck.getJoyX();
  inputY = nunchuck.getJoyY();

  // Map Nunchuck axis to joystick
  int rawX = map(inputX, JOYSTICK_NUNCHUCK_X_MIN, JOYSTICK_NUNCHUCK_X_MAX, 0, 1023); // Nunchuck returns 0 - 255
  int rawY = map(inputY, JOYSTICK_NUNCHUCK_Y_MIN, JOYSTICK_NUNCHUCK_Y_MAX, 0, 1023);

  // Apply pre-calculated response curve
  // Scales values from -511 to 511 and applies deadband
  int signed_x = rawX - 511;
  int signed_y = rawY - 511;

  responseX = 0;
  responseY = 0;

  if (signed_x < 0) {
    responseX = -X_RESPONSE[-signed_x];
  } else {
    responseX =  X_RESPONSE[ signed_x];
  }

  if (signed_y < 0) {
    responseY = -Y_RESPONSE[-signed_y];
  } else {
    responseY =  Y_RESPONSE[ signed_y];
  }

  outputX = 0;
  outputY = 0;

  
  switch(operatingMode) {
    case MODE_MOUSE: // Perform Mouse HID movement
      outputX = MOUSE_DIR_X * int(responseX * MOUSE_MAX_XY / 511.0f);
      outputY = MOUSE_DIR_Y * int(responseY * MOUSE_MAX_XY / 511.0f);
      break;
    case MODE_GAMEPAD: // Perform joystick HID movement
      outputX = int(responseX / 511.0f * JOYSTICK_MAX_VALUE);
      outputY = int(responseY / 511.0f * JOYSTICK_MAX_VALUE);
      break;
  }

  //  double outputMag = calcMag(outputX, outputY);
  //
  //  //Apply radial deadzone *********************************************************************
  //  if (outputMag <= currentDeadzoneValue)
  //  {
  //    outputX = 0;
  //    outputY = 0;
  //  }

}


//***JOYSTICK ACTIONS FUNCTION**//
// Function   : joystickActions
//
// Description: This function executes joystick or mouse movements.
//
// Parameters :  Void
//
// Return     : Void
//*********************************//

void joystickActions() {

  switch(operatingMode) {
    case MODE_MOUSE: // Perform Mouse HID movement
      Mouse.move(outputX, outputY, 0); // X pixels, Y pixels, scroll wheel
      break;
    case MODE_GAMEPAD: // Perform joystick HID movement
      gamepadJoystickMove(outputX, outputY);
      break;
  }
    
}

//***CALCULATE THE MAGNITUDE OF A VECTOR***//
// Function   : calcMag
//
// Description: This function calculates the magntidue of a vector using the x and y values.
//              It returns a double containing the magnitude of the vector.
//
// Parameters :  x : double : The x component of the vector
//               y : double : The y component of the vector
//
// Return     : double
//******************************************//
double calcMag(double x, double y) {
  double magnitude = sqrt(x * x + y * y);
  return magnitude;
}

//***READ SWITCH FUNCTION**//
// Function   : readSwitch
//
// Description: This function reads the current digital values of pull-up pins.
//
// Parameters :  Void
//
// Return     : Void
//*********************************//

void readSwitch() {

  // Update Prev Switch States
  switchAPrevState = switchAState;
  switchBPrevState = switchBState;
  switchCPrevState = switchCState;
  switchJoyPrevState = switchJoyState;


  nunchuck.readData();
  switchAState = !nunchuck.getButtonC(); // flip as getButtonC returns HIGH when button pressed.
  switchBState = !nunchuck.getButtonZ();
  switchCState = HIGH;
  switchJoyState = HIGH;

}

//***JOYSTICK MODE SWITCH FUNCTION**//
// Function   : joystickSwitches
//
// Description: This function executes joystick functions of the switches.
//
// Parameters :  Void
//
// Return     : Void
//*********************************//

void joystickSwitches() {

  //Perform button actions
  if (!switchAState) {
    gamepadButtonPress(switchProperty[0].switchButtonNumber);
  }
  else if (switchAState && !switchAPrevState) {
    gamepadButtonRelease(switchProperty[0].switchButtonNumber);
  }

  if (!switchBState) {
    gamepadButtonPress(switchProperty[1].switchButtonNumber);
  } else if (switchBState && !switchBPrevState) {
    gamepadButtonRelease(switchProperty[1].switchButtonNumber);
  }

  if (!switchCState) {
    gamepadButtonPress(switchProperty[2].switchButtonNumber);
  } else if (switchCState && !switchCPrevState) {
    gamepadButtonRelease(switchProperty[2].switchButtonNumber);
  }

  if (!switchJoyState) {
    gamepadButtonPress(switchProperty[3].switchButtonNumber);
  } else if (switchJoyState && !switchJoyPrevState) {
    gamepadButtonRelease(switchProperty[3].switchButtonNumber);
  }

}

//***MOUSE MODE SWITCH FUNCTION**//
// Function   : mouseSwitches
//
// Description: This function executes mouse functions of the switches.
//
// Parameters :  Void
//
// Return     : Void
//*********************************//

void mouseSwitches() {

  //Perform button actions
  if (!switchAState) {
    Mouse.press(MOUSE_LEFT);
  }
  else if (switchAState && !switchAPrevState) {
    Mouse.release(MOUSE_LEFT);
  }

  if (!switchBState) {
    Mouse.press(MOUSE_RIGHT);
    
    if (switchBPrevState){
      ZPressStartMillis = millis();
    }

    // IF LONG PRESS, ENTER SCROLL MODE
    if ((millis()-ZPressStartMillis) >= LONG_PRESS_MILLIS){
      int counter = 0;
      pixels.setPixelColor(0, pixels.Color(255, 0, 255)); // Turn LED purple
      pixels.show();
      
      while(1){
        readJoystick();
        readSwitch();

        if (outputY>0){
          Mouse.move(0,0,-1);
          counter++;
        } 
        else if (outputY<0){
          Mouse.move(0,0,1);
          counter++;
        } else if (outputY==0){
          counter=0;
        }

        if (counter==0){
          delay(2);                             // low delay before any scroll actions have been completed, continue to read switches and joystick
        }else if (counter==1){
          delay(500);                           // long delay after first scroll action, to allow user to do single scroll
        } else if (counter < SLOW_SCROLL_NUM){
          delay(SLOW_SCROLL_DELAY);             // first 10 scroll actions are slower (have longer delay) to allow precise scrolling
        } else{
          delay(FAST_SCROLL_DELAY);             // scroll actions after first 10 are faster (have less delay) to allow fast scrolling
        }

        if (!switchBState && switchBPrevState){
          ZPressStartMillis=millis();
          pixels.setPixelColor(0, pixels.Color(255, 255, 0)); // Turn LED yellow
          pixels.show();
          delay(5);
          break;
        }
        
      }
      
    }
    
  } else if (switchBState && !switchBPrevState) {
    Mouse.release(MOUSE_RIGHT);
    //Bpressed = false;
  }

  if (!switchCState) {
    Mouse.press(MOUSE_RIGHT);
  } else if (switchCState && !switchCPrevState) {
    Mouse.release(MOUSE_RIGHT);
  }

  if (!switchJoyState) {
    Mouse.press(MOUSE_LEFT);
  } else if (switchJoyState && !switchJoyPrevState) {
    Mouse.release(MOUSE_LEFT);
  }

}

//***CHECK SETUP MODE FUNCTION**//
// Function   : checkSetupMode
//
// Description: This function sets the operating mode
//
// Parameters :  Void
//
// Return     : void
//*********************************//

void checkSetupMode() {
  nunchuck.readData();

  switchAState = nunchuck.getButtonC();
  switchBState = nunchuck.getButtonZ();

  int mode = 0;

  if (switchAState == 1 && switchBState == 1) {
    Serial.println("Mode selection: Press Button C to Switch Mode, Button Z to Confirm selection");
    pixels.setPixelColor(0, pixels.Color(0, 255, 0)); //green
    pixels.show();
    delay(3000);
    pixels.clear();
    pixels.show(); //turn off LEDs

         switch (mode) {
          case MODE_MOUSE:
            pixels.setPixelColor(0, pixels.Color(255, 255, 0)); //yellow
            pixels.show();
            break;
          case MODE_GAMEPAD:
            pixels.setPixelColor(0, pixels.Color(0, 0, 255)); //blue
            pixels.show();
            break;
          default:
            break;
        }
  
    boolean continueLoop = true;

    while (continueLoop) {
      nunchuck.readData();
      switchAState = nunchuck.getButtonC();
      switchBState = nunchuck.getButtonZ();

      if (switchAState == 1) { // If button 1 pushed, change mode
        mode += 1;
        if (mode > 1) {
          mode = 0;
        }
        switch (mode) { //Update color
          case MODE_MOUSE:
            Serial.println("Mouse mode selected.");
            pixels.setPixelColor(0, pixels.Color(255, 255, 0)); //yellow
            pixels.show();
            break;
          case MODE_GAMEPAD:
            Serial.println("Gamepad mode selected.");
            pixels.setPixelColor(0, pixels.Color(0, 0, 255)); //blue
            pixels.show();
            break;
          default:
            break;
        }
      }

      if (switchBState == 1) { // If button 2 pushed, exit loop
        continueLoop = false;
      }

      delay(100);
    } // end while loop

// 
    if (mode != operatingMode) { // If operating mode has changed
      operatingMode = mode;
      operatingModeFlash.write(operatingMode); //write update mode to memory
      delay(5);
    }
    
    if (USB_DEBUG){
      Serial.println("Release all buttons and reset joystick...");
    }
    
    while (1) { //Blink operating mode color until reset
      switch(mode) { 
        case MODE_MOUSE:
          pixels.setPixelColor(0, pixels.Color(255, 255, 0)); // Turn LED yellow
          pixels.show();
          break;
        case MODE_GAMEPAD: 
          pixels.setPixelColor(0, pixels.Color(0, 0, 255)); // Turn LED blue
          pixels.show();
          break;
      }
      delay(1000);
      pixels.clear();
      pixels.show();
      delay(1000);
    }
    //resetJoystick();  //call reset regardless, as too much time has elapsed to enable USB
  } // end if both switches detected

}



//***UPDATE JOYSTICK DEADZONE FUNCTION***//
// Function   : updateDeadzone
//
// Description: This function updates deadzone value based on deadzone level.
//
// Parameters :  inputDeadzoneLevel : int : The input deadzone level
//
// Return     : void
//****************************************//

void updateDeadzone(int inputDeadzoneLevel) {
  if ((inputDeadzoneLevel >= JOYSTICK_MIN_DEADZONE_LEVEL) && (inputDeadzoneLevel <= JOYSTICK_MAX_DEADZONE_LEVEL)) {
    currentDeadzoneValue = int ((inputDeadzoneLevel * JOYSTICK_MAX_DEADZONE_VALUE) / JOYSTICK_MAX_DEADZONE_LEVEL);
    currentDeadzoneValue = constrain(currentDeadzoneValue, 0, JOYSTICK_MAX_DEADZONE_VALUE);
  }
  updateResponse(currentDeadzoneValue);     //Update joystick response based on deadzone

  if (USB_DEBUG) {
    Serial.print("CurrentDeadzoneValue:\t"); Serial.println(currentDeadzoneValue);
  }
}


//***GAMEPAD BUTTON PRESS FUNCTION***//
// Function   : gamepadButtonPress
//
// Description: This function performs button press action.
//
// Parameters : int : buttonNumber
//
// Return     : void
//****************************************//
void gamepadButtonPress(int buttonNumber)
{
  //Serial.println("Button Press");
  if (buttonNumber > 0 && buttonNumber <= 8 )
  {
    gamepad.press(buttonNumber - 1);
    gamepad.send();
  }
}

//***GAMEPAD BUTTON CLICK FUNCTION***//
// Function   : gamepadButtonClick
//
// Description: This function performs button click action.
//
// Parameters : int : buttonNumber
//
// Return     : void
//****************************************//
void gamepadButtonClick(int buttonNumber)
{
  //Serial.println("Button click");
  if (buttonNumber > 0 && buttonNumber <= 8 )
  {
    gamepad.press(buttonNumber - 1);
    gamepad.send();
    delay(SWITCH_REACTION_TIME);
    gamepadButtonRelease(buttonNumber);
  }

}

//***GAMEPAD BUTTON RELEASE FUNCTION***//
// Function   : gamepadButtonRelease
//
// Description: This function performs button release action.
//
// Parameters : int* : args
//
// Return     : void
//****************************************//
void gamepadButtonRelease(int buttonNumber)
{
  //Serial.println("Button Release");
  if (buttonNumber > 0 && buttonNumber <= 8) {
    gamepad.release(buttonNumber - 1);
    gamepad.send();
  }

}

//***GAMEPAD BUTTON RELEASE FUNCTION***//
// Function   : gamepadButtonReleaseAll
//
// Description: This function performs button release action.
//
// Parameters : void
//
// Return     : void
//****************************************//
void gamepadButtonReleaseAll()
{
  //Serial.println("Button Release");
  gamepad.releaseAll();
  gamepad.send();
}


//***PERFORM JOYSTICK MOVE FUNCTION***//
// Function   : gamepadJoystickMove
//
// Description: This function performs joystick move
//
// Parameters : int : x and y : The output gamepad x and y
//
// Return     : void
//****************************************//
void gamepadJoystickMove(int x, int y)
{
  gamepad.move(x, -y);
  gamepad.send();
}


//***SERIAL SETTINGS FUNCTION TO CHANGE SPEED AND COMMUNICATION MODE USING SOFTWARE***//
// Function   : serialSettings
//
// Description: This function confirms if serial settings should be enabled.
//              It returns true if it's in the settings mode and is waiting for a command.
//              It returns false if it's not in the settings mode or it needs to exit the settings mode.
//
// Parameters :  enabled : bool : The input flag
//
// Return     : bool
//*************************************************************************************//
bool serialSettings(bool enabled) {

  String commandString = "";
  bool settingsFlag = enabled;

  //Set the input parameter to the flag returned. This will help to detect that the settings actions should be performed.
  if (Serial.available() > 0)
  {
    //Check if serial has received or read input string and word "SETTINGS" is in input string.
    commandString = Serial.readString();
    if (settingsFlag == false && commandString == "SETTINGS") {
      //SETTING received
      //Set the return flag to true so settings actions can be performed in the next call to the function
      printResponseInt(true, true, true, 0, commandString, false, 0);
      settingsFlag = true;
    } else if (settingsFlag == true && commandString == "EXIT") {
      //EXIT Recieved
      //Set the return flag to false so settings actions can be exited
      printResponseInt(true, true, true, 0, commandString, false, 0);
      settingsFlag = false;
    } else if (settingsFlag == true && isValidCommandFormat(commandString)) { //Check if command's format is correct and it's in settings mode
      performCommand(commandString);                  //Sub function to process valid strings
      settingsFlag = false;
    } else {
      printResponseInt(true, true, false, 0, commandString, false, 0);
      settingsFlag = false;
    }
    Serial.flush();
  }
  return settingsFlag;
}

//***PERFORM COMMAND FUNCTION TO CHANGE SETTINGS USING SOFTWARE***//
// Function   : performCommand
//
// Description: This function takes processes an input string from the serial and calls the
//              corresponding API function, or outputs an error.
//
// Parameters :  inputString : String : The input command as a string.
//
// Return     : void
//*********************************//
void performCommand(String inputString) {
  int inputIndex = inputString.indexOf(':');

  //Extract command string from input string
  String inputCommandString = inputString.substring(0, inputIndex);

  int inputCommandIndex = inputCommandString.indexOf(',');

  String inputEndpointString = inputCommandString.substring(0, inputCommandIndex);

  String inputCodeString = inputCommandString.substring(inputCommandIndex + 1);

  //Extract parameter string from input string
  String inputParameterString = inputString.substring(inputIndex + 1);

  // Determine total number of API commands
  int apiTotalNumber = sizeof(apiFunction) / sizeof(apiFunction[0]);

  //Iterate through each API command
  for (int apiIndex = 0; apiIndex < apiTotalNumber; apiIndex++) {

    // Test if input command string matches API command and input parameter string matches API parameter string
    if (inputEndpointString == apiFunction[apiIndex].endpoint &&
        inputCodeString == apiFunction[apiIndex].code) {

      // Matching Command String found
      if (!isValidCommandParameter( inputParameterString )) {
        printResponseInt(true, true, false, 2, inputString, false, 0);
      }
      else if (inputParameterString == apiFunction[apiIndex].parameter || apiFunction[apiIndex].parameter == "") {
        apiFunction[apiIndex].function(true, true, inputParameterString);
      }
      else { // Invalid input parameter

        // Outut error message
        printResponseInt(true, true, false, 3, inputString, false, 0);
      }
      break;
    } else if (apiIndex == (apiTotalNumber - 1)) { // api doesn’t exist

      //Output error message
      printResponseInt(true, true, false, 1, inputString, false, 0);

      //delay(5);
      break;
    }
  } //end iterate through API functions
}

//***VALIDATE INPUT COMMAND FORMAT FUNCTION***//
// Function   : isValidCommandFormat
//
// Description: This function confirms command string has correct format.
//              It returns true if the string has a correct format.
//              It returns false if the string doesn't have a correct format.
//
// Parameters :  inputCommandString : String : The input string
//
// Return     : boolean
//***********************************************//
bool isValidCommandFormat(String inputCommandString) {
  bool isValidFormat = false;
  if ((inputCommandString.length() == (6) || //XX,d:d
       inputCommandString.length() == (7)) //XX,d:dd
      && inputCommandString.charAt(2) == ',' && inputCommandString.charAt(4) == ':') {
    isValidFormat = true;
  }
  return isValidFormat;
}

//***VALIDATE INPUT COMMAND PARAMETER FUNCTION***//
// Function   : isValidCommandParameter
//
// Description: This function checks if the input string is a valid command parameters.
//              It returns true if the string includes valid parameters.
//              It returns false if the string includes invalid parameters.
//
// Parameters :  inputParamterString : String : The input string
//
// Return     : boolean
//*************************************************//
bool isValidCommandParameter(String inputParamterString) {
  if (isStrNumber(inputParamterString)) {
    return true;
  }
  return false;
}

//***CHECK IF STRING IS A NUMBER FUNCTION***//
// Function   : isStrNumber
//
// Description: This function checks if the input string is a number.
//              It returns true if the string includes all numeric characters.
//              It returns false if the string includes a non numeric character.
//
// Parameters :  str : String : The input string
//
// Return     : boolean
//******************************************//
boolean isStrNumber(String str) {
  boolean isNumber = false;
  for (byte i = 0; i < str.length(); i++)
  {
    isNumber = isDigit(str.charAt(i)) || str.charAt(i) == '+' || str.charAt(i) == '.' || str.charAt(i) == '-';
    if (!isNumber) return false;
  }
  return true;
}

//***SERIAL PRINT OUT COMMAND RESPONSE WITH STRING PARAMETER FUNCTION***//
// Function   : printResponseString
//
// Description: Serial Print output of the responses from APIs with string parameter as the output
//
// Parameters :  responseEnabled : bool : Print the response if it's set to true, and skip the response if it's set to false.
//               apiEnabled : bool : Print the response and indicate if the the function was called via the API if it's set to true.
//                                   Print Manual response if the function wasn't called via API.
//               responseStatus : bool : The response status (SUCCESS,FAIL)
//               responseNumber : int : 0,1,2 (Different meanings depending on the responseStatus)
//               responseCommand : String : The End-Point command which is returned as output.
//               responseParameterEnabled : bool : Print the parameter if it's set to true, and skip the parameter if it's set to false.
//               responseParameter : String : The response parameters printed as output.
//
// Return     : void
//***********************************************************************//
void printResponseString(bool responseEnabled, bool apiEnabled, bool responseStatus, int responseNumber, String responseCommand, bool responseParameterEnabled, String responseParameter) {
  if (responseEnabled) {

    if (responseStatus) {
      (apiEnabled) ? Serial.print("SUCCESS") : Serial.print("MANUAL");
    } else {
      Serial.print("FAIL");
    }
    Serial.print(",");
    Serial.print(responseNumber);
    Serial.print(":");
    Serial.print(responseCommand);
    if (responseParameterEnabled) {
      Serial.print(":");
      Serial.println(responseParameter);
    } else {
      Serial.println("");
    }
  }
}

//***SERIAL PRINT OUT COMMAND RESPONSE WITH INT PARAMETER FUNCTION***//
// Function   : printResponseInt
//
// Description: Serial Print output of the responses from APIs with int parameter as the output
//
// Parameters :  responseEnabled : bool : Print the response if it's set to true, and skip the response if it's set to false.
//               apiEnabled : bool : Print the response and indicate if the the function was called via the API if it's set to true.
//                                   Print Manual response if the function wasn't called via API.
//               responseStatus : bool : The response status (SUCCESS,FAIL)
//               responseNumber : int : 0,1,2 (Different meanings depending on the responseStatus)
//               responseCommand : String : The End-Point command which is returned as output.
//               responseParameterEnabled : bool : Print the parameter if it's set to true, and skip the parameter if it's set to false.
//               responseParameter : int : The response parameter printed as output.
//
// Return     : void
//***********************************************************************//
void printResponseInt(bool responseEnabled, bool apiEnabled, bool responseStatus, int responseNumber, String responseCommand, bool responseParameterEnabled, int responseParameter) {
  printResponseString(responseEnabled, apiEnabled, responseStatus, responseNumber, responseCommand, responseParameterEnabled, String(responseParameter));
}

//***SERIAL PRINT OUT COMMAND RESPONSE WITH FLOAT PARAMETER FUNCTION***//
// Function   : printResponseFloat
//
// Description: Serial Print output of the responses from APIs with float parameter as the output
//
// Parameters :  responseEnabled : bool : Print the response if it's set to true, and skip the response if it's set to false.
//               apiEnabled : bool : Print the response and indicate if the the function was called via the API if it's set to true.
//                                   Print Manual response if the function wasn't called via API.
//               responseStatus : bool : The response status (SUCCESS,FAIL)
//               responseNumber : int : 0,1,2 (Different meanings depending on the responseStatus)
//               responseCommand : String : The End-Point command which is returned as output.
//               responseParameterEnabled : bool : Print the parameter if it's set to true, and skip the parameter if it's set to false.
//               responseParameter : float : The response parameter printed as output.
//
// Return     : void
//***********************************************************************//
void printResponseFloat(bool responseEnabled, bool apiEnabled, bool responseStatus, int responseNumber, String responseCommand, bool responseParameterEnabled, float responseParameter) {
  printResponseString(responseEnabled, apiEnabled, responseStatus, responseNumber, responseCommand, responseParameterEnabled, String(responseParameter));

}


//***GET MODEL NUMBER FUNCTION***//
// Function   : getModelNumber
//
// Description: This function retrieves the current LipSync firmware model number.
//
// Parameters :  responseEnabled : bool : The response for serial printing is enabled if it's set to true.
//                                        The serial printing is ignored if it's set to false.
//               apiEnabled : bool : The api response is sent if it's set to true.
//                                   Manual response is sent if it's set to false.
//
// Return     : void
//*********************************//
void getModelNumber(bool responseEnabled, bool apiEnabled) {
  int tempModelNumber = JOYSTICK_MODEL;
  printResponseInt(responseEnabled, apiEnabled, true, 0, "MN,0", true, tempModelNumber);

}
//***GET MODEL NUMBER API FUNCTION***//
// Function   : getModelNumber
//
// Description: This function is redefinition of main getModelNumber function to match the types of API function arguments.
//
// Parameters :  responseEnabled : bool : The response for serial printing is enabled if it's set to true.
//                                        The serial printing is ignored if it's set to false.
//               apiEnabled : bool : The api response is sent if it's set to true.
//                                   Manual response is sent if it's set to false.
//               optionalParameter : String : The input parameter string should contain one element with value of zero.
//
// Return     : void
void getModelNumber(bool responseEnabled, bool apiEnabled, String optionalParameter) {
  if (optionalParameter.length() == 1 && optionalParameter.toInt() == 0) {
    getModelNumber(responseEnabled, apiEnabled);
  }
}

//***GET VERSION FUNCTION***//
// Function   : getVersionNumber
//
// Description: This function retrieves the current LipSync firmware version number.
//
// Parameters :  responseEnabled : bool : The response for serial printing is enabled if it's set to true.
//                                        The serial printing is ignored if it's set to false.
//               apiEnabled : bool : The api response is sent if it's set to true.
//                                   Manual response is sent if it's set to false.
//
// Return     : void
//*********************************//
void getVersionNumber(bool responseEnabled, bool apiEnabled) {
  float tempVersionNumber = JOYSTICK_VERSION;
  printResponseFloat(responseEnabled, apiEnabled, true, 0, "VN,0", true, tempVersionNumber);
}

//***GET VERSION API FUNCTION***//
// Function   : getVersionNumber
//
// Description: This function is redefinition of main getVersionNumber function to match the types of API function arguments.
//
// Parameters :  responseEnabled : bool : The response for serial printing is enabled if it's set to true.
//                                        The serial printing is ignored if it's set to false.
//               apiEnabled : bool : The api response is sent if it's set to true.
//                                   Manual response is sent if it's set to false.
//               optionalParameter : String : The input parameter string should contain one element with value of zero.
//
// Return     : void
void getVersionNumber(bool responseEnabled, bool apiEnabled, String optionalParameter) {
  if (optionalParameter.length() == 1 && optionalParameter.toInt() == 0) {
    getVersionNumber(responseEnabled, apiEnabled);
  }
}

//*** GET JOYSTICK DEADZONE FUNCTION***//
/// Function   : getJoystickDeadZone
//
// Description: This function retrieves the joystick deadzone.
//
// Parameters :  responseEnabled : bool : The response for serial printing is enabled if it's set to true.
//                                        The serial printing is ignored if it's set to false.
//               apiEnabled : bool : The api response is sent if it's set to true.
//                                   Manual response is sent if it's set to false.
//
// Return     : void
//*********************************//
int getJoystickDeadZone(bool responseEnabled, bool apiEnabled) {
  String deadZoneCommand = "DZ";
  int tempDeadzoneLevel;
  tempDeadzoneLevel = deadzoneLevelFlash.read();

  if ((tempDeadzoneLevel <= JOYSTICK_MIN_DEADZONE_LEVEL) || (tempDeadzoneLevel >= JOYSTICK_MAX_DEADZONE_LEVEL)) {
    tempDeadzoneLevel = JOYSTICK_DEFAULT_DEADZONE_LEVEL;
    deadzoneLevelFlash.write(tempDeadzoneLevel);
  }
  updateDeadzone(tempDeadzoneLevel);
  printResponseInt(responseEnabled, apiEnabled, true, 0, "DZ,0", true, tempDeadzoneLevel);
  return tempDeadzoneLevel;
}
//***GET JOYSTICK DEADZONE API FUNCTION***//
// Function   : getJoystickDeadZone
//
// Description: This function is redefinition of main getJoystickDeadZone function to match the types of API function arguments.
//
// Parameters :  responseEnabled : bool : The response for serial printing is enabled if it's set to true.
//                                        The serial printing is ignored if it's set to false.
//               apiEnabled : bool : The api response is sent if it's set to true.
//                                   Manual response is sent if it's set to false.
//               optionalParameter : String : The input parameter string should contain one element with value of zero.
//
// Return     : void
void getJoystickDeadZone(bool responseEnabled, bool apiEnabled, String optionalParameter) {
  if (optionalParameter.length() == 1 && optionalParameter.toInt() == 0) {
    getJoystickDeadZone(responseEnabled, apiEnabled);
  }
}

//*** SET JOYSTICK DEADZONE FUNCTION***//
/// Function   : setJoystickDeadZone
//
// Description: This function starts the joystick deadzone.
//
// Parameters :  responseEnabled : bool : The response for serial printing is enabled if it's set to true.
//                                        The serial printing is ignored if it's set to false.
//               apiEnabled : bool : The api response is sent if it's set to true.
//                                   Manual response is sent if it's set to false.
//               inputDeadzoneLevel : int : The input deadzone level
//
// Return     : void
//*********************************//
void setJoystickDeadZone(bool responseEnabled, bool apiEnabled, int inputDeadzoneLevel) {
  String deadZoneCommand = "DZ";
  if ((inputDeadzoneLevel >= JOYSTICK_MIN_DEADZONE_LEVEL) && (inputDeadzoneLevel <= JOYSTICK_MAX_DEADZONE_LEVEL)) {
    deadzoneLevelFlash.write(inputDeadzoneLevel);
    updateDeadzone(inputDeadzoneLevel);
    printResponseInt(responseEnabled, apiEnabled, true, 0, "DZ,1", true, inputDeadzoneLevel);
  }
  else {
    printResponseInt(responseEnabled, apiEnabled, false, 3, "DZ,1", true, inputDeadzoneLevel);
  }
}
//***SET JOYSTICK DEADZONE API FUNCTION***//
// Function   : setJoystickDeadZone
//
// Description: This function is redefinition of main setJoystickDeadZone function to match the types of API function arguments.
//
// Parameters :  responseEnabled : bool : The response for serial printing is enabled if it's set to true.
//                                        The serial printing is ignored if it's set to false.
//               apiEnabled : bool : The api response is sent if it's set to true.
//                                   Manual response is sent if it's set to false.
//               optionalParameter : String : The input parameter string should contain one element with value of zero.
//
// Return     : void
void setJoystickDeadZone(bool responseEnabled, bool apiEnabled, String optionalParameter) {
  setJoystickDeadZone(responseEnabled, apiEnabled, optionalParameter.toInt());
}


//*** RESET FUNCTION***//
/// Function   : restFunc
//
// Description: This function resets the joystick.
//
// Parameters :
//
// Return     : void
//*********************************//

void resetJoystick(void) {

}
