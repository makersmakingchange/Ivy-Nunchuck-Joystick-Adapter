#ifndef PotentiometerInput_h
#define PotentiometerInput_h

#include "JoystickInput.h"


class PotentiometerInput: public JoystickInput
{
  public:
       
    int xMin = 0;
    int xMax = 1023;
    int yMin = 0;
    int yMax = 1023;
    int xNeutral = 524;
    int yNeutral = 524;
    
    PotentiometerInput();
    void begin() override;
    void readData() override;
    void reset() override;
    boolean isReady() override;
    void updateCenter() override;
    
    int getJoyX() override; 
    int getJoyY() override;

    protected:
      int xRaw = xNeutral;
      int yRaw = yNeutral;
      uint32_t lastRead = 0;
    
    
};

#endif
