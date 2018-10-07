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
#include "f_rtty.h"
#include "init.h"
#include "config.h"
#include "radio.h"
#include "ublox.h"
#include "delay.h"
#include "util.h"
#include "mfsk.h"
#include "horus_l2.h"
#include "contestia.h"

// IO Pins Definitions. The state of these pins are initilised in init.c
#define GREEN  GPIO_Pin_7
	// Inverted
#define RED  GPIO_Pin_8
	// Non-Inverted (?)

// Transmit Modulation Switching
#define STARTUP 0
#define RTTY 1
#define FSK_4 2
#define CONTEST 3
#define PREAMBLE 4
volatile int current_mode = STARTUP;

// Telemetry Data to Transmit - used in RTTY & MFSK packet generation functions.
unsigned int send_count;        //frame counter
int voltage;
int8_t si4032_temperature;
GPSEntry gpsData;

char callsign[15] = {CALLSIGN};
char status[2] = {'N'};
uint16_t CRC_rtty = 0x12ab;  //checksum (dummy initial value)

#define MAX_RTTY 90
#define MAX_MFSK (4 * MAX_RTTY)
char buf_rtty[MAX_RTTY]; // Usually less than 80 chars
char buf_mfsk[MAX_MFSK]; // contestia buffer needs to be 4x longer than rtty string

// Volatile Variables, used within interrupts.
volatile int adc_bottom = 2000;
volatile char flaga = 0; // GPS Status Flags
volatile int led_enabled = 1; // Flag to disable LEDs at altitude.

volatile unsigned char pun = 0;
volatile unsigned int cun = 10;
volatile unsigned char tx_on = 0;
volatile unsigned int tx_on_delay;
volatile unsigned char tx_enable = 0;
rttyStates send_rtty_status = rttyZero;
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
int8_t    Temp; // Twos Complement Temp value.
uint8_t   BattVoltage; // 0 = 0v, 255 = 5.0V, linear steps in-between.
uint16_t  Checksum; // CRC16-CCITT Checksum.
};  //  __attribute__ ((packed)); // Doesn't work?
#pragma pack(pop)


// Function Definitions
void collect_telemetry_data();
void send_rtty_packet();
void send_mfsk_packet();
void send_contest_packet();
uint16_t gps_CRC16_checksum (char *string);

