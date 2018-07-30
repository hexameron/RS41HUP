//
// Created by SQ5RWU on 2016-12-24.
//

#ifndef RS41HUP_RADIO_H
#define RS41HUP_RADIO_H

#include "config.h"
#include <stdint.h>
#include <stm32f10x_spi.h>
#include <stm32f10x_gpio.h>

static const uint16_t radioNSELpin = GPIO_Pin_13; // @ GPIOC
static const uint16_t radioSDIpin = GPIO_Pin_15; // @ GPIOB!
static const uint8_t WR = 0x80;

#define GENDIV 3
#define SI4032_KHZ 26000
#define SI4032_CLOCK (SI4032_KHZ * 1000)
// SI4032 manual gives minimum deviation of 625 Hz from 10 Mhz
//  but RS41 manages 270 shift for rtty.

#ifdef __cplusplus
extern "C" {
#endif

uint8_t _spi_sendrecv(const uint16_t data_word);

uint8_t radio_rw_register(const uint8_t register_addr, uint8_t value, uint8_t write);

void radio_set_tx_frequency(uint32_t centre_freq);

void radio_disable_tx();

void radio_soft_reset();

void radio_enable_tx();

int8_t radio_read_temperature();

#ifdef __cplusplus
}
#endif

#endif //RS41HUP_RADIO_H
