/***************************************************
  Arduino TFT graphics library for the SH1106

  typical use is:
      #include "SimpleSH1106.h"
      SimpleSH1106 SH1106;
      SH1106.init();
      SH1106.drawImage( 20, 1, imgSmiley );

***************************************************/

#ifndef SimpleSH1106_h
#define SimpleSH1106_h

#include <Wire.h>
#include <Arduino.h>

class SimpleSH1106 {
  
  public:
    void init();
    void clearScreen();
    uint8_t drawImage( uint8_t col, uint8_t row, const uint8_t *bitmap );
    uint8_t drawChar( uint8_t c, uint8_t col, uint8_t page, const uint8_t *Font );
    uint8_t drawString( const char *s, uint8_t col, uint8_t page, const uint8_t *Font );
    uint8_t drawString( const __FlashStringHelper *s, uint8_t col, uint8_t page, const uint8_t *Font );
    uint8_t drawInt( long i, uint8_t col, uint8_t page, const uint8_t *Font );
    void drawBar( uint8_t col, uint8_t page, uint8_t bar );
    void drawBox( const char* text );
    void drawBox( const __FlashStringHelper* text );
    bool bold = false;
    static const uint8_t smallFont[] PROGMEM;
    // static const uint8_t smallDigitsFont[] PROGMEM;
    static const uint8_t largeDigitsFont[] PROGMEM;
    static const uint8_t imgSmiley[] PROGMEM;

  private:
    const uint8_t addrI2C = 0x3C;
    const uint8_t PAGES = 8;
    const uint8_t COLUMNS = 128;
    const uint8_t colOffset = 0; // = 2 for 1.3" display
    void setupColPage( uint8_t col, uint8_t page );
    void setupCol( uint8_t col );
    void setupPage( uint8_t page );
    void drawBar( uint8_t bar );
    static const uint8_t imgBoxTop[] PROGMEM;
    static const uint8_t imgBoxMid[] PROGMEM;
    static const uint8_t imgBoxBot[] PROGMEM;
};


extern SimpleSH1106 SH1106;


#endif
