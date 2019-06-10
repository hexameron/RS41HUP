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
#include "mfsk.h"
#include "horus_l2.h"

// IO Pins Definitions. The state of these pins are initilised in init.c
#define GREEN  GPIO_Pin_7
	// Active low
#define RED  GPIO_Pin_8
	// Active low

#define PREAMBLE 0
#define SEND4FSK 1
#define IDLE4FSK 2

// Telemetry Data to Transmit - used in RTTY & MFSK packet generation functions.
unsigned int send_count;        //frame counter
int voltage;
int8_t si4032_temperature;
GPSEntry gpsData;

#define NO_SSDV_DELAY 1000		/* Half as long as a real packet */
#define SSDV_SIZE  255			/* SSDV packet checksum length  */
#define MAX_MFSK (2 * SSDV_SIZE)	/* (23,12) FEC  & preambles     */
char buf_mfsk[MAX_MFSK];		/* FEC appended and interleaved */
uint8_t buf_ssdv[MAX_MFSK];		/*  Temporary packet storage */

// Volatile Variables, used within interrupts.
volatile int adc_bottom = 2000;
volatile int led_enabled = 3; // off=0/red=1/green=2/red+green=orange

volatile unsigned char pun = 0;
volatile unsigned int cun = 1000; // 2 seconds of green LED at startup
volatile unsigned char tx_on = 0;
volatile unsigned int tx_on_delay;
volatile unsigned char tx_enable = 0;
volatile char *tx_buffer;
volatile uint16_t current_mfsk_byte = 0;
volatile uint16_t packet_length = 0;
volatile uint16_t button_pressed = 0;
volatile uint8_t disable_armed = 0;
static int tx_mode = PREAMBLE;
static int framecount = 0;

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
};  //  __attribute__ ((packed)); // Doesn't work?
#pragma pack(pop)

struct TBinaryPacket BinaryPacket;
#define LONG_BINARY 32	/* extended format, undefined */

// Function Definitions
void collect_telemetry_data();
void send_mfsk_packet();
uint16_t gps_CRC16_checksum (char *string);
void send_ssdv_packet();
uint8_t fill_image_packet(uint8_t *pkt);
