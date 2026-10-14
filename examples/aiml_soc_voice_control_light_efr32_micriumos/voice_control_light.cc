/***************************************************************************//**
 * @file
 * @brief Top level application functions
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
#include "os.h"
#include "sl_power_manager.h"
#include "sl_status.h"
#include "sl_led.h"
#include "sl_simple_led_instances.h"
#include "voice_control_light.h"
#include "recognize_commands.h"
#include "config/voice_control_light_config.h"
#include "sl_ml_model_keyword_spotting_on_off_v2.h"
#include "sl_fe_audio.h"
#include "sl_fe_audio_instances.h"

#include "sl_sleeptimer.h"
#include <cinttypes>
#include "tflite_micro_model.hpp"

/*******************************************************************************
 *******************************   DEFINES   ***********************************
 ******************************************************************************/
#ifndef TASK_STACK_SIZE
#define TASK_STACK_SIZE       512
#endif

#ifndef TASK_PRIORITY
#define TASK_PRIORITY         20
#endif

/*******************************************************************************
 ***************************  LOCAL VARIABLES   ********************************
 ******************************************************************************/
using namespace npu_toolkit;

// Pointer to RecognizeCommands object for handling recognitions.
static RecognizeCommands *command_recognizer = nullptr;

// Micrium task control block
static OS_TCB tcb;

// Micrium task stack
static CPU_STK stack[TASK_STACK_SIZE];

/*******************************************************************************
 **************************   LOCAL FUNCTIONS    *******************************
 ******************************************************************************/

// Application task
static void voice_control_light_task(void *arg);

/***************************************************************************//**
 * Run model inference
 *
 * Copies the currently available data from the feature_buffer into the input
 * tensor and runs inference, updating the global output_tensor.
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
 * Processes the output from a model inference
 ******************************************************************************/
static sl_status_t process_output(){
  // Determine whether a command was recognized based on the output of inference
  uint8_t found_command_index = 0;
  uint8_t score = 0;
  bool is_new_command = false;
  uint32_t current_time_stamp;

  // Get current time stamp needed by CommandRecognizer
  current_time_stamp = sl_sleeptimer_tick_to_ms(sl_sleeptimer_get_tick_count());

  // Perform post processing and filtering of the model output to get a labeled output with a score value.
  TfLiteStatus process_status = command_recognizer->ProcessLatestResults(
      sl_ml_keyword_spotting_on_off_v2_model_handle.output_tensor(0), current_time_stamp, &found_command_index, &score, &is_new_command);

  if (process_status != kTfLiteOk) {
    return SL_STATUS_FAIL;
  }

  if (is_new_command) {
        // Print heard command
    if (found_command_index == 0){
        // command = "on"
        sl_led_turn_on(&sl_led_led0);
        printf("Heard %s (%" PRIu8 ") @%" PRIu32 "ms\r\n", kCategoryLabels[found_command_index],
                         score, current_time_stamp);
    } else if (found_command_index == 1){
        // command = "off"
        sl_led_turn_off(&sl_led_led0);
        printf("Heard %s (%" PRIu8 ") @%" PRIu32 "ms\r\n", kCategoryLabels[found_command_index],
                         score, current_time_stamp);
    }
  }

  return SL_STATUS_OK;
}


/*******************************************************************************
 **************************   GLOBAL FUNCTIONS   *******************************
 ******************************************************************************/

/***************************************************************************//**
 * Initialize application.
 ******************************************************************************/
void voice_control_light_init(void)
{

  RTOS_ERR err;

  // Create Application Task
  char task_name[] = "voice control light task";
  OSTaskCreate(&tcb,
               task_name,
               voice_control_light_task,
               DEF_NULL,
               TASK_PRIORITY,
               &stack[0],
               (TASK_STACK_SIZE / 10u),
               TASK_STACK_SIZE,
               0u,
               0u,
               DEF_NULL,
               (OS_OPT_TASK_STK_CLR),
               &err);

  EFM_ASSERT((RTOS_ERR_CODE_GET(err) == RTOS_ERR_NONE));
}

/***************************************************************************//**
 * Voice control light task
 ******************************************************************************/
void voice_control_light_task(void *arg)
{
  RTOS_ERR err;
  (void)arg;

  printf("Voice Control Light\r\n");

  if (sl_ml_model_init(&sl_ml_keyword_spotting_on_off_v2_model_handle) != SL_STATUS_OK) {
    printf("Failed to load model\n");
    while (1)
      ;
  }

  if (!voice_control_light_config_load_model_settings(sl_ml_keyword_spotting_on_off_v2_model_handle.flatbuffer)) {
    printf("Failed to load app parameters from .tflite model\n");
    while (1)
      ;
  }

  printf("  threshold=%d\r\n", DETECTION_THRESHOLD);
  printf("  suppression time=%d ms\r\n", SUPPRESSION_TIME_MS);
  printf("Listening for keywords...\r\n");

  sl_ml_audio_feature_generation_init(&sl_fe_audio_keyword_spotting_on_off_v2_cfg);

  static RecognizeCommands static_recognizer(DETECTION_THRESHOLD,
                                             SUPPRESSION_TIME_MS);
  command_recognizer = &static_recognizer;

  // Add EM1 requirement to allow microphone sampling
  sl_power_manager_add_em_requirement(SL_POWER_MANAGER_EM1);

  while (1) {
    // Wait INFERENCE_INTERVAL_MS ms
    OSTimeDlyHMSM(0, 0, 0, INFERENCE_INTERVAL_MS, OS_OPT_TIME_PERIODIC, &err);
    EFM_ASSERT((RTOS_ERR_CODE_GET(err) == RTOS_ERR_NONE));

    // Perform a word detection
    sl_ml_audio_feature_generation_update_features();
    run_inference();
    process_output();
  }
}
