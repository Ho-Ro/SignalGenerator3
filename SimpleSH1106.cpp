// SPDX-License-Identifier: GPL-3.0-or-later
/***************************************************
    Arduino OLED graphics library for the SH1106
***************************************************/

#include <avr/pgmspace.h>
#include <Wire.h>

#include "SimpleSH1106.h"


SimpleSH1106::SimpleSH1106( uint8_t i2c ) : addrI2C( i2c ) {
}

//==============================================================
// init
//   initialises the SH1106 registers
//   typical I2C speed values are:
//      TWBR = 1; // freq=888kHz period=1.125uS
//      TWBR = 2; // freq=800kHz period=1.250uS
//      TWBR = 3; // freq=727kHz period=1.375uS
//      TWBR = 4; // freq=666kHz period=1.500uS
//      TWBR = 5; // freq=615kHz period=1.625uS
//      TWBR = 10; // freq=444kHz period=2.250uS
//      TWBR = 20; // freq=285kHz period=3.500uS
//      TWBR = 30; // freq=210kHz period=4.750uS
//      TWBR = 40; // freq=166kHz period=6.000uS
//      TWBR = 50; // freq=137kHz period=7.250uS
//
//==============================================================
void SimpleSH1106::init() {
    Wire.begin(); // join i2c bus as master
    TWBR = 4; // freq=666kHz period=1.500uS
    Wire.endTransmission();
    Wire.beginTransmission( addrI2C );
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
    clearScreen();
}


//==============================================================
// clearScreen
//   fills the screen with zeros
//==============================================================
void SimpleSH1106::clearScreen() {
    uint8_t col, page;
    const int n = 26;
    for ( page = 0; page < PAGES; page++ )
        for ( col = 0; col < COLUMNS; col++ ) {
            if ( col % n == 0 ) setupColPage( col, page );
            Wire.write( 0 );
            if ( ( col % n == n - 1 ) || ( col == COLUMNS - 1 ) ) Wire.endTransmission();
        }
}


//-----------------------------------------------------------------------------
// drawBox with text as normal C string as "text"
//   draws a box around the screen with "text" written at top-left
//-----------------------------------------------------------------------------
void SimpleSH1106::drawBox( const char* text ) {
    drawImage( 0, 0, imgBoxTop );
    for ( uint8_t page = 1; page < PAGES - 1; ++page )
        drawImage( 0, page, imgBoxMid );
    drawImage( 0, PAGES - 1, imgBoxBot );
    drawChar( ' ',  6, 0, smallFont );
    drawString( text,  7, 0, smallFont );
}


//-----------------------------------------------------------------------------
// drawBox with text from Flash via F("text")
//   draws a box around the screen with "text" written at top-left
//-----------------------------------------------------------------------------
void SimpleSH1106::drawBox( const __FlashStringHelper* text ) {
    drawImage( 0, 0, imgBoxTop );
    for ( int page = 1; page < PAGES - 1; ++page )
        drawImage( 0, page, imgBoxMid );
    drawImage( 0, PAGES - 1, imgBoxBot );
    drawChar( ' ',  6, 0, smallFont );
    drawString( text,  7, 0, smallFont );
}


//==============================================================
// setupColPage
//   sets up the column and row
//   then gets ready to send one or more bytes
//   should be followed by endTransmission
//==============================================================
void SimpleSH1106::setupColPage( uint8_t col, uint8_t page ) {
    col += colOffset;
    Wire.beginTransmission( addrI2C );
    Wire.write( 0x00 ); // the following bytes are commands
    Wire.write( 0xB0 + page ); // set page
    Wire.write( 0x00 + ( col & 15 ) ); // lower columns address
    Wire.write( 0x10 + ( col >> 4 ) ); // upper columns address
    Wire.endTransmission();

    Wire.beginTransmission( addrI2C );
    Wire.write( 0x40 ); // the following bytes are data
}


//==============================================================
// setupCol
//   sets up the column
//==============================================================
void SimpleSH1106::setupCol( uint8_t col ) {
    col += colOffset;
    Wire.beginTransmission( addrI2C );
    Wire.write( 0x00 ); // the following bytes are commands
    Wire.write( 0x00 + ( col & 15 ) ); // lower columns address
    Wire.write( 0x10 + ( col >> 4 ) ); // upper columns address
    Wire.endTransmission();
}


//==============================================================
// setupPage
//   sets up the page
//==============================================================
void SimpleSH1106::setupPage( uint8_t page ) {
    Wire.beginTransmission( addrI2C );
    Wire.write( 0x00 ); // the following bytes are commands
    Wire.write( 0xB0 + page ); // set page
    Wire.endTransmission();
}


//==============================================================
// drawBar
//   draws a single bar
//   a 'bar' is a byte on the screen - a col of 8 pix, LSB top
//   assumes you've set up the page and col
//==============================================================
void SimpleSH1106::drawBar( uint8_t bar ) {
    Wire.beginTransmission( addrI2C );
    Wire.write( 0x40 ); // the following bytes are data
    Wire.write( bar );
    Wire.endTransmission();
}


//==============================================================
// drawBar
//   draws a bar at col, page
//   sets up the row and column then sends the byte
//   quite slow
//==============================================================
void SimpleSH1106::drawBar( uint8_t col, uint8_t page, uint8_t bar ) {
    setupColPage( col, page );
    Wire.write( bar );
    Wire.endTransmission();
}


//==============================================================
// drawImage
//   at x, p*8
//   unpacks RLE and draws it
//   returns width of image
//==============================================================
uint8_t SimpleSH1106::drawImage( uint8_t col, uint8_t page, const uint8_t *bitmap ) {
    static uint8_t n = 0;

#define drawNextBar( bar ) {\
        if ( ( ap < PAGES ) && ( ac < COLUMNS ) ) {\
            n++;\
            if ( ( ap != curpage ) || ( n > 25 ) ){\
                if ( curpage < PAGES ) \
                    Wire.endTransmission();\
                setupColPage( ac, ap );\
                curpage = ap;\
                n = 0;\
            }\
            Wire.write( bar );\
        }  \
        ac++;\
        if ( ac > col + width - 1 ) {\
            ap++;\
            ac = col;\
        }\
    }

    uint8_t width = pgm_read_byte( bitmap++ );
    uint8_t pages = pgm_read_byte( bitmap++ );

    uint8_t ap = page;
    uint8_t ac = col;
    uint8_t curpage = 255;
    uint8_t bar;

    while ( ap <= page + pages - 1 ) {
        uint8_t j = pgm_read_byte( bitmap++ );
        if ( j > 127 ) { // repeat next bar (j-128)-times
            bar = pgm_read_byte( bitmap++ );
            for ( uint8_t i = 128; i < j; ++i ) {
                drawNextBar( bar );
            }
        } else { // draw next j bars
            for ( uint8_t i = 0; i < j; ++i ) {
                bar = pgm_read_byte( bitmap++ );
                drawNextBar( bar );
            }
        }
    }
    Wire.endTransmission();

    return width;
}


//==============================================================
// drawChar
//   draws a char at col, page
//   only 8-bit or less fonts are allowed
//   returns width of char + 1 ( letter_gap )
//==============================================================
uint8_t SimpleSH1106::drawChar( uint8_t c, uint8_t col, uint8_t page, const uint8_t *font ) {
    uint8_t n, i, j, h, result, b, prevB;
    result = 0;
    prevB = 0;
    j  =  pgm_read_byte_near( font ); // first char
    font++;
    if ( c < j ) return 0;

    h  =  pgm_read_byte_near( font ); // height in pages must be 1 or 2
    font++;

    while ( c > j ) {
        b  =  pgm_read_byte_near( font );
        font++;
        if ( b == 0 ) return 0;
        font += b * h;
        c--;
    }

    n  =  pgm_read_byte_near( font );
    font++;
    result = n + h; // letter_gap

    while ( h > 0 ) {
        setupColPage( col, page );
        for ( i = 0; i < n; i++ ) {
            b  =  pgm_read_byte_near( font );
            font++;
            if ( bold )
                Wire.write( b | prevB );
            else
                Wire.write( b );
            prevB = b;
        }

        if ( bold ) {
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
// drawString
//   draws a string at col, page
//   returns width drawn
//==============================================================
uint8_t SimpleSH1106::drawString( const char *s, uint8_t col, uint8_t page, const uint8_t *font ) {
    uint8_t start = col;
    if ( page <= 7 )
        while ( *s ) {
            col += drawChar( *s, col, page, font );
            s++;
        }
    return col - start;
}


//==============================================================
// drawString (from Flash)
//   draws a string at col, page
//   returns width drawn
//==============================================================
uint8_t SimpleSH1106::drawString( const __FlashStringHelper *f, uint8_t col, uint8_t page, const uint8_t *font ) {
    uint8_t start = col;
    PGM_P p = reinterpret_cast<PGM_P>( f );
    char c;
    if ( page <= 7 )
        while ( ( c = pgm_read_byte( p++ ) ) )
            col += drawChar( c, col, page, font );
    return col - start;
}


//==============================================================
// drawInt
//   draws an int at col, page
//   returns width drawn
//==============================================================
uint8_t SimpleSH1106::drawInt( long i, uint8_t col, uint8_t page, const uint8_t *font ) {
    uint8_t start = col;
    if ( i < 0 ) {
        i = -i;
        col += drawChar( '-', col, page, font );
    }

    bool hasDigit = false;
    long n =  1000000000L;
    if ( i == 0 ) {
        col += drawChar( '0', col, page, font );
    } else {
        while ( n > 0 ) {
            if ( ( i >= n ) or hasDigit ) {
                col += drawChar( '0' + ( i / n ), col, page, font );
                hasDigit = true;
            }
            i %= n;
            n /= 10;
        }
    }
    return col - start;
}



//==============================================================
// font definitons
//==============================================================
//
const uint8_t SimpleSH1106::smallFont[] = {
    ' ', // first char
    1,   // height in pages
    4, 0x00, 0x00, 0x00, 0x00, // <space>
    1, 0x5F,  // !
    3, 0x03, 0x00, 0x03, // "
    5, 0x14, 0x7F, 0x14, 0x7F, 0x14, // #
    5, 0x24, 0x4A, 0xFF, 0x52, 0x24, // $
    6, 0x46, 0x26, 0x10, 0x08, 0x64, 0x62, // %
    5, 0x3A, 0x45, 0x4A, 0x30, 0x48, // &
    1, 0x03,  // '
    2, 0x7E, 0x81, // (
    2, 0x81, 0x7E, // )
    2, 0x03, 0x03, // *
    3, 0x10, 0x38, 0x10, // +
    1, 0xC0,  // ,
    2, 0x10, 0x10, // -
    1, 0x40,  // .
    2, 0x78, 0x0F, // /
    4, 0x3E, 0x41, 0x41, 0x3E, // 0
    2, 0x02, 0x7F, // 1
    4, 0x62, 0x51, 0x49, 0x46, // 2
    4, 0x22, 0x41, 0x49, 0x36, // 3
    4, 0x18, 0x16, 0x7F, 0x10, // 4
    4, 0x2F, 0x45, 0x45, 0x39, // 5
    4, 0x3E, 0x49, 0x49, 0x32, // 6
    4, 0x01, 0x71, 0x0D, 0x03, // 7
    4, 0x36, 0x49, 0x49, 0x36, // 8
    4, 0x26, 0x49, 0x49, 0x3E, // 9
    1, 0x48,  // :
    1, 0xC8,  // ;
    3, 0x10, 0x28, 0x44, // <
    3, 0x28, 0x28, 0x28, // =
    3, 0x44, 0x28, 0x10, // >
    4, 0x02, 0x51, 0x09, 0x06, // ?
    8, 0x3C, 0x42, 0x99, 0xA5, 0x9D, 0xA1, 0x22, 0x1C, // @
    6, 0x60, 0x1C, 0x13, 0x13, 0x1C, 0x60, // A
    6, 0x7F, 0x49, 0x49, 0x49, 0x49, 0x36, // B
    6, 0x3E, 0x41, 0x41, 0x41, 0x41, 0x22, // C
    6, 0x7F, 0x41, 0x41, 0x41, 0x41, 0x3E, // D
    5, 0x7F, 0x49, 0x49, 0x49, 0x41, // E
    5, 0x7F, 0x09, 0x09, 0x09, 0x01, // F
    6, 0x3E, 0x41, 0x41, 0x49, 0x29, 0x7A, // G
    6, 0x7F, 0x08, 0x08, 0x08, 0x08, 0x7F, // H
    1, 0x7F,  // I
    4, 0x30, 0x40, 0x40, 0x3F, // J
    5, 0x7F, 0x08, 0x14, 0x22, 0x41, // K
    4, 0x7F, 0x40, 0x40, 0x40, // L
    8, 0x7F, 0x03, 0x0C, 0x30, 0x30, 0x0C, 0x03, 0x7F, // M
    6, 0x7F, 0x03, 0x0C, 0x30, 0x40, 0x7F, // N
    6, 0x3E, 0x41, 0x41, 0x41, 0x41, 0x3E, // O
    6, 0x7F, 0x09, 0x09, 0x09, 0x09, 0x06, // P
    6, 0x3E, 0x41, 0x41, 0x51, 0x61, 0xBE, // Q
    6, 0x7F, 0x09, 0x09, 0x09, 0x09, 0x76, // R
    5, 0x26, 0x49, 0x49, 0x49, 0x32, // S
    5, 0x01, 0x01, 0x7F, 0x01, 0x01, // T
    5, 0x3F, 0x40, 0x40, 0x40, 0x3F, // U
    5, 0x03, 0x1C, 0x60, 0x1C, 0x03, // V
    7, 0x03, 0x1C, 0x60, 0x18, 0x60, 0x1C, 0x03, // W
    5, 0x63, 0x14, 0x08, 0x14, 0x63, // X
    5, 0x03, 0x04, 0x78, 0x04, 0x03, // Y
    5, 0x61, 0x51, 0x49, 0x45, 0x43, // Z
    2, 0xFF, 0x81, // [
    2, 0x1E, 0x70,
    2, 0x81, 0xFF, // ]
    3, 0x02, 0x01, 0x02, // ^
    4, 0x00, 0x00, 0x00, 0x00, // _
    2, 0x01, 0x02, // `
    4, 0x20, 0x54, 0x54, 0x78, // a
    4, 0x7F, 0x44, 0x44, 0x38, // b
    4, 0x38, 0x44, 0x44, 0x28, // c
    4, 0x38, 0x44, 0x44, 0x7F, // d
    4, 0x38, 0x54, 0x54, 0x18, // e
    2, 0x7E, 0x09, // f
    4, 0x18, 0xA4, 0xA4, 0x7C, // g
    4, 0x7F, 0x08, 0x04, 0x78, // h
    1, 0x7D,  // i
    1, 0xFD,  // j
    4, 0x7F, 0x18, 0x24, 0x40, // k
    1, 0x7F,  // l
    7, 0x7C, 0x04, 0x04, 0x7C, 0x04, 0x04, 0x78, // m
    4, 0x7C, 0x04, 0x04, 0x78, // n
    5, 0x38, 0x44, 0x44, 0x44, 0x38, // o
    4, 0xFC, 0x24, 0x24, 0x18, // p
    4, 0x18, 0x24, 0x24, 0xFC, // q
    2, 0x7C, 0x04, // r
    4, 0x48, 0x54, 0x54, 0x24, // s
    2, 0x3E, 0x44, // t
    4, 0x3C, 0x40, 0x40, 0x7C, // u
    4, 0x1C, 0x60, 0x60, 0x1C, // v
    5, 0x1C, 0x60, 0x18, 0x60, 0x1C, // w
    3, 0x6C, 0x10, 0x6C, // x
    3, 0x9C, 0xA0, 0x7C, // y
    3, 0x64, 0x54, 0x4C, // z
    2, 0x91, 0x6E, // {
    1, 0xFE,  // |
    2, 0x6E, 0x91, // }
    4, 0x04, 0x02, 0x04, 0x02, // ~
    1, 0xFE,  // 
    0
};


#if 0
const uint8_t SimpleSH1106::smallDigitsFont[] = {
    '+', //first char
    1,   // height in pages
    3, 0x10, 0x38, 0x10,       // +
    1, 0xC0,                   // ,
    2, 0x10, 0x10,             // -
    1, 0x40,                   // .
    2, 0x78, 0x0F,             // /
    4, 0x3E, 0x41, 0x41, 0x3E, // 0
    2, 0x02, 0x7F,             // 1
    4, 0x62, 0x51, 0x49, 0x46, // 2
    4, 0x22, 0x41, 0x49, 0x36, // 3
    4, 0x18, 0x16, 0x7F, 0x10, // 4
    4, 0x2F, 0x45, 0x45, 0x39, // 5
    4, 0x3E, 0x49, 0x49, 0x32, // 6
    4, 0x01, 0x71, 0x0D, 0x03, // 7
    4, 0x36, 0x49, 0x49, 0x36, // 8
    4, 0x26, 0x49, 0x49, 0x3E, // 9
    0
};
#endif


const uint8_t SimpleSH1106::largeDigitsFont[] = {
    '+', // first char
    2,   // height in pages
    12, 0x00, 0x00, 0x00, 0x00, 0x00, 0xF0, 0xF0, 0x00, 0x00, 0x00, 0x00, 0x00, 0x06, 0x06, 0x06, 0x06, 0x06, 0xFF, 0xFF, 0x06, 0x06, 0x06, 0x06, 0x06, // +
    4,  0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0xC0, 0x40, // ,
    6,  0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x06, 0x06, 0x06, 0x06, 0x06, 0x06, // -
    2,  0x00, 0x00, 0xC0, 0xC0, // .
    6,  0x00, 0x00, 0xC0, 0xFC, 0xFE, 0x0E, 0xE0, 0xFC, 0x7F, 0x07, 0x00, 0x00, // /
    10, 0xF0, 0xFC, 0x1E, 0x0E, 0x06, 0x06, 0x0E, 0x1E, 0xFC, 0xF0, 0x1F, 0x7F, 0xF0, 0xE0, 0xC0, 0xC0, 0xE0, 0xF0, 0x7F, 0x1F, // 0
    5,  0x70, 0x38, 0x1C, 0xFE, 0xFE, 0x00, 0x00, 0x00, 0xFF, 0xFF, // 1
    10, 0x18, 0x1C, 0x0E, 0x06, 0x06, 0x06, 0x0E, 0x8E, 0xFC, 0xF8, 0xC0, 0xE0, 0xF0, 0xD8, 0xDC, 0xCE, 0xC7, 0xC3, 0xC1, 0xC0, // 2
    10, 0x18, 0x1C, 0x0E, 0x06, 0x86, 0x86, 0xCE, 0xFC, 0x78, 0x00, 0x30, 0x70, 0xE0, 0xC0, 0xC1, 0xC1, 0xC1, 0xE3, 0x7F, 0x3E, // 3
    10, 0x00, 0x00, 0xC0, 0xE0, 0x70, 0x38, 0xFC, 0xFE, 0x00, 0x00, 0x0E, 0x0F, 0x0F, 0x0C, 0x0C, 0x0C, 0xFF, 0xFF, 0x0C, 0x0C, // 4
    10, 0xC0, 0xFE, 0xFE, 0xCE, 0xC6, 0xC6, 0xC6, 0xC6, 0x86, 0x00, 0x31, 0x71, 0xE1, 0xC0, 0xC0, 0xC0, 0xC0, 0xF1, 0x7F, 0x3F, // 5
    10, 0xF0, 0xF8, 0xBC, 0xCE, 0xC6, 0xC6, 0xC6, 0xCE, 0x9C, 0x18, 0x1F, 0x7F, 0x73, 0xE1, 0xC0, 0xC0, 0xC0, 0xF1, 0x7F, 0x3F, // 6
    10, 0x06, 0x06, 0x06, 0x06, 0x86, 0xC6, 0xF6, 0x3E, 0x1E, 0x06, 0x00, 0x00, 0xF0, 0xFE, 0x3F, 0x03, 0x00, 0x00, 0x00, 0x00, // 7
    10, 0x00, 0x78, 0xFC, 0xCE, 0x86, 0x86, 0xCE, 0xFC, 0x78, 0x00, 0x3E, 0x7F, 0xE3, 0xC1, 0xC1, 0xC1, 0xC1, 0xE3, 0x7E, 0x3E, // 8
    10, 0xF8, 0xFC, 0x1E, 0x0E, 0x06, 0x06, 0x06, 0x9C, 0xFC, 0xF0, 0x31, 0x73, 0xE7, 0xC6, 0xC6, 0xC6, 0xE7, 0x7B, 0x3F, 0x1F, // 9
    0
};


const uint8_t SimpleSH1106::imgSmiley[] = {
    21, // width
    3,  // pages
    4, 0, 192, 48, 8, 130, 4, 130, 130, 133, 1, 130, 130, 130, 4, 7, 8, 48, 192, 0, 31, 96, 128, 130, 0, 1, 67, 130, 132, 1, 3, 131, 0,
    1, 3, 130, 132, 1, 67, 130, 0, 3, 128, 96, 31, 130, 0, 2, 1, 2, 130, 4, 130, 8, 133, 17, 130, 8, 130, 4, 2, 2, 1, 130, 0
};


const uint8_t SimpleSH1106::imgBoxTop[] = {
    128, // width
    1,   // pages
    1, 248,
    128 + 126, 8,
    1, 248,
};


const uint8_t SimpleSH1106::imgBoxMid[] = {
    128, // width
    1,   // pages
    1, 255,
    128 + 126, 0,
    1, 255,
};


const uint8_t SimpleSH1106::imgBoxBot[] = {
    128, // width
    1,   // pages
    1, 255,
    128 + 126, 128,
    1, 255,
};
