/***************************************************************************//**
 * @file
 * @brief mic helper
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

#include "sl_gpio_board.h"
#include "sl_si91x_driver_gpio.h"

#define enable_microphone()                                                                   \
    sl_gpio_driver_init();                                                                    \
    sl_gpio_set_configuration(sl_si91x_gpio_pin_config_t{                                     \
        {sl_gpio_port_t(SL_SI91X_UULP_GPIO_0_PORT), SL_SI91X_UULP_GPIO_0_PIN}, GPIO_OUTPUT}); \
    sl_gpio_set_pin_output(sl_gpio_port_t(SL_SI91X_UULP_GPIO_0_PORT), SL_SI91X_UULP_GPIO_0_PIN)
