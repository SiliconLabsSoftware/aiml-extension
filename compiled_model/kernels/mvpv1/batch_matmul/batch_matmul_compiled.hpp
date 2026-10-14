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
#include "mvpv1_kernel_utils.hpp"

struct OpDataBatchMatmul
{
  int32_t lhs_zero_point;
  int32_t rhs_zero_point;
  int32_t output_zero_point;
  float16_t output_scaler;
  bool adj_x;
  bool adj_y;
};

// Tensor indices as defined by Tensorflow
constexpr int kBatchMatmulInputLhsTensor = 0;
constexpr int kBatchMatmulInputRhsTensor = 1;
constexpr int kBatchMatmulOutputTensor = 0;

constexpr const unsigned MVP_LHS_ARRAY = 0;
constexpr const unsigned MVP_RHS_ARRAY = 1;
constexpr const unsigned MVP_OUTPUT_ARRAY = 2;

namespace tflite
{

extern int cmsis_nn_batch_matmul_get_init_size();
extern TfLiteStatus cmsis_nn_batch_matmul_prepare(TfLiteContext* context, TfLiteNode* node);
extern TfLiteStatus cmsis_nn_batch_matmul_invoke(TfLiteContext* context, TfLiteNode* node);

void* mvp_compiled_batch_matmul_init(TfLiteContext* context, const char* buffer, size_t length);
TfLiteStatus mvp_compiled_batch_matmul_prepare(TfLiteContext* context, TfLiteNode* node);
TfLiteStatus mvp_compiled_batch_matmul_invoke(TfLiteContext* context, TfLiteNode* node);

} // namespace tflite