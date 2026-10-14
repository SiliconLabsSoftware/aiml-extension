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
#include "transpose_conv2d/transpose_conv2d_compiled.hpp"
#include "compiled_registration_helper.hpp"
#include "compiled_model.hpp"
#include "tflite_micro_model_helper.hpp"
#include "tflite_micro_mvpv1_context.hpp"


namespace tflite {

using namespace npu_toolkit;

namespace
{

struct OpData
{
  float16_t* per_channel_output_scaler;
  float16_t* bias;
  int scratch_buffer_index;
};

TfLiteStatus mvp_prepare(TfLiteContext* context, TfLiteNode* node)
{
  OpData& data = *static_cast<OpData*>(node->user_data);
  const bool has_bias = node->inputs->size == 4;

  MicroContext* micro_context = GetMicroContext(context);
  TfLiteTensor* output = micro_context->AllocateTempOutputTensor(node, kOutputTensor);
  const TfLiteTensor* bias = micro_context->AllocateTempInputTensor(node, kBiasTensor);
  const TfLiteTensor* input = micro_context->AllocateTempInputTensor(node, kInputTensor);
  const TfLiteTensor* filter = micro_context->AllocateTempInputTensor(node, kFilterTensor);

  const auto output_shape = GetTensorShape(output);
  const int output_size = output_shape.FlatSize();
  const int num_channels = filter->dims->data[kConvQuantizedDimension];

  TF_LITE_ENSURE_STATUS(allocate_scaled_bias_tensor(context, bias, &data.bias));

  TF_LITE_ENSURE_STATUS(allocate_output_multiplier_buffer(
    context,
    input,
    filter,
    output,
    num_channels,
    &data.per_channel_output_scaler
  ));

  const int scratch_buffer_size = sizeof(float16_t) * output_size;
  TF_LITE_ENSURE_STATUS(context->RequestScratchBufferInArena(
    context,
    scratch_buffer_size,
    &data.scratch_buffer_index
  ));

  return kTfLiteOk;
}

TfLiteStatus mvp_invoke(TfLiteContext* context, TfLiteNode* node)
{
  const OpData& data = *(static_cast<const OpData*>(node->user_data));
  auto& mvpv1_context = *TfliteMicroMvpv1Context::get(context);
  auto array_base_addresses = mvpv1_context.array_base_addresses;
  auto array_base_addresses2 = mvpv1_context.array_base_addresses2;

  const auto input = tflite::micro::GetEvalInput(context, node, kInputTensor);
  const auto filter = tflite::micro::GetEvalInput(context, node, kFilterTensor);
  const auto bias = tflite::micro::GetEvalInput(context, node, kBiasTensor);
  auto output = tflite::micro::GetEvalOutput(context, node, kOutputTensor);

  const auto input_data = tflite::micro::GetTensorData<int8_t>(input);
  const auto filter_data = tflite::micro::GetTensorData<int8_t>(filter);
  auto output_data = tflite::micro::GetTensorData<int8_t>(output);
  auto tmp_buffer = NPU_TOOLKIT_GET_SCRATCH_BUFFER(float16_t, data.scratch_buffer_index);
  const auto bias_data =
    (data.bias != nullptr) ? data.bias : tflite::micro::GetOptionalTensorData<float16_t>(bias);

  array_base_addresses[MVP_TEMP_BUFFER_ARRAY] =  (uintptr_t)tmp_buffer;
  array_base_addresses[MVP_INPUT_ARRAY] = (uintptr_t)input_data;
  array_base_addresses[MVP_FILTER_ARRAY] = (uintptr_t)filter_data;

  array_base_addresses2[MVP_TEMP_BUFFER_ARRAY] =  (uintptr_t)tmp_buffer;
  array_base_addresses2[MVP_OUTPUT_ARRAY] = (uintptr_t)output_data;
  array_base_addresses2[MVP_OUTPUT_SCALER_ARRAY] = (uintptr_t)data.per_channel_output_scaler;
  array_base_addresses2[MVP_BIAS_ARRAY] = (uintptr_t)bias_data;

  mvpv1_context.update_array_base_addresses_callback = [](
    TfliteMicroMvpv1Context& ctx
  )
  {
    auto array_base_addresses = ctx.array_base_addresses;
    auto array_base_addresses2 = ctx.array_base_addresses2;
    memcpy(array_base_addresses, array_base_addresses2, sizeof(ctx.array_base_addresses));
  };

  return compiled_model_process_layer_with_accelerator(context);
}

} // namespace

ADD_COMPILED_KERNEL_CALLBACKS(
  transpose_conv2d,
  sizeof(OpData),
  cmsis_nn_transpose_conv_get_init_size(),
  cmsis_nn_transpose_conv_prepare,
  cmsis_nn_transpose_conv_invoke
)

TFLMRegistration Register_TRANSPOSE_CONV() {
  return tflite::micro::RegisterOp(
    mvp_compiled_transpose_conv2d_init,
    mvp_compiled_transpose_conv2d_prepare,
    mvp_compiled_transpose_conv2d_invoke);
}

TFLMRegistration Register_TRANSPOSE_CONV_no_fallback() {
  return tflite::micro::RegisterOp(
    mvp_compiled_transpose_conv2d_no_fallback_init,
    mvp_compiled_transpose_conv2d_no_fallback_prepare,
    mvp_compiled_transpose_conv2d_no_fallback_invoke);
}

} // namespace tflite