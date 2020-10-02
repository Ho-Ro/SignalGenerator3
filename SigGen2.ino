//-----------------------------------------------------------------------------
// Stand Alone Signal Generator
// Based on this project:
// https://www.instructables.com/id/Signal-Generator-AD9833/
// Copyright 2018 Peter Balch
// subject to the GNU General Public License
//-----------------------------------------------------------------------------

#include <SPI.h>
#include <Wire.h>
#include <math.h>

#include "SimpleSH1106.h"

//-----------------------------------------------------------------------------
// Defines and Typedefs
//-----------------------------------------------------------------------------

// get register bit - faster: doesn't turn it into 0/1
#ifndef getBit
#define getBit( sfr, bit ) ( _SFR_uint8_t( sfr ) & _BV( bit ) )
#endif


//-----------------------------------------------------------------------------
// Global Constants
//-----------------------------------------------------------------------------

const long BAUDRATE  = 115200;  // Baud rate of UART in bps

//-----------------------------------------------------------------------------
// Global Variables
//-----------------------------------------------------------------------------

//-----------------------------------------------------------------------------
// globals used in SigGen
//-----------------------------------------------------------------------------

const uint8_t digits = 6; // number of digits ( nOD ) in the frequency arrays
uint8_t freqStart[ digits ] = { 0, 0, 1, 0, 0, 0 }; // 1000Hz cursor = 0..digits-1
uint8_t freqStop[ digits ]  = { 0, 2, 0, 0, 0, 0 }; // 20kHz  cursor = digits..2*digits-1
const uint8_t waveformPos  = 2 * digits;
const uint8_t sweepPos     = 2 * digits + 1;
const uint8_t gainPos      = 2 * digits + 2;
const uint8_t exchgPos     = 2 * digits + 3;

uint8_t cursor = 0; // MSB freqStart

const uint16_t wReset     = 0b0000000100000000;
const uint16_t wSine      = 0b0000000000000000;
const uint16_t wTriangle  = 0b0000000000000010;
const uint16_t wRectangle = 0b0000000000101000;

enum sweep_t { swOff = 0, sw1Sec, sw3Sec, sw10Sec, sw30Sec };
sweep_t sweep = swOff;

uint16_t waveType = wSine;
uint8_t gain = 8;

// connections to AD9833
const int AD_FSYNC = 10;

// connections to MCP41XXX
const int MCP_CS = 9;

// button inputs
const int btnLeft  = 8; // pushbutton
const int btnRight = 7; // pushbutton
const int btnUp    = 6; // pushbutton
const int btnDown  = 5; // pushbutton
const int testOut  = 4; // output for test signal
const int pwmOut   = 3; // output rectangle to create neg voltage

uint16_t SG_iSweep, SG_nSweep;


//-----------------------------------------------------------------------------
// Main routines
// The setup function
//-----------------------------------------------------------------------------
void setup( void ) {
  // Open serial port with a baud rate of BAUDRATE b/s
  Serial.begin( BAUDRATE );

  // Activate interrupts
  sei ();

  Serial.println( F( "SigGen2 " __DATE__ ) ); // compilation date
  Serial.println( F( "OK" ) );

  initTimer1(250);  // init timer1 for sweep timing
  initTimer2();  // init timer2 output for neg. voltage charge pump
  initButtons(); // prepare the UI buttons
  SH1106.init(); // init the display
  initSigGen();  // and finally init the signal generator

}


//-----------------------------------------------------------------------------
// Main routines
// loop
//-----------------------------------------------------------------------------
void loop( void ) {
  execMenu();
}


//-----------------------------------------------------------------------------
// images for main menu
//-----------------------------------------------------------------------------

const uint8_t imgCurRt[] PROGMEM = {
  4, // width
  1, // pages
  4, // uint8_ts
  0xFF, 0x7E, 0x3C, 0x18,
};

const uint8_t imgCurUp[] PROGMEM = {
  7, // width
  1, // pages
  7, // uint8_ts
  0x20, 0x30, 0x38, 0x3C, 0x38, 0x30, 0x20,
};

const uint8_t imgTria[] PROGMEM = {
  14, // width
  2,  // pages
  28, // uint8_ts
  0xC0, 0x30, 0x0C, 0x03, 0x03, 0x0C, 0x30, 0xC0, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
  0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x83, 0x8C, 0xB0, 0xB0, 0x8C, 0x83,
};

const uint8_t imgSine[] PROGMEM = {
  14, // width
  2,  // pages
  28, // 28 bars
  0xE0, 0x1C, 0x02, 0x01, 0x01, 0x02, 0x1C, 0xE0, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
  0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x81, 0x8E, 0x90, 0xA0, 0xA0, 0x90, 0x8F
};

const uint8_t imgRect[] PROGMEM = {
  14, // width
  2,  // pages
  28, // 28 bars
  0x00, 0x00, 0x00, 0xFF, 0x01, 0x01, 0x01, 0x01, 0x01, 0x01, 0xFF, 0x00, 0x00, 0x00,
  0xA0, 0xA0, 0xA0, 0xBF, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0xBF, 0xA0, 0xA0, 0xA0,
};

#if 0
const uint8_t imgHz[] PROGMEM = { // unencoded bars (->43 bytes)
  20, // width
  2,  // pages
  40, //
  0xFE, 0xFE, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0xFE, 0xFE,
  0x00, 0x00, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80,
  0xFF, 0xFF, 0x01, 0x01, 0x01, 0x01, 0x01, 0x01, 0xFF, 0xFF,
  0x00, 0x00, 0xC1, 0xE1, 0xF1, 0xD9, 0xCD, 0xC7, 0xC3, 0xC1,
};

#else

const uint8_t imgHz[] PROGMEM = { // run length encoded (RLE) bars (->29 bytes)
  20, // width
  2,  // pages
  128 + 2, 0xFE, // 2x -> 0xFE, 0xFE,
  128 + 6, 0x80, // 6x -> 0x80, 0x80, 0x80, 0x80, 0x80, 0x80,
  128 + 2, 0xFE, // 2x
  128 + 2, 0x00, // 2x
  128 + 8, 0x80, // 8x
  128 + 2, 0xFF, // 2x
  128 + 6, 0x01, // 6x
  128 + 2, 0xFF, // 2x
  128 + 2, 0x00, // 2x
  8, // 8 bars
  0xC1, 0xE1, 0xF1, 0xD9, 0xCD, 0xC7, 0xC3, 0xC1,
};
#endif


const uint8_t imgUpDown[] PROGMEM = {
  7, // width
  2,  // pages
  14, //
  0x10, 0x18, 0xFC, 0xFE, 0xFC, 0x18, 0x10,
  0x10, 0x30, 0x7F, 0xFF, 0x7F, 0x30, 0x10,
};


//-----------------------------------------------------------------------------
// showMenu
//-----------------------------------------------------------------------------
void showMenu( void ) {
  uint8_t x, y, i;
  SH1106.drawBox( F("Signal Generator") );

  drawGain();

  if ( sweep == swOff ) {
    x = 24;
    y = 2;
    for ( i = 0; i < digits; ++i ) {
      if ( i == cursor )
        SH1106.drawImage( x + 2, y + 2, imgCurUp );
      x += SH1106.drawInt( freqStart[ i ], x, y, SH1106.largeDigitsFont );
    }
    SH1106.drawImage( x + 6, y, imgHz );
  } else {
    x = 70;
    y = 2;
    SH1106.drawString( F("Start Freq:"), 24, y, SH1106.smallFont );
    for ( i = 0; i < digits; ++i ) {
      if ( i == cursor )
        SH1106.drawImage( x - 2, y + 1, imgCurUp );
      x += SH1106.drawInt( freqStart[ i ], x, y, SH1106.smallFont );
    }
    SH1106.drawString( F(" Hz"),  x, y, SH1106.smallFont );

    x = 70;
    y = 4;
    SH1106.drawString( F("Stop Freq:"), 24, y, SH1106.smallFont );
    for ( i = 0; i < digits; ++i ) {
      if ( i == cursor - digits )
        SH1106.drawImage( x - 2, y + 1, imgCurUp );
      x += SH1106.drawInt( freqStop[ i ], x, y, SH1106.smallFont );
    }
    SH1106.drawString( F(" Hz"),  x, y, SH1106.smallFont );
  }

  x = 14;
  y = 6;
  if ( cursor == waveformPos )
    SH1106.drawImage( x - 6, y, imgCurRt );
  //  switch ( waveType ) {
  //    case wSine:      SH1106.drawString( F("Sine"),     x, y, SH1106.smallFont ); break;
  //    case wTriangle:  SH1106.drawString( F("Triangle"), x, y, SH1106.smallFont ); break;
  //    case wRectangle: SH1106.drawString( F("Rectangle"),   x, y, SH1106.smallFont ); break;
  //  }
  for ( x = 14; x < 42; x += 14 )
    switch ( waveType ) {
      case wReset:     if ( 14 == x ) SH1106.drawString( F("OFF"), 20, y, SH1106.smallFont ); break;
      case wSine:      SH1106.drawImage( x, y, imgSine ); break;
      case wTriangle:  SH1106.drawImage( x, y, imgTria ); break;
      case wRectangle: SH1106.drawImage( x, y, imgRect ); break;
    }

  x = 54;
  y = 6;
  switch ( sweep ) {
    case swOff:
      SH1106.drawString( F("Constant"), x, y, SH1106.smallFont );
      //SH1106.drawString( F("X"), 110, y, SH1106.smallFont );
      break;
    case sw1Sec:      SH1106.drawString( F("Sweep 1 Sec"),  x, y, SH1106.smallFont ); break;
    case sw3Sec:      SH1106.drawString( F("Sweep 3 Sec"),  x, y, SH1106.smallFont ); break;
    case sw10Sec:     SH1106.drawString( F("Sweep 10 Sec"), x, y, SH1106.smallFont ); break;
    case sw30Sec:     SH1106.drawString( F("Sweep 30 Sec"), x, y, SH1106.smallFont ); break;
  }

  if ( cursor == sweepPos )
    SH1106.drawImage( x - 6, y, imgCurRt );
  else if ( cursor == exchgPos )
    SH1106.drawImage( 13, 2, imgUpDown );
  else if ( cursor == gainPos )
    SH1106.drawImage( 2, 5, imgCurUp );
}


void drawGain() {
  uint32_t bar4 = 0xFFFFFFFFL << 32 - 2 * gain;
  for ( uint8_t page = 1; page < 5; ++page ) {
    for ( uint8_t col = 3; col <= 7; ++col )
      SH1106.drawBar( col, page, lowByte( bar4 ) );
    bar4 >>= 8;
  }
}


//-----------------------------------------------------------------------------
// execMenu
//   SigGen menu
//   user presses sel or Adj buttons
//   return if no button for 2 sec
//-----------------------------------------------------------------------------
void execMenu( void ) {
  static uint8_t prevLeft  = 0;
  static uint8_t prevRight = 0;
  static uint8_t prevUp    = 0;
  static uint8_t prevDown  = 0;
  uint8_t btn;
  uint8_t fChg = 0;

  showMenu();

  SG_iSweep = 0;

  do {
    if ( parseSerial() )
      fChg = 1;


    btn = digitalRead( btnUp );
    if ( btn != prevUp ) {
      prevUp = btn;
      if ( btn == LOW ) {
        incItem ();
        showMenu ();
        fChg = 1;
      }
      myDelay( 100 );
      //StartTimer1( 0 );
    }

    btn = digitalRead( btnDown );
    if ( btn != prevDown ) {
      prevDown = btn;
      if ( btn == LOW ) {
        decItem ();
        showMenu ();
        fChg = 1;
      }
      myDelay( 100 );
      //StartTimer1( 0 );
    }

    btn = digitalRead( btnLeft );
    if ( btn != prevLeft ) {
      prevLeft = btn;
      if ( btn == LOW ) {
        cursorLeft ();
        showMenu ();
      }
      myDelay( 100 );
      //StartTimer1( 0 );
    }

    btn = digitalRead( btnRight );
    if ( btn != prevRight ) {
      prevRight = btn;
      if ( btn == LOW ) {
        cursorRight ();
        showMenu ();
      }
      myDelay( 100 );
      //StartTimer1( 0 );
    }

    if ( sweep == swOff ) {
      if ( fChg )
        setFrequency( calcFreq( freqStart ), waveType );
    } else {
      switch ( sweep ) {
        case sw1Sec:  SG_nSweep =  1000L; break;
        case sw3Sec:  SG_nSweep =  3000L; break;
        case sw10Sec: SG_nSweep = 10000L; break;
        case sw30Sec: SG_nSweep = 30000L; break;
      }
      static bool test = true;
      while ( ! TIFR1 ) // wait for timer1 overflow
        ;
      test = ! test;
      digitalWrite( testOut, test );
      stepSweep();
    }
    TIFR1 = 0xFF; // clear all timer1 flags
  } while ( true );
}


//-----------------------------------------------------------------------------
// parseSerial
//   if a uint8_t is available in the serial input buffer
//   execute it as a command
//-----------------------------------------------------------------------------
bool parseSerial( void ) {
  static bool numeric = false; // input of argument
  static bool kiloMod = false; // true if char 'k' was input
  bool fChg = false;
  if ( Serial.available () > 0 ) {
    char c = Serial.read ();
    if ( ( c >= '0' ) && ( c <= '9' ) ) {
      for ( int i = 0; i < digits - 1 ; ++i )
        freqStart[ i ] = numeric ? freqStart[ i + 1 ] : 0; // clear all or shift left
      freqStart[ digits - 1 ] = c - '0'; // add new digit at the right
      numeric = true; // we are in argument input mode
      kiloMod = false;
    } else { // all non numeric char stop number input
      numeric = false; // no more digits
      switch ( c ) {
        case 'k': // multiply by 1000
          if ( !kiloMod ) { // apply only once
            for ( int i = 0 ; i < digits - 3 ; i++ ) {
              freqStart[ i ] = freqStart[ i + 3 ]; // value << 3 digits
              freqStart[ i + 3 ] = 0; // clear last 3 digits
            }
            kiloMod = true;
          }
          break;
        case 'S':
          waveType = wSine;
          fChg = true;
          // resetFrequency( calcFreq( freqStart ), waveType );
          break;
        case 'T':
          waveType = wTriangle;
          fChg = true;
          // resetFrequency( calcFreq( freqStart ), waveType );
          break;
        case 'R':
          waveType = wRectangle;
          fChg = true;
          // resetFrequency( calcFreq( freqStart ), waveType );
          break;
        case 'O':
          resetAD9833 ();
          waveType = wReset;
          break;
        case 'X': // exchange start and stop array
          exchgFreq();
          fChg = true;
          break;
        case 'C':
          sweep = swOff;
          fChg = true;
          // resetFrequency( calcFreq( freqStart ), waveType );
          break;
        case 'G':
          sweep = sw1Sec;
          break;
        case 'H':
          sweep = sw3Sec;
          break;
        case 'I':
          sweep = sw10Sec;
          break;
        case 'J':
          sweep = sw30Sec;
          break;
        default:
          return false;
      }
    }
    showMenu ();
  }
  return fChg;
}


//-----------------------------------------------------------------------------
// cursorRight
//   increment caret position for SigGen Menu
//-----------------------------------------------------------------------------
void cursorRight( void ) {
  if ( cursor == exchgPos )
    cursor = 0;
  else
    ++cursor;
  // skip over the stop frequency display if no sweep
  if ( ( cursor >= digits ) && ( cursor < 2 * digits ) && ( sweep == swOff ) )
    cursor = waveformPos;
}


//-----------------------------------------------------------------------------
// cursorLeft
//   decrement caret position for SigGen Menu
//-----------------------------------------------------------------------------
void cursorLeft( void ) {
  if ( cursor == 0 )
    cursor = exchgPos;
  // cursor = sweep ? sweepPos : exchgPos;
  else
    --cursor;
  // skip over the stop frequency display if no sweep
  if ( ( cursor >= digits ) && ( cursor < 2 * digits ) && ( sweep == swOff ) )
    cursor = digits - 1;
}


//-----------------------------------------------------------------------------
// incItem
//   increment item at cursor
//-----------------------------------------------------------------------------
void incItem( void ) {
  if (cursor == gainPos ) {
    if ( gain < 16 )
      ++gain;
    writeMCP41( gain * gain );
  } else if ( cursor == exchgPos ) {
    exchgFreq();
  } else if ( cursor == sweepPos ) {
    if ( sweep == sw30Sec )
      sweep = swOff;
    else
      sweep = sweep_t( sweep + 1 );
  } else if ( cursor == waveformPos ) {
    switch ( waveType ) {
      case wReset:     waveType = wSine; break;
      case wSine:      waveType = wTriangle; break;
      case wTriangle:  waveType = wRectangle; break;
      case wRectangle: waveType = wReset; break;
    }
  } else if ( cursor < digits ) {
    if ( freqStart[ cursor ] >= 9 )
      freqStart[ cursor ] = 0;
    else
      freqStart[ cursor ]++;
  } else if ( sweep != swOff ) {
    if ( freqStop[ cursor - digits ] >= 9 )
      freqStop[ cursor - digits ] = 0;
    else
      freqStop[ cursor - digits ]++;
  }
}


//-----------------------------------------------------------------------------
// decItem
//   decrement item at cursor
//-----------------------------------------------------------------------------
void decItem( void ) {
  if (cursor == gainPos ) {
    if ( gain )
      --gain;
    writeMCP41( gain * gain );
  } else if ( cursor == exchgPos ) {
    exchgFreq();
  } else if ( cursor == sweepPos ) { // Off, 1s, 3s, 10s, 30s
    if ( sweep == swOff )
      sweep = sw30Sec;
    else
      sweep = sweep_t( sweep - 1 );
  } else if ( cursor == waveformPos ) {
    switch ( waveType ) {
      case wReset:     waveType = wRectangle; break;
      case wRectangle: waveType = wTriangle; break;
      case wTriangle:  waveType = wSine; break;
      case wSine:      waveType = wReset; break;
    }
  } else if ( cursor < digits ) {
    if ( freqStart[ cursor ] <= 0 )
      freqStart[ cursor ] = 9;
    else
      freqStart[ cursor]--;
  } else if ( sweep != swOff ) {
    if ( freqStop[cursor - digits ] <= 0 )
      freqStop[ cursor - digits ] = 9;
    else
      freqStop[ cursor - digits ]--;
  }
}


//-----------------------------------------------------------------------------
//calculate the frequency from an array.
//-----------------------------------------------------------------------------
unsigned long calcFreq( const uint8_t *freqArray ) {
  unsigned long f = 0;
  for ( uint8_t x = 0; x < digits; ++x ) {
    f *= 10;
    f += freqArray[ x ];
  }
  return f;
}


//-----------------------------------------------------------------------------
// stepSweep
//    increment the frequency
//-----------------------------------------------------------------------------
void stepSweep( void ) {
  if ( SG_iSweep > SG_nSweep ) SG_iSweep = 0;
  long f = exp( ( log( calcFreq( freqStop ) ) - log( calcFreq( freqStart ) ) ) * SG_iSweep / SG_nSweep + log( calcFreq( freqStart ) ) ) + 0.5;
  setFrequency( f, waveType );
  SG_iSweep++;
}


#if 0
//-----------------------------------------------------------------------------
// Sweep
//   sweeps siggen freq continuously
//   takes n mS for whole sweep
//   SDC regs are saved and restored
//   stops when receives a serial char
//-----------------------------------------------------------------------------
void Sweep( int n ) {
  long fstart = calcFreq( freqStart );
  long fstop  = calcFreq( freqStop );
  int i = 0;

  showMenu ();

  do {
    long f = exp( ( log( fstop ) - log( fstart ) ) * i / ( n - 1 ) + log( fstart ) ) + 0.5;
    setFrequency( f, waveType );
    delay( 1 );
    if ( ++i >= n ) {
      i = 0;
    }
  } while ( !Serial.available () );
  sweep = swOff;
  setFrequency( calcFreq( freqStart ), waveType );
}
#endif


//-----------------------------------------------------------------------------
// exchgFreq
//    exchange start and stop frequency
//-----------------------------------------------------------------------------
void exchgFreq() {
  uint8_t x;
  for ( int i = 0; i < digits ; i++ ) {
    x = freqStart[ i ];
    freqStart[ i ] = freqStop[ i ];
    freqStop[ i ] = x;
  }
}


//-----------------------------------------------------------------------------
// initSigGen
//-----------------------------------------------------------------------------
void initSigGen( void ) {
  digitalWrite( AD_FSYNC, HIGH );
  pinMode( AD_FSYNC, OUTPUT );
  digitalWrite( MCP_CS, HIGH );
  pinMode( MCP_CS, OUTPUT );
  SPI.begin ();

  writeMCP41( 0x80 ); // R=0..255
  resetAD9833();
  resetFrequency( calcFreq( freqStart ), waveType );
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
// initTimer1
// TIFR1 becomes non-zero after
//    overflow * 64 / 16000000 sec
//-----------------------------------------------------------------------------

void initTimer1( word overflow ) {
  TCCR1A = 0x00; // Set OC1A on Compare Match
  TCCR1B = 0x0B; // CTCmode, prescaler = 64 -> 250kHz
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
// configTimer2
// output 50 kHz rectangele at D3 for a charge pump
//-----------------------------------------------------------------------------
void initTimer2()
{
  //Initialize Timer2
  TCCR2A = 0;
  TCCR2B = 0;
  TCNT2 = 0;

  // Set OC2B for Compare Match (digital pin3)
  pinMode( pwmOut, OUTPUT );

  bitSet( TCCR2A, COM2B1 );//clear OC2B on up count compare match

  // Set mode 5 -> Phase correct PWM to OCR2A counts up and down
  bitSet( TCCR2A, WGM20 );
  bitSet( TCCR2B, WGM22 );

  // Set up prescaler to 001 = clk (16 MHz)
  bitSet(TCCR2B, CS20);
  //bitSet(TCCR2B, CS21);
  //bitSet(TCCR2B, CS22);

  OCR2A = 160; // Sets t = 10 µs up + 10 µs down -> freq = 50 kHz
  OCR2B = 80;  // 50% duty cycle, valid values: 0 (permanent low), 1..79, 80 (permanent high)
}


void initButtons() {
  pinMode( btnLeft,  INPUT_PULLUP );
  pinMode( btnRight, INPUT_PULLUP );
  pinMode( btnUp,    INPUT_PULLUP );
  pinMode( btnDown,  INPUT_PULLUP );
  pinMode( testOut,  OUTPUT );
}


//-----------------------------------------------------------------------------
// writeMCP41 ( SPI )
//-----------------------------------------------------------------------------
void writeMCP41( int data ) {
  if ( data > 255 )
    data = 255;
  else if ( data < 0 )
    data = 0;
  SPI.beginTransaction( SPISettings( 10000000, MSBFIRST, SPI_MODE0 ) );
  digitalWrite( MCP_CS, LOW );
  SPI.transfer16( 0x1100 + data );
  digitalWrite( MCP_CS, HIGH );
}


//-----------------------------------------------------------------------------
// resetAD9833
//-----------------------------------------------------------------------------
void resetAD9833 () {
  delay( 100 );
  SPI.beginTransaction( SPISettings( 10000000, MSBFIRST, SPI_MODE3 ) );
  digitalWrite( AD_FSYNC, LOW );
  SPI.transfer16( wReset );
  digitalWrite( AD_FSYNC, HIGH );
  sweep = swOff;
  delay( 100 );
}


//-----------------------------------------------------------------------------
// resetFrequency
//    reset the SG regs then set the frequency and wave type
//-----------------------------------------------------------------------------
void resetFrequency( long frequency, uint16_t wave ) {
  long fl = frequency * ( 0x10000000L / 25000000.0 );
  SPI.beginTransaction( SPISettings( 10000000, MSBFIRST, SPI_MODE3 ) );
  digitalWrite( AD_FSYNC, LOW );
  SPI.transfer16( 0x2100 );
  SPI.transfer16( uint16_t( fl & 0x3FFFL ) | 0x4000 );
  SPI.transfer16( uint16_t( ( fl & 0xFFFC000L ) >> 14 ) | 0x4000 );
  SPI.transfer16( 0xC000 );
  SPI.transfer16( wave );
  digitalWrite( AD_FSYNC, HIGH );
  waveType = wave;
}


//-----------------------------------------------------------------------------
// setFrequency
//    set the SG frequency and waveform regs
//-----------------------------------------------------------------------------
void setFrequency( long frequency, uint16_t wave ) {
  long fl = frequency * ( 0x10000000L / 25000000.0 );
  digitalWrite( AD_FSYNC, LOW );
  SPI.transfer16( 0x2000 | wave );
  SPI.transfer16( uint16_t( fl & 0x3FFFL ) | 0x4000 );
  SPI.transfer16( uint16_t( ( fl & 0xFFFC000L ) >> 14 ) | 0x4000 );
  digitalWrite( AD_FSYNC, HIGH );
}


/******************************************************************************
  AD9833 register ( 16 bit )
  D15 D14   00: CONTROL ( 14 bits )
          01: FREQ0   ( 14 bits )
          10: FREQ1   ( 14 bits )
          11: PHASE   ( 12 bits ) ( D13 D12 0X: PHASE0, 1X: PHASE1 )

  CONTROL bits:

  D13: B28
  Two write operations are required to load a complete word into either of the frequency registers.
  B28 = 1 allows a complete word to be loaded into a frequency register in two consecutive writes.
  The first write contains the 14 LSBs of the frequency word, and the next write contains the 14 MSBs.
  The first two bits of each 16-bit word define the frequency register to which the word is loaded,
  and should therefore be the same for both of the consecutive writes.
  See Table 8 for the appropriate addresses.
  The write to the frequency register occurs after both words have been loaded; therefore, the register
  never holds an intermediate value. An example of a complete 28-bit write is shown in Table 9.
  When B28 = 0, the 28-bit frequency register operates as two 14-bit registers, one containing
  the 14 MSBs and the other containing the 14 LSBs. This means that the 14 MSBs of the frequency word
  can be altered independent of the 14 LSBs, and vice versa. To alter the 14 MSBs or the 14 LSBs,
  a single write is made to the appropriate frequency address. The control bit D12 ( HLB ) informs
  the AD9833 whether the bits to be altered are the 14 MSBs or 14 LSBs.

  D12: HLB
  This control bit allows the user to continuously load the MSBs or LSBs of a frequency register while
  ignoring the remaining 14 bits. This is useful if the complete 28-bit resolution is not required.
  HLB is used in conjunction with D13 ( B28 ). This control bit indicates whether the 14 bits being
  loaded are being transferred to the 14 MSBs or 14 LSBs of the addressed frequency register.
  D13 ( B28 ) must be set to 0 to be able to change the MSBs and LSBs of a frequency word separately.
  When D13 ( B28 ) = 1, this control bit is ignored. HLB = 1 allows a write to the 14 MSBs of the addressed
  frequency register. HLB = 0 allows a write to the 14 LSBs of the addressed frequency register.

  D11: FSELECT
  The FSELECT bit defines whether the FREQ0 register or the FREQ1 register is used in the phase accumulator.

  D10: PSELECT
  The PSELECT bit defines whether the PHASE0 register or the PHASE1 register data is added to the output
  of the phase accumulator.

  D9: Reserved
  This bit should be set to 0.

  D8: Reset
  Reset = 1 resets internal registers to 0, which corresponds to an analog output of midscale.
  Reset = 0 disables reset.

  D7: SLEEP1
  When SLEEP1 = 1, the internal MCLK clock is disabled, and the DAC output remains at its present value
  because the NCO is no longer accumulating. When SLEEP1 = 0, MCLK is enabled.

  D6: SLEEP12
  SLEEP12 = 1 powers down the on-chip DAC. This is useful when the AD9833 is used to output the MSB
  of the DAC data. SLEEP12 = 0 implies that the DAC is active.

  D5: OPBITEN
  The function of this bit, in association with D1 ( mode ), is to control what is output at the VOUT pin.
  When OPBITEN = 1, the output of the DAC is no longer available at the VOUT pin. Instead, the MSB
  ( or MSB/2 ) of the DAC data is connected to the VOUT pin. This is useful as a coarse clock source.
  The DIV2 bit controls whether it is the MSB or MSB/2 that is output. When OPBITEN = 0, the DAC is
  connected to VOUT. The mode bit determines whether it is a sinusoidal or a ramp output that is available.

  D4: Reserved
  This bit must be set to 0.

  D3: DIV2
  DIV2 is used in association with D5 ( OPBITEN ). When DIV2 = 1, the MSB of the DAC data is passed
  directly to the VOUT pin. When DIV2 = 0, the MSB/2 of the DAC data is output at the VOUT pin.

  D2: Reserved
  This bit must be set to 0.

  D1: MODE
  This bit is used in association with OPBITEN ( D5 ). The function of this bit is to control
  what is output at the VOUT pin when the on-chip DAC is connected to VOUT. This bit should
  be set to 0 if the control bit OPBITEN = 1. When MODE = 1, the SIN ROM is bypassed, resulting
  in a triangle output from the DAC. When MODE = 0, the SIN ROM is used to convert the phase
  information into amplitude information, which results in a sinusoidal signal at the output.

  D0: Reserved
  This bit must be set to 0.

*******************************************************************************/
