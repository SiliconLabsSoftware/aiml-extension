/***************************************************************************//**
 * @file
 * @brief Initalization and main functionality for application
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
#include "data_capture.h"
#include "imu.h"
#include "constants.h"
#include "sl_sleeptimer.h"
#include "sl_status.h"
#include "sl_led.h"
#include "sl_simple_led_instances.h"
#include "sl_ml_jlink_stream.hpp"
#include <cstdio>

static int input_length;
static volatile bool inference_timeout;
static sl_sleeptimer_timer_handle_t inference_timer;

// Triggered by the inference_timer
static void on_timeout_inference(sl_sleeptimer_timer_handle_t *handle, void* data)
{
  (void)handle; // unused
  (void)data; // unused
  inference_timeout = true;
}
void data_capture_init(void)
{
  printf("Data Capture\n");
  slx_ml_jlink_stream::slx_ml_register_stream("imu", slx_ml_jlink_stream::Write);
  sl_status_t setup_status = imu_setup();

  if (setup_status != SL_STATUS_OK) {
    printf("error: accelerometer setup failed\n");
    EFM_ASSERT(false);
    return;
  }

  // Waiting for accelerometer to become ready
  while (true) {
    sl_status_t status = imu_read(NULL, 0);
    if (status == SL_STATUS_OK) {
      break;
    }
  }

  inference_timeout = false;
  sl_sleeptimer_start_periodic_timer_ms(&inference_timer, INFERENCE_PERIOD_MS, on_timeout_inference, NULL, 0, 0);
  printf("ready\n");
}
void sl_ml_dump_feature_data(const imu_data_float_t* feature_data, uint32_t feature_length,const char* arg_type)
{
    bool connected = false;

    // Check if the Python script has connected
    slx_ml_jlink_stream::slx_ml_is_connected(arg_type, &connected);
    if(connected)
    {
        const uint32_t num_bytes=feature_length *sizeof(imu_data_float_t);
        slx_ml_jlink_stream::slx_ml_write_all(arg_type, feature_data, num_bytes);
    }
}
void data_capture_loop(void)
{
  static imu_data_float_t s_dst_buf[90];
  // Insert data from accelerometer to the model.
  size_t n =90 ;
  sl_status_t status = imu_read(s_dst_buf, n);

  // If there was no new data, wait until next time.
  if (status == SL_STATUS_FAIL) {
    return;
  }
  input_length=n;
  sl_ml_dump_feature_data(s_dst_buf,(uint32_t)input_length,"imu");
  }
