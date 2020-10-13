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

#include "MCP4x.h"
#include "SimpleSH1106.h"
#include "AD9833.h"


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

// connection to MCP41010
const int MCP_CS = 9;

// connections to AD9833
const int AD_FSYNC = 10;


//-----------------------------------------------------------------------------
// Global HW objects
//-----------------------------------------------------------------------------

SimpleSH1106 OLED;

MCP4x MCP( 9 );

AD9833 AD( 10 );

//-----------------------------------------------------------------------------
// globals used in SigGen
//-----------------------------------------------------------------------------

const uint8_t digits = 6; // number of digits ( nOD ) in the frequency arrays
// three number arrays: data input, start and stop frequency
uint8_t dataInput[ digits ] = { 0, 0, 1, 0, 0, 0 }; // data input accumulator
uint8_t freqStart[ digits ] = { 0, 0, 1, 0, 0, 0 }; // 1000Hz, cursor pos = 0..digits-1
uint8_t freqStop[ digits ]  = { 0, 2, 0, 0, 0, 0 }; // 20kHz,  cursor pos = digits..2*digits-1
const uint8_t waveformPos  = 2 * digits; // cursor position for these items
const uint8_t sweepPos     = 2 * digits + 1;
const uint8_t gainPos      = 2 * digits + 2;
const uint8_t exchgPos     = 2 * digits + 3;

uint8_t cursor = 0; // point to MSB of freqStart

enum sweep_t { swOff = 0, sw1Sec, sw3Sec, sw10Sec, sw30Sec };
sweep_t sweep = swOff;

uint16_t waveType = AD9833::wSine;
uint8_t gain = 0;
int8_t dB = 0;

// button inputs
const int btnLeft  = 8; // pushbutton
const int btnRight = 7; // pushbutton
const int btnUp    = 6; // pushbutton
const int btnDown  = 5; // pushbutton
const int testOut  = 4; // output for test signal
const int pwmOut   = 3; // output rectangle to create a neg. voltage

uint16_t sweepPosition, sweepSteps;


//-----------------------------------------------------------------------------
// Main routines
// The setup function
//-----------------------------------------------------------------------------
void setup( void ) {
  // Open serial port with a baud rate of BAUDRATE b/s
  Serial.begin( BAUDRATE );

  // Activate interrupts
  sei ();

  Serial.println( F( "SignalGenerator3 " __DATE__ ) ); // compilation date

  initTimer1(250);  // init timer1 for sweep timing
  initTimer2();  // init timer2 output for neg. voltage charge pump
  initButtons(); // prepare the UI buttons
  OLED.init(); // init the display
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

// up pointing cursor
const uint8_t imgCurRt[] PROGMEM = {
  4, // width
  1, // pages
  4, // uint8_ts
  0xFF, 0x7E, 0x3C, 0x18,
};

// right pointing cursor
const uint8_t imgCurUp[] PROGMEM = {
  7, // width
  1, // pages
  7, // uint8_ts
  0x20, 0x30, 0x38, 0x3C, 0x38, 0x30, 0x20,
};

// one period of triangle
const uint8_t imgTria[] PROGMEM = {
  14, // width
  2,  // pages
  28, // uint8_ts
  0xC0, 0x30, 0x0C, 0x03, 0x03, 0x0C, 0x30, 0xC0, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
  0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x83, 0x8C, 0xB0, 0xB0, 0x8C, 0x83,
};

// one period of sine
const uint8_t imgSine[] PROGMEM = {
  14, // width
  2,  // pages
  28, // 28 bars
  0xE0, 0x1C, 0x02, 0x01, 0x01, 0x02, 0x1C, 0xE0, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
  0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x81, 0x8E, 0x90, 0xA0, 0xA0, 0x90, 0x8F
};

// one period of rectangle
const uint8_t imgRect[] PROGMEM = {
  14, // width
  2,  // pages
  28, // 28 bars
  0x00, 0x00, 0x00, 0xFF, 0x01, 0x01, 0x01, 0x01, 0x01, 0x01, 0xFF, 0x00, 0x00, 0x00,
  0xA0, 0xA0, 0xA0, 0xBF, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0xBF, 0xA0, 0xA0, 0xA0,
};

// large text "Hz" image for use with largeDigitsFont
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

// up-down pointing double arrow
const uint8_t imgUpDown[] PROGMEM = {
  7, // width
  2,  // pages
  14, //
  0x10, 0x18, 0xFC, 0xFE, 0xFC, 0x18, 0x10,
  0x10, 0x30, 0x7F, 0xFF, 0x7F, 0x30, 0x10,
};


//-----------------------------------------------------------------------------
// showMenu
// draw a box and show complete generator status
//-----------------------------------------------------------------------------
void showMenu( void ) {
  uint8_t col, page, i;
  OLED.drawBox( F("Signal Generator") );

  // show a vertical logarithmic gain bar on the left 
  drawGain();

  if ( sweep == swOff ) { // one large frequency display
    page = 2;
    col = 25;
    for ( i = 0; i < digits; ++i ) {
      if ( i == cursor )
        OLED.drawImage( col + 2, page + 2, imgCurUp );
      col += OLED.drawInt( freqStart[ i ], col, page, OLED.largeDigitsFont );
    }
    OLED.drawImage( col + 6, page, imgHz ); // display "Hz" as image (large font is num-only)
  } else { // two small frequencies (sweep start and stop frequency) 
    // 1st row
    page = 2;
    col = 70;
    OLED.drawString( F("Start Freq:"), 24, page, OLED.smallFont );
    for ( i = 0; i < digits; ++i ) {
      if ( i == cursor )
        OLED.drawImage( col - 2, page + 1, imgCurUp );
      col += OLED.drawInt( freqStart[ i ], col, page, OLED.smallFont );
    }
    OLED.drawString( F(" Hz"), col, page, OLED.smallFont );
    // 2nd row
    page = 4;
    col = 70;
    OLED.drawString( F("Stop Freq:"), 24, page, OLED.smallFont );
    for ( i = 0; i < digits; ++i ) {
      if ( i == cursor - digits )
        OLED.drawImage( col - 2, page + 1, imgCurUp );
      col += OLED.drawInt( freqStop[ i ], col, page, OLED.smallFont );
    }
    OLED.drawString( F(" Hz"), col, page, OLED.smallFont );
  }

  // show dB amplitude below gain bar
  page = 6;
  col = 2;
  col += OLED.drawInt( dB, col, page, OLED.smallFont );
  OLED.drawString( F("dB" ), col + 2, page, OLED.smallFont );

  if ( cursor == waveformPos )
    OLED.drawImage( 25, page + 1, imgCurRt );

  // show two periods of wave form
  for ( col = 32; col < 60; col += 14 )
    switch ( waveType ) {
      case AD9833::wReset:     if ( 32 == col ) OLED.drawString( F("OFF"), col, page, OLED.smallFont ); break;
      case AD9833::wSine:      OLED.drawImage( col, page, imgSine ); break;
      case AD9833::wTriangle:  OLED.drawImage( col, page, imgTria ); break;
      case AD9833::wRectangle: OLED.drawImage( col, page, imgRect ); break;
    }

  // display sweep time
  page = 6;
  col = 70;
  switch ( sweep ) {
    case swOff:   OLED.drawString( F("Constant"), col, page, OLED.smallFont ); break;
    case sw1Sec:  OLED.drawString( F("Sweep 1 s"),  col, page, OLED.smallFont ); break;
    case sw3Sec:  OLED.drawString( F("Sweep 3 s"),  col, page, OLED.smallFont ); break;
    case sw10Sec: OLED.drawString( F("Sweep 10 s"), col, page, OLED.smallFont ); break;
    case sw30Sec: OLED.drawString( F("Sweep 30 s"), col, page, OLED.smallFont ); break;
  }

  if ( cursor == sweepPos )
    OLED.drawImage( col - 6, 6, imgCurRt );
  else if ( cursor == exchgPos )
    OLED.drawImage( 14, 2, imgUpDown );
  else if ( cursor == gainPos )
    OLED.drawImage( 4, 5, imgCurUp );
}


// show a vertical logarithmic gain bar
void drawGain() {
  uint32_t bar4 = 0xFFFFFFFFL << 32 - 2 * gain;
  for ( uint8_t page = 1; page < 5; ++page ) {
    for ( uint8_t col = 5; col < 10; ++col )
      OLED.drawBar( col, page, lowByte( bar4 ) );
    bar4 >>= 8;
  }
}


//-----------------------------------------------------------------------------
// execMenu
//   show menu
//   check for serial command
//   check for pressed select or adjust buttons
//   advance sweep frequency if sweep active 
//-----------------------------------------------------------------------------
void execMenu( void ) {
  static uint8_t prevLeft  = 0;
  static uint8_t prevRight = 0;
  static uint8_t prevUp    = 0;
  static uint8_t prevDown  = 0;

  uint8_t btn;

  showMenu();

  sweepPosition = 0;

  do {
    bool newFrequency = false;
    if ( parseSerial() ) {
      enterFreq();
      newFrequency = true;
    }
    btn = digitalRead( btnUp );
    if ( btn != prevUp ) {
      prevUp = btn;
      if ( btn == LOW ) {
        incItem();
        showMenu();
        newFrequency = true;
      }
      myDelay( 100 );
      //StartTimer1( 0 );
    }

    btn = digitalRead( btnDown );
    if ( btn != prevDown ) {
      prevDown = btn;
      if ( btn == LOW ) {
        decItem();
        showMenu();
        newFrequency = true;
      }
      myDelay( 100 );
      //StartTimer1( 0 );
    }

    btn = digitalRead( btnLeft );
    if ( btn != prevLeft ) {
      prevLeft = btn;
      if ( btn == LOW ) {
        cursorLeft ();
        showMenu();
      }
      myDelay( 100 );
      //StartTimer1( 0 );
    }

    btn = digitalRead( btnRight );
    if ( btn != prevRight ) {
      prevRight = btn;
      if ( btn == LOW ) {
        cursorRight();
        showMenu ();
      }
      myDelay( 100 );
      //StartTimer1( 0 );
    }

    if ( sweep == swOff ) {
      if ( newFrequency )
        AD.setFrequency( calcNumber( freqStart ), waveType );
    } else {
      switch ( sweep ) {
        case sw1Sec:  sweepSteps =  1000L; break;
        case sw3Sec:  sweepSteps =  3000L; break;
        case sw10Sec: sweepSteps = 10000L; break;
        case sw30Sec: sweepSteps = 30000L; break;
      }
      static bool test = true;
      while ( ! (TIFR1 & _BV( OCF1A ) )  ) // wait for timer1 output compare match every ms
        ;
      test = ! test; // toggle the test pin
      digitalWrite( testOut, test );
      stepSweep( true ); // advance the frequency one step (true: up, false: down)
    }
    TIFR1 = 0xFF; // clear all timer1 flags
  } while ( true );
}


//-----------------------------------------------------------------------------
// parseSerial
//   if a uint8_t is available in the serial input buffer
//   collect numbers or execute it as a command
//-----------------------------------------------------------------------------
bool parseSerial( void ) {
  static bool numeric = false; // input of argument
  static bool kiloMod = false; // true if char 'k' was input
  static bool minus = false;
  bool newFrequency = false;
  if ( Serial.available () > 0 ) {
    char c = Serial.read ();
    if ( ( c >= '0' ) && ( c <= '9' ) ) {
      for ( int i = 0; i < digits - 1 ; ++i )
        dataInput[ i ] = numeric ? dataInput[ i + 1 ] : 0; // clear all or shift left
      dataInput[ digits - 1 ] = c - '0'; // add new digit at the right
      numeric = true; // we are in argument input mode
      kiloMod = false;
    } else if ( c == 'k' || c == 'K' ) {
      if ( !kiloMod ) { // apply only once
        for ( int i = 0 ; i < digits - 3 ; i++ ) {
          dataInput[ i ] = dataInput[ i + 3 ]; // value << 3 digits
          dataInput[ i + 3 ] = 0; // clear last 3 digits
        }
        kiloMod = true;
      }
    } else if ( c == '-' ) {
      minus = true;
    } else { // all other non numeric char stop number input
      numeric = false; // no more digits
      switch ( toupper( c ) ) {
        case 'A':
          setLinGain( calcNumber( dataInput ) );
          minus = false;
          popFreq();
          showMenu();
          break;
        case 'B': {
            int8_t b = int8_t( minus ? -calcNumber( dataInput ) : calcNumber( dataInput ) );
            minus = false;
            if ( b < 0 )
              b = 0;
            else if ( b > 16 )
              b = 16;
            gain = b;
            setGain();
            popFreq();
            showMenu();
            break;
          }
        case 'D':
          setdBGain( minus ? -calcNumber( dataInput ) : calcNumber( dataInput ) );
          minus = false;
          popFreq();
          showMenu;
          break;
        case 'S':
          waveType = AD9833::wSine;
          newFrequency = true;
          break;
        case 'T':
          waveType = AD9833::wTriangle;
          newFrequency = true;
          break;
        case 'R':
          waveType = AD9833::wRectangle;
          newFrequency = true;
          break;
        case 'O':
          AD.reset();
          waveType = AD9833::wReset;
          break;
        case 'C':
          sweep = swOff;
          newFrequency = true;
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
        case 'X': // exchange start and stop array
          exchgFreq();
          popFreq();
          newFrequency = true;
          break;
        default:
          return false;
      }
      if ( newFrequency ) {
        enterFreq();
      }
      showMenu();
      minus = false;
    }
  }
  return newFrequency;
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
    if ( gain < 16 ) {
      ++gain;
      setGain();
    }
  } else if ( cursor == exchgPos ) {
    exchgFreq();
  } else if ( cursor == sweepPos ) {
    if ( sweep == sw30Sec )
      sweep = swOff;
    else
      sweep = sweep_t( sweep + 1 );
  } else if ( cursor == waveformPos ) {
    switch ( waveType ) {
      case AD9833::wReset:     waveType = AD9833::wSine; break;
      case AD9833::wSine:      waveType = AD9833::wTriangle; break;
      case AD9833::wTriangle:  waveType = AD9833::wRectangle; break;
      case AD9833::wRectangle: waveType = AD9833::wReset; break;
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
    if ( gain ) {
      --gain;
      setGain();
    }
  } else if ( cursor == exchgPos ) {
    exchgFreq();
  } else if ( cursor == sweepPos ) { // Off, 1s, 3s, 10s, 30s
    if ( sweep == swOff )
      sweep = sw30Sec;
    else
      sweep = sweep_t( sweep - 1 );
  } else if ( cursor == waveformPos ) {
    switch ( waveType ) {
      case AD.wReset:     waveType = AD9833::wRectangle; break;
      case AD.wRectangle: waveType = AD9833::wTriangle; break;
      case AD.wTriangle:  waveType = AD9833::wSine; break;
      case AD.wSine:      waveType = AD9833::wReset; break;
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
//calculate the numeric value from an char array.
//-----------------------------------------------------------------------------
unsigned long calcNumber( const uint8_t *charArray ) {
  unsigned long number = 0;
  for ( uint8_t pos = 0; pos < digits; ++pos ) {
    number *= 10;
    number += charArray[ pos ];
  }
  return number;
}


static const uint8_t gainToPot[ 16 ] = {
  0,    1,   2,   3,   5,   8,  11,  17, // -42, -36, -32, -30, -26, -22, -20, -17,
  24,  37,  60,  85, 120, 170, 242, 255, // -14, -10,  -6,  -3,   0,   3,   6,   7
};


void setGain() {
  if ( gain ) {
    int value = gainToPot[ gain - 1 ];
    MCP.setPot( value );
    dB = int8_t( round( 20.0 * log10( ( value + 1 ) / 256.0 ) + 6.5 ) );
    // Serial.println( gain );
    // Serial.println( dB );
    // Serial.println( value );
  } else {
    MCP.shutdown();
    dB = -60;
  }
}


void setLinGain( int value ) { // 0..256
  if ( --value < 0 ) {
    MCP.shutdown();
    gain = 0;
    dB = -60;
  } else {
    if ( value > 255 )
      value = 255;
    MCP.setPot( value );
    for ( gain = sizeof( gainToPot ); gain > 0; --gain )
      if ( gainToPot[ gain - 1 ] < value )
        break;
    ++gain;
    dB = int8_t( round( 20.0 * log10( ( value + 1 ) / 256.0 ) + 6.5 ) );
  }
  // Serial.println( gain );
  // Serial.println( dB );
  // Serial.println( value );
}


void setdBGain( int value ) {
  value = int( 256 * pow( 10.0, ( value - 6.5 ) / 20.0  ) + 0.5 );
  setLinGain( value );
}


//-----------------------------------------------------------------------------
// stepSweep
//    increment the frequency
//-----------------------------------------------------------------------------
void stepSweep( bool stepUp ) {
  if ( sweepPosition > sweepSteps ) sweepPosition = 0;
  long f = exp( ( log( calcNumber( freqStop ) ) - log( calcNumber( freqStart ) ) )
                * ( stepUp ? sweepPosition : sweepSteps - sweepPosition )
                / sweepSteps + log( calcNumber( freqStart ) ) ) + 0.5;
  AD.setFrequency( f, waveType );
  sweepPosition++;
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
  long fstart = calcNumber( freqStart );
  long fstop  = calcNumber( freqStop );
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
  setFrequency( calcNumber( freqStart ), waveType );
}
#endif


//-----------------------------------------------------------------------------
// exchgFreq
//    exchange start and stop frequency
//-----------------------------------------------------------------------------
void exchgFreq() {
  uint8_t x;
  for ( uint8_t i = 0; i < digits ; i++ ) {
    x = freqStart[ i ];
    freqStart[ i ] = freqStop[ i ];
    freqStop[ i ] = x;
  }
}


//-----------------------------------------------------------------------------
// enterFreq
//    transfer dataInput into freqStart
//-----------------------------------------------------------------------------
void enterFreq() {
  for ( uint8_t i = 0; i < digits ; i++ ) {
    freqStart[ i ] = dataInput[ i ];
  }
}


//-----------------------------------------------------------------------------
// popFreq
//    transfer freqStart back into dataInput
//-----------------------------------------------------------------------------
void popFreq() {
  for ( uint8_t i = 0; i < digits ; i++ ) {
    dataInput[ i ] = freqStart[ i ] ;
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

  setdBGain( 0 );
  // gain = 13;

  AD.reset();
  AD.setFrequency( calcNumber( freqStart ), waveType );
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

#if 0
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
#endif