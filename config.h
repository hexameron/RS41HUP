//
// Created by SQ5RWU on 2016-12-24.
//

#ifndef RS41HUP_CONFIG_H
#define RS41HUP_CONFIG_H

#ifdef USE_EXTERNAL_CONFIG
#include "config_external.h"
#else


//************GLOBAL Settings*****************
#define TRANSMIT_FREQUENCY  434412000
	// Centre frequency

// Modulation Settings - Comment out a line below to disable a modulation.
#define USE_RTTY
#define USE_HORUS
#define USE_OLIVIA
#define USE_CONTESTIA

// TX Power
#define TX_POWER  3
// Power Levels measured at 434.650 MHz, using a Rigol DSA815, and a 10 kHz RBW
// Power measured by connecting a short (30cm) length of RG316 directly to the
// antenna/ground pads at the bottom of the RS41 PCB.
// 0 --> -1.9dBm
// 1 --> 1.3dBm
// 2 --> 3.6dBm
// 3 --> 7.0dBm
// 4 --> 10.0dBm - MAX for UK ISM airbourne continuous
// 5 --> 13.1dBm
// 6 --> 15.0dBm
// 7 --> 16.3dBm

// WARNING: RS41s have been observed to lose transmitter PLL lock at low temperature, even with
// stock insulation. This results in the transmitted signal drifting up the 70cm band with temperature.

// Delay between transmitted packets at 500 Hz
#define OFF_TIME 5
#define TX_DELAY  (500 * OFF_TIME)

//*************RTTY SETTINGS******************
#define CALLSIGN "MAGNU"
	// put your RTTY callsign here, max. 15 characters
#define RTTY_DEVIATION 0x3
	// RTTY shift = RTTY_DEVIATION x 267Hz
#define RTTY_7BIT 1
#define RTTY_USE_2_STOP_BITS   1
#define RTTY_PRE_START_BITS  32


//************MFSK Binary Settings************
// Binary Payload ID (0 though 255) - For your own flights, you will need to choose a payload ID,
// and set this value to that. 
// Refer to the payload ID list here: https://github.com/projecthorus/horusbinary/blob/master/payload_id_list.txt
#define BINARY_PAYLOAD_ID 23
	// Payload ID for use in Binary packets


// Ignored: Tx is always off between packets.
#define MFSK_CONTINUOUS 1


//***********Other Settings ******************
// Switch sonde ON/OFF via Button
// If this is a flight you might prevent sonde from powered off by button
#define ALLOW_DISABLE_BY_BUTTON 0


#endif

#endif //RS41HUP_CONFIG_H
