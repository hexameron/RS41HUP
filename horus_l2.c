/*---------------------------------------------------------------------------*\

  FILE........: horus_l2.c
  AUTHOR......: David Rowe
  DATE CREATED: Dec 2015

  Horus telemetry layer 2 processing.  Takes an array of 8 bit payload
  data, generates parity bits for a (23,12) Golay code, interleaves
  data and parity bits, pre-pends a Unique Word for modem sync.
  Caller is responsible for providing storage for output packet.

  [ ] code based interleaver
  [ ] test correction of 1,2 & 3 error patterms    

  1/ Unit test on a PC:

     $ gcc horus_l2.c -o horus_l2 -Wall -DHORUS_L2_UNITTEST
     $ ./horus_l2

     test 0: 22 bytes of payload data BER: 0.00 errors: 0
     test 0: 22 bytes of payload data BER: 0.01 errors: 0
     test 0: 22 bytes of payload data BER: 0.05 errors: 0
     test 0: 22 bytes of payload data BER: 0.10 errors: 7
     
     This indicates it's correcting all channel errors for 22 bytes of
     payload data, at bit error rate (BER) of 0, 0.01, 0.05.  It falls
     over at a BER of 0.10 which is expected.

  2/ To build with just the tx function, ie for linking with the payload
  firmware:

    $ gcc horus_l2.c -c -Wall
    
  By default the RX side is #ifdef-ed out, leaving the minimal amount
  of code for tx.

  3/ Generate some tx_bits as input for testing with fsk_horus:
 
    $ gcc horus_l2.c -o horus_l2 -Wall -DGEN_TX_BITS -DSCRAMBLER
    $ ./horus_l2
    $ more ../octave/horus_tx_bits_binary.txt
   
  4/ Unit testing interleaver:

    $ gcc horus_l2.c -o horus_l2 -Wall -DINTERLEAVER -DTEST_INTERLEAVER -DSCRAMBLER

  5/ Compile for use as decoder called by  fsk_horus.m and fsk_horus_stream.m:

    $ gcc horus_l2.c -o horus_l2 -Wall -DDEC_RX_BITS -DHORUS_L2_RX

\*---------------------------------------------------------------------------*/

#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include "horus_l2.h"

#include "HRA128_384.h"


/* Target is 10% BER, for 10% packet loss. => 32 bit magic word with 4 errors
	for 36k bits between collisions: 6 minutes at 100 baud. */
static const char uw[] = { 0x1b, 0x1b,'$','$' };

/* Functions ----------------------------------------------------------*/

/*
   We are using a Golay (23,12) code which has a codeword 23 bits
   long.  The tx packet format is:

      | Unique Word | payload data bits | parity bits |

      (4 + 43) bytes out for a 22 byte standard payload
 */

int horus_l2_get_num_tx_data_bytes(int num_payload_data_bytes) {
    int num_golay_codewords;
    
    num_golay_codewords = (num_payload_data_bytes * 8 + 11) / 12;
    /* round up to 12 bits, may mean some unused bits */

    return sizeof(uw) + num_payload_data_bytes + (num_golay_codewords*11 + 7)/ 8;
    /* round up to nearest byte, may mean some unused bits */
}


/*
  Takes an array of payload data bytes, prepends a unique word and appends
  parity bits.

  The encoder will run on the payload on a small 8-bit uC.  As we are
  memory constrained so we do a lot of burrowing for bits out of
  packed arrays, and don't use a LUT for Golay encoding.  Hopefully it
  will run fast enough.  This was quite difficult to get going,
  suspect there is a better way to write this.  Oh well, have to start
  somewhere.
 */

int horus_l2_encode_tx_packet(unsigned char *output_tx_data,
                              unsigned char *input_payload_data,
                              int            num_payload_data_bytes)
{
    int            num_tx_data_bytes, num_payload_data_bits;
    unsigned char *pout = output_tx_data;
    int            ninbit, ningolay, nparitybits;
    int32_t        ingolay, paritybyte, inbit, golayparity;
    int            ninbyte, shift, golayparitybit, i;

    num_tx_data_bytes = horus_l2_get_num_tx_data_bytes(num_payload_data_bytes);
    memcpy(pout, uw, sizeof(uw)); pout += sizeof(uw);
    memcpy(pout, input_payload_data, num_payload_data_bytes); pout += num_payload_data_bytes;

    /* Read input bits one at a time.  Fill input Golay codeword.  Find output Golay codeword.
       Write this to parity bits.  Write parity bytes when we have 8 parity bits.  Bits are
       written MSB first. */

    num_payload_data_bits = num_payload_data_bytes*8;
    ninbit = 0;
    ingolay = 0;
    ningolay = 0;
    paritybyte = 0;
    nparitybits = 0;

    while (ninbit < num_payload_data_bits) {

        /* extract input data bit */
        ninbyte = ninbit/8;
        shift = 7 - (ninbit % 8);
        inbit = (input_payload_data[ninbyte] >> shift) & 0x1;
        ninbit++;

        /* build up input golay codeword */
        ingolay = ingolay | inbit;
        ningolay++;

        /* when we get 12 bits do a Golay encode */
        if (ningolay % 12) {
            ingolay <<= 1;
        }
        else {
            golayparity = get_syndrome(ingolay<<11);
            ingolay = 0;

            /* write parity bits to output data */
            for (i=0; i<11; i++) {
                golayparitybit = (golayparity >> (10-i)) & 0x1;
                paritybyte = paritybyte | golayparitybit;
                nparitybits++;
                if (nparitybits % 8) {
                   paritybyte <<= 1;
                }
                else {
                    /* OK we have a full byte ready */
                    *pout = paritybyte;
                    pout++;
                    paritybyte = 0;
                }
            }
        }
    } /* while(.... */


    /* Complete final Golay encode, we may have partially finished ingolay, paritybyte */
    if (ningolay % 12) {
        ingolay >>= 1;
        golayparity = get_syndrome(ingolay<<12);

        /* write parity bits to output data */
        for (i=0; i<11; i++) {
            golayparitybit = (golayparity >> (10 - i)) & 0x1;
            paritybyte = paritybyte | golayparitybit;
            nparitybits++;
            if (nparitybits % 8) {
                paritybyte <<= 1;
            }
            else {
                /* OK we have a full byte ready */
                *pout++ = (unsigned char)paritybyte;
                paritybyte = 0;
            }
        }
    }
 
    /* and final, partially complete, parity byte */
    if (nparitybits % 8) {
        paritybyte <<= 7 - (nparitybits % 8);  // use MS bits first
        *pout++ = (unsigned char)paritybyte;
    }

    interleave(&output_tx_data[sizeof(uw)], num_tx_data_bytes-sizeof(uw));
    scramble(&output_tx_data[sizeof(uw)], num_tx_data_bytes-sizeof(uw));

    return num_tx_data_bytes;
}

//  Take payload data bytes, prepend a unique word and append parity bits
int ldpc_encode_packet(unsigned char *out_data, unsigned char *in_data) {
    unsigned int   i, last = 0;
    unsigned char *pout;
    pout = out_data;
    memcpy(pout, uw, sizeof(uw));
    pout += sizeof(uw);
    memcpy(pout, in_data, DATA_BYTES);
    pout += DATA_BYTES;
    memset(pout, 0, PARITY_BYTES);

    // process parity bit offsets
    for (i = 0; i < NUMBERPARITYBITS; i++) {
        unsigned int shift, j;

	for(j = 0; j < MAX_ROW_WEIGHT; j++) {
		uint8_t tmp  = H_rows[i + NUMBERPARITYBITS * j] - 1;
		shift = 7 - (tmp & 7); // MSB
		last ^= in_data[tmp >> 3] >> shift;
	}
	shift = 7 - (i & 7); // MSB
	pout[i >> 3] |= (last & 1) << shift;
    }

    pout = out_data + sizeof(uw);
    interleave(pout, DATA_BYTES + PARITY_BYTES);
    scramble(pout, DATA_BYTES + PARITY_BYTES);

    return DATA_BYTES + PARITY_BYTES + sizeof(uw);
}

// single directional for encoding
void interleave(unsigned char *inout, int nbytes)
{
    uint16_t nbits = (uint16_t)nbytes*8;
    uint32_t i, j, ibit, ibyte, ishift, jbyte, jshift;
    unsigned char out[nbytes];

    memset(out, 0, nbytes);
    for(i=0; i<nbits; i++) {
        /*  "On the Analysis and Design of Good Algebraic Interleavers", Xie et al,eq (5) */
        j = (COPRIME * i) % nbits;
        
        /* read bit i  */
        ibyte = i>>3;
        ishift = i&7;
        ibit = (inout[ibyte] >> ishift) & 0x1;

	/* write bit i to bit j position */
        jbyte = j>>3;
        jshift = j&7;
        out[jbyte] |= ibit << jshift; // replace with i-th bit
    }
 
    memcpy(inout, out, nbytes);
}

/* 16 bit DVB additive scrambler as per Wikpedia example */
void scramble(unsigned char *inout, int nbytes)
{
    int nbits = nbytes*8;
    int i, ibit, ibits, ibyte, ishift, mask;
    uint16_t scrambler = 0x4a80;  /* init additive scrambler at start of every frame */
    uint16_t scrambler_out;

    /* in place modification of each bit */
    for(i=0; i<nbits; i++) {

        scrambler_out = ((scrambler & 0x2) >> 1) ^ (scrambler & 0x1);

        /* modify i-th bit by xor-ing with scrambler output sequence */
        ibyte = i>>3;
        ishift = i&7;
        ibit = (inout[ibyte] >> ishift) & 0x1;
        ibits = ibit ^ scrambler_out;                  // xor ibit with scrambler output

        mask = 1 << ishift;
        inout[ibyte] &= ~mask;                  // clear i-th bit
        inout[ibyte] |= ibits << ishift;         // set to scrambled value

        /* update scrambler */
        scrambler >>= 1;
        scrambler |= scrambler_out << 14;
    }
}

/*---------------------------------------------------------------------------*\

                                   GOLAY FUNCTIONS

\*---------------------------------------------------------------------------*/

/* File:    golay23.c
 * Title:   Encoder/decoder for a binary (23,12,7) Golay code
 * Author:  Robert Morelos-Zaragoza (robert@spectra.eng.hawaii.edu)
 * Date:    August 1994
 *
 * The binary (23,12,7) Golay code is an example of a perfect code, that is,
 * the number of syndromes equals the number of correctable error patterns.
 * The minimum distance is 7, so all error patterns of Hamming weight up to
 * 3 can be corrected. The total number of these error patterns is:
 *
 *       Number of errors         Number of patterns
 *       ----------------         ------------------
 *              0                         1
 *              1                        23
 *              2                       253
 *              3                      1771
 *                                     ----
 *    Total number of error patterns = 2048 = 2^{11} = number of syndromes
 *                                               --
 *                number of redundant bits -------^
 *
 * Because of its relatively low length (23), dimension (12) and number of
 * redundant bits (11), the binary (23,12,7) Golay code can be encoded and
 * decoded simply by using look-up tables. The program below uses a 16K
 * encoding table and an 8K decoding table.
 *
 * For more information, suggestions, or other ideas on implementing error
 * correcting codes, please contact me at (I'm temporarily in Japan, but
 * below is my U.S. address):
 *
 *                    Robert Morelos-Zaragoza
 *                    770 S. Post Oak Ln. #200
 *                      Houston, Texas 77056
 *
 *             email: robert@spectra.eng.hawaii.edu
 *
 *       Homework: Add an overall parity-check bit to get the (24,12,8)
 *                 extended Golay code.
 *
 * COPYRIGHT NOTICE: This computer program is free for non-commercial purposes.
 * You may implement this program for any non-commercial application. You may
 * also implement this program for commercial purposes, provided that you
 * obtain my written permission. Any modification of this program is covered
 * by this copyright.
 *
 * ==   Copyright (c) 1994  Robert Morelos-Zaragoza. All rights reserved.   ==
 */

#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#define X22             0x00400000   /* vector representation of X^{22} */
#define X11             0x00000800   /* vector representation of X^{11} */
#define MASK12          0xfffff800   /* auxiliary vector for testing */
#define GENPOL          0x00000c75   /* generator polinomial, g(x) */

/* Global variables:
 *
 * pattern = error pattern, or information, or received vector
 * encoding_table[] = encoding table
 * decoding_table[] = decoding table
 * data = information bits, i(x)
 * codeword = code bits = x^{11}i(x) + (x^{11}i(x) mod g(x))
 * numerr = number of errors = Hamming weight of error polynomial e(x)
 * position[] = error positions in the vector representation of e(x)
 * recd = representation of corrupted received polynomial r(x) = c(x) + e(x)
 * decerror = number of decoding errors
 * a[] = auxiliary array to generate correctable error patterns
 */

int32_t get_syndrome(int32_t pattern)
/*
 * Compute the syndrome corresponding to the given pattern, i.e., the
 * remainder after dividing the pattern (when considering it as the vector
 * representation of a polynomial) by the generator polynomial, GENPOL.
 * In the program this pattern has several meanings: (1) pattern = infomation
 * bits, when constructing the encoding table; (2) pattern = error pattern,
 * when constructing the decoding table; and (3) pattern = received vector, to
 * obtain its syndrome in decoding.
 */
{
    int32_t aux = X22;

    if (pattern >= X11)
       while (pattern & MASK12) {
           while (!(aux & pattern))
              aux = aux >> 1;
           pattern ^= (aux/X11) * GENPOL;
           }
    return(pattern);
}

