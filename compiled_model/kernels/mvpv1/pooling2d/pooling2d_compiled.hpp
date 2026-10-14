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

constexpr int kInputTensor = 0;
constexpr int kOutputTensor = 0;

constexpr const unsigned MVP_INPUT_ARRAY = 0;
constexpr const unsigned MVP_OUTPUT_ARRAY = 1;

namespace tflite {

int cmsis_nn_pooling_get_init_size();
TfLiteStatus cmsis_nn_max_pool_prepare(TfLiteContext* context, TfLiteNode* node);
TfLiteStatus cmsis_nn_max_pool_invoke(TfLiteContext* context, TfLiteNode* node);
TfLiteStatus cmsis_nn_average_pool_prepare(TfLiteContext* context, TfLiteNode* node);
TfLiteStatus cmsis_nn_average_pool_invoke(TfLiteContext* context, TfLiteNode* node);

void* mvp_compiled_average_pool_init(TfLiteContext* context, const char* buffer, size_t length);
TfLiteStatus mvp_compiled_average_pool_prepare(TfLiteContext* context, TfLiteNode* node);
TfLiteStatus mvp_compiled_average_pool_invoke(TfLiteContext* context, TfLiteNode* node);

void* mvp_compiled_max_pool_init(TfLiteContext* context, const char* buffer, size_t length);
TfLiteStatus mvp_compiled_max_pool_prepare(TfLiteContext* context, TfLiteNode* node);
TfLiteStatus mvp_compiled_max_pool_invoke(TfLiteContext* context, TfLiteNode* node);

} // namespace tflite