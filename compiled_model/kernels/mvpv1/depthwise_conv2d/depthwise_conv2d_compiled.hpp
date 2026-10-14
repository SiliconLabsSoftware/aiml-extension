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
#include "compiled_model.hpp"


// Tensor indices as defined by Tensorflow
// https://github.com/tensorflow/tensorflow/blob/master/tensorflow/lite/micro/kernels/depthwise_conv.cc#L35
constexpr int kInputTensor = 0;
constexpr int kFilterTensor = 1;
constexpr int kBiasTensor = 2;
constexpr int kOutputTensor = 0;
// Depthwise conv is quantized along dimension 3:
// https://www.tensorflow.org/lite/performance/quantization_spec
constexpr int kDepthwiseConvQuantizedDimension = 3;

constexpr const unsigned MVP_OUTPUT_ARRAY   = 0; // must be position 0, since fused relu reuses
constexpr const unsigned MVP_INPUT_ARRAY    = 1;
constexpr const unsigned MVP_FILTER_ARRAY   = 2;
constexpr const unsigned MVP_BIAS_ARRAY     = 3;
constexpr const unsigned MVP_OUTPUT_SCALER_ARRAY = 4;


namespace tflite {

extern int          cmsis_nn_depthwise_conv_get_init_size();
extern TfLiteStatus cmsis_nn_depthwise_conv_prepare(TfLiteContext* context, TfLiteNode* node);
extern TfLiteStatus cmsis_nn_depthwise_conv_invoke(TfLiteContext* context, TfLiteNode* node);

void* mvp_compiled_depthwise_conv2d_init(TfLiteContext* context, const char* buffer, size_t length);
TfLiteStatus mvp_compiled_depthwise_conv2d_prepare(TfLiteContext* context, TfLiteNode* node);
TfLiteStatus mvp_compiled_depthwise_conv2d_invoke(TfLiteContext* context, TfLiteNode* node);



} // namespace tflite