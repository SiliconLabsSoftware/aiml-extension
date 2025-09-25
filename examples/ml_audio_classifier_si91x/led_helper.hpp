#pragma once

#if defined(SI917_DEVKIT) && __has_include("sl_si91x_rgb_led_instances.h")
    #include "sl_si91x_rgb_led_instances.h"
    #define DETECTION_LED (&led_led0)
    #define ACTIVITY_LED (&led_led0)
    #define green_color 0x00FF00
    #define red_color 0xFF0000
    #define led_init() rgb_led_init_instances()
    #define led_turn_on(led_handle) sl_si91x_simple_rgb_led_on(led_handle)
    #define led_set_color(led_handle, color) sl_si91x_simple_rgb_led_set_colour(led_handle, color)
    #define led_turn_off(led_handle) sl_si91x_simple_rgb_led_off(led_handle)
    #define led_toggle(led_handle) sl_si91x_simple_rgb_led_toggle(led_handle)

#elif __has_include("sl_si91x_led_instances.h")
    #include "sl_si91x_led_instances.h"
    #include "sl_si91x_led.h"
    #define DETECTION_LED (&led_led0)
    #define ACTIVITY_LED (&led_led1)
    #define led_init() led_init_instances()
    #define led_turn_on(led_handle) sl_si91x_led_set(led_handle->pin)
    #define led_turn_off(led_handle) sl_si91x_led_clear(led_handle->pin)
    #define led_toggle(led_handle) sl_si91x_led_toggle(led_handle->pin)

#else
    #error Platform LEDs not supported
#endif
