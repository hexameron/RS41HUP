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

void stop_sending() {
	current_mfsk_byte = 0;  // reset next sentence to start
	tx_on = 0;		// tx_on while sending data
	tx_on_delay = 500;	// one second idle
	tx_enable = 0;		// enable main loop after idle period
}

//
// Symbol Timing Interrupt
// In here symbol transmission occurs.
//

void TIM2_IRQHandler(void) {
	static int mfsk_symbol = 0;
	static int clockcount = 0;

	if ( TIM_GetITStatus(TIM2, TIM_IT_Update) != RESET ) {
		TIM_ClearITPendingBit(TIM2, TIM_IT_Update);

		// Button does not disable anything because BAD IDEA unless testing
		// Code will be removed by compiler if flag is unset
		if ( ALLOW_DISABLE_BY_BUTTON ) {
			if ( ADCVal[1] > adc_bottom ) {
				button_pressed++;
				if ( button_pressed > (BAUD_RATE / 3) ) {
					disable_armed = 1;
					GPIO_SetBits(GPIOB, RED);
					//GPIO_SetBits(GPIOB, GREEN);
				}
			} else {
				if ( disable_armed ) {
					GPIO_SetBits(GPIOA, GPIO_Pin_12);
				}
				button_pressed = 0;
			}

			if ( button_pressed == 0 ) {
				adc_bottom = ADCVal[1] * 1.1; // dynamical reference for power down level
			}
		}

		int hz100, hz250;
		if (++clockcount > 9)
			clockcount = 0;
		hz250 = clockcount & 1;
		hz100 = ((clockcount == 5) || !clockcount) ? 1 : 0;

		if ( tx_on ) {
			// RTTY Symbol selection logic.
			if (( current_mode == RTTY ) && hz100) {
				send_rtty_status = send_rtty( (char *) tx_buffer);

				if ( send_rtty_status == rttyEnd ) {
					if ( led_enabled )
						GPIO_SetBits(GPIOB, RED);
					if ( *(++tx_buffer) == 0 )
						stop_sending();
				} else if ( send_rtty_status == rttyOne ) {
					radio_rw_register(0x73, RTTY_DEVIATION, 1);
					if ( led_enabled )
						GPIO_SetBits(GPIOB, RED);
				} else if ( send_rtty_status == rttyZero ) {
					radio_rw_register(0x73, 0x00, 1);
					if ( led_enabled )
						GPIO_ResetBits(GPIOB, RED);
				}
			} else if (( current_mode == FSK_4 ) && hz100) {
				// 4FSK Symbol Selection Logic
				mfsk_symbol = send_mfsk(tx_buffer[current_mfsk_byte]);

				if ( mfsk_symbol == -1 ) {
					// Reached the end of the current character, increment the current-byte pointer.
					if ( current_mfsk_byte++ >= packet_length ) {
						stop_sending();
					} else {
						// We've now advanced to the next byte, grab the first symbol from it.
						mfsk_symbol = send_mfsk(tx_buffer[current_mfsk_byte]);
					}
				}
				// Set the symbol!
				if ( mfsk_symbol != -1 ) {
					radio_rw_register(0x73, (uint8_t)mfsk_symbol, 1);
				}
			} else if (( current_mode == CONTEST ) && hz250 ) {
				// Symbol Selection Logic EXACTLY the same as 4fsk
				mfsk_symbol = send_mfsk(tx_buffer[current_mfsk_byte]);

				if ( mfsk_symbol == -1 ) {
					// Reached the end of the current character, increment the current-byte pointer.
					if ( current_mfsk_byte++ >= packet_length ) {
						stop_sending();
					} else {
						// We've now advanced to the next byte, grab the first symbol from it.
						mfsk_symbol = send_mfsk(tx_buffer[current_mfsk_byte]);
					}
				}
				// Set the symbol!
				if ( mfsk_symbol != -1 ) {
					radio_rw_register(0x73, (uint8_t)mfsk_symbol, 1);
				}
			} else {
				// Ummmm.
			}
		} else {
			// TX is off
			// transmit continuous MFSK symbols.
			if (!clockcount) {
				mfsk_symbol = (mfsk_symbol + 1) & 0x03;
				radio_rw_register(0x73, (uint8_t)mfsk_symbol, 1);
			}

			// tx_on_delay is set at the end of a RTTY transmission above, and counts down
			// at the interrupt rate. When it hits zero, we set tx_enable to 1, which allows
			// the main loop to continue.
			if ( !tx_on_delay-- )
				tx_enable = 1;
		}

		// Green LED Blinking Logic
		if ( led_enabled && (--cun == 0) ) {
			if ( pun ) {
				// Clear Green LED.
				GPIO_SetBits(GPIOB, GREEN);
				pun = 0;
				cun = 1000; // 2s off
			} else {
				// If we have GPS lock, set LED
				if ( flaga & 0x80 )
					GPIO_ResetBits(GPIOB, GREEN);
				pun = 1;
				cun = 200; // 0.4s on
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

	GPIO_SetBits(GPIOB, RED);
	// NOTE - Green LED is inverted. (Reset to activate, Set to deactivate)
	GPIO_SetBits(GPIOB, GREEN);
	USART_SendData(USART3, 0xc);

	radio_soft_reset();
	// setting RTTY TX frequency
	radio_set_tx_frequency(TRANSMIT_FREQUENCY);

	// setting TX power
	radio_rw_register(0x6D, 00 | (TX_POWER & 0x0007), 1);

	// initial RTTY modulation
	radio_rw_register(0x71, 0x00, 1);

	// Temperature Value Offset
	radio_rw_register(0x13, 0xF0, 1);

	// Temperature Sensor Calibration
	radio_rw_register(0x12, 0x00, 1);

	// ADC configuration
	radio_rw_register(0x0f, 0x80, 1);
	tx_buffer = buf_rtty;
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
			if ( current_mode == STARTUP ) {
				// Grab telemetry information.
				collect_telemetry_data();
				current_mode = RTTY;
		#ifdef RTTY_ENABLED
				send_rtty_packet();
		#endif
			} else if ( current_mode == RTTY ) {
				current_mode = FSK_4;
		#ifdef MFSK_ENABLED
				send_mfsk_packet();
		#endif
			} else if ( current_mode == FSK_4 ) {
				current_mode = CONTEST;
		#ifdef USE_CONTESTIA
				send_contest_packet();
		#endif
			} else {
				current_mode = STARTUP;
			}
		} else {
			NVIC_SystemLPConfig(NVIC_LP_SEVONPEND, DISABLE);
			__WFI();
		}
	}
}


void collect_telemetry_data() {
	// Assemble and proccess the telemetry data we need to construct our RTTY and MFSK packets.
	send_count++;
	si4032_temperature = radio_read_temperature();
	voltage = ADCVal[0] * 600 / 4096;
	ublox_get_last_data(&gpsData);

	if ( gpsData.fix >= 3 ) {
		flaga |= 0x80;
		// Disable LEDs if altitude is > 1000m. (Power saving? Maybe?)
		if ( (gpsData.alt_raw / 1000) > 1000 ) {
			led_enabled = 0;
		} else {
			led_enabled = 1;
		}
	} else {
		// No GPS fix.
		flaga &= ~0x80;
		led_enabled = 1; // Enable LEDs when there is no GPS fix (i.e. during startup)

		// Null out lat / lon data to avoid spamming invalid positions all over the map.
		gpsData.lat_raw = 0;
		gpsData.lon_raw = 0;
	}
}


void send_rtty_packet() {
	// Write a RTTY packet into the tx buffer, and start transmission.

	// Convert raw lat/lon values into degrees and decimal degree values.
	uint8_t lat_d = (uint8_t) abs(gpsData.lat_raw / 10000000);
	uint32_t lat_fl = (uint32_t) abs(abs(gpsData.lat_raw) - lat_d * 10000000) / 1000;
	uint8_t lon_d = (uint8_t) abs(gpsData.lon_raw / 10000000);
	uint32_t lon_fl = (uint32_t) abs(abs(gpsData.lon_raw) - lon_d * 10000000) / 1000;

	uint8_t speed_kph = (uint8_t)(9 * gpsData.speed_raw / 2500);

	// Produce a RTTY Sentence (Compatible with the existing HORUS RTTY payloads)
	sprintf(buf_mfsk, "%s,%d,%02u:%02u:%02u,%s%d.%04ld,%s%d.%04ld,%ld,%d,%d,%d,%d",
			callsign,
			send_count,
			gpsData.hours, gpsData.minutes, gpsData.seconds,
			gpsData.lat_raw < 0 ? "-" : "", lat_d, lat_fl,
			gpsData.lon_raw < 0 ? "-" : "", lon_d, lon_fl,
			(gpsData.alt_raw / 1000),
			speed_kph,
			gpsData.sats_raw,
			voltage * 10,
			si4032_temperature
			);

	// Calculate and append CRC16 checksum to end of sentence.
	contestiaize(buf_mfsk); // replace lower case chars etc.
	CRC_rtty = string_CRC16_checksum(buf_mfsk);
	snprintf(buf_rtty, MAX_RTTY, "~~~\n$$%s*%04X\n--", buf_mfsk, CRC_rtty);

	tx_buffer = buf_rtty;
	start_bits = RTTY_PRE_START_BITS;
	radio_enable_tx();
	tx_on = 1;
	// From here the timer interrupt handles things.
}


void send_mfsk_packet(){
	// Generate a MFSK Binary Packet

	// Sanitise and convert some of the data.
	if ( gpsData.alt_raw < 0 ) {
		gpsData.alt_raw = 0;
	}
	uint8_t volts_scaled = (uint8_t)(51 * voltage / 100);

	// Assemble a binary packet
	struct TBinaryPacket BinaryPacket;
	BinaryPacket.PayloadID = BINARY_PAYLOAD_ID % 256;
	BinaryPacket.Counter = send_count;
	BinaryPacket.Hours = gpsData.hours;
	BinaryPacket.Minutes = gpsData.minutes;
	BinaryPacket.Seconds = gpsData.seconds;
	BinaryPacket.Latitude = ublox2float(gpsData.lat_raw);
	BinaryPacket.Longitude = ublox2float(gpsData.lon_raw);
	BinaryPacket.Altitude = (uint16_t)(gpsData.alt_raw / 1000);
	BinaryPacket.Speed = (uint8_t)(9 * gpsData.speed_raw / 2500);
	BinaryPacket.BattVoltage = volts_scaled;
	BinaryPacket.Sats = gpsData.sats_raw;
	BinaryPacket.Temp = si4032_temperature;

	BinaryPacket.Checksum = (uint16_t)array_CRC16_checksum( (char*)&BinaryPacket,sizeof(BinaryPacket) - 2);

	// Write Preamble characters into mfsk buffer.
	sprintf(buf_mfsk, "\x1b\x1b\x1b\x1b");
	// Encode the packet, and write into the mfsk buffer.
	int coded_len = horus_l2_encode_tx_packet( (unsigned char*)buf_mfsk + 4,(unsigned char*)&BinaryPacket,sizeof(BinaryPacket) );

	// Data to transmit is the coded packet length, plus the 4-byte preamble.
	packet_length = coded_len + 4;
	tx_buffer = buf_mfsk;
	radio_enable_tx();
	tx_on = 1;
}


void send_contest_packet(){
	// Convert rtty string into 4fsk symbols
	packet_length = 0;
	memset(buf_mfsk, 0, MAX_MFSK);
	for (int index = 0; index < MAX_RTTY - 1; index +=2) {
		contestia_start( &buf_rtty[index] ); // convert two chars into a temporary buffer ...
		packet_length += contestia_convert( &buf_mfsk[packet_length] ); // and  read back out
	}

	tx_buffer = buf_mfsk;
	radio_enable_tx();
	tx_on = 1;
}


#ifdef  DEBUG
void assert_failed(uint8_t* file, uint32_t line){
	while ( 1 ) ;
}
#endif
