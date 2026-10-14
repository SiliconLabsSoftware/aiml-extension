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
#include "batch_matmul/batch_matmul_compiled.hpp"
#include "compiled_model.hpp"
#include "compiled_registration_helper.hpp"
#include "tflite_micro_mvpv1_context.hpp"

namespace tflite
{

using namespace npu_toolkit;

namespace
{

TfLiteStatus mvp_prepare(TfLiteContext* context, TfLiteNode* node)
{
  return kTfLiteOk;
}

TfLiteStatus mvp_invoke(TfLiteContext* context, TfLiteNode* node)
{
  const auto lhs = tflite::micro::GetEvalInput(context, node, kBatchMatmulInputLhsTensor);
  const auto rhs = tflite::micro::GetEvalInput(context, node, kBatchMatmulInputRhsTensor);
  auto output = tflite::micro::GetEvalOutput(context, node, kBatchMatmulOutputTensor);

  const auto lhs_data = tflite::micro::GetTensorData<int8_t>(lhs);
  const auto rhs_data = tflite::micro::GetTensorData<int8_t>(rhs);
  auto output_data = tflite::micro::GetTensorData<int8_t>(output);

  auto& mvpv1_context = *TfliteMicroMvpv1Context::get(context);
  auto array_base_addresses = mvpv1_context.array_base_addresses;

  array_base_addresses[MVP_LHS_ARRAY] = (uintptr_t)lhs_data;
  array_base_addresses[MVP_RHS_ARRAY] = (uintptr_t)rhs_data;
  array_base_addresses[MVP_OUTPUT_ARRAY] = (uintptr_t)output_data;
  mvpv1_context.update_array_base_addresses_callback = nullptr;

  return compiled_model_process_layer_with_accelerator(context);
}

} // namespace

ADD_COMPILED_KERNEL_CALLBACKS(
  batch_matmul,
  0,
  cmsis_nn_batch_matmul_get_init_size(),
  cmsis_nn_batch_matmul_prepare,
  cmsis_nn_batch_matmul_invoke
)

TFLMRegistration Register_BATCH_MATMUL() {
  return tflite::micro::RegisterOp(
      mvp_compiled_batch_matmul_init,
      mvp_compiled_batch_matmul_prepare,
      mvp_compiled_batch_matmul_invoke
  );
}

TFLMRegistration Register_BATCH_MATMUL_no_fallback() {
  return tflite::micro::RegisterOp(
      mvp_compiled_batch_matmul_no_fallback_init,
      mvp_compiled_batch_matmul_no_fallback_prepare,
      mvp_compiled_batch_matmul_no_fallback_invoke
  );
}

} // namespace tflite