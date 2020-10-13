#include <Arduino.h>

//********************************************************
// 
//********************************************************

class AD9833 {
  private:
    const uint8_t _FSYNC;

  public:
    AD9833( uint8_t fsync = 10 );
    void reset();
    void setFrequency( long frequency, uint16_t wave );
    static const uint16_t wReset     = 0b0000000100000000;
    static const uint16_t wSine      = 0b0000000000000000;
    static const uint16_t wTriangle  = 0b0000000000000010;
    static const uint16_t wRectangle = 0b0000000000101000;
};
