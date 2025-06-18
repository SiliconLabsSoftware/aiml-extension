/***************************************************************************/ /**
 * @file app.c
 * @brief Top level application functions
 *******************************************************************************
 * # License
 * <b>Copyright 2023 Silicon Laboratories Inc. www.silabs.com</b>
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

#include "app.h"

#include "ml/platform/ml_clock_helper.h"
#include "sl_sleeptimer.h"
#include "sl_system_init.h"
#include "voice_control_light.h"
#ifdef __cplusplus
extern "C" {
#endif
#include "led_helper.hpp"
#ifdef __cplusplus
}
#endif

#include "em_cmu.h"
#include "mic_helper.hpp"

/*******************************************************************************
 * Initialize application.
 *
 ******************************************************************************/
extern void app_init(void) {
    ml_configure_clocks_to_max_rate();

    // Initialize the LEDs
    led_init();
    // Enable the microphone
    enable_microphone();
}

/*******************************************************************************
 * App ticking function.
 ******************************************************************************/
extern void app_process_action(void) {
    voice_control_light_task();
}
