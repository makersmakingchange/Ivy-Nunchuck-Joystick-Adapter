//JOYSTICK DEFINITION

//General
//boolean isMouseMode = true;


//General JOYSTICK Parameters
#define JOYSTICK_DEFAULT_DEADZONE_LEVEL      1              //Joystick deadzone
#define JOYSTICK_MIN_DEADZONE_LEVEL          1
#define JOYSTICK_MAX_DEADZONE_LEVEL          10
#define JOYSTICK_MAX_DEADZONE_VALUE          64             //Out of 127
#define JOYSTICK_MAX_VALUE                   127 

//General Mouse Parameters
#define MOUSE_MAX_XY                       12            // Amount to move mouse at max deflection (max "speed")

//Potentiometer Type Input

//#include "PotentiometerInput.h"


//Define Joystick Analog pins
//#define JOYSTICK_X_PIN    A0
//#define JOYSTICK_Y_PIN    A1  

//#define SWITCH_A_PIN      9 
//#define SWITCH_B_PIN      8 
//#define SWITCH_C_PIN      7 
//#define SWITCH_JOY_PIN    10                           // Switch in the joystick (press down)

//#define SWITCH_MODE_PIN   6                            // Slide switch for mode



//Nunchuck Type Input
#include "NunchuckInput.h"
