/***************************************************************************//**
 * @file
 * @brief Voice control light with sequential sine blink model loading.
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
#include "sl_pwm.h"
#include "sl_pwm_instances.h"
#include "voice_control_light.h"
#include "recognize_commands.h"
#include "config/voice_control_light_config.h"
#include "sl_ml_model_keyword_spotting_on_off_v2.h"
#include "sl_ml_model_blink.h"
#include "sl_ml_tflite_micro_model.h"
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

// Use a visible PWM update interval for the sine-driven LED animation.
#ifndef SINE_STEP_DELAY_MS
#define SINE_STEP_DELAY_MS 50
#endif

// Number of samples used to cover one complete sine-wave phase.
#ifndef SINE_SAMPLES_PER_CYCLE
#define SINE_SAMPLES_PER_CYCLE 20
#endif
#define SINE_X_RANGE (2.f * 3.14159265359f)

#ifndef SINE_WAVE_LOOPS
#define SINE_WAVE_LOOPS 3
#endif

/*******************************************************************************
 ***************************  LOCAL VARIABLES   ********************************
 ******************************************************************************/
using namespace npu_toolkit;

// Handles keyword recognition smoothing and suppression.
static RecognizeCommands *command_recognizer = nullptr;

// Micrium task control block
static OS_TCB tcb;

// Micrium task stack
static CPU_STK stack[TASK_STACK_SIZE];

// Last command handled: 0 = "on", 1 = "off", 0xFF = none.
// Repeated detections after suppression are ignored so the sine animation only
// runs when the command state changes from off to on.
static uint8_t last_light_voice_cmd = 0xFFu;

/*******************************************************************************
 **************************   LOCAL FUNCTIONS    *******************************
 ******************************************************************************/

// MicriumOS task entry point.
static void voice_control_light_task(void *arg);

/***************************************************************************//**
 * Switch runtime memory from keyword spotting to the sine blink model.
 ******************************************************************************/
static bool switch_to_blink(void)
{
  if (sl_ml_model_deinit(&sl_ml_keyword_spotting_on_off_v2_model_handle) != SL_STATUS_OK) {
    printf("sinwave: seq keyword deinit fail\r\n");
    return false;
  }
  if (sl_ml_model_init(&sl_ml_blink_model_handle) != SL_STATUS_OK) {
    printf("sinwave: seq blink init fail\r\n");
    while (1) {}
  }
  printf("Model switch to blink success\r\n");
  return true;
}

/***************************************************************************//**
 * Switch runtime memory from the sine blink model back to keyword spotting.
 ******************************************************************************/
static void switch_to_keyword(void)
{
  (void)sl_ml_model_deinit(&sl_ml_blink_model_handle);
  if (sl_ml_model_init(&sl_ml_keyword_spotting_on_off_v2_model_handle) != SL_STATUS_OK) {
    printf("sinwave: seq keyword reinit fail\r\n");
    while (1) {}
  }
  printf("Model switch to keyword success\r\n");
}

/***************************************************************************//**
 * Run one sine blink animation, then return to keyword spotting.
 ******************************************************************************/
static bool sinwave_step(void)
{
  if (!switch_to_blink()) {
    return false;
  }

  for (uint32_t loop = 0; loop < (uint32_t)SINE_WAVE_LOOPS; ++loop) {
    for (uint32_t s = 0; s < (uint32_t)SINE_SAMPLES_PER_CYCLE; ++s) {
      const float position = (float)s / (float)SINE_SAMPLES_PER_CYCLE;
      const float x = position * SINE_X_RANGE;

      TfLiteTensor *input_tensor = sl_ml_blink_model_handle.input_tensor(0);
      int8_t x_quantized = x / input_tensor->params.scale + input_tensor->params.zero_point;
      input_tensor->data.int8[0] = x_quantized;

      if (sl_ml_model_run(&sl_ml_blink_model_handle) != SL_STATUS_OK) {
        sl_pwm_set_duty_cycle(&sl_pwm_led0, 0);
        switch_to_keyword();
        return false;
      }

      TfLiteTensor *output_tensor = sl_ml_blink_model_handle.output_tensor(0);
      int8_t y_quantized = output_tensor->data.int8[0];
      float y = (y_quantized - output_tensor->params.zero_point) * output_tensor->params.scale;

      uint8_t pwm_percent = (uint8_t)(((y + 1.f)) * 50.f);
      if (pwm_percent > 100U) {
        pwm_percent = 100U;
      }
      sl_pwm_set_duty_cycle(&sl_pwm_led0, pwm_percent);
      sl_sleeptimer_delay_millisecond(SINE_STEP_DELAY_MS);
    }
  }

  sl_pwm_set_duty_cycle(&sl_pwm_led0, 100);

  switch_to_keyword();
  return true;
}

/***************************************************************************//**
 * Run keyword spotting inference.
 *
 * Copies the latest audio features into the keyword spotting input tensor and
 * runs inference, updating the model output tensor.
 ******************************************************************************/
static sl_status_t run_inference()
{
  // Fill the keyword model input tensor from the latest audio features.
  sl_status_t status =
      sl_ml_audio_feature_generation_fill_tensor(sl_ml_keyword_spotting_on_off_v2_model_handle.input_tensor(0));
  if (status != SL_STATUS_OK) {
    return SL_STATUS_FAIL;
  }
  // Run keyword spotting on the spectrogram input.
  if (sl_ml_model_run(&sl_ml_keyword_spotting_on_off_v2_model_handle) != SL_STATUS_OK) {
    return SL_STATUS_FAIL;
  }

  (void)sl_ml_keyword_spotting_on_off_v2_model_handle.output_tensor(0);
  return SL_STATUS_OK;
}


/***************************************************************************//**
 * Process keyword spotting output.
 ******************************************************************************/
static sl_status_t process_output(){
  // Determine whether a spoken command was recognized.
  uint8_t found_command_index = 0;
  uint8_t score = 0;
  bool is_new_command = false;
  uint32_t current_time_stamp;

  // Provide the recognizer with a timestamp for suppression filtering.
  current_time_stamp = sl_sleeptimer_tick_to_ms(sl_sleeptimer_get_tick_count());

  // Convert model output scores into a filtered command decision.
  TfLiteStatus process_status = command_recognizer->ProcessLatestResults(
      sl_ml_keyword_spotting_on_off_v2_model_handle.output_tensor(0), current_time_stamp, &found_command_index, &score, &is_new_command);

  if (process_status != kTfLiteOk) {
    return SL_STATUS_FAIL;
  }

  if (is_new_command) {
    if (found_command_index == 0) {
      if (last_light_voice_cmd != 0u) {
        printf("Heard %s (%" PRIu8 ") @%" PRIu32 "ms\r\n", kCategoryLabels[found_command_index],
               score, current_time_stamp);
        if (sinwave_step()) {
          last_light_voice_cmd = 0u;
        }
      }
    } else if (found_command_index == 1) {
      if (last_light_voice_cmd != 1u) {
        sl_pwm_set_duty_cycle(&sl_pwm_led0, 0);
        printf("Heard %s (%" PRIu8 ") @%" PRIu32 "ms\r\n", kCategoryLabels[found_command_index],
               score, current_time_stamp);
        last_light_voice_cmd = 1u;
      }
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

  // Create the MicriumOS application task.
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

  printf("Voice Control Light + Sine Blink\r\n");
  printf("Model load mode: sequential\r\n");

  if (sl_ml_model_init(&sl_ml_keyword_spotting_on_off_v2_model_handle) != SL_STATUS_OK) {
    printf("Failed to load model\n");
    while (1) {}
  }

  if (!voice_control_light_config_load_model_settings(sl_ml_keyword_spotting_on_off_v2_model_handle.flatbuffer)) {
    printf("Failed to load app parameters from .tflite model\n");
    while (1) {}
  }

  printf("  threshold=%d\r\n", DETECTION_THRESHOLD);
  printf("  suppression time=%d ms\r\n", SUPPRESSION_TIME_MS);
  printf("Listening for keywords...\r\n");

  sl_ml_audio_feature_generation_init(&sl_fe_audio_keyword_spotting_on_off_v2_cfg);

  static RecognizeCommands static_recognizer(DETECTION_THRESHOLD,
                                             SUPPRESSION_TIME_MS);
  command_recognizer = &static_recognizer;

  sl_pwm_start(&sl_pwm_led0);
  sl_pwm_set_duty_cycle(&sl_pwm_led0, 0);

  // Add EM1 requirement to allow microphone sampling
  sl_power_manager_add_em_requirement(SL_POWER_MANAGER_EM1);

  while (1) {
    // Wait until the next inference interval.
    OSTimeDlyHMSM(0, 0, 0, INFERENCE_INTERVAL_MS, OS_OPT_TIME_PERIODIC, &err);
    EFM_ASSERT((RTOS_ERR_CODE_GET(err) == RTOS_ERR_NONE));

    // Update audio features and process the latest keyword result.
    sl_ml_audio_feature_generation_update_features();
    run_inference();
    process_output();
  }
}
