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

    #define DETECTION_ON (&led_red)
    #define ACTIVITY_LED (&led_blue)

    #define led_init() rgb_led_init_instances()
    #define led_turn_on(x) sl_si91x_rgb_led_on(x)
    #define led_turn_off(x) sl_si91x_rgb_led_off(x)
    #define led_toggle(x) sl_si91x_rgb_led_toggle(x)

#else

    #error Platform LEDs not supported

#endif
