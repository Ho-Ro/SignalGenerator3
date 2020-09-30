/***************************************************
  Arduino TFT graphics library for the SH1106

***************************************************/

#include "SimpleSH1106.h"

#include <avr/pgmspace.h>
#include <Wire.h>

bool boldSH1106 = false;

const int colOffset = 0; // = 2 for 1.3" display


//==============================================================
// drawByteSH1106
//   draws a byte at x,page
//   sets up the row and column then sends the byte
//   quite slow
//==============================================================
void drawByteSH1106( byte x, byte page, byte i ) {
  setupPageCol( page, x );
  Wire.write( i );
  Wire.endTransmission();
}


//==============================================================
// setupPageCol
//   sets up the row and column
//   then gets ready to send one or more bytes
//   should be followed by endTransmission
//==============================================================
void setupPageCol( uint8_t page, uint8_t col ) {
  col += colOffset;
  Wire.beginTransmission( addr );
  Wire.write( 0x00 ); // the following bytes are commands
  Wire.write( 0xB0 + page ); // set page
  Wire.write( 0x00 + ( col & 15 ) ); // lower columns address
  Wire.write( 0x10 + ( col >> 4 ) ); // upper columns address
  Wire.endTransmission();

  Wire.beginTransmission( addr );
  Wire.write( 0x40 ); // the following bytes are data
}


//==============================================================
// setupCol
//   sets up the column
//==============================================================
void setupCol( uint8_t col ) {
  col += colOffset;
  Wire.beginTransmission( addr );
  Wire.write( 0x00 ); // the following bytes are commands
  Wire.write( 0x00 + ( col & 15 ) ); // lower columns address
  Wire.write( 0x10 + ( col >> 4 ) ); // upper columns address
  Wire.endTransmission();
}


//==============================================================
// setupPage
//   sets up the page
//==============================================================
void setupPage( uint8_t page ) {
  Wire.beginTransmission( addr );
  Wire.write( 0x00 ); // the following bytes are commands
  Wire.write( 0xB0 + page ); // set page
  Wire.endTransmission();
}


//==============================================================
// clearSH1106
//   fills the screen with zeros
//==============================================================
void clearSH1106() {
  int x, p;
  const int n = 26;
  for ( p = 0; p <= 7; p++ )
    for ( x = 0; x <= 127; x++ ) {
      if ( x % n == 0 ) setupPageCol( p, x );
      Wire.write( 0 );
      if ( ( x % n == n - 1 ) || ( x == 127 ) ) Wire.endTransmission();
    }
}


//==============================================================
// initSH1106
//   initialises the SH1106 registers
//==============================================================
void initSH1106() {
  Wire.endTransmission();
  Wire.beginTransmission( addr );
  Wire.write( 0x00 ); // the following bytes are commands
  Wire.write( 0xAE ); // display off
  Wire.write( 0xD5 ); Wire.write( 0x80 ); // clock divider
  Wire.write( 0xA8 ); Wire.write( 0x3F ); // multiplex ratio ( height - 1 )
  Wire.write( 0xD3 ); Wire.write( 0x00 ); // no display offset
  Wire.write( 0x40 ); // start line address=0
  Wire.write( 0x33 ); // charge pump max
  Wire.write( 0x8D ); Wire.write( 0x14 ); // enable charge pump
  Wire.write( 0x20 ); Wire.write( 0x02 ); // memory adressing mode=horizontal ( only for 1306?? ) maybe 0x00
  Wire.write( 0xA1 ); // segment remapping mode
  Wire.write( 0xC8 ); // COM output scan direction
  Wire.write( 0xDA ); Wire.write( 0x12 );   // com pins hardware configuration
  Wire.write( 0x81 ); Wire.write( 0xFF ); // contrast control // could be 0x81
  Wire.write( 0xD9 ); Wire.write( 0xF1 ); // pre-charge period or 0x22
  Wire.write( 0xDB ); Wire.write( 0x40 ); // set vcomh deselect level or 0x20
  Wire.write( 0xA4 ); // output RAM to display
  Wire.write( 0xA6 ); // display mode A6=normal, A7=inverse
  Wire.write( 0x2E ); // stop scrolling
  Wire.write( 0xAF ); // display on
  Wire.endTransmission();

  clearSH1106();
}


//==============================================================
// drawBarSH1106
//   draws a single byte
//   a 'bar' is a byte on the screen - a col of 8 pix
//   assumes you've set up the page and col
//==============================================================
void drawBarSH1106( byte bar ) {
  Wire.beginTransmission( addr );
  Wire.write( 0x40 ); // the following bytes are data
  Wire.write( bar );
  Wire.endTransmission();
}


//==============================================================
// drawImageSH1106
//   at x,p*8
//   unpacks RLE and draws it
//   returns width of image
//==============================================================
uint8_t drawImageSH1106( uint8_t x, uint8_t p, const uint8_t *bitmap ) {
#define drawImageSH1106Bar {\
    if ( ( page >= 0 ) && ( page <= 7 ) && ( ax >= 0 ) && ( ax <= 127 ) ) {\
      n++;\
      if ( ( page != curpage ) || ( n > 25 ) ){\
        if ( curpage >= 0 ) \
          Wire.endTransmission();\
        setupPageCol( page, ax );\
        curpage = page;\
        n = 0;\
      }\
      Wire.write( bar );\
    }  \
    ax++;\
    if ( ax > x + width - 1 ) {\
      page++;\
      ax = x;\
    }\
  }

  uint8_t width, pages, page, ax, bar, curpage, n;

  width = pgm_read_byte( bitmap++ );
  pages = pgm_read_byte( bitmap++ );

  page = p;
  ax = x;
  curpage = -1;

  while ( page <= p + pages - 1 ) {
    uint8_t j = pgm_read_byte( bitmap++ );
    if ( j > 127 ) { // repeat next bar (j-128)-times
      bar = pgm_read_byte( bitmap++ );
      for ( uint8_t i = 128; i < j; ++i ) {
        drawImageSH1106Bar
      }
    } else { // draw next j bars
      for ( uint8_t i = 0; i < j; ++i ) {
        bar = pgm_read_byte( bitmap++ );
        drawImageSH1106Bar
      }
    }
  }
  Wire.endTransmission();

  return width;
}


//==============================================================
// drawCharSH1106
//   draws a char at x,page
//   only 8-bit or less fonts are allowed
//   returns width of char + 1 ( letter_gap )
//==============================================================
uint8_t drawCharSH1106( uint8_t c, uint8_t x, uint8_t page, const uint8_t *Font ) {
  uint8_t n, i, j, h, result, b, prevB;
  result = 0;
  prevB = 0;
  j  =  pgm_read_byte_near( Font ); // first char
  Font++;
  if ( c < j ) return 0;

  h  =  pgm_read_byte_near( Font ); // height in pages must be 1 or 2
  Font++;

  while ( c > j ) {
    b  =  pgm_read_byte_near( Font );
    Font++;
    if ( b == 0 ) return 0;
    Font += b * h;
    c--;
  }

  n  =  pgm_read_byte_near( Font );
  Font++;
  result = n + h; // letter_gap

  while ( h > 0 ) {
    setupPageCol( page, x );
    for ( i = 0; i < n; i++ ) {
      b  =  pgm_read_byte_near( Font );
      Font++;
      if ( boldSH1106 )
        Wire.write( b | prevB );
      else
        Wire.write( b );
      prevB = b;
    }

    if ( boldSH1106 ) {
      Wire.write( prevB );
      result++;
    }

    Wire.write( 0 );

    h--;
    page++;
    Wire.endTransmission();
  }
  return result;
}


//==============================================================
// drawStringSH1106
//   draws a string at x,page
//   returns width drawn
//==============================================================
uint8_t drawStringSH1106( const char *s, uint8_t x, uint8_t page, const uint8_t *Font ) {
  uint8_t start = x;
  if ( page <= 7 )
    while ( *s ) {
      x += drawCharSH1106( *s, x, page, Font );
      s++;
    }
  return x - start;
}


//==============================================================
// drawIntSH1106
//   draws an int at x,page
//   returns width drawn
//==============================================================
uint8_t drawIntSH1106( long i, uint8_t x, uint8_t page, const uint8_t *Font ) {
  uint8_t start = x;
  if ( i < 0 ) {
    i = -i;
    x += drawCharSH1106( '-', x, page, Font );
  }

  bool hasDigit = false;
  long n =  1000000000L;
  if ( i == 0 ) {
    x += drawCharSH1106( '0', x, page, Font );
  } else {
    while ( n > 0 ) {
      if ( ( i >= n ) or hasDigit ) {
        x += drawCharSH1106( '0' + ( i / n ), x, page, Font );
        hasDigit = true;
      }
      i %= n;
      n /= 10;
    }
  }
  return x - start;
}
