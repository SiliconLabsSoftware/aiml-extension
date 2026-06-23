/***************************************************************************/ /**
 * @file app_ml_bridge.cc
 * @brief C-callable bridge for LED and mic init (C++); voice task in voice_control_light.cc.
 *******************************************************************************
 * # License
 * <b>Copyright 2024 Silicon Laboratories Inc. www.silabs.com</b>
 *******************************************************************************
 *
 * SPDX-License-Identifier: Zlib
 *
 * LED and mic behavior come from led_helper.hpp and mic_helper.hpp (macros / board
 * selection via __has_include and runtime-generated instance headers). This file
 * only exposes extern "C" entry points for app.c.
 *
 ******************************************************************************/

#include "app_ml_bridge.h"
#include "voice_control_light.h"
#ifdef __cplusplus
extern "C" {
#endif
#include "led_helper.hpp"
#ifdef __cplusplus
}
#endif

#include "mic_helper.hpp"

void app_led_init(void) {
  led_init();
}

void app_enable_mic(void) {
  enable_microphone();
}

void voice_control_light_task(void) {
  voice_control_light_task_cpp();
}