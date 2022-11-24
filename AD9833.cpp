// SPDX-License-Identifier: GPL-3.0-or-later

#include "AD9833.h"
#include <SPI.h>

//-----------------------------------------------------------------------------
// Constructor for the AD9833 object, define select pin
//-----------------------------------------------------------------------------
AD9833::AD9833( uint8_t fsync ) : _FSYNC( fsync ) {}


//-----------------------------------------------------------------------------
// reset
//   reset the AD9833
//-----------------------------------------------------------------------------
void AD9833::reset() {
    SPI.beginTransaction( SPISettings( 10000000, MSBFIRST, SPI_MODE3 ) );
    digitalWrite( _FSYNC, LOW );
    SPI.transfer16( wReset );
    digitalWrite( _FSYNC, HIGH );
    SPI.endTransaction();
}


//-----------------------------------------------------------------------------
// setFrequency
//    set the SG frequency and waveform regs
//-----------------------------------------------------------------------------
void AD9833::setFrequency( long frequency, uint16_t wave ) {
    long fl = long( frequency * ( 0x10000000L / 25000000.0 ) + 0.5 );
    SPI.beginTransaction( SPISettings( 10000000, MSBFIRST, SPI_MODE3 ) );
    digitalWrite( _FSYNC, LOW );
    SPI.transfer16( 0x2000 | wave );
    SPI.transfer16( uint16_t( fl & 0x3FFFL ) | 0x4000 );
    SPI.transfer16( uint16_t( ( fl & 0xFFFC000L ) >> 14 ) | 0x4000 );
    digitalWrite( _FSYNC, HIGH );
    SPI.endTransaction();
}


/******************************************************************************
    AD9833 register ( 16 bit )
    D15 D14 00: CONTROL ( 14 bits )
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
