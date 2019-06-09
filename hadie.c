/* hadie - High Altitude Balloon flight software              */
/*============================================================*/
/* Copyright (C)2010 Philip Heron <phil@sanslogic.co.uk>      */
/*                                                            */
/* This program is distributed under the terms of the GNU     */
/* General Public License, version 2. You may use, modify,    */
/* and redistribute it under the terms of this license. A     */
/* copy should be included with this source.                  */

#include "config.h"
#include <stdio.h>
#include <string.h>
#include "rs8.h"
#include "ssdv.h"
#include "pisky.h"

/* Jpeg Image Pointer */
const uint8_t *img;

uint8_t fill_image_packet(uint8_t *pkt)
{
	static uint8_t setup = 0;
	static uint8_t img_id = 0;
	static ssdv_t ssdv;
	static size_t errcode, image_ptr = 0;

	if(!setup)
	{
		/* re-init camera */
		image_ptr = 0;

		setup = 1;
		ssdv_enc_init(&ssdv, SSDV_TYPE_NOFEC, CALLSIGN, img_id++, SSDV_QUALITY_NORMAL);
		ssdv_enc_set_buffer(&ssdv, pkt);
		return 0; // TODO: This is only here to test failure.
	}
	
	while((errcode = ssdv_enc_get_packet(&ssdv)) == SSDV_FEED_ME)
	{
		size_t len;
		// r = readCamera(img, 64);
		img = &pisky[image_ptr];
		len = pisky_len - image_ptr;
		if (len == 0)
			break; // image incomplete ?
		if (len > 64)
			len = 64;
		image_ptr += len;
		ssdv_enc_feed(&ssdv, img, len);
	}
	
	if(errcode != SSDV_OK)
	{
		/* Something went wrong! */
		// camera_close();
		setup = 0;
		return 0;
	}
	
	if(ssdv.state == S_EOI)
	{
		/* The end of the image has been reached */
		//camera_close();
		setup = 0;
		/* May need to know when an image has ended */
		return 2;
	}
	
	/* Got the packet */
	return 1;
}

