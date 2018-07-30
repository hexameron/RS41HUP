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
#define BAUD_RATE  100
	// RTTY & MFSK Baud rate

// Modulation Settings - Comment out a line below to enable/disable a modulation.
#define RTTY_ENABLED 1
#define MFSK_ENABLED 1

// TX Power
#define TX_POWER  4
// Power Levels measured at 434.650 MHz, using a Rigol DSA815, and a 10 kHz RBW
// Power measured by connecting a short (30cm) length of RG316 directly to the
// antenna/ground pads at the bottom of the RS41 PCB.
// 0 --> -1.9dBm
// 1 --> 1.3dBm
// 2 --> 3.6dBm
// 3 --> 7.0dBm
// 4 --> 10.0dBm - DEFAULT
// 5 --> 13.1dBm
// 6 --> 15.0dBm
// 7 --> 16.3dBm

// Delay *between* transmitted packets
#define TX_DELAY  1000

//*************RTTY SETTINGS******************
#define CALLSIGN "CHANGEME"
	// put your RTTY callsign here, max. 15 characters
#define RTTY_DEVIATION 0x3
	// RTTY shift = RTTY_DEVIATION x 270Hz
#define RTTY_7BIT 1
#define RTTY_USE_2_STOP_BITS   1


//************MFSK Binary Settings************
// Binary Payload ID (0 though 255) - For your own flights, you will need to choose a payload ID,
// and set this value to that. 
// Refer to the payload ID list here: https://github.com/projecthorus/horusbinary/blob/master/payload_id_list.txt
#define BINARY_PAYLOAD_ID 0
	// Payload ID for use in Binary packets


// If enabled, transmit incrementing tones in the 'idle' period between packets.
// This will only function if ONLY MFSK is enabled.
#define MFSK_CONTINUOUS 1


//***********Other Settings ******************
// Switch sonde ON/OFF via Button
// If this is a flight you might prevent sonde from powered off by button
#define ALLOW_DISABLE_BY_BUTTON 0


//************* APRS Settings *************************
// Note - APRS functionality currently disabled, and will eventually be removed.
//
#define APRS_CALLSIGN "N0CALL"
	// put your APRS callsign here, 6 characters. If your callsign is shorter add spaces
#define APRS_SSID 'B'
	// put your APRS SSID here
// 0 --> Your primary station usually fixed and message capable
// 1 --> generic additional station, digi, mobile, wx, etc.
// 2 --> generic additional station, digi, mobile, wx, etc.
// 3 --> generic additional station, digi, mobile, wx, etc.
// 4 --> generic additional station, digi, mobile, wx, etc.
// 5 --> Other network sources (Dstar, Iphones, Blackberry's etc)
// 6 --> Special activity, Satellite ops, camping or 6 meters, etc.
// 7 --> walkie talkies, HT's or other human portable
// 8 --> boats, sailboats, RV's or second main mobile
// 9 --> Primary Mobile (usually message capable)
// A --> internet, Igates, echolink, winlink, AVRS, APRN, etc.
// B --> balloons, aircraft, spacecraft, etc.
// C --> APRStt, DTMF, RFID, devices, one-way trackers*, etc.
// D --> Weather stations
// E --> Truckers or generally full time drivers
// F --> generic additional station, digi, mobile, wx, etc.

#define APRS_COMMENT " Hello from the sky!"
#define APRS_FREQUENCY  439.100f //Mhz middle frequency - Australian 70cm APRS Frequency

#endif

#endif //RS41HUP_CONFIG_H
