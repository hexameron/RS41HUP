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

//TODO: Do not enable Tx if the "disable" button was pushed
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

//
// Symbol Timing Interrupt
// In here symbol transmission occurs.
//

void TIM2_IRQHandler(void) {
	static int mfsk_symbol = 0;
	static int preamble_byte = 20;

	if ( TIM_GetITStatus(TIM2, TIM_IT_Update) != RESET ) {
		TIM_ClearITPendingBit(TIM2, TIM_IT_Update);

		// BAD IDEA unless testing
		// Code will be removed by compiler if flag is unset
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
			if ( current_mode == SEND4FSK ) {
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
			} else {
				// Preamble for horus_demod to lock on:
				// Transmit continuous MFSK symbols
				if (current_mode == HORUS) {
					mfsk_symbol = (mfsk_symbol + 1) & 0x03;
					radio_rw_register(0x73, (uint8_t)mfsk_symbol, 1);
					if ( !preamble_byte-- ) {
						preamble_byte = 20;
						// start sending data
						current_mode = SEND4FSK;
					}
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
	// for (int i=0; i<5; i++)
	//	USART_SendData(USART3, 0x17);

	radio_soft_reset();
	radio_set_tx_frequency(TRANSMIT_FREQUENCY);

	// setting TX power
	radio_rw_register(0x6D, 00 | (TX_POWER & 0x0007), 1);

	// initial modulation
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
	spi_init();
	radio_set_tx_frequency(TRANSMIT_FREQUENCY);
	radio_rw_register(0x71, 0x00, 1);
	init_timer();
	radio_enable_tx();

	while ( 1 ) {
		// Don't do anything until the previous transmission has finished.
		if ( tx_on == 0 && tx_enable ) {
			if ( current_mode == STARTUP ) {
				current_mode = HORUS;
		#ifdef USE_HORUS
				collect_telemetry_data();
				send_mfsk_packet();
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

/* You could process this better on the ground, but habhub draws nice graphs in real time */
void update_motion() {
	static uint32_t oldtime = 0;
	uint32_t timenow, pingtime;
	int16_t  xspeed, yspeed, averagex, averagey; 

	xspeed = gpsData.xspeed;
	yspeed = gpsData.yspeed;
	averagex = gpsData.averagex;
	averagey = gpsData.averagey;

	lateral = squareroot(averagex, averagey);
	if (lateral > 0)
		orthogonal = ((averagey * xspeed) - (averagex  * yspeed)) / lateral;
	else
		orthogonal = gpsData.speed_raw;


	timenow = gpsData.seconds;
	if (timenow < oldtime)
		pingtime = 60 + timenow - oldtime;
	else
		pingtime = timenow - oldtime;
	distance += lateral * pingtime;
	oldtime = timenow;
}

void collect_telemetry_data() {
	// Assemble and process the telemetry data we need to construct our RTTY and MFSK packets.
	send_count++;
	si4032_temperature = radio_read_temperature();
	voltage = ADCVal[0] * 600 / 4096;
	ublox_get_last_data(&gpsData);

	if ( gpsData.fix >= 3 ) {
#if 0
		// disable powersave for more precise tracking in v2
		if (!(send_count & 31))
			ubx_powersave();
#endif

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

		// May fix with 4 sats: first position can be wildly inaccurate.
		if (gpsData.sats_raw > 4)
			update_motion();
	} else {
		// No GPS fix.
		led_enabled = 1; // flash red for no GPS fix
	}
}



void send_mfsk_packet(){
	// Generate a MFSK Binary Packet

	uint8_t volts_scaled = (uint8_t)(51 * voltage / 100);		// 5v in 8bits
	uint8_t speed_scaled = (uint8_t)(9 * gpsData.speed_raw / 250);	// 100 cm.s => 3.6 kph

	// Assemble a binary packet
	struct TBinaryPacket BinaryPacket;
	BinaryPacket.PayloadID = BINARY_PAYLOAD_ID;
	BinaryPacket.Counter = send_count;
	BinaryPacket.Hours = gpsData.hours;
	BinaryPacket.Minutes = gpsData.minutes;
	BinaryPacket.Seconds = gpsData.seconds;
	BinaryPacket.Latitude = ublox2float(last_lat);
	BinaryPacket.Longitude = ublox2float(last_lon);
	BinaryPacket.Altitude = (uint16_t)(last_alt);
	BinaryPacket.Speed = speed_scaled;
	BinaryPacket.BattVoltage = volts_scaled;
	BinaryPacket.Sats = gpsData.sats_raw;
	BinaryPacket.Temp = si4032_temperature;

	BinaryPacket.Vertical = (int16_t)gpsData.averagez;
	BinaryPacket.Lateral = lateral;
	BinaryPacket.Orthogonal = orthogonal;
	BinaryPacket.Distance = (uint16_t)(distance / 1000); // convert cm into 10m resolution
	BinaryPacket.Heading = (int8_t)( gpsData.heading / 180 / 1000 - 100 );

	BinaryPacket.Checksum = (uint16_t)array_CRC16_checksum( (char*)&BinaryPacket,sizeof(BinaryPacket) - 2);

	// Write Preamble characters into mfsk buffer.
	sprintf(buf_mfsk, "\x1b\x1b\x1b\x1b");
	// Encode the packet, and write into the mfsk buffer.
	int coded_len = horus_l2_encode_tx_packet( (unsigned char*)buf_mfsk + 4,(unsigned char*)&BinaryPacket,sizeof(BinaryPacket) );

	// Data to transmit is the coded packet length, plus the 4-byte preamble.
	packet_length = coded_len + 4;
	tx_buffer = buf_mfsk;
	start_sending();
}


#ifdef  DEBUG
void assert_failed(uint8_t* file, uint32_t line){
	while ( 1 ) ;
}
#endif
