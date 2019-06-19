/*  Keep it simple: We write Rx bytes into a 256 long ring buffer,
 *  Head is only written by interrupt context
 *  Tail is only read by main() context
 *  Bad things will happen if the buffer head eats its tail
 *  - Camera data is 5+240+5 bytes MAX.
 */
#include "usart3.h"
#include "delay.h"

#define bufferSize 256
uint8_t u3buffer[bufferSize];
volatile uint8_t bufferHead = 0; // data in
uint8_t bufferTail = 0; // data out

void usart3_send( uint8_t item ) {
	while ( USART_GetFlagStatus( USART3, USART_FLAG_TC ) == RESET )
		_delay_ms( 1 );
	USART_SendData( USART3, item );
	while ( USART_GetFlagStatus( USART3, USART_FLAG_TC ) == RESET )
		_delay_ms( 1 );
}

void USART3_IRQHandler( void ) {
	if ( USART_GetITStatus( USART3, USART_IT_RXNE ) != RESET ) {
		u3buffer[bufferHead++] = (uint8_t)USART_ReceiveData( USART3 );
	} else if ( USART_GetITStatus( USART3, USART_IT_ORE ) != RESET ) {
		USART_ReceiveData( USART3 );
	} else {
		USART_ReceiveData( USART3 );
	}
}

void usart3_reset() {
	bufferHead = bufferTail = 0;
}

void usart3_flush() {
	bufferTail = bufferHead;
}

uint8_t usart3_available() {
	uint8_t len = bufferHead - bufferTail;
	return len;
}

uint8_t usart3_waitfor( uint8_t number, uint8_t timeout ) {
	while ( ( usart3_available() < number ) && timeout-- )
		_delay_ms( 5 );
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
