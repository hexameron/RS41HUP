//
// Created by SQ5RWU on 2016-12-24.
//

#ifndef RS41HUP_CONFIG_H
#define RS41HUP_CONFIG_H

#ifdef USE_EXTERNAL_CONFIG
#include "config_external.h"
#else


//************GLOBAL Settings*****************
#define TRANSMIT_FREQUENCY  434417000
	// lowest frequency

//************MFSK Binary Settings************
#define CALLSIGN "VC0706"
	// put your SSDV callsign here, max. 6 characters uppercase

// TX Power
#define TX_POWER  4
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

// Delay between transmitted packets at 1000 Hz
#define TX_DELAY (50)

// 5 * 266 Hz = 1330 Hz tone spacing, for 5000 Hz Bandwidth
#define FSK_SHIFT 5

//***********Other Settings ******************
// Switch sonde ON/OFF via Button
#define ALLOW_DISABLE_BY_BUTTON 1


#endif

#endif //RS41HUP_CONFIG_H
