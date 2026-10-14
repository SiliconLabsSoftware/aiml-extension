/***************************************************************************//**
 * @file
 * @brief Keyword detection application config (model metadata + recognition tuning)
 *******************************************************************************
 * # License
 * <b>Copyright 2022 Silicon Laboratories Inc. www.silabs.com</b>
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

#ifndef KEYWORD_DETECTION_CONFIG_H
#define KEYWORD_DETECTION_CONFIG_H

#include "tflite_model_parameters.hpp"

#ifdef __cplusplus
extern "C" {
#endif

extern int SL_TFLITE_MODEL_AVERAGE_WINDOW_DURATION_MS;
extern int SL_TFLITE_MODEL_MINIMUM_COUNT;
extern int SL_TFLITE_MODEL_DETECTION_THRESHOLD;
extern int SL_TFLITE_MODEL_SUPPRESSION_MS;
extern int SL_TFLITE_MODEL_VERBOSE_MODEL_OUTPUT_LOGS;
extern int SL_TFLITE_MODEL_INFERENCE_INTERVAL_MS;
extern npu_toolkit::StringList SL_TFLITE_MODEL_CLASSES;
extern int SL_TFLITE_MODEL_BAUD_RATE;

// <<< Use Configuration Wizard in Context Menu >>>
// <h> Keyword detection configuration
// <i> Defaults are overridden by parameters embedded in the .tflite flatbuffer.

// <o SMOOTHING_WINDOW_DURATION_MS> Smoothing window duration [ms] <300-1000>
#define SMOOTHING_WINDOW_DURATION_MS SL_TFLITE_MODEL_AVERAGE_WINDOW_DURATION_MS

// <o MINIMUM_DETECTION_COUNT> Minimum detection count <0-50>
#define MINIMUM_DETECTION_COUNT SL_TFLITE_MODEL_MINIMUM_COUNT

// <o DETECTION_THRESHOLD> Detection Threshold <0-255>
#define DETECTION_THRESHOLD SL_TFLITE_MODEL_DETECTION_THRESHOLD

// <o SUPPRESSION_TIME_MS> Suppression time after detection [ms] <0-2000>
#define SUPPRESSION_TIME_MS SL_TFLITE_MODEL_SUPPRESSION_MS

// <q IGNORE_UNDERSCORE_LABELS> Ignore labels with leading underscore
#define IGNORE_UNDERSCORE_LABELS 1

// <q VERBOSE_MODEL_OUTPUT_LOGS> Enable verbose model output logging
#define VERBOSE_MODEL_OUTPUT_LOGS SL_TFLITE_MODEL_VERBOSE_MODEL_OUTPUT_LOGS

// <o INFERENCE_INTERVAL_MS> Delay between each inference
#define INFERENCE_INTERVAL_MS SL_TFLITE_MODEL_INFERENCE_INTERVAL_MS

// <o MAX_CATEGORY_COUNT> Max number of categories supported.
#define MAX_CATEGORY_COUNT 32

// <o MAX_RESULT_COUNT> Max number of results supported.
#define MAX_RESULT_COUNT 50

// <o CATEGORY_LABELS> Label for each category (from model metadata when loaded)
#define CATEGORY_LABELS SL_TFLITE_MODEL_CLASSES

// <<< end of configuration section >>>

bool keyword_detection_config_load_model_settings(const void* tflite_flatbuffer);

#ifdef __cplusplus
}
#endif

#endif // KEYWORD_DETECTION_CONFIG_H
