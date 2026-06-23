/***************************************************************************//**
 * @file
 * @brief Voice control light application config (from model flatbuffer)
 *******************************************************************************
 * # License
 * <b>Copyright 2021 Silicon Laboratories Inc. www.silabs.com</b>
 *******************************************************************************
 *
 * The licensor of this software is Silicon Laboratories Inc.  Your use of this
 * software is governed by the terms of Silicon Labs Master Software License
 * Agreement (MSLA) available at
 * www.silabs.com/about-us/legal/master-software-license-agreement.  This
 * software is distributed to you in Source Code format and is governed by the
 * sections of the MSLA applicable to Source Code.
 *
 ******************************************************************************/

#ifndef VOICE_CONTROL_LIGHT_CONFIG_H
#define VOICE_CONTROL_LIGHT_CONFIG_H

#include "ml/tflite_micro_model/tflite_model_parameters.hpp"

#ifdef __cplusplus
extern "C" {
#endif

extern int SL_TFLITE_MODEL_DETECTION_THRESHOLD;
extern int SL_TFLITE_MODEL_SUPPRESSION_MS;
extern int SL_TFLITE_MODEL_INFERENCE_INTERVAL_MS;
extern int SL_TFLITE_MODEL_VERBOSE_MODEL_OUTPUT_LOGS;
extern npu_toolkit::StringList SL_TFLITE_MODEL_CLASSES;
extern int SL_TFLITE_MODEL_BAUD_RATE;

// <<< Use Configuration Wizard in Context Menu >>>
// <h> Keyword recognition configuration
// <i> Values below are filled from the .tflite metadata when the app starts.

// <o> Detection Threshold <0-255>
#define DETECTION_THRESHOLD SL_TFLITE_MODEL_DETECTION_THRESHOLD

// <o> Suppression time after detection [ms] <0-2000>
#define SUPPRESSION_TIME_MS SL_TFLITE_MODEL_SUPPRESSION_MS

// <o INFERENCE_INTERVAL_MS> Delay between each inference [ms]
#define INFERENCE_INTERVAL_MS SL_TFLITE_MODEL_INFERENCE_INTERVAL_MS

// <<< end of configuration section >>>

bool voice_control_light_config_load_model_settings(const void* tflite_flatbuffer);

#ifdef __cplusplus
}
#endif

#endif // VOICE_CONTROL_LIGHT_CONFIG_H
