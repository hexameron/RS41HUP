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

//
// Modulation Settings - Comment out a line below to disable a modulation.
//
#define USE_HORUS
// 4fsk Horus Binary - 2 second payload
//
#define USE_LDPC
// 100 Hz 4fsk Short - 2 second payload
// 	16 bytes; may get longer or slower

//************MFSK Binary Settings************
// Binary Payload ID (0 though 32) - For your own flights, you will need to choose a payload ID,
// and set this value to that. 
// Refer to the payload ID list here: https://github.com/projecthorus/horusbinary/blob/master/payload_id_list.txt
#define BINARY_PAYLOAD_ID  0
#define LDPC_PAYLOAD_ID    4

//******************* TX Power ***************
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


//***********Other Settings ******************

// Delay between transmitted packets at 100 Hz
#define TX_DELAY  (10)

// Send some extra symbols at the start of the packet
#define PREAMBLE_LENGTH 20

// Switch sonde ON/OFF via Button
#define ALLOW_DISABLE_BY_BUTTON 1

#endif

#endif //RS41HUP_CONFIG_H
