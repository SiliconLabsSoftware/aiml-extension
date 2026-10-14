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



// Tensor indices as defined by Tensorflow
// https://github.com/tensorflow/tensorflow/blob/master/tensorflow/lite/micro/kernels/add.cc
constexpr int kInputTensor1 = 0;
constexpr int kInputTensor2 = 1;
constexpr int kOutputTensor = 0;


constexpr const unsigned MVP_OUTPUT_ARRAY   = 0; // must be position 0, since fused relu reuses
constexpr const unsigned MVP_INPUT1_ARRAY   = 1;
constexpr const unsigned MVP_INPUT2_ARRAY   = 2;


namespace tflite {

extern int          cmsis_nn_add_get_init_size();
extern TfLiteStatus cmsis_nn_add_prepare(TfLiteContext* context, TfLiteNode* node);
extern TfLiteStatus cmsis_nn_add_invoke(TfLiteContext* context, TfLiteNode* node);

void* mvp_compiled_add_init(TfLiteContext* context, const char* buffer, size_t length);
TfLiteStatus mvp_compiled_add_prepare(TfLiteContext* context, TfLiteNode* node);
TfLiteStatus mvp_compiled_add_invoke(TfLiteContext* context, TfLiteNode* node);


} // namespace tflite