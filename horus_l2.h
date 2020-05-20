/*---------------------------------------------------------------------------*\

  FILE........: horus_l2.h
  AUTHOR......: David Rowe
  DATE CREATED: Dec 2015

\*---------------------------------------------------------------------------*/

#ifndef __HORUS_L2__
#define __HORUS_L2__

int horus_l2_get_num_tx_data_bytes(int num_payload_data_bytes);

int horus_l2_encode_tx_packet(unsigned char *output_tx_data,
                              unsigned char *input_payload_data,
                              int            num_payload_data_bytes);

void horus_l2_decode_rx_packet(unsigned char *output_payload_data,
                               unsigned char *input_rx_data,
                               int            num_payload_data_bytes);

int32_t get_syndrome(int32_t pattern);

void interleave(unsigned char *inout, int nbytes);

void scramble(unsigned char *inout, int nbytes);

int ldpc_encode_packet(uint8_t *buff_mfsk, uint8_t *FSK);

#endif
