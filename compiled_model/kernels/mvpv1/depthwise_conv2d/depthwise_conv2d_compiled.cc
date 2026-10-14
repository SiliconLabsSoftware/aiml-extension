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
#include "depthwise_conv2d/depthwise_conv2d_compiled.hpp"
#include "compiled_model.hpp"
#include "compiled_registration_helper.hpp"
#include "tflite_micro_mvpv1_context.hpp"

namespace tflite
{

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
  TfLiteTensor* output =
      micro_context->AllocateTempOutputTensor(node, kOutputTensor);
  TfLiteTensor* bias = (node->inputs->size == 3) ?
      micro_context->AllocateTempInputTensor(node, kBiasTensor) : nullptr;
  TfLiteTensor* input =
      micro_context->AllocateTempInputTensor(node, kInputTensor);
  TfLiteTensor* filter =
      micro_context->AllocateTempInputTensor(node, kFilterTensor);

  const int num_channels = filter->dims->data[kDepthwiseConvQuantizedDimension];

  TF_LITE_ENSURE_STATUS(allocate_scaled_bias_tensor(context, bias, &data.bias));

  TF_LITE_ENSURE_STATUS(allocate_output_multiplier_buffer(
      context, input, filter, output,
      num_channels,
      &data.per_channel_output_scaler
  ));

  return kTfLiteOk;
}


TfLiteStatus mvp_invoke(TfLiteContext* context, TfLiteNode* node)
{
  const float16_t zero_data(0);
  const OpData& data = *(static_cast<const OpData*>(node->user_data));
  MicroContext* micro_context = GetMicroContext(context);
  auto& mvpv1_context = *TfliteMicroMvpv1Context::get(context);
  auto array_base_addresses = mvpv1_context.array_base_addresses;

  const auto input = tflite::micro::GetEvalInput(context, node, kInputTensor);
  const auto filter = tflite::micro::GetEvalInput(context, node, kFilterTensor);
  const auto bias   = NumInputs(node) == 3
                      ? tflite::micro::GetEvalInput(context, node, kBiasTensor)
                      : nullptr;
  auto output       = tflite::micro::GetEvalOutput(context, node, kOutputTensor);

  auto input_data = tflite::micro::GetTensorData<int8_t>(input);
  auto filter_data = tflite::micro::GetTensorData<int8_t>(filter);;
  auto output_data = tflite::micro::GetTensorData<int8_t>(output);
  const auto bias_data = (data.bias != nullptr) ? data.bias :
      tflite::micro::GetOptionalTensorData<float16_t>(bias);

  array_base_addresses[MVP_INPUT_ARRAY] = (uintptr_t)input_data;
  array_base_addresses[MVP_FILTER_ARRAY] = (uintptr_t)filter_data;
  array_base_addresses[MVP_BIAS_ARRAY] = (uintptr_t)((bias_data == nullptr) ? &zero_data : bias_data);
  array_base_addresses[MVP_OUTPUT_SCALER_ARRAY] = (uintptr_t)data.per_channel_output_scaler;
  array_base_addresses[MVP_OUTPUT_ARRAY] = (uintptr_t)output_data;
  mvpv1_context.update_array_base_addresses_callback = nullptr;

  return compiled_model_process_layer_with_accelerator(context);
}

}  // namespace



ADD_COMPILED_KERNEL_CALLBACKS(
  depthwise_conv2d,
  sizeof(OpData),
  cmsis_nn_depthwise_conv_get_init_size(),
  cmsis_nn_depthwise_conv_prepare,
  cmsis_nn_depthwise_conv_invoke
)

// Define this kernel's registration if this is an embedded build.
TFLMRegistration Register_DEPTHWISE_CONV_2D() {
  return tflite::micro::RegisterOp(
      mvp_compiled_depthwise_conv2d_init,
      mvp_compiled_depthwise_conv2d_prepare,
      mvp_compiled_depthwise_conv2d_invoke
  );
}

TFLMRegistration Register_DEPTHWISE_CONV_2D_no_fallback() {
  return tflite::micro::RegisterOp(
      mvp_compiled_depthwise_conv2d_no_fallback_init,
      mvp_compiled_depthwise_conv2d_no_fallback_prepare,
      mvp_compiled_depthwise_conv2d_no_fallback_invoke
  );
}


} // namespace tflite