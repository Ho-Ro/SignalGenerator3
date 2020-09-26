//-----------------------------------------------------------------------------
// Copyright 2018 Peter Balch
// subject to the GNU General Public License
//-----------------------------------------------------------------------------

#include <Wire.h>
#include <SPI.h>
#include <limits.h>
#include "SimpleSH1106.h"
#include <math.h>

//-----------------------------------------------------------------------------
// Defines and Typedefs
//-----------------------------------------------------------------------------

// get register bit - faster: doesn't turn it into 0/1
#ifndef getBit
#define getBit(sfr, bit) (_SFR_BYTE(sfr) & _BV(bit))
#endif

enum Tmode {DC5V, AC500mV, AC100mV, AC20mV,
            mFreqAC,
            mVoltmeter,
            maxMode1
           };
const Tmode maxMode = maxMode1 - 1;

enum TmenuSel {sTime, sMode, sTrigger, sTestSig, sSigGen};

//-----------------------------------------------------------------------------
// Global Constants
//-----------------------------------------------------------------------------

const long BAUDRATE  = 115200;  // Baud rate of UART in bps

const int BtnLeft  = 8; // pushbutton
const int BtnRight = 7; // pushbutton
const int BtnUp    = 6; // pushbutton
const int BtnDown  = 5; // pushbutton

//-----------------------------------------------------------------------------
// Global Variables
//-----------------------------------------------------------------------------

// bool SendingSerial = false;

//-----------------------------------------------------------------------------
// globals used in SigGen
//-----------------------------------------------------------------------------

const byte numberOfDigits = 6; // number of digits (nOD) in the frequense
byte freqStart[numberOfDigits] = {0, 0, 1, 0, 0, 0}; // 1000Hz SelPos = 0..nOD-1
byte freqStop[numberOfDigits]  = {0, 2, 0, 0, 0, 0}; // 20kHz  SelPos = nOD..2*nOD-1
byte SelPos = 0; // MSB freqStart
const byte WvFormPos  = 2 * numberOfDigits;
const byte SweepPos   = 2 * numberOfDigits + 1;

const int wSine      = 0b0000000000000000;
const int wTriangle  = 0b0000000000000010;
const int wRectangle = 0b0000000000101000;

enum TsweepType { swOff, sw1Sec, sw3Sec, sw10Sec, sw30Sec };

int waveType = wSine;
TsweepType sweepType = swOff;

// connections to AD9833
const int SG_fsyncPin = 10;
const int SG_CLK = 13;  // SCK
const int SG_DATA = 11; // MOSI

int SG_iSweep, SG_nSweep;


//-----------------------------------------------------------------------------
// images for main menu
//-----------------------------------------------------------------------------

const byte imgMainMenuMid[] PROGMEM = {
  128, // width
  1, // pages
  1, 255, 254, 0, 1, 255
};
const byte imgMainMenuBot[] PROGMEM = {
  128, // width
  1, // pages
  1, 255, 254, 128, 1, 255
};
const byte imgBoxTop[] PROGMEM = {
  128, // width
  1, // pages
  1, 248, 254, 8, 1, 248
};

const byte imgCaret1[] PROGMEM = {
  4, // width
  1, // pages
  4, 255, 126, 60, 24
};

const byte imgCaret2[] PROGMEM = {
  7, // width
  1, // pages
  7, 32, 48, 56, 60, 56, 48, 32
};

const byte imgTrian[] PROGMEM = {
  14, // width
  2, // pages
  28, 3, 12, 48, 192, 0, 0, 0, 0, 0, 0, 192, 48, 12, 3, 128, 128, 128, 128, 131, 140, 176, 176, 140, 131, 128, 128, 128, 128
};

const byte imgSine[] PROGMEM = {
  14, // width
  2, // pages
  28, 1, 2, 28, 224, 0, 0, 0, 0, 0, 0, 224, 28, 2, 1, 128, 128, 128, 129, 142, 144, 160, 160, 144, 142, 129, 128, 128, 128
};

const byte imgRect[] PROGMEM = {
  14, // width
  2, // pages
  28, 0, 0, 0, 255, 1, 1, 1, 1, 1, 1, 255, 0, 0, 0, 160, 160, 160, 191, 128, 128, 128, 128, 128, 128, 191, 160, 160, 160
};


//-----------------------------------------------------------------------------
// drawBox
//   draws a box around the screen with "text" written at top-left
//-----------------------------------------------------------------------------
void drawBox( const char* text ) {
  //      clearSH1106();
  DrawImageSH1106( 0, 0, imgBoxTop );
  for ( int y = 1; y < 7; ++y )
    DrawImageSH1106( 0, y, imgMainMenuMid );
  DrawImageSH1106( 0, 7, imgMainMenuBot );
  DrawCharSH1106( ' ',  6, 0, SmallFont );
  DrawStringSH1106( text,  7, 0, SmallFont );
}


//-----------------------------------------------------------------------------
// StartTimer1
// TIFR1 becomes non-zero after
//    overflow*1024/16000000 sec
//-----------------------------------------------------------------------------
void StartTimer1( word overflow ) {
  TCCR1A = 0xC0; // Set OC1A on Compare Match
  TCCR1B = 0x05; // prescaler = 1024
  TCCR1C = 0x00; // no pwm output
  OCR1AH = highByte( overflow );
  OCR1AL = lowByte( overflow );
  OCR1BH = 0;
  OCR1BL = 0;
  TIMSK1 = 0x00; // no interrupts

  TCNT1H = 0; // must be written first
  TCNT1L = 0; // clear the counter
  TIFR1 = 0xFF; // clear all flags
}


//-----------------------------------------------------------------------------
// drawSigGenMenu
//-----------------------------------------------------------------------------
void drawSigGenMenu( void ) {
  byte x, y, i;
  drawBox( "Signal Generator" );

  if ( sweepType == swOff ) {
    x = 20;
    y = 3;
    for (i = 0; i < numberOfDigits; ++i) {
      if (i == SelPos)
        DrawImageSH1106(x + 2, y + 2, imgCaret2);
      x += DrawIntSH1106(freqStart[i],  x, y, LargeDigitsFont);
    }
  } else {
    x = 60;
    y = 2;
    DrawStringSH1106("Start Freq:",     12, y, SmallFont);
    for (i = 0; i < numberOfDigits; ++i) {
      if (i == SelPos)
        DrawImageSH1106(x - 2, y + 1, imgCaret2);
      x += DrawIntSH1106(freqStart[i],  x, y, SmallFont);
    }
    DrawStringSH1106(" Hz",  x, y, SmallFont);

    x = 60;
    y = 4;
    DrawStringSH1106("Stop Freq:",     12, y, SmallFont);
    for (i = 0; i < numberOfDigits; ++i) {
      if (i == SelPos - numberOfDigits)
        DrawImageSH1106(x - 2, y + 1, imgCaret2);
      x += DrawIntSH1106(freqStop[i],  x, y, SmallFont);
    }
    DrawStringSH1106(" Hz",  x, y, SmallFont);
  }

  x = 12;
  y = 6;
  if (SelPos == WvFormPos)
    DrawImageSH1106(x - 6, y, imgCaret1);
  //  switch (waveType) {
  //    case wSine:      DrawStringSH1106("Sine",     x, y, SmallFont); break;
  //    case wTriangle:  DrawStringSH1106("Triangle", x, y, SmallFont); break;
  //    case wRectangle:    DrawStringSH1106("Rectangle",   x, y, SmallFont); break;
  //  }
  for (x = 12; x < 40; x += 14)
    switch (waveType) {
      case wSine:      DrawImageSH1106(x, y, imgSine); break;
      case wTriangle:  DrawImageSH1106(x, y, imgTrian); break;
      case wRectangle: DrawImageSH1106(x, y, imgRect); break;
    }

  x = 54;
  y = 6;
  switch ( sweepType ) {
    case swOff:       DrawStringSH1106("Constant",       x, y, SmallFont); break;
    case sw1Sec:      DrawStringSH1106("Sweep 1 Sec",    x, y, SmallFont); break;
    case sw3Sec:      DrawStringSH1106("Sweep 3 Sec",    x, y, SmallFont); break;
    case sw10Sec:     DrawStringSH1106("Sweep 10 Sec",   x, y, SmallFont); break;
    case sw30Sec:     DrawStringSH1106("Sweep 30 Sec",   x, y, SmallFont); break;
  }
  if ( SelPos == SweepPos )
    DrawImageSH1106(x - 6, y, imgCaret1);
}

/*
  //-----------------------------------------------------------------------------
  //returns 10^y
  //-----------------------------------------------------------------------------
  unsigned long Power(int y) {
  unsigned long t = 1;
  for (byte i = 0; i < y; i++)
    t = t * 10;
  return t;
  }
*/

//-----------------------------------------------------------------------------
//calculate the frequency from the array.
//-----------------------------------------------------------------------------
unsigned long calcFreq(byte* freqSG) {
  unsigned long f = 0;
  unsigned long p = 1;
  for (byte x = numberOfDigits - 1; x < numberOfDigits; --x) {
    f = f + freqSG[x] * p; //* Power(x);
    p *= 10;
  }
  return f;
}


//-----------------------------------------------------------------------------
// SG_WriteRegister
//-----------------------------------------------------------------------------
void SG_WriteRegister( word dat ) {
  digitalWrite( SG_fsyncPin, LOW );
  SPI.transfer( dat >> 8 );
  SPI.transfer( dat & 0xFF );
  digitalWrite( SG_fsyncPin, HIGH );
}


//-----------------------------------------------------------------------------
// SG_Reset
//-----------------------------------------------------------------------------
void SG_Reset() {
  delay(100);
  SG_WriteRegister(0x100);
  sweepType = swOff;
  delay(100);
}


//-----------------------------------------------------------------------------
// SG_freqReset
//    reset the SG regs then set the frequency and wave type
//-----------------------------------------------------------------------------
void SG_freqReset( long frequency, int wave ) {
  long fl = frequency * (0x10000000 / 25000000.0);
  SG_WriteRegister(0x2100);
  SG_WriteRegister((int)(fl & 0x3FFF) | 0x4000);
  SG_WriteRegister((int)((fl & 0xFFFC000) >> 14) | 0x4000);
  SG_WriteRegister(0xC000);
  SG_WriteRegister(wave);
  waveType = wave;
}


//-----------------------------------------------------------------------------
// SG_freqSet
//    set the SG frequency regs
//-----------------------------------------------------------------------------
void SG_freqSet( long frequency, int wave ) {
  long fl = frequency * (0x10000000 / 25000000.0);
  SG_WriteRegister(0x2000 | wave);
  SG_WriteRegister((int)(fl & 0x3FFF) | 0x4000);
  SG_WriteRegister((int)((fl & 0xFFFC000) >> 14) | 0x4000);
}


//-----------------------------------------------------------------------------
// SG_StepSweep
//    increment the FG frequency
//-----------------------------------------------------------------------------
void SG_StepSweep( void ) {
  if ( SG_iSweep > SG_nSweep ) SG_iSweep = 0;
  long f = exp((log(calcFreq(freqStop)) - log(calcFreq(freqStart))) * SG_iSweep / SG_nSweep + log(calcFreq(freqStart))) + 0.5;
  SG_freqSet(f, waveType);
  SG_iSweep++;
}


//-----------------------------------------------------------------------------
// Sweep
//   sweeps siggen freq continuously
//   takes n mS for whole sweep
//   SDC regs are saved and restored
//   stops when receives a serial char
//-----------------------------------------------------------------------------
void Sweep(int n) {
  long fstart = calcFreq( freqStart );
  long fstop  = calcFreq( freqStop );
  int i = 0;

  do {
    long f = exp( ( log( fstop ) - log( fstart ) ) * i / ( n - 1 ) + log( fstart ) ) + 0.5;
    SG_freqSet( f, waveType );
    delay(1);
    if ( ++i >= n )
      i = 0;
  } while (!Serial.available());

  SG_freqSet( calcFreq( freqStart ), waveType );
}


//-----------------------------------------------------------------------------
// incItem
//   increment digit for SigGen Menu
//-----------------------------------------------------------------------------
void incItem( void ) {
  if (SelPos == WvFormPos) {
    switch (waveType) {
      case wSine:      waveType = wTriangle; break;
      case wTriangle:  waveType = wRectangle; break;
      case wRectangle: waveType = wSine; break;
    }
  } else if (SelPos == SweepPos) {
    if (sweepType == sw30Sec)
      sweepType = swOff;
    else
      sweepType = sweepType + 1;
  } else if ( SelPos < numberOfDigits ) {
    if ( freqStart[ SelPos ] >= 9 )
      freqStart[ SelPos ] = 0;
    else
      freqStart[SelPos]++;
  } else if (sweepType != swOff) {
    if (freqStop[ SelPos - numberOfDigits ] >= 9)
      freqStop[ SelPos - numberOfDigits ] = 0;
    else
      freqStop[ SelPos - numberOfDigits ]++;
  }
  drawSigGenMenu();
}


//-----------------------------------------------------------------------------
// decItem
//   decrement digit for SigGen Menu
//-----------------------------------------------------------------------------
void decItem( void ) {
  if ( SelPos == WvFormPos ) {
    switch (waveType) {
      case wRectangle: waveType = wTriangle; break;
      case wTriangle:  waveType = wSine; break;
      case wSine:      waveType = wRectangle; break;
    }
  } else if (SelPos == SweepPos) { // 1, 3, 10, 30
    if (sweepType == swOff)
      sweepType = sw30Sec;
    else
      sweepType = sweepType - 1;
  } else if (SelPos < numberOfDigits) {
    if (freqStart[SelPos] <= 0)
      freqStart[SelPos] = 9;
    else
      freqStart[SelPos]--;
  } else if (sweepType != swOff) {
    if ( freqStop[SelPos - numberOfDigits ] <= 0)
      freqStop[ SelPos - numberOfDigits ] = 9;
    else
      freqStop[ SelPos - numberOfDigits ]--;
  }
  drawSigGenMenu();
}

//-----------------------------------------------------------------------------
// caretRight
//   increment caret position for SigGen Menu
//-----------------------------------------------------------------------------
void caretRight( void ) {
  if ( SelPos == SweepPos )
    SelPos = 0;
  else
    ++SelPos;
  // skip over the stop frequency display if no sweep
  if ( ( SelPos >= numberOfDigits ) && ( SelPos < 2 * numberOfDigits ) && ( sweepType == swOff ) )
    SelPos = WvFormPos;
  drawSigGenMenu();
}


//-----------------------------------------------------------------------------
// caretLeft
//   decrement caret position for SigGen Menu
//-----------------------------------------------------------------------------
void caretLeft( void ) {
  if ( SelPos == 0 )
    SelPos = SweepPos;
  else
    --SelPos;
  // skip over the stop frequency display if no sweep
  if ( ( SelPos >= numberOfDigits ) && ( SelPos < 2 * numberOfDigits ) && ( sweepType == swOff ) )
    SelPos = numberOfDigits - 1;
  drawSigGenMenu();
}


//-----------------------------------------------------------------------------
// ExecSigGenMenu
//   SigGen menu
//   user presses sel or Adj buttons
//   return if no button for 2 sec
//-----------------------------------------------------------------------------
void ExecSigGenMenu(void) {
  static int prevLeft  = 0;
  static int prevRight = 0;
  static int prevUp    = 0;
  static int prevDown  = 0;
  int i;
  const byte timeout = 0xC0; // 3 sec to exit

  drawSigGenMenu();

  StartTimer1(0);
  SG_iSweep = 0;

  do {
    SerialCommand();

    i = digitalRead( BtnUp );
    if ( i != prevUp ) {
      //Serial.println(F("^"));
      prevUp = i;
      if ( i == LOW ) {
        incItem();
        drawSigGenMenu();
        SG_freqReset( calcFreq( freqStart ), waveType );
      }
      myDelay( 100 );
      StartTimer1( 0 );
    }

    i = digitalRead( BtnDown );
    if ( i != prevDown ) {
      //Serial.println(F("v"));
      prevDown = i;
      if ( i == LOW ) {
        decItem();
        drawSigGenMenu();
        SG_freqReset( calcFreq( freqStart ), waveType );
      }
      myDelay( 100 );
      StartTimer1( 0 );
    }

    i = digitalRead( BtnLeft );
    if ( i != prevLeft ) {
      //Serial.println(F("<"));
      prevLeft = i;
      if ( i == LOW ) {
        caretLeft();
        drawSigGenMenu();
      }
      myDelay( 100 );
      StartTimer1( 0 );
    }

    i = digitalRead( BtnRight );
    if ( i != prevRight ) {
      //Serial.println(F(">"));
      prevRight = i;
      if ( i == LOW ) {
        caretRight();
        drawSigGenMenu();
      }
      myDelay( 100 );
      StartTimer1( 0 );
    }

    if ( sweepType != swOff ) {
      switch ( sweepType ) {
        case sw1Sec:  SG_nSweep =  1000L * 65 / 100; break;
        case sw3Sec:  SG_nSweep =  3000L * 65 / 100; break;
        case sw10Sec: SG_nSweep = 10000L * 65 / 100; break;
        case sw30Sec: SG_nSweep = 30000L * 65 / 100; break;
      }
      SG_StepSweep();
    }

    i = TCNT1L; // to force read of TCNT1H
  } while ( true );
}


//-----------------------------------------------------------------------------
// myDelay
//   delays for approx mS milliSeconds
//   doesn't use any timers
//   doesn't affect interrupts
//-----------------------------------------------------------------------------

void myDelay( int mS ) {
  for ( int j = 0; j < mS; j++ )
    delayMicroseconds( 1000 );
}

//-----------------------------------------------------------------------------
// SerialCommand
//   if a byte is available in teh seril input buffer
//   execute it as a command
//-----------------------------------------------------------------------------
void SerialCommand( void ) {
  static bool newNumber = true;
  if ( Serial.available() > 0 ) {
    char c = Serial.read();

    if ( ( c >= '0' ) && ( c <= '9' ) ) {
      for ( int i = 0; i < numberOfDigits - 1 ; ++i )
        freqStart[ i ] = newNumber ? 0 : freqStart[ i + 1 ];
      freqStart[ numberOfDigits - 1 ] = c - '0';
      newNumber = false;
    } else {
      newNumber = true;
      switch (c) {
        case 'S': waveType = wSine; SG_freqReset(calcFreq(freqStart), waveType); break;   // SigGen wave is sine
        case 'T': waveType = wTriangle; SG_freqReset(calcFreq(freqStart), waveType); break;   // SigGen wave is triangle
        case 'R': waveType = wRectangle; SG_freqReset(calcFreq(freqStart), waveType); break;   // SigGen wave is rectangle
        case 'O': SG_Reset(); break;   // SigGen off
        case 'M': for ( int i = 0; i < numberOfDigits ; i++ ) freqStop[i] = freqStart[i]; break; // move freq to high array
        case 'G': sweepType = sw1Sec; Sweep(1000);  break; // sweep SigGen
        case 'H': sweepType = sw3Sec; Sweep(3000);  break; // sweep SigGen
        case 'I': sweepType = sw10Sec; Sweep(10000);  break; // sweep SigGen
        case 'J': sweepType = sw30Sec; Sweep(30000);  break; // sweep SigGen
        case '\n': break;
        default: return;
      }
      drawSigGenMenu();
    }
  }
}


//-----------------------------------------------------------------------------
// InitSigGen
//-----------------------------------------------------------------------------
void InitSigGen(void) {
  pinMode(SG_DATA, OUTPUT);
  pinMode(SG_CLK, OUTPUT);
  pinMode(SG_fsyncPin, OUTPUT);
  digitalWrite(SG_fsyncPin, HIGH);
  digitalWrite(SG_CLK, HIGH);
  SG_Reset();
  SG_freqReset( calcFreq( freqStart ), waveType);
}


//-----------------------------------------------------------------------------
// Main routines
// The setup function
//-----------------------------------------------------------------------------
void setup (void) {
  // Open serial port with a baud rate of BAUDRATE b/s
  Serial.begin(BAUDRATE);

  // Activate interrupts
  sei();

  Serial.println("SigGen2 " __DATE__); // compilation date
  Serial.println("OK");

  pinMode(BtnLeft,  INPUT_PULLUP);
  pinMode(BtnRight, INPUT_PULLUP);
  pinMode(BtnUp,    INPUT_PULLUP);
  pinMode(BtnDown,  INPUT_PULLUP);

  SPI.begin();

  Wire.begin(); // join i2c bus as master
  //  TWBR = 1; // freq=888kHz period=1.125uS
  //  TWBR = 2; // freq=800kHz period=1.250uS
  //  TWBR = 3; // freq=727kHz period=1.375uS
  //  TWBR = 4; // freq=666kHz period=1.500uS
  //  TWBR = 5; // freq=615kHz period=1.625uS
  TWBR = 10; // freq=444kHz period=2.250uS
  //  TWBR = 20; // freq=285kHz period=3.500uS
  //  TWBR = 30; // freq=210kHz period=4.750uS
  //  TWBR = 40; // freq=166kHz period=6.000uS
  //  TWBR = 50; // freq=137kHz period=7.250uS

  initSH1106();
  InitSigGen();
}


//-----------------------------------------------------------------------------
// Main routines
// loop
//-----------------------------------------------------------------------------
void loop (void) {
  ExecSigGenMenu();
}
