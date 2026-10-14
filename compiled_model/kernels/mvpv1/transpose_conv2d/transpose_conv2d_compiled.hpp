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

#include "mvpv1_kernel_utils.hpp"

// Tensor indices as defined by Tensorflow
// https://github.com/tensorflow/tensorflow/blob/master/tensorflow/lite/micro/kernels/transpose_conv.cc
constexpr int kOutputShapeTensor = 0;
constexpr int kFilterTensor = 1;
constexpr int kInputTensor = 2;
constexpr int kBiasTensor = 3;
constexpr int kOutputTensor = 0;

// Conv is quantized along dimension 0:
// https://www.tensorflow.org/lite/performance/quantization_spec
constexpr int kConvQuantizedDimension = 0;

constexpr const unsigned MVP_TEMP_BUFFER_ARRAY = 0;
constexpr const unsigned MVP_INPUT_ARRAY = 1;
constexpr const unsigned MVP_FILTER_ARRAY = 2;

constexpr const unsigned MVP_OUTPUT_ARRAY = 1;
constexpr const unsigned MVP_OUTPUT_SCALER_ARRAY = 2;
constexpr const unsigned MVP_BIAS_ARRAY = 3;

namespace tflite {

extern int cmsis_nn_transpose_conv_get_init_size();
extern TfLiteStatus cmsis_nn_transpose_conv_prepare(TfLiteContext* context, TfLiteNode* node);
extern TfLiteStatus cmsis_nn_transpose_conv_invoke(TfLiteContext* context, TfLiteNode* node);

void* mvp_compiled_transpose_conv2d_init(TfLiteContext* context, const char* buffer, size_t length);
TfLiteStatus mvp_compiled_transpose_conv2d_prepare(TfLiteContext* context, TfLiteNode* node);
TfLiteStatus mvp_compiled_transpose_conv2d_invoke(TfLiteContext* context, TfLiteNode* node);

} // namespace tflite