/***************************************************************************//**
 * @file
 * @brief led helper
 *******************************************************************************
 * # License
 * <b>Copyright 2026 Silicon Laboratories Inc. www.silabs.com</b>
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

#pragma once

#if __has_include("sl_simple_led_instances.h")

    #include "sl_simple_led_instances.h"

    #define DETECTION_LED (&sl_led_led1)
    #define ACTIVITY_LED (&sl_led_led0)

    #define led_init() sl_simple_led_init_instances()
    #define led_turn_on(x) sl_led_turn_on(x)
    #define led_turn_off(x) sl_led_turn_off(x)
    #define led_toggle(x) sl_led_toggle(x)

#elif __has_include("sl_si91x_rgb_led_instances.h")
    #include "sl_si91x_rgb_led_instances.h"

    #define DETECTION_ON (&led_led0)
    #define ACTIVITY_LED (&blue)

    #define led_init() rgb_led_init_instances()
    #define led_turn_on(x) sl_si91x_simple_rgb_led_on(x)
    #define led_turn_off(x) sl_si91x_simple_rgb_led_off(x)
    #define led_toggle(x) sl_si91x_simple_rgb_led_toggle(x)

#elif __has_include("sl_si91x_led_instances.h")
    #include "sl_si91x_led_instances.h"
    #include "sl_si91x_led.h"
    #define DETECTION_ON (led_led0)
    #define ACTIVITY_LED (&led_blue)
    #define led_init() led_init_instances()
    #define led_turn_on(x) sl_si91x_led_set(x.pin)
    #define led_turn_off(x) sl_si91x_led_clear(x.pin)
    #define led_toggle(x) sl_si91x_led_toggle(x.pin)

#else

    #error Platform LEDs not supported

#endif
