/***************************************************************************//**
 * @file
 * @brief ML profiler firmware application
 *******************************************************************************
 * # License
 * <b>Copyright 2025 Silicon Laboratories Inc. www.silabs.com</b>
 *******************************************************************************
 *
 * The licensor of this software is Silicon Laboratories Inc. Your use of this
 * software is governed by the terms of the Silicon Labs Master Software License
 * Agreement (MSLA) available at
 * www.silabs.com/about-us/legal/master-software-license-agreement. This
 * software is distributed to you in Source Code format and is governed by the
 * sections of the MSLA applicable to Source Code.
 *
 ******************************************************************************/
#include "sl_ml_profiler_config.h"
#include "sl_ml_model_firmware_model.h"
#include "sl_ml_profiler_firmware.h"
#include "sl_status.h"

#include <cstdio>
#include <cstdlib>
#include <ctime>



/***************************************************************************//**
 * @brief
 *   Fill the input tensor with random data for testing.
 *
 * @param tensor Pointer to the input tensor to be filled.
 ******************************************************************************/
static void fill_random_input(TfLiteTensor* tensor)
{
  if (tensor == nullptr || tensor->dims == nullptr) {
    return;
  }

  static bool seeded = false;
  if (!seeded) {
    srand((unsigned)time(nullptr));
    seeded = true;
  }

  int num_elements = 1;
  for (int i = 0; i < tensor->dims->size; i++) {
    num_elements *= tensor->dims->data[i];
  }

  switch (tensor->type) {
    case kTfLiteUInt8: {
      uint8_t* data = tensor->data.uint8;
      for (int i = 0; i < num_elements; i++) {
        data[i] = (uint8_t)(rand() % 256);
      }
      break;
    }
    case kTfLiteInt8: {
      int8_t* data = tensor->data.int8;
      for (int i = 0; i < num_elements; i++) {
        data[i] = (int8_t)(rand() % 256 - 128);
      }
      break;
    }
    case kTfLiteFloat32: {
      float* data = tensor->data.f;
      for (int i = 0; i < num_elements; i++) {
        data[i] = (float)rand() / (float)RAND_MAX;
      }
      break;
    }
    case kTfLiteUInt16: {
      uint16_t* data = tensor->data.ui16;
      for (int i = 0; i < num_elements; i++) {
        data[i] = (uint16_t)(rand() % 65536);
      }
      break;
    }
    case kTfLiteInt16: {
      int16_t* data = tensor->data.i16;
      for (int i = 0; i < num_elements; i++) {
        data[i] = (int16_t)(rand() % 65536 - 32768);
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
 * Fills the compiled or dynamically loaded model input tensor with test data and
 * runs one invoke (via @ref sl_ml_model_run on the firmware model handle).
 *
 * @return
 *   SL_STATUS_OK on success, other value on failure.
 ******************************************************************************/
static sl_status_t run_inference()
{
  TfLiteTensor* input_tensor = sl_ml_firmware_model_model_handle.input_tensor(0);
  if (input_tensor == nullptr) {
    return SL_STATUS_FAIL;
  }
  fill_random_input(input_tensor);

  return sl_ml_model_run(&sl_ml_firmware_model_model_handle);
}

/***************************************************************************//**
 * profiler task function
 ******************************************************************************/
void ml_profiler_task()
{
#if defined(SL_ML_ENABLE_DYNAMIC_MODEL_LOAD) && (SL_ML_ENABLE_DYNAMIC_MODEL_LOAD == 1)

  if (!load_dynamic_tflite_micro_model(sl_ml_firmware_model_model_handle.model)) {
    printf("load_dynamic_tflite_micro_model failed\r\n");
    while (1) {
      ;
    }
  }
#else
  sl_status_t st = sl_ml_model_init(&sl_ml_firmware_model_model_handle);
  if (st != SL_STATUS_OK) {
    printf("firmware_model init failed: %d\r\n", (int)st);
    while (1) {
      ;
    }
  }
#endif

  while (1) {
    (void)run_inference();
  }
}
