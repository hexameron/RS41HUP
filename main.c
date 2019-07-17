// STM32F100 and SI4032 RTTY transmitter
// released under GPL v.2 by anonymous developer
// enjoy and have a nice day
// ver 1.5a

#include "main.h"

/* data or preamble only, short or long gap */
void start_sending(int mode, int delay) {
	tx_mode = mode;
	tx_on_delay = delay;	// leave a gap before next packet
	radio_enable_tx();	// Radio power on
	tx_on = 1;		// From here the timer interrupt handles things.
}

/* Step through a byte, and return 4FSK symbols.*/
static inline int get_mfsk() {
	static uint8_t deminibble = 0;
	uint8_t c;

	if (deminibble == 8){
		deminibble = 0;
		current_mfsk_byte++;
		if ( current_mfsk_byte >= packet_length )
			return -1;
	}

	// Get the current symbol (2-bits MSB)
	c = tx_buffer[current_mfsk_byte];
	deminibble += 2;
	return (0x3) & (c >> (8 - deminibble));
}

static inline void stop_sending() {
	current_mfsk_byte = 0;  // reset next sentence to start
	tx_on = 0;		// not sending data or preamble
	tx_enable = 0;		// allow next sentence after idle period
	radio_disable_tx();	// no transmit powersave
}
//
// Symbol Timing Interrupt
// In here symbol transmission occurs.
//

void TIM2_IRQHandler(void) {
	static int mfsk_symbol = 0;
	static int clockcount = 0;
	static int preamble_byte = 20;

	if ( TIM_GetITStatus(TIM2, TIM_IT_Update) != RESET ) {
		TIM_ClearITPendingBit(TIM2, TIM_IT_Update);

		/* divide by 10 for preamble and button press */
		if (++clockcount > 9)
			clockcount = 0;

		// Trusting Poweroff to an ADC button is a BAD IDEA unless testing
		if ( ALLOW_DISABLE_BY_BUTTON && ! clockcount) {
			if ( ADCVal[1] > adc_bottom ) {
				button_pressed++;
				GPIO_ResetBits(GPIOB, RED); // warn on button press
				if ( button_pressed > (500 / 3) ) {
					disable_armed = 1; // start button held down
					GPIO_ResetBits(GPIOB, GREEN);
				}
			} else {
				if ( disable_armed ) {
					// cut battery power when button released !
					GPIO_SetBits(GPIOA, GPIO_Pin_12);
				}
				button_pressed = 0; // button released early
			}
			if ( button_pressed == 0 ) {
				adc_bottom = ADCVal[1] * 1.1; // dynamical reference for power down level
			}
		}

		if ( tx_on ) {
			if ( tx_mode == SEND4FSK ) {
				mfsk_symbol = get_mfsk();
				if ( mfsk_symbol < 0 ) {
					// Reached the end of the packet.
					stop_sending();
				} else {
					radio_rw_register(0x73, (uint8_t)(FSK_SHIFT * mfsk_symbol), 1);
				}
			} else {
				// Preamble for horus_demod to lock on:
				// Transmit continuous MFSK symbols at 100 baud.
				if ( !clockcount) {
					mfsk_symbol = (mfsk_symbol + 1) & 0x03;
					radio_rw_register(0x73, (uint8_t)(FSK_SHIFT * mfsk_symbol), 1);
					if ( !preamble_byte-- ) {
						preamble_byte = 12;
						// switch mode
						if (tx_mode == IDLE4FSK)
							stop_sending();
						else
							tx_mode = SEND4FSK;
					}
				}
			}
		} else {
			// TX is off

			// tx_on_delay starts at the end of the Transmission above, and counts down
			// at the interrupt rate. When it hits zero, we set tx_enable to 1, which allows
			// the main loop to continue.
			if ( !tx_on_delay-- )
				tx_enable = 1;
		}

		// LED Blinking Logic
		if ( --cun == 0 ) {
			if ( pun ) {
				// Clear LEDs.
				GPIO_SetBits(GPIOB, GREEN);
				GPIO_SetBits(GPIOB, RED);
				cun = 4000; // 4s off
				pun = 0;
			} else {
				// If we have GPS lock, set LED
				if (led_enabled & 2)
					GPIO_ResetBits(GPIOB, GREEN);
				if (led_enabled & 1)
					GPIO_ResetBits(GPIOB, RED);
				pun = 1;
				cun = 300; // 0.3s on
			}
		}
	}
}

int main(void) {
#ifdef DEBUG
	debug();
#endif
	RCC_Conf();
	NVIC_Conf();
	init_port();
	init_timer();
	delay_init();
	ublox_init();

	// NOTE - LEDs are inverted. (Reset to activate, Set to deactivate)
	GPIO_SetBits(GPIOB, RED);
	GPIO_ResetBits(GPIOB, GREEN);
	for (int i=0; i<sizeof(CALLSIGN); i++)
		usart3_put(CALLSIGN[i]);

	radio_soft_reset();
	// setting RTTY TX frequency
	radio_set_tx_frequency(TRANSMIT_FREQUENCY);

	// setting TX power
	radio_rw_register(0x6D, 00 | (TX_POWER & 0x0007), 1);

	// initial RTTY modulation
	radio_rw_register(0x71, 0x00, 1);

	// Temperature Value Offset
	radio_rw_register(0x13, 0x00, 1);

	// Temperature Sensor Calibration
	radio_rw_register(0x12, 0x20, 1);

	// ADC configuration
	radio_rw_register(0x0f, 0x80, 1);
	tx_buffer = buf_mfsk;
	tx_on = 0;
	tx_enable = 1;

	// Why do we have to do this again?
	// spi_init();
	radio_set_tx_frequency(TRANSMIT_FREQUENCY);
	radio_rw_register(0x71, 0x00, 1);
	// init_timer();
	// radio_enable_tx();

	while ( 1 ) {
		// Don't do anything until the previous transmission has finished.
		if ( tx_enable && !tx_on ) {
			if (7 & framecount++) {
				send_ssdv_packet();
			} else {
				collect_telemetry_data();
				send_mfsk_packet();
			}
		} else {
			NVIC_SystemLPConfig(NVIC_LP_SEVONPEND, DISABLE);
			__WFI();
		}
	}
}

int32_t last_lat = 0, last_lon = 0, last_alt = 0;
void collect_telemetry_data() {
	// Assemble and process the telemetry data we need to construct our RTTY and MFSK packets.
	send_count++;
	si4032_temperature = radio_read_temperature();
	voltage = ADCVal[0] * 600 / 4096;
	ublox_get_last_data(&gpsData);

	if ( (gpsData.fix >= 3)&&(gpsData.fix < 5) ) {
		// if (!(send_count & 31))
		//	ubx_powersave();

		last_lat = gpsData.lat_raw;
		last_lon = gpsData.lon_raw;
		last_alt = gpsData.alt_raw / 1000;
		if (last_alt < 0)
			last_alt = 0;
		// Disable LEDs if altitude is > 500m. (Power saving? Maybe?)
		if (last_alt > 500) {
			led_enabled = 0;
		} else {
			led_enabled = 2; // flash green when GPS has lock
		}
	} else {
		// No GPS fix.
		led_enabled = 1; // flash red for no GPS fix
	}
}

void send_mfsk_packet(){
	// Generate a MFSK Binary Packet

	uint8_t volts_scaled = (uint8_t)(51 * voltage / 100);

	// Assemble a binary packet
	BinaryPacket.PayloadID = LONG_BINARY;
	BinaryPacket.Counter = send_count;
	BinaryPacket.Hours = gpsData.hours;
	BinaryPacket.Minutes = gpsData.minutes;
	BinaryPacket.Seconds = gpsData.seconds;
	BinaryPacket.Latitude = ublox2float(last_lat);
	BinaryPacket.Longitude = ublox2float(last_lon);
	BinaryPacket.Altitude = (uint16_t)(last_alt);
	BinaryPacket.Speed = get_reset_count();
	BinaryPacket.BattVoltage = volts_scaled;
	BinaryPacket.Sats = gpsData.sats_raw;
	BinaryPacket.Temp = si4032_temperature;

	BinaryPacket.User1 = camera_bytes();
	BinaryPacket.User2 = image_packets;

	// Callsign is MSB
	uint32_t name = encode_callsign(callsign);
	name = (name >> 24) | ((name << 8) & (0xff << 16)) |
		((name >> 8) & (0xff << 8)) | (name << 24);
	BinaryPacket.NameID = name;

	// Wrap a long packet around a legacy packet.
	memcpy(buf_ssdv, &BinaryPacket, sizeof(BinaryPacket));
	uint32_t CRC32_checksum = crc32(buf_ssdv, LONG_BINARY - 4);
	buf_ssdv[LONG_BINARY - 1] = (CRC32_checksum >> 0) & 0xff;
	buf_ssdv[LONG_BINARY - 2] = (CRC32_checksum >> 8) & 0xff;
	buf_ssdv[LONG_BINARY - 3] = (CRC32_checksum >>16) & 0xff;
	buf_ssdv[LONG_BINARY - 4] = (CRC32_checksum >>24) & 0xff;

	//  Important: preamble is used extend sync word
	sprintf(buf_mfsk, "\x1b\x1b\x1b\x1b");

	// Encode the packet, and write into the mfsk buffer.
	int coded_len = horus_l2_encode_tx_packet( (unsigned char*)buf_mfsk + 4, buf_ssdv, LONG_BINARY );

	// Data to transmit is the coded packet length, plus the 4-byte preamble.
	packet_length = coded_len + 4;
	tx_buffer = buf_mfsk;
	start_sending(PREAMBLE, NOT_SSDV_DELAY);
}

void send_ssdv_packet() {
	int status = fill_image_packet(buf_ssdv);
	if (status < 0) {
		start_sending(IDLE4FSK, NOT_SSDV_DELAY);
		return;
	}

	sprintf(buf_mfsk, "\x1b\x1b\x1b\x1b");
	int coded_len = horus_l2_encode_tx_packet( (unsigned char*)buf_mfsk + 4, buf_ssdv + 1, SSDV_SIZE );
	packet_length = coded_len + 4;
	tx_buffer = buf_mfsk;
	start_sending(PREAMBLE, TX_DELAY);
	image_packets++;
}

#ifdef  DEBUG
void assert_failed(uint8_t* file, uint32_t line){
	while ( 1 ) ;
}
#endif
