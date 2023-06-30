#include "NunchuckInput.h"

NunchuckInput::NunchuckInput() {
  type = NunchuckInputType;
}

void NunchuckInput::begin() {
  this->nunchuck_.begin();
  startMillis = millis();
}

void NunchuckInput::readData() {
  this->nunchuck_.readData();
  
  xRaw = nunchuck_.getJoyX();
  yRaw = nunchuck_.getJoyY();
  button1State = nunchuck_.getButtonC(); 
  button2State = nunchuck_.getButtonZ();
  button3State = false;
  button4State = false;
  rollAngle = nunchuck_.getRollAngle();
  pitchAngle = nunchuck_.getPitchAngle();
  accelX = nunchuck_.getAccelX();
  accelY = nunchuck_.getAccelY();
  accelZ = nunchuck_.getAccelY();
  
}

void NunchuckInput::reset() {
  //todo reset function
}

boolean NunchuckInput::isReady() {
//  if (usb_hid.ready())
//    return true;
//  return false;
return true;
}

void NunchuckInput::updateCenter() {
  readData();
  xNeutral = xRaw;
  yNeutral = yRaw;
  
}
int NunchuckInput::getJoyX() {
  return xRaw;
}

int NunchuckInput::getJoyY() {
  return yRaw;
}
