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
#include "add/add_compiled.hpp"
#include "compiled_model.hpp"
#include "compiled_registration_helper.hpp"
#include "tflite_micro_mvpv1_context.hpp"


namespace tflite {

using namespace npu_toolkit;

namespace
{

TfLiteStatus mvp_prepare(TfLiteContext* context, TfLiteNode* node)
{
  return kTfLiteOk;
}

TfLiteStatus mvp_invoke(TfLiteContext* context, TfLiteNode* node)
{
  const auto input1 = tflite::micro::GetEvalInput(context, node, kInputTensor1);
  const auto input1_data  = tflite::micro::GetTensorData<int8_t>(input1);
  const auto input2 = tflite::micro::GetEvalInput(context, node, kInputTensor2);
  const auto input2_data  = tflite::micro::GetTensorData<int8_t>(input2);
  auto output       = tflite::micro::GetEvalOutput(context, node, kOutputTensor);
  auto output_data  = tflite::micro::GetTensorData<int8_t>(output);

  auto& mvpv1_context = *TfliteMicroMvpv1Context::get(context);
  auto array_base_addresses = mvpv1_context.array_base_addresses;

  array_base_addresses[MVP_INPUT1_ARRAY] = (uintptr_t)input1_data;
  array_base_addresses[MVP_INPUT2_ARRAY] = (uintptr_t)input2_data;
  array_base_addresses[MVP_OUTPUT_ARRAY] = (uintptr_t)output_data;
  mvpv1_context.update_array_base_addresses_callback = nullptr;

  return compiled_model_process_layer_with_accelerator(context);
}

} // namespace



// Generate the kernel callbacks
// that are called by the TFLMRegistration below
ADD_COMPILED_KERNEL_CALLBACKS(
  add,
  0,
  cmsis_nn_add_get_init_size(),
  cmsis_nn_add_prepare,
  cmsis_nn_add_invoke
)

TFLMRegistration Register_ADD() {
  return tflite::micro::RegisterOp(
    mvp_compiled_add_init,
    mvp_compiled_add_prepare,
    mvp_compiled_add_invoke
  );
}

TFLMRegistration Register_ADD_no_fallback() {
  return tflite::micro::RegisterOp(
    mvp_compiled_add_no_fallback_init,
    mvp_compiled_add_no_fallback_prepare,
    mvp_compiled_add_no_fallback_invoke
  );
}

} // namespace tflite