/*  CCH Beacon
 *  Copyright (c) 2016 Stelios Ioakim.
 *  All Rights Reserved.
 *
 *  Developed by s.ioakim@engino.net
 */

#ifndef __led_h
#define __led_h

/**
 *  @file
 *  @brief       LED definitions
 *  @addtogroup  CCH-Beacon
 *  @{
 */

#define LED_RED         0   /**< Red LED ID */
#define LED_GREEN       1   /**< Green LED ID */
#define LED_BLUE        2   /**< Blue LED ID */

void led_rgb_set(uint32_t led, bool state);
void led_init(void);
void adc_init(void);

/** @} */
#endif
