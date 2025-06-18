#pragma once

#include "sl_gpio_board.h"
#include "sl_si91x_driver_gpio.h"

#define enable_microphone()                                                                   \
    sl_gpio_driver_init();                                                                    \
    sl_gpio_set_configuration(sl_si91x_gpio_pin_config_t{                                     \
        {sl_gpio_port_t(SL_SI91X_UULP_GPIO_0_PORT), SL_SI91X_UULP_GPIO_0_PIN}, GPIO_OUTPUT}); \
    sl_gpio_set_pin_output(sl_gpio_port_t(SL_SI91X_UULP_GPIO_0_PORT), SL_SI91X_UULP_GPIO_0_PIN)
