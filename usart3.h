/*  Keep it simple: We write Rx bytes into a 256 long ring buffer,
 *  Head is only written by interrupt context
 *  Tail is only read by main() context
 *  Bad things will happen if the buffer head eats its tail
 *  - Camera data is 5+240+5 bytes MAX.
 */

#include <stdint.h>
#include <stm32f10x_usart.h>

void usart3_reset();
void usart3_flush();
uint8_t usart3_available();
uint8_t usart3_waitfor( uint8_t number, uint8_t timeout );
int  usart3_get();
void usart3_send( uint8_t item );
