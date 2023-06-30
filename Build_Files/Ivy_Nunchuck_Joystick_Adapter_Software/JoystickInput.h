#ifndef JoystickInput_h
#define JoystickInput_h

#include "Arduino.h"

typedef enum _joystickinputtype {
  PotentiometerInputType,
  HallEffectInputType,
  NunchuckInputType,
} JoystickInputType;


class JoystickInput
{
  public:
    JoystickInputType type;
    int xMin = 0;
    int xMax = 1023;
    int yMin = 0;
    int yMax = 1023;

    int xNeutral = 524;
    int yNeutral = 524;

    int xRaw = xNeutral;
    int yRaw = yNeutral;
        
    boolean button1State = false;
    boolean button2State = false;
    boolean button3State = false;
    boolean button4State = false;
    
    float rollAngle = 0;
    float pitchAngle = 0;
    int accelX = 0;
    int accelY = 0;
    int accelZ = 0;
    
    JoystickInput();
    virtual ~JoystickInput();
    virtual void begin()=0;
    virtual void readData()=0;
    virtual void reset()=0;
    virtual boolean isReady()=0;

    virtual void updateCenter()=0; // update center position
    virtual int getJoyX()=0; 
    virtual int getJoyY()=0; 
       
    protected:
    uint32_t startMillis;  
    
    
};

#endif
