/*
  File: Ivy_Nunchuck_Joystick_Adapter_Software.ino
  Developed by: MakersMakingChange
  Version: (22 June 2023)
  Copyright Neil Squire Society 2023.
  License:
*/
#include "XACGamepad.h"
#include "JoystickDefinition.h"
#include <TinyUSB_Mouse_and_Keyboard.h> // For convenient use of Mouse code
#include <FlashStorage.h> // For non-volatile storage of settings
#include <Adafruit_NeoPixel.h> // To control light on Adafruit QTPY


#define DEBUG_MODE false

#define JOYSTICK_REACTION_TIME               50             //Minimum time between each action in ms
#define SWITCH_REACTION_TIME                 100            //Minimum time between each switch action in ms

//Define model number and version number
#define JOYSTICK_MODEL                        1
#define JOYSTICK_VERSION                      2

XACGamepad gamepad;                                    //Starts an instance of the USB gamepad object
NunchuckInput joystickInput;

//Declare variables for settings
int isConfigured; // 
int modelNumber;
int versionNumber;
int deadzoneLevel;
int operatingMode = 0; // 0 - Mouse  ; 1 - Joystick

FlashStorage(isConfiguredFlash, int);
FlashStorage(modelNumberFlash, int);
FlashStorage(versionNumberFlash, int);
FlashStorage(deadzoneLevelFlash, int);
FlashStorage(operatingModeFlash, int);

//Declare joystick input variables
int inputX;
int inputY;

//Declare switch state variables for each switch
int switchAState;
int switchBState;
int switchCState;
int switchDState;

//Previous status of switches
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
  {4, "D", 4}

};

// Create a pixel strand with 1 pixel on
Adafruit_NeoPixel pixels(1, PIN_NEOPIXEL);


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
  delay(1000);


  // Begin serial
  Serial.begin(115200);

  if (DEBUG_MODE) {
    while (!Serial) {
      delay(100);
    }
  }

  initMemory();   // Initialize Memory
  delay(5);

  // Initialize Joystick
  initJoystick();

  checkSetupMode();

  // Begin HID gamepad or mouse, depending on mode selection
  switch (operatingMode) {
    case 0:
      Serial.println("USB HID Mouse activataed");
      Mouse.begin();
      pixels.setPixelColor(0, pixels.Color(255, 255, 0)); // Turn LED yellow
      pixels.show();
      break;

    case 1:
      Serial.println("USB HID Gamepad activated");
      gamepad.begin();
      pixels.setPixelColor(0, pixels.Color(0, 0, 255)); // Turn LED blue
      pixels.show();
      break;
  }



}

void loop() {

  settingsEnabled = serialSettings(settingsEnabled); //Check to see if setting option is enabled
  readJoystick();
  readSwitch();
  delay(JOYSTICK_REACTION_TIME);
}

//*********************************//
// Joystick Functions
//*********************************//

//***INITIALIZE Memory FUNCTION***//
// Function   : initMemory
//
// Description: This function initializes Memory
//
// Parameters : void
//
// Return     : void
//****************************************//
void initMemory() {
  //Check if it's first time running the code
  isConfigured = isConfiguredFlash.read();
  delay(5);

  if (isConfigured == 0) {
    //Define default settings if it's first time running the code
    modelNumber = 0;
    versionNumber = 2;
    deadzoneLevel = 6;
    isConfigured = 1;
    operatingMode = 0; //default to mouse mode

    //Write default settings to flash storage
    modelNumberFlash.write(modelNumber);
    versionNumberFlash.write(versionNumber);
    deadzoneLevelFlash.write(deadzoneLevel);
    isConfiguredFlash.write(isConfigured);
    operatingModeFlash.write(operatingMode);
    delay(5);

  } else {
    //Load settings from flash storage
    modelNumber = modelNumberFlash.read();
    versionNumber = versionNumberFlash.read();
    deadzoneLevel = deadzoneLevelFlash.read();
    operatingMode = operatingModeFlash.read();
    delay(5);
  }

  //Serial print settings
  Serial.print("Model Number: ");
  Serial.println(modelNumber);

  Serial.print("Version Number: ");
  Serial.println(versionNumber);

  Serial.print("Deadzone Level: ");
  Serial.println(deadzoneLevel);

  Serial.print("Operating Mode: ");
  Serial.println(operatingMode);
}



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
  getJoystickDeadZone(true, false);                                         //Get joystick deadzone stored in memory                                      //Get joystick calibration points stored in flash memory
  joystickInput.begin();                                                    //Begin joystickInput object
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

  joystickInput.readData(); // Update joystick input sensors

  inputX = joystickInput.getJoyX();
  inputY = joystickInput.getJoyY();

  int xMin = joystickInput.xMin;
  int xMax = joystickInput.xMax;
  int yMin = joystickInput.yMin;
  int yMax = joystickInput.yMax;

  //Map joystick x and y to joystick range
  int outputX = map(inputX, xMin, xMax, -127, 127);
  int outputY = map(inputY, yMin, yMax, -127, 127);

  //Apply simple deadzone
  if (outputX <= currentDeadzoneValue && outputX >= -currentDeadzoneValue)
  {
    outputX = 0;
  }

  if (outputY <= currentDeadzoneValue && outputY >= -currentDeadzoneValue) {
    outputY = 0;
  }



  //Perform USB HID action
  switch (operatingMode) {
    case 0:
      Mouse.move(MOUSE_MAX_XY * outputX / 127, -MOUSE_MAX_XY * outputY / 127, 0);
      break;
    case 1:
      gamepadJoystickMove(outputX, outputY);
      break;
  }



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

  //Update status of switch inputs
  joystickInput.readData();

  switchAState = joystickInput.button1State;
  switchBState = joystickInput.button2State;
  switchCState = joystickInput.button3State;
  switchDState = joystickInput.button4State;

  switch (operatingMode) {
    case 0:
      mouseSwitches();
      break;
    case 1:
      joystickSwitches();
      break;

  }



}

//***CHECK SETUP MODE FUNCTION**//
// Function   : checkSetupMode
//
// Description: This function returns the operating mode
//
// Parameters :  Void
//
// Return     : void
//*********************************//

void checkSetupMode() {
  joystickInput.readData();

  switchAState = joystickInput.button1State;
  switchBState = joystickInput.button2State;

  if (switchAState == 1 && switchBState == 1) {
    Serial.println("Mode selection: Press Button C to Switch Mode, Button Z to Confirm selection");
    boolean continueLoop = true;
    int mode = 0;
    while (continueLoop) {
      joystickInput.readData();
      switchAState = joystickInput.button1State;
      switchBState = joystickInput.button2State;

      if (switchAState == 1) {
        mode += 1;
        if (mode > 1) {
          mode = 0;
        }
        switch (mode) {

          case 0:
            Serial.println("Mouse mode selected.");
            pixels.setPixelColor(0, pixels.Color(255, 255, 0)); //yellow
            pixels.show();
            break;

          case 1:
            Serial.println("Gamepad mode selected.");
            pixels.setPixelColor(0, pixels.Color(0, 0, 255)); //blue
            pixels.show();
            break;

          default:
            break;
        }
      }
      if (switchBState == 1) {
        continueLoop = false;
      }


      delay(100);
    }

    if (mode != operatingMode) {
      operatingMode = mode;
      operatingModeFlash.write(operatingMode);
      delay(5);
    }

  }

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
  if (switchAState) {
    gamepadButtonPress(switchProperty[0].switchButtonNumber);
  }
  else if (!switchAState && switchAPrevState) {
    gamepadButtonRelease(switchProperty[0].switchButtonNumber);
  }

  if (switchBState) {
    gamepadButtonPress(switchProperty[1].switchButtonNumber);
  } else if (!switchBState && switchBPrevState) {
    gamepadButtonRelease(switchProperty[1].switchButtonNumber);
  }

  if (switchCState) {
    gamepadButtonPress(switchProperty[2].switchButtonNumber);
  } else if (!switchCState && switchCPrevState) {
    gamepadButtonRelease(switchProperty[2].switchButtonNumber);
  }

  if (switchDState) {
    gamepadButtonPress(switchProperty[3].switchButtonNumber);
  } else if (!switchDState && switchJoyPrevState) {
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
  if (switchAState) {
    Mouse.press(MOUSE_LEFT);
  }
  else if (!switchAState && switchAPrevState) {
    Mouse.release(MOUSE_LEFT);
  }

  if (switchBState) {
    Mouse.press(MOUSE_RIGHT);
  } else if (!switchBState && switchBPrevState) {
    Mouse.release(MOUSE_RIGHT);
  }

  if (switchCState) {
    Mouse.press(MOUSE_RIGHT);
  } else if (!switchCState && switchCPrevState) {
    Mouse.release(MOUSE_RIGHT);
  }

  if (switchDState) {
    Mouse.press(MOUSE_LEFT);
  } else if (!switchDState && switchJoyPrevState) {
    Mouse.release(MOUSE_LEFT);
  }

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