/*  Keep it simple: We write Rx bytes into a 256 long ring buffer,
 *  Head is only written by interrupt context (except on "reset")
 *  Tail is only read by main() context
 *  256 bytes will be lost if the head catches the tail
 *  - Camera data is 5+240+5 bytes MAX.
 */
#include "usart3.h"
#include "delay.h"

#define bufferSize 256
uint8_t u3buffer[bufferSize];
volatile uint8_t bufferHead = 0; // data in
uint8_t bufferTail = 0; // data out
uint16_t datasent = 0;
uint16_t databack = 0;


void usart3_put( uint8_t item ) {
	while ( USART_GetFlagStatus( USART3, USART_FLAG_TC ) == RESET )
		_delay_ms( 1 );
	USART_SendData( USART3, item );
	while ( USART_GetFlagStatus( USART3, USART_FLAG_TC ) == RESET )
		_delay_ms( 1 );
	datasent++;
}

void USART3_IRQHandler( void ) {
	if ( USART_GetITStatus( USART3, USART_IT_RXNE ) != RESET ) {
		u3buffer[bufferHead++] = (uint8_t)USART_ReceiveData( USART3 );
		databack++;
	} else if ( USART_GetITStatus( USART3, USART_IT_ORE ) != RESET ) {
		USART_ReceiveData( USART3 );
	} else {
		USART_ReceiveData( USART3 );
	}
}

uint16_t usart3_wasin() {
	return databack;
}


uint16_t usart3_wasout() {
	return datasent;
}


void usart3_reset() { // mostly safe
	bufferHead = bufferTail = 0;
}

void usart3_flush() { // very safe
	bufferTail = bufferHead;
}

uint8_t usart3_available() {
	uint8_t len = bufferHead - bufferTail;
	return len;
}

uint8_t usart3_waitfor( uint8_t number, uint8_t timeout ) {
	while ( ( usart3_available() < number ) && timeout-- )
		_delay_ms( 1 );
	return usart3_available();
}

int usart3_get() {
	int value = (int)u3buffer[bufferTail];
	if ( !usart3_available() ) {
		return -1;
	}
	bufferTail++;
	return value;
}
