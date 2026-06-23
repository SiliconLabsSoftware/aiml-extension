/***************************************************************************/ /**
 * @file app_ml_bridge.h
 * @brief C-callable declarations for ML voice / LED / mic (implemented in C++).
 *******************************************************************************
 * # License
 * <b>Copyright 2024 Silicon Laboratories Inc. www.silabs.com</b>
 *******************************************************************************
 *
 * SPDX-License-Identifier: Zlib
 *
 ******************************************************************************/

#ifndef APP_ML_BRIDGE_H
#define APP_ML_BRIDGE_H

#ifdef __cplusplus
extern "C" {
#endif

void app_led_init(void);
void app_enable_mic(void);
void voice_control_light_task(void);

#ifdef __cplusplus
}
#endif

#endif /* APP_ML_BRIDGE_H */
