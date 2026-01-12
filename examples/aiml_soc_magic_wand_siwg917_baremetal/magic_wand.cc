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
#include "magic_wand.h"
#include "accelerometer.h"
#include "constants.h"
#include "predictor.h"
#include "magic_wand_model_generated.h"
#include "sl_sleeptimer.h"
#include "sl_status.h"
#include "sl_si91x_rgb_led.h"
#include "sl_si91x_rgb_led_instances.h"
#include "sl_si91x_icm40627.h"
#include "rgb_led.h"
#include "ml/platform/ml_clock_helper.h"
#include <cstdio>

using namespace npu_toolkit;

/*******************************************************************************
 ***************************  LOCAL VARIABLES   *******************************
 ******************************************************************************/

// TensorFlow Lite model
TfliteMicroModel model;

// Model input configuration
static int input_length;

// Timer and inference control
static volatile bool inference_timeout;
static sl_sleeptimer_timer_handle_t inference_timer;
static sl_sleeptimer_timer_handle_t led_timer;

// Unused model components (kept for potential future use)
static TfLiteTensor* model_input;
static tflite::MicroInterpreter* interpreter;


/*******************************************************************************
 **************************   LOCAL FUNCTIONS   *******************************
 ******************************************************************************/

/**
 * @brief Callback function triggered by the inference timer
 * @param[in] handle Timer handle (unused)
 * @param[in] data Timer data (unused)
 */
static void on_timeout_inference(sl_sleeptimer_timer_handle_t *handle, void* data)
{
  (void)handle; // unused
  (void)data;   // unused
  inference_timeout = true;
}

/**
 * @brief Callback function triggered by the LED timer
 * @param[in] led_timer Timer handle (unused)
 * @param[in] data Timer data (unused)
 */
void on_timeout_led(sl_sleeptimer_timer_handle_t *led_timer, void *data)
{
  (void)led_timer; // unused
  (void)data;      // unused

  // Turn off all LEDs after timeout
  turn_off_green();
  turn_off_red();
  turn_off_blue();
}

/*******************************************************************************
 **************************   GLOBAL FUNCTIONS   *******************************
 ******************************************************************************/

/**
 * @brief Initialize the Magic Wand application
 */
void magic_wand_init(void)
{
  printf("Magic Wand\n");
  printf("Initializing...\n");

  // Configure clocks to maximum rate for optimal performance
  ml_configure_clocks_to_max_rate();

  // Load the TensorFlow Lite model
  if (!load_tflite_micro_model(model)) {
    printf("Failed to load model\n");
    for (;;) {
      // Infinite loop on failure
    }
  }

  // Validate model input tensor
  auto& input_tensor = *model.input(0);
  if ((input_tensor.dims->size != 4) || (input_tensor.dims->data[0] != 1)
      || (input_tensor.dims->data[1] != SEQUENCE_LENGTH)
      || (input_tensor.dims->data[2] != ACCELEROMETER_CHANNELS)
      || (input_tensor.type != kTfLiteFloat32)) {
    printf("Error: bad input tensor parameters in model\n");
    EFM_ASSERT(false);
    return;
  }

  input_length = input_tensor.bytes / sizeof(float);

  // Setup accelerometer
  sl_status_t setup_status = accelerometer_setup();
  if (setup_status != SL_STATUS_OK) {
    printf("Error: accelerometer setup failed\n");
    EFM_ASSERT(false);
    return;
  }

  // Wait for accelerometer to become ready
  while (true) {
    sl_status_t status = accelerometer_read(NULL, 0);
    if (status == SL_STATUS_OK) {
      break;
    }
  }

  // Start periodic inference timer
  inference_timeout = false;
  sl_sleeptimer_start_periodic_timer_ms(&inference_timer, INFERENCE_PERIOD_MS, 
                                        on_timeout_inference, NULL, 0, 0);
  printf("Ready\n");
}


/**
 * @brief Handle gesture detection output and control LEDs
 * @param[in] gesture Detected gesture type
 * 
 * LED mapping:
 * - Green: Wing gesture (W)
 * - Red: Ring gesture (O) 
 * - Blue: Slope gesture (L)
 */
static void handle_output(int gesture)
{
  uint32_t timestamp = sl_sleeptimer_tick_to_ms(sl_sleeptimer_get_tick_count());

  switch (gesture) {
    case WING_GESTURE:
      printf("t=%lu detection=wing (W)\n", timestamp);
      turn_on_green();
      sl_sleeptimer_start_timer_ms(&led_timer, TOGGLE_DELAY_MS, on_timeout_led, NULL, 0,
                                   SL_SLEEPTIMER_NO_HIGH_PRECISION_HF_CLOCKS_REQUIRED_FLAG);
      break;

    case RING_GESTURE:
      printf("t=%lu detection=ring (O)\n", timestamp);
      turn_on_red();
      sl_sleeptimer_start_timer_ms(&led_timer, TOGGLE_DELAY_MS, on_timeout_led, NULL, 0,
                                   SL_SLEEPTIMER_NO_HIGH_PRECISION_HF_CLOCKS_REQUIRED_FLAG);
      break;

    case SLOPE_GESTURE:
      printf("t=%lu detection=slope (L)\n", timestamp);
      turn_on_blue();
      sl_sleeptimer_start_timer_ms(&led_timer, TOGGLE_DELAY_MS, on_timeout_led, NULL, 0,
                                   SL_SLEEPTIMER_NO_HIGH_PRECISION_HF_CLOCKS_REQUIRED_FLAG);
      break;

    case NO_GESTURE:
      // No gesture detected - no action needed
      break;

    default:
      // Unknown gesture - no action needed
      break;
  }
}

/**
 * @brief Main processing loop for Magic Wand application
 */
void magic_wand_loop(void)
{
  // Get input tensor from the model
  auto& input_tensor = *model.input(0);

  // Read accelerometer data into model input
  acc_data_t *dst = (acc_data_t *)input_tensor.data.f;
  size_t n = input_length / 3;
  sl_status_t status = accelerometer_read(dst, n);

  // If there was no new data, wait until next time
  if (status == SL_STATUS_FAIL) {
    return;
  }

  // Inference is triggered periodically by a timer
  if (inference_timeout) {
    inference_timeout = false;

    // Run model inference
    if (!model.invoke()) {
      printf("Failed to invoke model\n");
      for (;;) {
        // Infinite loop on failure
      }
    }

    // Get model output
    auto& output_tensor = *model.output(0);

    // Analyze the results to obtain a prediction
    const model_output_t *output = (const model_output_t *)output_tensor.data.f;
    int gesture = predict_gesture(output);

    // Handle the detected gesture
    handle_output(gesture);
  }
}
