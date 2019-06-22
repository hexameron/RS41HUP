/*  Keep it simple: We write Rx bytes into a 256 long ring buffer,
 *  Head is only written by interrupt context
 *  Tail is only read by main() context
 */

#include <stdint.h>
#include <stm32f10x_usart.h>

void usart3_reset();
void usart3_flush();
uint8_t usart3_available();
uint8_t usart3_waitfor( uint8_t number, uint8_t timeout );
int  usart3_get();
void usart3_put( uint8_t item );
uint16_t usart3_wasin();
uint16_t usart3_wasout();

