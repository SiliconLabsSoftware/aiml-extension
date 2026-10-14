/*******************************************************************************
 * # License
 * <b>Copyright 2026 Silicon Laboratories Inc. www.silabs.com</b>
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

#include "tflite_micro_model_config.h"
#pragma once

#include "tensorflow/lite/c/common.h"

constexpr int kInputTensor1 = 0;
constexpr int kInputTensor2 = 1;
constexpr int kOutputTensor = 0;

constexpr const unsigned MVP_INPUT_ARRAY_1 = 0;
constexpr const unsigned MVP_INPUT_ARRAY_2 = 1;
constexpr const unsigned MVP_OUTPUT_ARRAY = 2;

namespace tflite
{

TfLiteStatus cmsis_nn_maximum_invoke(TfLiteContext* context, TfLiteNode* node);
TfLiteStatus cmsis_nn_minimum_invoke(TfLiteContext* context, TfLiteNode* node);

void* mvp_compiled_maximum_init(TfLiteContext* context, const char* buffer, size_t length);
TfLiteStatus mvp_compiled_maximum_prepare(TfLiteContext* context, TfLiteNode* node);
TfLiteStatus mvp_compiled_maximum_invoke(TfLiteContext* context, TfLiteNode* node);

void* mvp_compiled_minimum_init(TfLiteContext* context, const char* buffer, size_t length);
TfLiteStatus mvp_compiled_minimum_prepare(TfLiteContext* context, TfLiteNode* node);
TfLiteStatus mvp_compiled_minimum_invoke(TfLiteContext* context, TfLiteNode* node);

} // namespace tflite