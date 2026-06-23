/***************************************************************************//**
 * @file app.cc
 * @brief application functions.
 *******************************************************************************
 * # License
 * <b>Copyright 2022 Silicon Laboratories Inc. www.silabs.com</b>
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
#include "timestamp.h"
#include "sl_ml_model_blink.h"
#include "sl_pwm_instances.h"
#include "sl_pwm.h"
#include "sl_sleeptimer.h"
#include <stdio.h>

#define TIME_DELAY_MS        50
#define INFERENCE_PER_CYCLE (1000 / TIME_DELAY_MS)
#define X_RANGE             (2.f * 3.14159265359f)

static int count = 0;
static bool blink_model_initialized = false;

static void handle_output(timestamp& ms, float x, float y);

void app_init(void)
{
  printf("Starting ML Blink Application\n");
  sl_pwm_start(&sl_pwm_led0);

  sl_status_t st = sl_ml_model_init(&sl_ml_blink_model_handle);
  if (st != SL_STATUS_OK) {
    printf("error: Failed to load model\n");
    return;
  }
  blink_model_initialized = true;
  printf("Model loaded successfully\n");
}

void app_process_action(void)
{
  if (!blink_model_initialized) {
    printf("error: blink model is not initialized\n");
    return;
  }
  timestamp ts;
  sl_sleeptimer_delay_millisecond(TIME_DELAY_MS);

  float position = (float)count / INFERENCE_PER_CYCLE;
  float x = position * X_RANGE;

  TfLiteTensor* input_tensor = sl_ml_blink_model_handle.input_tensor(0);
  int8_t x_quantized = x / input_tensor->params.scale + input_tensor->params.zero_point;
  input_tensor->data.int8[0] = x_quantized;

  ts.start();
  sl_status_t inference_status = sl_ml_model_run(&sl_ml_blink_model_handle);
  ts.stop();

  if (inference_status != SL_STATUS_OK) {
    printf("error: inference failed, x=%f status=%d\n", x, (int)inference_status);
    return;
  }

  TfLiteTensor* output_tensor = sl_ml_blink_model_handle.output_tensor(0);
  int8_t y_quantized = output_tensor->data.int8[0];
  float y = (y_quantized - output_tensor->params.zero_point) * output_tensor->params.scale;

  handle_output(ts, x, y);

  count += 1;
  if (count >= INFERENCE_PER_CYCLE) {
    count = 0;
  }
}

static void handle_output(timestamp& ms, float x, float y)
{
  uint8_t pwm_percent = (uint8_t)(((y + 1)) * 50);

  printf("x=%f y=%f (%.3f ms)\n", x, y, ms.duration_ms());
  sl_pwm_set_duty_cycle(&sl_pwm_led0, pwm_percent);
}

void HardFault_Handler(void)
{
  printf("error: HardFault\n");
  while (1)
    ;
}
