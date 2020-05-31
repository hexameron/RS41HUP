// STM32F100 and SI4032 RTTY transmitter
// released under GPL v.2 by anonymous developer
// enjoy and have a nice day
// ver 1.5a
#include <stm32f10x_gpio.h>
#include <stm32f10x_tim.h>
#include <stm32f10x_spi.h>
#include <stm32f10x_tim.h>
#include <stm32f10x_usart.h>
#include <stm32f10x_adc.h>
#include <stm32f10x_rcc.h>
#include "stdlib.h"
#include <stdio.h>
#include <string.h>
#include <misc.h>
#include "init.h"
#include "config.h"
#include "radio.h"
#include "ublox.h"
#include "delay.h"
#include "util.h"
#include "horus_l2.h"

// IO Pins Definitions. The state of these pins are initilised in init.c
#define GREEN  GPIO_Pin_7
#define RED  GPIO_Pin_8
//	both inverted: active low

// Transmit Modulation Switching
#define STARTUP 0
#define SEND_LDPC 1
#define DO_LDPC 2
#define SEND_HORUS 3
#define DO_HORUS 4
#define SEND_NEMO 5

volatile int current_mode = STARTUP;

// Telemetry Data to Transmit - used in RTTY & MFSK packet generation functions.
unsigned int send_count;        //frame counter
int voltage;
int8_t si4032_temperature;
GPSEntry gpsData;

#define MAX_MFSK (60)
char buff_mfsk[MAX_MFSK]; // 16 bytes * 3  + 4 = 52

// Volatile Variables, used within interrupts.
volatile int adc_bottom = 2000;
volatile int led_enabled = 3; // off=0/red=1/green=2/red+green=orange

volatile unsigned char pun = 0;
volatile unsigned int cun = 100; // 1 seconds of green LED at startup
volatile unsigned char tx_on = 0;
volatile unsigned int tx_on_delay;
volatile unsigned char tx_enable = 0;
volatile char *tx_buffer;
volatile uint16_t current_mfsk_byte = 0;
volatile uint16_t packet_length = 0;
volatile uint16_t button_pressed = 0;
volatile uint8_t disable_armed = 0;

// Binary Packet Format
// Note that we need to pack this to 1-byte alignment, hence the #pragma flags below
// Refer: https://gcc.gnu.org/onlinedocs/gcc-4.4.4/gcc/Structure_002dPacking-Pragmas.html
#pragma pack(push,1) 
struct TBinaryPacket
{
uint8_t   PayloadID;
uint16_t  Counter;
uint8_t   Hours;
uint8_t   Minutes;
uint8_t   Seconds;
int32_t   Latitude;
int32_t   Longitude;
uint16_t  Altitude;
uint8_t   Speed; // Speed in Knots (1-255 knots)
uint8_t   Sats;
int8_t    Temp; // -64 to +64; SI4032 internal chip temp.
uint8_t   BattVoltage; // 0 = 0v, 255 = 5.0V, linear steps in-between.
uint16_t  Checksum; // CRC16-CCITT Checksum.
};
#pragma pack(pop)

/* Short binary packet */
#pragma pack(push,1)
struct SBinaryPacket {
// 4 byte preamble for high error rates ("0x96696996")
// All data 8 bit Gray coded (before Checksum)
//	- to improve soft bit prediction
uint8_t   PayloadID;	// Legacy list
uint8_t   Counter;	// 8 bit counter
uint16_t  BiSeconds;	// Time of day / 2
uint8_t   Latitude[3];	// (int)(float * 1.0e7) / (1<<8)
uint8_t   Longitude[3];	// ( better than 10m precision )
uint16_t  Altitude;	// 0 - 65 km
uint8_t   Voltage;	// scaled 5.0v in 255 range
uint8_t   User;		// Temp / Sats
	// Temperature	6 bits MSB => (+30 to -32)
	// Satellites	2 bits LSB => 0,4,8,12 is good enough
uint16_t  Checksum;	// CRC16-CCITT Checksum.
};	// 16 data bytes, for (128,384) LDPC FEC
	// => 52 bytes at 100Hz 4fsk => 2 seconds
#pragma pack(pop)

