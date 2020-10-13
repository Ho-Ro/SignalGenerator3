/*
  MCP4x.cpp
  Interface for MCP41xxx digital pot
*/

#include "MCP4x.h"
#include <SPI.h>
#include <math.h>


MCP4x::MCP4x( uint8_t cs  ) : _cs( cs ) {}


void MCP4x::begin(void) {
  // setup SPI
  SPI.begin();
  // define MCP /CS line
  digitalWrite( _cs, HIGH );
  pinMode( _cs, OUTPUT );
}


void MCP4x::setPot( uint8_t value ) {
  // select device
  digitalWrite( _cs, LOW );

  // init SPI transfer
  SPI.beginTransaction( SPISettings( 10000000, MSBFIRST, SPI_MODE0 ) );
  SPI.transfer( 0x11);   // shift out command
  SPI.transfer( value ); // shift out data
  SPI.endTransaction();  // release SPI

  // deselect device
  digitalWrite( _cs,  HIGH );
}


void MCP4x::shutdown() {
  // select device
  digitalWrite( _cs, LOW );

  // init SPI transfer
  SPI.beginTransaction( SPISettings( 10000000, MSBFIRST, SPI_MODE0 ) );
  SPI.transfer( 0x21);   // shift out command
  SPI.transfer( 0 ); // shift out data
  SPI.endTransaction();  // release SPI

  // deselect device
  digitalWrite( _cs,  HIGH );
}
