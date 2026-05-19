#ifndef __LED_SPI_H
#define __LED_SPI_H


#include "spi.h"


void led_spi_init(hw_spi_dev spi, u32 spi_do);

void led_spi_rgb_to_24byte(u8 r, u8 g, u8 b, u8 *buf, int idx);

void led_spi_rgb_to_24byte2(u32 data, u8 *buf);

void led_spi_send_reset();

void led_spi_send_rgbbuf(u8 *rgb_buf, u16 led_num); //rgb_buf  led_num * 24

void led_spi_send_rgbbuf_isr(u8 *rgb_buf, u16 led_num); //rgb_buf  led_num * 24

u32 led_spi_suspend(void);

u32 led_spi_resume(void);

void led_spi_test(void);


#endif


