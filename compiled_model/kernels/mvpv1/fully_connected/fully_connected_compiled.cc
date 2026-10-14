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
#include "tensorflow/lite/kernels/kernel_util.h"
#include "fully_connected/fully_connected_compiled.hpp"
#include "compiled_model.hpp"
#include "compiled_registration_helper.hpp"
#include "tflite_micro_mvpv1_context.hpp"


namespace tflite {

using namespace npu_toolkit;

namespace
{

struct OpData
{
  float16_t* bias;
  float16_t* per_channel_output_scaler;
};

TfLiteStatus mvp_prepare(TfLiteContext* context, TfLiteNode* node)
{
  OpData& data = *static_cast<OpData*>(node->user_data);
  MicroContext* micro_context = GetMicroContext(context);

  TfLiteTensor* input =
          micro_context->AllocateTempInputTensor(node, kInputTensor);
  TfLiteTensor* weights = micro_context->AllocateTempInputTensor(
          node, kWeightsTensor);
  TfLiteTensor* bias = (node->inputs->size == 3) ?
      micro_context->AllocateTempInputTensor(node, kBiasTensor) : nullptr;
  TfLiteTensor* output = micro_context->AllocateTempOutputTensor(
          node, kOutputTensor);

  TF_LITE_ENSURE_STATUS(allocate_scaled_bias_tensor(context, bias, &data.bias));
  data.per_channel_output_scaler = nullptr;

  if (weights->quantization.type == kTfLiteAffineQuantization &&
      weights->quantization.params != nullptr)
  {
    const auto* affine_quantization = reinterpret_cast<TfLiteAffineQuantization*>(weights->quantization.params);

    // Dynamically allocate per-channel quantization parameters.
    const int per_channel_quantization_size = affine_quantization->scale->size;

    if(per_channel_quantization_size > 1)
    {
      TF_LITE_ENSURE_STATUS(allocate_output_multiplier_buffer(
        context,
        input,
        weights,
        output,
        per_channel_quantization_size,
        &data.per_channel_output_scaler
      ));
    }
  }

  return kTfLiteOk;
}


TfLiteStatus mvp_invoke(TfLiteContext* context, TfLiteNode* node)
{
  const float16_t zero_data(0);
  const OpData& data = *(static_cast<const OpData*>(node->user_data));
  const auto input = tflite::micro::GetEvalInput(context, node, kInputTensor);
  const auto weights = tflite::micro::GetEvalInput(context, node, kWeightsTensor);
  const auto bias   = NumInputs(node) == 3
                      ? tflite::micro::GetEvalInput(context, node, kBiasTensor)
                      : nullptr;
  auto output       = tflite::micro::GetEvalOutput(context, node, kOutputTensor);
  auto input_data     = tflite::micro::GetTensorData<int8_t>(input);
  auto weights_data   = tflite::micro::GetTensorData<int8_t>(weights);
  auto output_data    = tflite::micro::GetTensorData<int8_t>(output);
  const auto bias_data = (data.bias != nullptr) ? data.bias :
      tflite::micro::GetOptionalTensorData<float16_t>(bias);

  auto& mvpv1_context = *TfliteMicroMvpv1Context::get(context);
  auto array_base_addresses = mvpv1_context.array_base_addresses;

  array_base_addresses[MVP_OUTPUT_ARRAY] = (uintptr_t)output_data;
  array_base_addresses[MVP_INPUT_ARRAY] = (uintptr_t)input_data;
  array_base_addresses[MVP_WEIGHTS_ARRAY] = (uintptr_t)weights_data;
  array_base_addresses[MVP_BIAS_ARRAY] = (uintptr_t)((bias_data == nullptr) ? &zero_data : bias_data);
  array_base_addresses[MVP_OUTPUT_MULTIPLIER_ARRAY] = (uintptr_t)data.per_channel_output_scaler;
  mvpv1_context.update_array_base_addresses_callback = nullptr;

  return compiled_model_process_layer_with_accelerator(context);
}


} // namespace




ADD_COMPILED_KERNEL_CALLBACKS(
  fully_connected,
  sizeof(OpData),
  cmsis_nn_fully_connected_get_init_size(),
  cmsis_nn_fully_connected_prepare,
  cmsis_nn_fully_connected_invoke
)

// Define the this kernel's registeration if this is an embedded build
TFLMRegistration Register_FULLY_CONNECTED() {
  return tflite::micro::RegisterOp(
    mvp_compiled_fully_connected_init,
    mvp_compiled_fully_connected_prepare,
    mvp_compiled_fully_connected_invoke
  );
}

TFLMRegistration Register_FULLY_CONNECTED_no_fallback() {
  return tflite::micro::RegisterOp(
    mvp_compiled_fully_connected_no_fallback_init,
    mvp_compiled_fully_connected_no_fallback_prepare,
    mvp_compiled_fully_connected_no_fallback_invoke
  );
}

} // namespace tflite