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

// IO Pins Definitions. Active low, initilised in init.c
#define GREEN  GPIO_Pin_7
#define RED  GPIO_Pin_8

// Transmit Modulation Switching
#define STARTUP 0
#define HORUS 1
#define SEND4FSK 2
volatile int current_mode = STARTUP;

// Telemetry Data to Transmit - used in packet generation functions.
unsigned int send_count;	// frame counter
int voltage;			// battery
int8_t si4032_temperature;	// radio internal temp
int16_t lateral, orthogonal;	// ground speed foward and sideways
uint32_t distance = 0;		// ground speed * time
int32_t last_lat = 0, last_lon = 0, last_alt = 0;	// persistant data if the GPS loses lock
GPSEntry gpsData;		// Packet from Ublox GPS

#define MAX_MFSK 70
char buf_mfsk[MAX_MFSK]; // 32 bytes * 23/12 + 2 + 4 = 68

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
uint16_t  PayloadID; // 16 bit for 32 byte payload
uint16_t  Counter;
uint8_t   Hours;
uint8_t   Minutes;
uint8_t   Seconds;
int32_t   Latitude;
int32_t   Longitude;
uint16_t  Altitude;
uint8_t   Speed; // GPS Speed instantaneous  (0-255 kph)
uint8_t   Sats;
int8_t    Temp; // -64 to +64; SI4032 internal chip temp.
uint8_t   BattVoltage; // 0 = 0v, 255 = 5.0V, linear steps in-between.
int16_t   Vertical;	// ascent rate(+/- 100.00 m/s)
uint16_t  Lateral;	// average speed (0-100.00 m/s))
uint16_t  Orthogonal;	// orthogonal speed (0-100.00 m/s)
uint16_t  Distance;	// distance (0-640.00km)
int8_t    Heading;	// GPS heading (+/- 1.00)
uint16_t  Checksum; // CRC16-CCITT Checksum.
}  __attribute__ ((packed));
#pragma pack(pop)


// Function Definitions
void collect_telemetry_data();
void send_mfsk_packet();

