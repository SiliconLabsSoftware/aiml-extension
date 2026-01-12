/***************************************************************************//**
 * @file
 * @brief ML profiler firmware application
 *******************************************************************************
 * # License
 * <b>Copyright 2025 Silicon Laboratories Inc. www.silabs.com</b>
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
#include "sl_power_manager.h"
#include "sl_status.h"
#include "sl_ml_profiler_firmware.h"
#include "sl_tflite_micro_init.h"
#include "sl_sleeptimer.h"
#include <cmath>
#include "sl_ml_profiler_debug_channel.h"
#include <stdlib.h>
#include <stdint.h>
#include <time.h>

/***************************************************************************//**
 * @brief
 *   Fill the input tensor with random data for testing.
 *
 * @param tensor Pointer to the input tensor to be filled.
 ******************************************************************************/
static void fill_random_input(TfLiteTensor* tensor) {
    // Seed RNG once
    srand((unsigned)time(NULL));

    int num_elements = 1;
    for (int i = 0; i < tensor->dims->size; i++) {
        num_elements *= tensor->dims->data[i];
    }

    switch (tensor->type) {
        case kTfLiteUInt8: {
            uint8_t* data = tensor->data.uint8;
            for (int i = 0; i < num_elements; i++) {
                data[i] = (uint8_t)(rand() % 256); // 0..255
            }
            break;
        }
        case kTfLiteInt8: {
            int8_t* data = tensor->data.int8;
            for (int i = 0; i < num_elements; i++) {
                data[i] = (int8_t)(rand() % 256 - 128); // -128..127
            }
            break;
        }
        case kTfLiteFloat32: {
            float* data = tensor->data.f;
            for (int i = 0; i < num_elements; i++) {
                data[i] = (float)rand() / (float)RAND_MAX; // 0.0..1.0
            }
            break;
        }
        case kTfLiteUInt16: {
            uint16_t* data = tensor->data.ui16;
            for (int i = 0; i < num_elements; i++) {
                data[i] = (uint16_t)(rand() % 65536); // 0..65535
            }
            break;
        }
        case kTfLiteInt16: {
            int16_t* data = tensor->data.i16;
            for (int i = 0; i < num_elements; i++) {
                data[i] = (int16_t)(rand() % 65536 - 32768); // -32768..32767
            }
            break;
        }
        default:
            printf("Unsupported tensor type: %d\n", tensor->type);
            break;
    }
}
/***************************************************************************//**
 * Run model inference
 *
 * Copies the currently available data from the feature_buffer into the input
 * tensor and runs inference, updating the global output tensor.
 *
 * @return
 *   SL_STATUS_OK on success, other value on failure.
 ******************************************************************************/
static sl_status_t run_inference()
{
  TfLiteTensor* input_tensor = sl_tflite_micro_get_input_tensor();
  //memset(input_tensor->data.int8, 0, input_tensor->bytes);
  // Fill random input data for testing
  fill_random_input(input_tensor);
  // Run the model on the spectrogram input and make sure it succeeds.
  TfLiteStatus invoke_status = sl_tflite_micro_get_interpreter()->Invoke();
  if (invoke_status != kTfLiteOk) {
    return SL_STATUS_FAIL;
  }

  return SL_STATUS_OK;
}

/***************************************************************************//**
 * profiler task function
 ******************************************************************************/
void ml_profiler_task()
{
  while (1) { // !stop_flag
    run_inference();
  }
}
