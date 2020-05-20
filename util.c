#include <util.h>

uint16_t string_CRC16_checksum(char *string) {
  uint16_t crc = 0xffff;
  char i;
  while (*(string) != 0) {
    crc = crc ^ (*(string++) << 8);
    for (i = 0; i < 8; i++) {
      if (crc & 0x8000)
        crc = (uint16_t) ((crc << 1) ^ 0x1021);
      else
        crc <<= 1;
    }
  }
  return crc;
}

uint16_t array_CRC16_checksum(char *string, int len) {
  uint16_t crc = 0xffff;
  char i;
  int ptr = 0;
  while (ptr < len) {
    ptr++;
    crc = crc ^ (*(string++) << 8);
    for (i = 0; i < 8; i++) {
      if (crc & 0x8000)
        crc = (uint16_t) ((crc << 1) ^ 0x1021);
      else
        crc <<= 1;
    }
  }
  return crc;
}

void print_hex(char *data, uint8_t length, char *tmp) // prints 8-bit data in hex
{
 uint8_t first ;
 int j=0;
 for (uint8_t i=0; i<length; i++) 
 {
   first = ((uint8_t)data[i] >> 4) | 48;
   if (first > 57) tmp[j] = first + (uint8_t)39;
   else tmp[j] = first ;
   j++;

   first = ((uint8_t)data[i] & 0x0F) | 48;
   if (first > 57) tmp[j] = first + (uint8_t)39; 
   else tmp[j] = first;
   j++;
 }
 tmp[length*2] = '\n';
 tmp[length*2+1] = 0;
 // Serial.println(tmp);
}

void array2gray(uint8_t *data, uint8_t len) {
	int i;
	for (i = 0; i < len; i++)
		data[i] ^= data[i] >> 1;
}


// divide by 10^7 and convert to float
int32_t ublox2float(int32_t x){
	uint32_t s, e, m, y, z;
	uint64_t mm;
	s = x & (1 << 31); // sign bit is same.
	if (x < 0)
		y = -x;
	else
		y = x;

	// compiler funkyness: we want 32bit multiply to 64bit result
	mm = (uint64_t)((1LL<<54)/10000000) * y;
	y = (uint32_t)(mm >> 32);
	if (y == 0)
		return 0;
	z = __builtin_clz(y);
	y <<= z;
	m = (y >> 8) & ((1 << 23) - 1);
	e = (128 + 8 - z) << 23;
	return s|e|m;
}

#if 0
#include "stdio.h"
int main() {
	for (int x=0; x<32; x++) {
		int32_t y = 101 << x;
		int32_t z = ublox2float(y);
		printf("%f,%f\n", (float)(y * 1.0e-7), *(float *)&z);
	}
	return 0;
}
#endif
