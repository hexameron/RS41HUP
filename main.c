// STM32F100 and SI4032 RTTY transmitter
// released under GPL v.2 by anonymous developer
// enjoy and have a nice day
// ver 1.5a

#include "main.h"

/**
 * GPS data processing
 */
void USART1_IRQHandler(void) {
	if ( USART_GetITStatus(USART1, USART_IT_RXNE) != RESET ) {
		ublox_handle_incoming_byte( (uint8_t) USART_ReceiveData(USART1) );
	} else if ( USART_GetITStatus(USART1, USART_IT_ORE) != RESET )    {
		USART_ReceiveData(USART1);
	} else {
		USART_ReceiveData(USART1);
	}
}

void start_sending() {
	radio_enable_tx();
	tx_on = 1; // From here the timer interrupt handles things.
}


void stop_sending() {
	current_mfsk_byte = 0;  // reset next sentence to start
	tx_on = 0;		// sending data(1) or idle tones(0)
	tx_on_delay = TX_DELAY;
	tx_enable = 0;		// allow next sentence after idle period
	radio_disable_tx();	// no transmit powersave
}

static inline int send_fsk(char current_char) {
	static uint8_t nibble = 0;
	int m, shift, c;

	if (current_mode == DO_LDPC) {
		m = 0;	//2fsk
		shift = 1;
	} else {
		m = 1;  //4fsk
		shift = 3;
	}
	if ((nibble << m) > 7){
		nibble = 0;
		return -1;
	} else {
		c = current_char >> (8  - (++nibble << m));
		return (c & shift) * (4 - shift);
	}
}

//
// Symbol Timing Interrupt
// In here symbol transmission occurs.
//

void TIM2_IRQHandler(void) {
	static int mfsk_symbol = 0;
	static int preamble_byte = 20;
	static int clockcount = 0;

	if ( TIM_GetITStatus(TIM2, TIM_IT_Update) != RESET ) {
		TIM_ClearITPendingBit(TIM2, TIM_IT_Update);

		if ( ALLOW_DISABLE_BY_BUTTON ) {
			if ( ADCVal[1] > adc_bottom ) {
				button_pressed++;
					GPIO_ResetBits(GPIOB, RED);
				if ( button_pressed > (150) ) {
					disable_armed = 1;
					GPIO_SetBits(GPIOB, RED);
					GPIO_ResetBits(GPIOB, GREEN);
				}
			} else {
				if ( disable_armed ) {
					// Cut Battery power.
					GPIO_SetBits(GPIOA, GPIO_Pin_12);
				}
				button_pressed = 0;
			}

			if ( button_pressed == 0 ) {
				adc_bottom = ADCVal[1] * 1.1; // dynamical reference for power down level
			}
		}

		if ( tx_on ) {
			int hz100 = !(clockcount++); // divide 500Hz by five
				if (clockcount > 4)
					clockcount = 0;

			if ( (hz100 && (current_mode == DO_HORUS)) || (current_mode == DO_LDPC) ) {
				// 4FSK Symbol Selection Logic
				mfsk_symbol = send_fsk(tx_buffer[current_mfsk_byte]);

				if ( mfsk_symbol == -1 ) {
					// Reached the end of the current character, increment the current-byte pointer.
					if ( current_mfsk_byte++ >= packet_length ) {
						stop_sending();
						current_mode++;
						// DO_LDPC -> SEND_HORUS, or DO_HORUS -> STARTUP
					} else {
						mfsk_symbol = send_fsk(tx_buffer[current_mfsk_byte]);
					}
				}
				// Set the symbol!
				if ( mfsk_symbol != -1 ) {
					radio_rw_register(0x73, (uint8_t)mfsk_symbol, 1);
				}
			} else if (hz100)  {
				// 4fsk Preamble at 100 baud
				mfsk_symbol = (mfsk_symbol + 1) & 0x3;
				radio_rw_register(0x73, (uint8_t)(mfsk_symbol), 1);
				if ( !preamble_byte-- ) {
					preamble_byte = PREAMBLE_LENGTH;
					current_mode++;
					// SEND_LDPC -> DO_LDPC, or SEND_HORUS -> DO_HORUS
				}
			}
		} else {
			// TX is off

			// tx_on_delay is set at the end of a Transmission above, and counts down
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
				cun = 400; // 4s off
				pun = 0;
			} else {
				// If we have GPS lock, set LED
				if (led_enabled & 2)
					GPIO_ResetBits(GPIOB, GREEN);
				if (led_enabled & 1)
					GPIO_ResetBits(GPIOB, RED);
				pun = 1;
				cun = 40; // 0.4s on
			}
		}
	}
}

int32_t last_lat = 0, last_lon = 0, last_alt = 0;
void collect_telemetry_data() {
	// Assemble and process the telemetry data we need to construct our RTTY and MFSK packets.
	send_count++;
	si4032_temperature = radio_read_temperature();
	voltage = ADCVal[0] * 600 / 4096; // scaled to 0.01 V
	ublox_get_last_data(&gpsData);

	if ( (gpsData.fix >= 3)&&(gpsData.fix < 5) ) {
		if (!(send_count & 31))
			ubx_powersave();

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

void send_binary() {
	struct TBinaryPacket BinaryPacket;

	BinaryPacket.PayloadID = BINARY_PAYLOAD_ID % 256;
	BinaryPacket.Counter = send_count;
	BinaryPacket.Hours = gpsData.hours;
	BinaryPacket.Minutes = gpsData.minutes;
	BinaryPacket.Seconds = gpsData.seconds;
	BinaryPacket.Latitude = ublox2float(last_lat);
	BinaryPacket.Longitude = ublox2float(last_lon);
	BinaryPacket.Altitude = (uint16_t)(last_alt);
	BinaryPacket.Speed = gpsData.bad_packets; //(uint8_t)(9 * gpsData.speed_raw / 2500);
	BinaryPacket.BattVoltage = (uint8_t)(51 * voltage / 100);
	BinaryPacket.Sats = gpsData.sats_raw;
	BinaryPacket.Temp = si4032_temperature;

	BinaryPacket.Checksum = (uint16_t)array_CRC16_checksum( (char*)&BinaryPacket,sizeof(BinaryPacket) - 2);

	packet_length = horus_l2_encode_tx_packet( (unsigned char*)buff_mfsk,(unsigned char*)&BinaryPacket,sizeof(BinaryPacket) );
	tx_buffer = buff_mfsk;
	start_sending();
}

void send_ldpc() {
	int32_t user, sats, temp;
	struct SBinaryPacket FSK;

	FSK.PayloadID = LDPC_PAYLOAD_ID % 256;
	FSK.Counter = send_count;

	FSK.BiSeconds = 30 * 60 * gpsData.hours
			+ 30 * gpsData.minutes
			+ (gpsData.seconds >> 1);

	FSK.Longitude[0] = 0xFF & (last_lon >> 8);
	FSK.Longitude[1] = 0xFF & (last_lon >>16);
	FSK.Longitude[2] = 0xFF & (last_lon >>24);

	FSK.Latitude[0] = 0xFF & (last_lat >> 8);
	FSK.Latitude[1] = 0xFF & (last_lat >>16);
	FSK.Latitude[2] = 0xFF & (last_lat >>24);

	FSK.Altitude = (uint16_t)last_alt;
	FSK.Voltage = (uint8_t)(51 * voltage / 100);

	// Six bit temperature: +31C to -32C in 1C steps
	temp = si4032_temperature;
	if (temp > 31) temp = 31;
	if (temp < -32) temp = -32;
	user = (uint8_t)(temp << 2);

	// rough guide to GPS quality, (0,4,8,12 sats)
	sats = (gpsData.sats_raw + 1) >> 2;
	if (sats < 0) sats = 0;
	if (sats > 3) sats = 3;
	user |= sats;
	FSK.User = user;

	array2gray((uint8_t*)&FSK, sizeof(FSK) - 2);
	FSK.Checksum = (uint16_t)array_CRC16_checksum((char*)&FSK, sizeof(FSK) - 2);
	packet_length = ldpc_encode_packet((uint8_t*)buff_mfsk, (uint8_t*)&FSK);
	tx_buffer = buff_mfsk;
	start_sending();
}

/*---------------------------------------------------------------------------*/

int main(void) {
	RCC_Conf();
	NVIC_Conf();
	init_port();
	init_timer();
	delay_init();
	ublox_init();

	// NOTE - LEDs are inverted. (Reset to activate, Set to deactivate)
	GPIO_SetBits(GPIOB, RED);
	GPIO_ResetBits(GPIOB, GREEN);

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
	tx_buffer = buff_mfsk;
	tx_on = 0;
	tx_enable = 1;

	// Why do we have to do this again?
	spi_init();
	radio_set_tx_frequency(TRANSMIT_FREQUENCY);
	radio_rw_register(0x71, 0x00, 1);
	init_timer();
	radio_enable_tx();

	while ( 1 ) {
		// Don't do anything until the previous transmission has finished.
		if ( tx_on == 0 && tx_enable ) {
			if ( current_mode == SEND_LDPC )
			{
		#ifdef USE_LDPC
				collect_telemetry_data();
				send_ldpc();
		#else
				current_mode = SEND_HORUS;
		#endif
			}
			else if ( current_mode == SEND_HORUS )
			{
		#ifdef USE_HORUS
				collect_telemetry_data();
				send_binary();
		#else
				current_mode = SEND_LDPC;
		#endif
			}
			else	current_mode = SEND_LDPC;

		} else {
			NVIC_SystemLPConfig(NVIC_LP_SEVONPEND, DISABLE);
			__WFI();
		}
	}
}

/*---------------------------------------------------------------------------*/

