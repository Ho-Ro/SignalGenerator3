// SPDX-License-Identifier: GPL-3.0-or-later
//******************************************
//  MCP4X.h
//    Interface for MCP41xxx digital pot
//
//******************************************

#pragma once

#include <Arduino.h>

class MCP4x {
    private:
        const uint8_t _cs;

    public:
        MCP4x( uint8_t cs );
        void begin( void );
        void setPot( uint8_t value );
        void shutdown();
};
