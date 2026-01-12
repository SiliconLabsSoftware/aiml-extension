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
#include "anomaly_detection.h"
#include "imu.h"
#include "constants.h"
#include "predictor.h"
#include "sl_tflite_micro_model.h"
#include "sl_tflite_micro_init.h"
#include "sl_sleeptimer.h"
#include "sl_status.h"
#include "sl_led.h"
#include "sl_simple_led_instances.h"
#include <cstdio>

static int input_length;
static volatile bool inference_timeout;
static sl_sleeptimer_timer_handle_t inference_timer;
static sl_sleeptimer_timer_handle_t led_timer;
static TfLiteTensor* model_input;
static tflite::MicroInterpreter* interpreter;

#ifndef NDEBUG
static uint32_t s_last_window_ms = 0;
static float    s_window_sample_hz = 0.0f;
static uint32_t s_rate_print_counter = 0;  // to throttle console prints
#endif

static const char* LABELS[SCENARIO_COUNT] = {
  "anomaly_jolt_drop",
  "anomaly_pick_move",
  "anomaly_shaking_vibrating",
  "anomaly_tilting",
  "no_anomaly_magnetometer",
  "no_anomaly_stationery",
  "no_anomaly_tilt",
  "no_anomaly_yaw"
};
// Triggered by the inference_timer
static void on_timeout_inference(sl_sleeptimer_timer_handle_t *handle, void* data)
{
  (void)handle; // unused
  (void)data; // unused
  inference_timeout = true;
}

// Triggered by the led_timer
void on_timeout_led(sl_sleeptimer_timer_handle_t *led_timer, void *data)
{
  (void)led_timer; // unused
  (void)data; // unused
  sl_led_turn_off(&sl_led_led0);
  sl_led_turn_off(&sl_led_led1);
}

void anomaly_detection_init(void)
{
  printf("Anomaly Detection\n");
  printf("init..\n");
  // Obtain pointer to the model's input tensor.
  model_input = sl_tflite_micro_get_input_tensor();
  interpreter = sl_tflite_micro_get_interpreter();
  if ((model_input->dims->size != 4) || (model_input->dims->data[0] != 1)
      || (model_input->dims->data[1] != SEQUENCE_LENGTH)
      || (model_input->dims->data[2] != IMU_CHANNELS)
      || (model_input->type != kTfLiteInt8)) {
    printf("%d %d %d %d %d",model_input->dims->size,model_input->dims->data[0],model_input->dims->data[1],model_input->dims->data[2],model_input->type);
    printf("error: bad input tensor parameters in model\n");
    EFM_ASSERT(false);
    return;
  }

  input_length = model_input->bytes / sizeof(float);

  sl_status_t setup_status = imu_setup();

  if (setup_status != SL_STATUS_OK) {
    printf("error: imu setup failed\n");
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

// LED feedback by class groups:
//    Group A (LED0): classes {0,1,3,6}
//    Group B (LED1): all other classes
// Prints result and toggle LEDs.
static void handle_output(int decision)
{
  bool groupA = (decision == 0) || (decision == 1) || (decision == 2) || (decision == 3);

   if (groupA) {
     sl_led_turn_on(&sl_led_led1);
     sl_led_turn_off(&sl_led_led0);
   } else {
     sl_led_turn_on(&sl_led_led0);
     sl_led_turn_off(&sl_led_led1);
   }

   sl_sleeptimer_start_timer_ms(&led_timer,
                                TOGGLE_DELAY_MS,
                                on_timeout_led,
                                NULL,
                                0,
                                SL_SLEEPTIMER_NO_HIGH_PRECISION_HF_CLOCKS_REQUIRED_FLAG);
}
static inline int32_t round_to_int32(float x)
{
  // round-half-away-from-zero
  return (int32_t)(x >= 0.0f ? x + 0.5f : x - 0.5f);
}

void anomaly_detection_loop(void)
{
  // 1) Read IMU window (keep buffer off the stack)
  static imu_data_float_t raw_data[SEQUENCE_LENGTH];
  sl_status_t status = imu_read(raw_data, SEQUENCE_LENGTH);
  if (status == SL_STATUS_FAIL) return;
#ifndef NDEBUG
  uint32_t now_ms = sl_sleeptimer_tick_to_ms(sl_sleeptimer_get_tick_count());
  if (s_last_window_ms != 0) {
    uint32_t dt_ms = now_ms - s_last_window_ms;
    if (dt_ms > 0) {
      // SEQUENCE_LENGTH samples arrived over dt_ms milliseconds
      s_window_sample_hz = (float)SEQUENCE_LENGTH * 1000.0f / (float)dt_ms;
    }
  }
  s_last_window_ms = now_ms;

  // Print occasionally to avoid spamming
  if (++s_rate_print_counter >= 10) {  // every 10 windows
    printf("[ANOM] window-rate: ~%.2f Hz (SEQUENCE_LENGTH=%d)\r\n",
           s_window_sample_hz, SEQUENCE_LENGTH);
    s_rate_print_counter = 0;
  }
#endif

  // 2) Quantization params for INT8 input
  if (model_input == NULL || model_input->type != kTfLiteInt8) return;
  int8_t*     input_buffer = model_input->data.int8;
  const float in_scale     = model_input->params.scale;
  const int   in_zp        = model_input->params.zero_point;
  if (in_scale == 0.0f) return;

  // 3) Per-channel z-score, then quantize to int8
  for (int i = 0; i < SEQUENCE_LENGTH; ++i) {
    const float v[IMU_CHANNELS] = {
      raw_data[i].x,  raw_data[i].y,  raw_data[i].z,
      raw_data[i].gx, raw_data[i].gy, raw_data[i].gz,
      raw_data[i].ox, raw_data[i].oy, raw_data[i].oz
    };

    for (int j = 0; j < IMU_CHANNELS; ++j) {
      const float z  = (v[j] - MEAN[j]) / STD[j];                 // <- your stats
      const float qf = z / in_scale + (float)in_zp;               // TFLM input quant
      int32_t qi = (int32_t)(qf >= 0.0f ? qf + 0.5f : qf - 0.5f); // round-half-away-from-zero
      if (qi > 127)  qi = 127;
      if (qi < -128) qi = -128;

      // NHWC layout with channels-last and last dim=1
      input_buffer[i * IMU_CHANNELS + j] = (int8_t)qi;
    }
  }

  // 4) Run at periodic cadence only
  if (!inference_timeout) return;
  inference_timeout = false;

  // 5) Invoke TFLM
  if (interpreter->Invoke() != kTfLiteOk) return;

  // 6) Read INT8 output -> dequantize to float scores
  TfLiteTensor* output = interpreter->output(0);
  if (output == NULL || output->type != kTfLiteInt8) return;

  // Determine num classes; accept [num_classes] or [1,num_classes]
  int out_size = 1;
  for (int d = 0; d < output->dims->size; ++d) out_size *= output->dims->data[d];
  if (output->dims->size >= 2 && output->dims->data[0] == 1) {
    out_size = output->dims->data[output->dims->size - 1];
  }
  if (out_size != SCENARIO_COUNT) {
    // Model/output size mismatch with constants
    // EFM_ASSERT(false);
    return;
  }

  const int8_t* out_i8   = output->data.int8;
  const float   out_scale = output->params.scale;
  const int     out_zp    = output->params.zero_point;

  model_output_t latest = {0};   // float scenario[SCENARIO_COUNT]
  for (int i = 0; i < SCENARIO_COUNT; ++i) {
    // model ends with softmax, this is ~probability
    latest.scenario[i] = ((float)out_i8[i] - (float)out_zp) * out_scale;
  }

  // 7) Use predictor’s rolling-average + suppression
  const int decided = predict_scenario(&latest);

  // 8) Act only on a valid scenario index
if (decided != NO_SCENARIO) {
  const char* name = (decided >= 0 && decided < SCENARIO_COUNT) ? LABELS[decided] : "unknown";
  printf("Detected Scenario %d (%s)\n", decided, name);
  handle_output(decided);
}

}


