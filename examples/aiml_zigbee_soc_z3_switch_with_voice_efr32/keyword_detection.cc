/***************************************************************************//**
 * @file
 * @brief Keyword detection using ML compiler generated model (TFLM + MVP)
 *******************************************************************************
 * # License
 * <b>Copyright 2021 Silicon Laboratories Inc. www.silabs.com</b>
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
#include "sl_status.h"
#include "sl_sleeptimer.h"
#include <cinttypes>
#ifdef SL_COMPONENT_CATALOG_PRESENT
#include "sl_component_catalog.h"
#endif
#if defined(SL_CATALOG_POWER_MANAGER_PRESENT)
#include "sl_power_manager.h"
#endif

#include "keyword_detection.h"
#include "recognize_commands.h"
#include "config/keyword_detection_config.h"

#include "sl_fe_audio.h"
#include "sl_fe_audio_instances.h"
#include "tflite_micro_model.hpp"
#include "sl_ml_model_keyword_spotting_on_off_v2.h"

#include <cstdio>

/*******************************************************************************
 *******************************   DEFINES   ***********************************
 ******************************************************************************/

/* INFERENCE_INTERVAL_MS is supplied by keyword_detection_config.h after
 * keyword_detection_config_load_model_settings() reads latency_ms from the
 * model flatbuffer metadata (default 200 ms if missing).
 *
 * Legacy default when the interval was not model-driven (kept for reference):
 *   #ifndef INFERENCE_INTERVAL_MS
 *   #define INFERENCE_INTERVAL_MS 200
 *   #endif
 */

/*******************************************************************************
 ***************************  LOCAL VARIABLES   ********************************
 ******************************************************************************/

using namespace npu_toolkit;

// Pointer to RecognizeCommands object for handling recognitions.
static RecognizeCommands *command_recognizer = nullptr;

static volatile bool inference_timeout;
static sl_sleeptimer_timer_handle_t inference_timer;

/***************************************************************************//**
 * Run model inference
 *
 * Copies the currently available data from the feature buffer into the input
 * tensor and runs inference on the ML-compiler-generated
 * keyword_spotting_on_off_v2 model, updating the output tensor.
 *
 * @return SL_STATUS_OK on success, other value on failure.
 ******************************************************************************/
static sl_status_t run_inference()
{
  // Update model input tensor
  sl_status_t status =
      sl_ml_audio_feature_generation_fill_tensor(sl_ml_keyword_spotting_on_off_v2_model_handle.input_tensor(0));
  if (status != SL_STATUS_OK) {
    return SL_STATUS_FAIL;
  }
  // Run the model on the spectrogram input and make sure it succeeds.
  if (sl_ml_model_run(&sl_ml_keyword_spotting_on_off_v2_model_handle) != SL_STATUS_OK) {
    return SL_STATUS_FAIL;
  }

  (void)sl_ml_keyword_spotting_on_off_v2_model_handle.output_tensor(0);
  return SL_STATUS_OK;
}

/***************************************************************************//**
 * Processes the output from the output tensor
 *
 * Determine whether a command was recognized based on the output of inference.
 * Feeds the latest classification scores to RecognizeCommands and, when a new
 * command is detected for class indices 0 or 1, notifies the Zigbee app.
 *
 * @return SL_STATUS_OK on success, other value on failure.
 ******************************************************************************/
static sl_status_t process_output()
{
  // Determine whether a command was recognized based on the output of inference
  // (same flow as legacy sl_tflite_micro_get_output_tensor() path).
  uint8_t found_command_index = 0;
  uint8_t score = 0;
  bool is_new_command = false;

  // Get current time stamp needed by CommandRecognizer
  uint32_t current_time_stamp =
      sl_sleeptimer_tick_to_ms(sl_sleeptimer_get_tick_count());

  TfLiteStatus process_status = command_recognizer->ProcessLatestResults(
      sl_ml_keyword_spotting_on_off_v2_model_handle.output_tensor(0), (int32_t)current_time_stamp, &found_command_index, &score, &is_new_command);

  if (process_status != kTfLiteOk) {
    return SL_STATUS_FAIL;
  }

  if (is_new_command) {
    if (found_command_index == 0 || found_command_index == 1) {
      printf("Heard %s (%" PRIu8 ") @%" PRIu32 "ms\r\n",
             get_category_label((int)found_command_index),
             score, current_time_stamp);
      keyword_detected(found_command_index);
    }
  }

  return SL_STATUS_OK;
}

/***************************************************************************//**
 * Inference timer callback
 *
 * Sets a flag polled from the Zigbee main tick so inference runs in task
 * context, not directly from the timer callback.
 *
 * @param handle sleeptimer handle (unused)
 * @param data    user context (unused)
 ******************************************************************************/
static void inference_timer_callback(sl_sleeptimer_timer_handle_t *handle, void *data)
{
  (void)handle;
  (void)data;
  inference_timeout = true;
}

/*******************************************************************************
 **************************   GLOBAL FUNCTIONS   *******************************
 ******************************************************************************/

// extern "C": keyword_detection_init / process_action / get_category_label /
// category_count are called from app.c (C); keep unmangled linkage.

extern "C" {

int category_count = 0;

/***************************************************************************//**
 * Get the label string for a classification index (from model metadata).
 ******************************************************************************/
const char *get_category_label(int index)
{
  if ((index >= 0) && (index < category_count)) {
    return CATEGORY_LABELS[index];
  }
  return "?";
}

/***************************************************************************//**
 * Initialize keyword detection (model, audio frontend, recognizer, timer).
 *
 * Call once from the application main-init callback after the Zigbee stack
 * is ready. Blocks with a tight loop on fatal configuration errors.
 ******************************************************************************/
void keyword_detection_init(void)
{
#if defined(SL_CATALOG_POWER_MANAGER_PRESENT)
  // Add EM1 requirement to allow for continuous microphone sampling
  sl_power_manager_add_em_requirement(SL_POWER_MANAGER_EM1);
#endif

  // Load compiled TFLM model (autogen from ml_compiler + keyword_spotting_on_off_v2.tflite)
  if (sl_ml_model_init(&sl_ml_keyword_spotting_on_off_v2_model_handle) != SL_STATUS_OK) {
    printf("Failed to load keyword model\r\n");
    while (1) {
      ;
    }
  }

  // Application recognition parameters from flatbuffer metadata
  if (!keyword_detection_config_load_model_settings(sl_ml_keyword_spotting_on_off_v2_model_handle.flatbuffer)) {
    printf("Failed to load app parameters from .tflite model\r\n");
    while (1) {
      ;
    }
  }

  if (sl_ml_audio_feature_generation_init(&sl_fe_audio_keyword_spotting_on_off_v2_cfg) != SL_STATUS_OK) {
    printf("Failed to init audio feature generator\r\n");
    while (1) {
      ;
    }
  }

  const TfLiteTensor *output = sl_ml_keyword_spotting_on_off_v2_model_handle.output_tensor(0);
  // Validate model output shape [1, num_classes]
  if ((output->dims->size == 2) && (output->dims->data[0] == 1)) {
    category_count = output->dims->data[1];
  } else {
    printf("ERROR: Invalid output tensor shape\r\n");
    while (1) {
      ;
    }
  }

  if (category_count != static_cast<int>(SL_TFLITE_MODEL_CLASSES.size())) {
    printf("WARNING: categories(%d) != labels(%ld); check model classes metadata.\r\n",
           category_count,
           (long)SL_TFLITE_MODEL_CLASSES.size());
  }

  // Instantiate command recognizer (smoothing, threshold, suppression from metadata)
  static RecognizeCommands static_recognizer(SMOOTHING_WINDOW_DURATION_MS,
                                             DETECTION_THRESHOLD,
                                             SUPPRESSION_TIME_MS,
                                             MINIMUM_DETECTION_COUNT,
                                             IGNORE_UNDERSCORE_LABELS);
  command_recognizer = &static_recognizer;

  // Start periodic timer for inference interval (latency_ms from metadata)
  sl_sleeptimer_start_periodic_timer_ms(&inference_timer,
                                        (uint32_t)INFERENCE_INTERVAL_MS,
                                        inference_timer_callback,
                                        NULL,
                                        0,
                                        0);
}

/***************************************************************************//**
 * Keyword detection process action
 *
 * Call from sl_zigbee_af_main_tick_cb each main-loop iteration. When the
 * inference timer has fired, updates audio features, runs inference, and runs
 * the recognizer on the latest output.
 ******************************************************************************/
void keyword_detection_process_action(void)
{
  // Perform keyword detection every INFERENCE_INTERVAL_MS ms (from model metadata)
  if (inference_timeout) {
    sl_ml_audio_feature_generation_update_features();
    run_inference();
    process_output();
    inference_timeout = false;
  }
}

}  // extern "C"
