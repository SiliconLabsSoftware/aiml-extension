/***************************************************************************/ /**
 * @file rgb_led.h
 * @brief RGB LED examples functions
 *******************************************************************************
 * # License
 * <b>Copyright 2024 Silicon Laboratories Inc. www.silabs.com</b>
 *******************************************************************************
 *
 * The licensor of this software is Silicon Laboratories Inc. Your use of this
 * software is governed by the terms of Silicon Labs Master Software License
 * Agreement (MSLA) available at
 * www.silabs.com/about-us/legal/master-software-license-agreement. This
 * software is distributed to you in Source Code format and is governed by the
 * sections of the MSLA applicable to Source Code.
 *
 ******************************************************************************/

#ifndef RGB_LED_H
#define RGB_LED_H

#include <stdio.h>

#include "app.h"
#include "rgb_led.h"
#include "timestamp.h"
// #include "sl_tflite_micro_init.h"
#include "sl_sleeptimer.h"

#ifdef __cplusplus
extern "C" {
#endif

/***************************************************************************/ /**
 * Initialize example
 ******************************************************************************/
void rgb_led_init(void);

/***************************************************************************/ /**
 * RGB LED ticking function
 ******************************************************************************/
void rgb_led_process_action(uint32_t percentage_cycle);

#ifdef __cplusplus
}
#endif

#endif  // RGB_LED_H
