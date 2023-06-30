#ifndef NunchuckInput_h
#define NunchuckInput_h

#include "JoystickInput.h"

#include <WiiChuck.h>

class NunchuckInput: public JoystickInput
{
  public:
       
    int xMin = 0;
    int xMax = 255;
    int yMin = 0;
    int yMax = 255;
    
    NunchuckInput();
    void begin() override;
    void readData() override;
    void reset() override;
    boolean isReady() override;
    void updateCenter() override;
    
    int getJoyX() override;
    int getJoyY() override;

    protected:
      Accessory nunchuck_;
    
};

#endif
