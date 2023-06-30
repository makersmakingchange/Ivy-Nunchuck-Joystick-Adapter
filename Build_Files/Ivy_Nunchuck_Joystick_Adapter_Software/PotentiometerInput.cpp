#include "PotentiometerInput.h"

#define SWITCH_A_PIN      9 
#define SWITCH_B_PIN      8 
#define SWITCH_C_PIN      7 
#define SWITCH_JOY_PIN    10  

PotentiometerInput::PotentiometerInput(){
  type = PotentiometerInputType;
}


void PotentiometerInput::begin(){

  // Initialize analog read pins?
  
    //Initialize the switch pins as inputs
  pinMode(SWITCH_A_PIN, INPUT_PULLUP);    
  pinMode(SWITCH_B_PIN, INPUT_PULLUP);   
  pinMode(SWITCH_C_PIN, INPUT_PULLUP);   
  pinMode(SWITCH_JOY_PIN, INPUT_PULLUP);
   
  startMillis = millis();
}


void PotentiometerInput::readData() {
    
}

void PotentiometerInput::reset() {
  //todo reset function
}

boolean PotentiometerInput::isReady() {
//  if (usb_hid.ready())
//    return true;
//  return false;
  return true;
}

void PotentiometerInput::updateCenter() {
  readData();
  xNeutral = xRaw;
  yNeutral = yRaw;
  
}

int PotentiometerInput::getJoyX() {
  return xRaw;
}

int PotentiometerInput::getJoyY() {
  return yRaw;
}
