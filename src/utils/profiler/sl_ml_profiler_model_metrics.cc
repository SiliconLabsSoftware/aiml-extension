/***************************************************************************//**
 * @file
 *   sl_ml_profiler_model_metrics.cc
 * @brief
 *   Computes model profiler metrics .
 *******************************************************************************
 * # License
 * <b>Copyright 2025 Silicon Laboratories Inc. www.silabs.com</b>
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
#include "sl_ml_silabs_profiler.h"
#include <cstring>   // for memset
#if defined(SL_BOARD_SI91X)
#include "ml/third_party/tflm/micro_allocator.h"
#endif
#if defined(SL_BOARD_EFX)
#include "tensorflow/lite/schema/schema_utils.h"
#include "tensorflow/lite/schema/schema_generated.h"
#endif
using namespace tflite;

namespace sl {
namespace ml {

/***************************************************************************//**
 * @brief Estimated activation ops added by a fused activation.
 *
 * For ReLU-family activations we approximate with two comparisons per element
 * (min/max or clamp). For NONE we add nothing.
 *
 * @param activation TFLite flatbuffer activation enum.
 * @param count      Number of elements affected by the activation.
 * @return Estimated extra scalar ops contributed by the activation.
 ******************************************************************************/
uint32_t SilabsProfiler::add_activation_ops(tflite::ActivationFunctionType activation, int count)
{
    uint32_t retval = 0;

    switch (activation)
    {
    case tflite::ActivationFunctionType_NONE:
        break;
    case tflite::ActivationFunctionType_RELU:
    case tflite::ActivationFunctionType_RELU_N1_TO_1:
    case tflite::ActivationFunctionType_RELU6:
        retval = count * 2; // min(max(lower, x), upper)
        break;
    default:
        break;
    }

    return retval;
}

/*************** Helpers *************************************************************************/

// Flat size of an Eval tensor (N*H*W*C...)
static inline uint64_t FlatSize(const TfLiteEvalTensor* t) {
    if (!t || !t->dims) return 0ULL;
    uint64_t n = 1;
    for (int i = 0; i < t->dims->size; ++i) {
        n *= static_cast<uint64_t>(t->dims->data[i]);
    }
    return n;
}

/*************** Shape + metrics recording helpers ***********************************************/

/***************************************************************************//**
 * @brief Convert TfLiteEvalTensor dims to layer_dimension_t (NHWC semantics).
 *
 * Missing dimensions default to 1. If @p t or @p t->dims is null, a zero-
 * initialized struct is written. Assumes layer_dimension_t has fields n/h/w/c.
 *
 * @param t   Eval tensor pointer (may be nullptr).
 * @param dst Output pointer to write shape.
 ******************************************************************************/
void SilabsProfiler::FillLayerDimsFrom(const TfLiteEvalTensor* t, layer_dimension_t* dst) {
    if (!dst) return;
    std::memset(dst, 0, sizeof(*dst));
    if (!t || !t->dims) return;

    const TfLiteIntArray* a = t->dims;
    dst->shape[0] = 0;
    dst->shape[1] = 0;
    dst->shape[2] = 0;
    dst->shape[3] = 0;

    // TFLM commonly uses NHWC for 4D tensors
    switch (a->size) {
        case 4: 
            dst->shape[0] = a->data[0];
            dst->shape[1] = a->data[1];
            dst->shape[2] = a->data[2];
            dst->shape[3] = a->data[3]; 
            break;
        case 3: 
            dst->shape[0] = a->data[0];
            dst->shape[1] = a->data[1];
            dst->shape[2] = a->data[2];
            break;
        case 2: 
            dst->shape[0] = a->data[0];
            dst->shape[1] = a->data[1];
            break;
        case 1: 
            dst->shape[0] = a->data[0];
            break;
    }
    dst->n_dimensions = a->size;
}

/***************************************************************************//**
 * @brief Store shapes and metrics into the global profiler event.
 *
 * This is the single place that writes event_info->{input_shape,output_shape,
 * macs,ops}. Call at the end of each per-op metrics function.
 *
 * @param in   Input activation tensor (can be nullptr).
 * @param out  Output activation tensor (can be nullptr).
 * @param macs MAC count for the op.
 * @param ops  Scalar op count for the op.
 ******************************************************************************/
void SilabsProfiler::NoteShapesAndCounters(const TfLiteEvalTensor* in,
                                         const TfLiteEvalTensor* out,
                                         uint64_t macs,
                                         uint64_t ops) {

    FillLayerDimsFrom(in,  &event_info.input_shape);
    FillLayerDimsFrom(out, &event_info.output_shape);
    event_info.macs = macs;
    event_info.ops  = ops;
}

/***************************************************************************//**
 * @brief Compute MACs/OPs for FULLY_CONNECTED and update event_info.
 *
 * Copies input/output shapes into event_info and records MAC/OP counts.
 * @param op Flatbuffer operator descriptor for FULLY_CONNECTED.
 ******************************************************************************/
void SilabsProfiler::calculate_fully_connected(const tflite::Operator* op)
{
    const int kWeightsTensor = 1;
    const int kOutputTensor  = 0;
    const int kBiasTensor    = 2;
    const int kInputTensor   = 0;

    if (!interpreter_profiler) return;

    const TfLiteEvalTensor* input_tensor   = interpreter_profiler->GetEvalTensor(op->inputs()->Get(kInputTensor));
    const TfLiteEvalTensor* weights_tensor = interpreter_profiler->GetEvalTensor(op->inputs()->Get(kWeightsTensor));
    const TfLiteEvalTensor* output_tensor  = interpreter_profiler->GetEvalTensor(op->outputs()->Get(kOutputTensor));
    const TfLiteEvalTensor* bias_tensor    = (op->inputs()->size() > kBiasTensor)
                                           ? interpreter_profiler->GetEvalTensor(op->inputs()->Get(kBiasTensor))
                                           : nullptr;

    const int output_depth = output_tensor->dims->data[output_tensor->dims->size - 1];
    const int accum_depth  = weights_tensor->dims->data[weights_tensor->dims->size - 1];

    uint64_t macs = static_cast<uint64_t>(output_depth) * accum_depth;
    uint64_t ops  = macs * 2;

    // Bias adds (disabled to keep parity with your previous style)
    if (bias_tensor) ops += static_cast<uint64_t>(output_depth);

    // Fused activation from FlatBuffer options
    if (const auto* opts = op->builtin_options_as_FullyConnectedOptions()) {
        ops += static_cast<uint64_t>(add_activation_ops(opts->fused_activation_function(),
                                                        output_depth));
    }

    //printf("MACs: %llu\n", (unsigned long long)macs);
    printf("OPs: %llu\n",  (unsigned long long)ops);
    NoteShapesAndCounters(input_tensor, output_tensor, macs, ops);
}

/***************************************************************************//**
 * @brief Compute MACs/OPs for CONV_2D and update event_info.
 *
 * Uses NHWC dims from tensors; includes fused activation cost when present.
 * @param op Flatbuffer operator descriptor for CONV_2D.
 ******************************************************************************/
void SilabsProfiler::calculate_conv2d(const tflite::Operator* op)
{
    const int kInputTensor  = 0;
    const int kFilterTensor = 1;
    const int kOutputTensor = 0;
    const int kBiasTensor   = 2;

    if (!interpreter_profiler) return;

    const TfLiteEvalTensor* input_tensor  = interpreter_profiler->GetEvalTensor(op->inputs()->Get(kInputTensor));
    const TfLiteEvalTensor* filter_tensor = interpreter_profiler->GetEvalTensor(op->inputs()->Get(kFilterTensor));
    const TfLiteEvalTensor* output_tensor = interpreter_profiler->GetEvalTensor(op->outputs()->Get(kOutputTensor));
    const TfLiteEvalTensor* bias_tensor   = (op->inputs()->size() > kBiasTensor)
                                           ? interpreter_profiler->GetEvalTensor(op->inputs()->Get(kBiasTensor))
                                           : nullptr;

    const int input_depth   = input_tensor->dims->data[3];
    const int filter_height = filter_tensor->dims->data[1];
    const int filter_width  = filter_tensor->dims->data[2];
    const int output_height = output_tensor->dims->data[1];
    const int output_width  = output_tensor->dims->data[2];
    const int output_depth  = output_tensor->dims->data[3];

    uint64_t macs = static_cast<uint64_t>(filter_height) * filter_width * input_depth
                  * output_width * output_height * output_depth;
    uint64_t ops  = macs * 2;

    // Bias adds (disabled for parity with your previous style)
    if (bias_tensor) ops += static_cast<uint64_t>(output_width) * output_height * output_depth;

    // Fused activation from FlatBuffer options
    if (const auto* opts = op->builtin_options_as_Conv2DOptions()) {
        const int out_elems = output_height * output_width * output_depth;
        ops += static_cast<uint64_t>(add_activation_ops(opts->fused_activation_function(),
                                                        out_elems));
    }
    //printf("MACs: %llu\n", (unsigned long long)macs);
    //printf("OPs: %llu\n",  (unsigned long long)ops);
    NoteShapesAndCounters(input_tensor, output_tensor, macs, ops);
}

/***************************************************************************//**
 * @brief Compute MACs/OPs for TRANSPOSE_CONV and update event_info.
 *
 * @param op Flatbuffer operator descriptor for TRANSPOSE_CONV.
 ******************************************************************************/
void SilabsProfiler::calculate_transpose_conv(const tflite::Operator* op)
{
    // TFLite ordering for TRANSPOSE_CONV: [output_shape, filter, input, (optional) bias]
    const int kFilterTensor = 1;
    const int kInputTensor  = 2;
    const int kOutputTensor = 0;
    const int kBiasTensor   = 3;

    if (!interpreter_profiler) return;

    const TfLiteEvalTensor* filter = interpreter_profiler->GetEvalTensor(op->inputs()->Get(kFilterTensor));
    const TfLiteEvalTensor* input  = interpreter_profiler->GetEvalTensor(op->inputs()->Get(kInputTensor));
    const TfLiteEvalTensor* output = interpreter_profiler->GetEvalTensor(op->outputs()->Get(kOutputTensor));
    const TfLiteEvalTensor* bias   = (op->inputs()->size() > kBiasTensor)
                                   ? interpreter_profiler->GetEvalTensor(op->inputs()->Get(kBiasTensor))
                                   : nullptr;

    const int in_h  = input->dims->data[1];
    const int in_w  = input->dims->data[2];
    const int in_c  = input->dims->data[3];
    const int filt_h= filter->dims->data[1];
    const int filt_w= filter->dims->data[2];
    const int out_h = output->dims->data[1];
    const int out_w = output->dims->data[2];
    const int out_c = output->dims->data[3];

    uint64_t macs = static_cast<uint64_t>(filt_h) * filt_w * in_h * in_w * in_c * out_c;
    uint64_t ops  = macs * 2;

    if (bias) ops += static_cast<uint64_t>(out_h) * out_w * out_c;

    printf("MACs: %llu\n", (unsigned long long)macs);
    printf("OPs: %llu\n",  (unsigned long long)ops);
    NoteShapesAndCounters(input, output, macs, ops);
}

/***************************************************************************//**
 * @brief Compute MACs/OPs for DEPTHWISE_CONV_2D and update event_info.
 *
 * Includes fused activation cost when present.
 * @param op Flatbuffer operator descriptor for DEPTHWISE_CONV_2D.
 ******************************************************************************/
void SilabsProfiler::calculate_depthwise_conv2d(const tflite::Operator* op)
{
    const int kInputTensor  = 0;
    const int kFilterTensor = 1;
    const int kOutputTensor = 0;
    const int kBiasTensor   = 2;

    if (!interpreter_profiler) return;

    const TfLiteEvalTensor* input  = interpreter_profiler->GetEvalTensor(op->inputs()->Get(kInputTensor));
    const TfLiteEvalTensor* filter = interpreter_profiler->GetEvalTensor(op->inputs()->Get(kFilterTensor));
    const TfLiteEvalTensor* output = interpreter_profiler->GetEvalTensor(op->outputs()->Get(kOutputTensor));
    const TfLiteEvalTensor* bias   = (op->inputs()->size() > kBiasTensor)
                                   ? interpreter_profiler->GetEvalTensor(op->inputs()->Get(kBiasTensor))
                                   : nullptr;

    const int filt_h = filter->dims->data[1];
    const int filt_w = filter->dims->data[2];
    const int out_h  = output->dims->data[1];
    const int out_w  = output->dims->data[2];
    const int out_c  = output->dims->data[3];

    uint64_t macs = static_cast<uint64_t>(filt_h) * filt_w * out_h * out_w * out_c;
    uint64_t ops  = macs * 2;

    if (bias) ops += static_cast<uint64_t>(out_h) * out_w * out_c;

    if (const auto* opts = op->builtin_options_as_DepthwiseConv2DOptions()) {
        const int out_elems = out_h * out_w * out_c;
        ops += static_cast<uint64_t>(add_activation_ops(opts->fused_activation_function(),
                                                        out_elems));
    }

    //printf("MACs: %llu\n", (unsigned long long)macs);
    //printf("OPs: %llu\n",  (unsigned long long)ops);
    NoteShapesAndCounters(input, output, macs, ops);
}

/***************************************************************************//**
 * @brief Compute MACs/OPs for MAX_POOL_2D and update event_info.
 *
 * @param op Flatbuffer operator descriptor for MAX_POOL_2D.
 ******************************************************************************/
void SilabsProfiler::calculate_max_pool2d(const tflite::Operator* op)
{
    const int kInputTensor  = 0;
    const int kOutputTensor = 0;

    if (!interpreter_profiler) return;

    const TfLiteEvalTensor* input  = interpreter_profiler->GetEvalTensor(op->inputs()->Get(kInputTensor));
    const TfLiteEvalTensor* output = interpreter_profiler->GetEvalTensor(op->outputs()->Get(kOutputTensor));

    const int out_h = output->dims->data[1];
    const int out_w = output->dims->data[2];
    const int out_c = output->dims->data[3];

    int filt_h = 1, filt_w = 1;
    if (const auto* opts = op->builtin_options_as_Pool2DOptions()) {
        filt_h = opts->filter_height();
        filt_w = opts->filter_width();
    }

    uint64_t macs = 0;
    uint64_t ops  = static_cast<uint64_t>(out_h) * out_w * out_c * filt_h * filt_w;

    if (const auto* opts = op->builtin_options_as_Pool2DOptions()) {
        const int out_elems = out_h * out_w * out_c;
        ops += static_cast<uint64_t>(add_activation_ops(opts->fused_activation_function(),
                                                        out_elems));
    }

    //printf("MACs: %llu\n", (unsigned long long)macs);
    //printf("OPs: %llu\n",  (unsigned long long)ops);
    NoteShapesAndCounters(input, output, macs, ops);
}

/***************************************************************************//**
 * @brief Compute MACs/OPs for AVERAGE_POOL_2D and update event_info.
 *
 * @param op Flatbuffer operator descriptor for AVERAGE_POOL_2D.
 ******************************************************************************/
void SilabsProfiler::calculate_average_pool2d(const tflite::Operator* op)
{
    const int kInputTensor  = 0;
    const int kOutputTensor = 0;

    if (!interpreter_profiler) return;

    const TfLiteEvalTensor* input  = interpreter_profiler->GetEvalTensor(op->inputs()->Get(kInputTensor));
    const TfLiteEvalTensor* output = interpreter_profiler->GetEvalTensor(op->outputs()->Get(kOutputTensor));

    const int out_h = output->dims->data[1];
    const int out_w = output->dims->data[2];
    const int out_c = output->dims->data[3];

    int filt_h = 1, filt_w = 1;
    if (const auto* opts = op->builtin_options_as_Pool2DOptions()) {
        filt_h = opts->filter_height();
        filt_w = opts->filter_width();
    }

    uint64_t macs = 0;
    uint64_t ops  = static_cast<uint64_t>(out_h) * out_w * out_c * (filt_h * filt_w + 1);

    if (const auto* opts = op->builtin_options_as_Pool2DOptions()) {
        const int out_elems = out_h * out_w * out_c;
        ops += static_cast<uint64_t>(add_activation_ops(opts->fused_activation_function(),
                                                        out_elems));
    }

    //printf("MACs: %llu\n", (unsigned long long)macs);
    //printf("OPs: %llu\n",  (unsigned long long)ops);
    NoteShapesAndCounters(input, output, macs, ops);
}

/***************************************************************************//**
 * @brief Compute MACs/OPs for SOFTMAX and update event_info.
 *
 * @param op Flatbuffer operator descriptor for SOFTMAX.
 ******************************************************************************/
void SilabsProfiler::calculate_softmax(const tflite::Operator* op)
{
    const int kInputTensor  = 0;
    const int kOutputTensor = 0;

    if (!interpreter_profiler) return;

    const TfLiteEvalTensor* input  = interpreter_profiler->GetEvalTensor(op->inputs()->Get(kInputTensor));
    const TfLiteEvalTensor* output = interpreter_profiler->GetEvalTensor(op->outputs()->Get(kOutputTensor));
    const TfLiteIntArray* dims = input->dims;
    const int input_size = dims->data[(dims->size == 2) ? 1 : 0];

    uint64_t macs = 0;
    uint64_t ops  = static_cast<uint64_t>(input_size) * (1 + 3 + 1);

    //printf("MACs: %llu\n", (unsigned long long)macs);
    //printf("OPs: %llu\n",  (unsigned long long)ops);
    NoteShapesAndCounters(input, output, macs, ops);
}

/***************************************************************************//**
 * @brief Compute MACs/OPs for ADD (elementwise) and update event_info.
 *
 * Includes fused activation cost when present.
 * @param op Flatbuffer operator descriptor for ADD.
 ******************************************************************************/
void SilabsProfiler::calculate_add(const tflite::Operator* op)
{
    const int kInputTensor0  = 0;
    const int kOutputTensor  = 0;
    if (!interpreter_profiler) return;

    const TfLiteEvalTensor* input0 = interpreter_profiler->GetEvalTensor(op->inputs()->Get(kInputTensor0));
    const TfLiteEvalTensor* output = interpreter_profiler->GetEvalTensor(op->outputs()->Get(kOutputTensor));

    uint64_t macs = 0;
    uint64_t ops  = FlatSize(output); // element-wise add

    if (const auto* opts = op->builtin_options_as_AddOptions()) {
        const int out_elems = static_cast<int>(FlatSize(output));
        ops += static_cast<uint64_t>(add_activation_ops(opts->fused_activation_function(),
                                                        out_elems));
    }

    //printf("MACs: %llu\n", (unsigned long long)macs);
    //printf("OPs: %llu\n",  (unsigned long long)ops);
    NoteShapesAndCounters(input0, output, macs, ops);
}

/***************************************************************************//**
 * @brief Compute MACs/OPs for QUANTIZE and update event_info.
 *
 * @param op Flatbuffer operator descriptor for QUANTIZE.
 ******************************************************************************/
void SilabsProfiler::calculate_quantize(const tflite::Operator* op)
{
    const int kInputTensor  = 0;
    const int kOutputTensor = 0;
    if (!interpreter_profiler) return;

    const TfLiteEvalTensor* input  = interpreter_profiler->GetEvalTensor(op->inputs()->Get(kInputTensor));
    const TfLiteEvalTensor* output = interpreter_profiler->GetEvalTensor(op->outputs()->Get(kOutputTensor));

    uint64_t macs = 0;
    uint64_t ops  = FlatSize(input) * 4ULL; // divide + add + min + max

    printf("MACs: %llu\n", (unsigned long long)macs);
    printf("OPs: %llu\n",  (unsigned long long)ops);
    NoteShapesAndCounters(input, output, macs, ops);
}

/***************************************************************************//**
 * @brief Compute MACs/OPs for DEQUANTIZE and update event_info.
 *
 * @param op Flatbuffer operator descriptor for DEQUANTIZE.
 ******************************************************************************/
void SilabsProfiler::calculate_dequantize(const tflite::Operator* op)
{
    const int kInputTensor  = 0;
    const int kOutputTensor = 0;
    if (!interpreter_profiler) return;

    const TfLiteEvalTensor* input  = interpreter_profiler->GetEvalTensor(op->inputs()->Get(kInputTensor));
    const TfLiteEvalTensor* output = interpreter_profiler->GetEvalTensor(op->outputs()->Get(kOutputTensor));

    uint64_t macs = 0;
    uint64_t ops  = FlatSize(input) * 2ULL; // multiply + subtract

    printf("MACs: %llu\n", (unsigned long long)macs);
    printf("OPs: %llu\n",  (unsigned long long)ops);
    NoteShapesAndCounters(input, output, macs, ops);
}

/***************************************************************************//**
 * @brief Compute MACs/OPs for PAD and update event_info.
 *
 * @param op Flatbuffer operator descriptor for PAD.
 ******************************************************************************/
void SilabsProfiler::calculate_pad(const tflite::Operator* op)
{
    const int kInputTensor  = 0; // paddings tensor is at 1
    const int kOutputTensor = 0;
    if (!interpreter_profiler) return;

    const TfLiteEvalTensor* input  = interpreter_profiler->GetEvalTensor(op->inputs()->Get(kInputTensor));
    const TfLiteEvalTensor* output = interpreter_profiler->GetEvalTensor(op->outputs()->Get(kOutputTensor));

    uint64_t macs = 0;
    uint64_t ops  = FlatSize(output) * 6ULL; // 1 move + ~5 comparisons

    printf("MACs: %llu\n", (unsigned long long)macs);
    printf("OPs: %llu\n",  (unsigned long long)ops);
    NoteShapesAndCounters(input, output, macs, ops);
}

/***************************************************************************//**
 * @brief Compute MACs/OPs for RESHAPE and update event_info.
 *
 * @param op Flatbuffer operator descriptor for RESHAPE.
 ******************************************************************************/
void SilabsProfiler::calculate_reshape(const tflite::Operator* op)
{
    const int kInputTensor  = 0;
    const int kOutputTensor = 0;
    if (!interpreter_profiler) return;

    const TfLiteEvalTensor* input  = interpreter_profiler->GetEvalTensor(op->inputs()->Get(kInputTensor));
    const TfLiteEvalTensor* output = interpreter_profiler->GetEvalTensor(op->outputs()->Get(kOutputTensor));

    uint64_t macs = 0;
    uint64_t ops  = 0;

    if (input && output && input->data.raw != output->data.raw) {
        ops = FlatSize(input); // memcpy-like
    }

    printf("MACs: %llu\n", (unsigned long long)macs);
    printf("OPs: %llu\n",  (unsigned long long)ops);
    NoteShapesAndCounters(input, output, macs, ops);
}

/***************************************************************************//**
 * @brief Compute MACs/OPs for MEAN (reduce) and update event_info.
 *
 * @param op Flatbuffer operator descriptor for MEAN.
 ******************************************************************************/
void SilabsProfiler::calculate_mean(const tflite::Operator* op)
{
    const int kInputTensor  = 0;
    const int kOutputTensor = 0;
    if (!interpreter_profiler) return;

    const TfLiteEvalTensor* input  = interpreter_profiler->GetEvalTensor(op->inputs()->Get(kInputTensor));
    const TfLiteEvalTensor* output = interpreter_profiler->GetEvalTensor(op->outputs()->Get(kOutputTensor));

    const uint64_t in_elems  = FlatSize(input);
    const uint64_t out_elems = FlatSize(output);

    uint64_t macs = 0;
    uint64_t ops  = 0;

    if (out_elems > 0) {
        const uint64_t r = in_elems / out_elems; // assume perfect division
        if (r >= 1) {
            ops = out_elems * ((r > 0 ? (r - 1) : 0) + 1); // (r-1) adds + 1 divide per output
        }
    }

    printf("MACs: %llu\n", (unsigned long long)macs);
    printf("OPs: %llu\n",  (unsigned long long)ops);
    NoteShapesAndCounters(input, output, macs, ops);
}

/***************************************************************************//**
 * @brief Compute MACs/OPs for RESIZE_NEAREST_NEIGHBOR and update event_info.
 *
 * @param op Flatbuffer operator descriptor for RESIZE_NEAREST_NEIGHBOR.
 ******************************************************************************/
void SilabsProfiler::calculate_resize_nearest_neighbor(const tflite::Operator* op)
{
    const int kInputTensor  = 0;
    const int kOutputTensor = 0;
    if (!interpreter_profiler) return;

    const TfLiteEvalTensor* input  = interpreter_profiler->GetEvalTensor(op->inputs()->Get(kInputTensor));
    const TfLiteEvalTensor* output = interpreter_profiler->GetEvalTensor(op->outputs()->Get(kOutputTensor));

    const int out_h = output->dims->data[1];
    const int out_w = output->dims->data[2];
    const int out_c = output->dims->data[3];

    constexpr int kNearestFlopsApprox = 8;

    uint64_t macs = 0;
    uint64_t ops  = static_cast<uint64_t>(out_h) * out_w * out_c * kNearestFlopsApprox;

    printf("MACs: %llu\n", (unsigned long long)macs);
    printf("OPs: %llu\n",  (unsigned long long)ops);
    NoteShapesAndCounters(input, output, macs, ops);
}

/***************************************************************************//**
 * @brief Compute MACs/OPs for RELU/RELU6 and update event_info.
 *
 * @param op Flatbuffer operator descriptor for RELU or RELU6.
 ******************************************************************************/
void SilabsProfiler::calculate_relu(const tflite::Operator* op)
{
    const int kInputTensor  = 0;
    const int kOutputTensor = 0;
    if (!interpreter_profiler) return;

    const TfLiteEvalTensor* input  = interpreter_profiler->GetEvalTensor(op->inputs()->Get(kInputTensor));
    const TfLiteEvalTensor* output = interpreter_profiler->GetEvalTensor(op->outputs()->Get(kOutputTensor));

    uint64_t macs = 0;
    uint64_t ops  = FlatSize(input) * 2ULL; // clamp lower/upper

    printf("MACs: %llu\n", (unsigned long long)macs);
    printf("OPs: %llu\n",  (unsigned long long)ops);
    NoteShapesAndCounters(input, output, macs, ops);
}

/***************************************************************************//**
 * @brief Compute MACs/OPs for MUL (elementwise) and update event_info.
 *
 * Includes fused activation cost when present.
 * @param op Flatbuffer operator descriptor for MUL.
 ******************************************************************************/
void SilabsProfiler::calculate_multiply(const tflite::Operator* op)
{
    const int kInputTensor0  = 0;
    const int kOutputTensor  = 0;
    if (!interpreter_profiler) return;

    const TfLiteEvalTensor* input0 = interpreter_profiler->GetEvalTensor(op->inputs()->Get(kInputTensor0));
    const TfLiteEvalTensor* output = interpreter_profiler->GetEvalTensor(op->outputs()->Get(kOutputTensor));

    uint64_t macs = 0;
    uint64_t ops  = FlatSize(output); // element-wise multiply

    if (const auto* opts = op->builtin_options_as_MulOptions()) {
        const int out_elems = static_cast<int>(FlatSize(output));
        ops += static_cast<uint64_t>(add_activation_ops(opts->fused_activation_function(),
                                                        out_elems));
    }

    //printf("MACs: %llu\n", (unsigned long long)macs);
    //printf("OPs: %llu\n",  (unsigned long long)ops);
    NoteShapesAndCounters(input0, output, macs, ops);
}

/***************************************************************************//**
 * @brief Dispatch to per-op calculators and update event_info via the callee.
 *
 * Called once per operator. Selects the appropriate `calculate_*` implementation.
 * Each calculator is responsible for writing
 * `event_info->{input_shape, output_shape, macs, ops}`.
 *
 * @return true on success.
 ******************************************************************************/
bool SilabsProfiler::update_start_event_info()
{
    event_info.line_number = 0; // TODO
    event_info.function_name = op_name_;
    event_info.processor_id = map_mcu_to_processor_id(start_session_info.mcu_id);// TODO

    // We need both the runtime interpreter (for eval tensors + registrations)
    // and the flatbuffer model (for builtin options) to mirror
    // tflite_micro_profiler_metrics logic.
    if (!interpreter_profiler || !model_profiler) {
        return false;
    }
#if defined(SL_BOARD_EFX)
    const tflite::SubGraph* subgraph = model_profiler->subgraphs()->Get(0);
    /*const auto* tensor = subgraph->tensors()->Get(operator_num_in_model_);
    const char *full_layer_name = tensor->name()->c_str();
    printf("%s\n", full_layer_name);*/
    const tflite::Operator* op = subgraph->operators()->Get(operator_num_in_model_);
    const auto& op_code = model_profiler->operator_codes()->Get(op->opcode_index());
    const tflite::BuiltinOperator builtin_code =
        static_cast<tflite::BuiltinOperator>(tflite::GetBuiltinCode(op_code));
#elif defined(SL_BOARD_SI91X) //SL_BOARD_EFX
    const int subgraph_index = interpreter_profiler->graph_.GetCurrentSubgraphIndex();
    const auto* subgraph = model_profiler->subgraphs()->Get(subgraph_index);
    if (subgraph == nullptr || subgraph->operators() == nullptr) {
        return false;
    }
    //int op_index = interpreter_profiler->graph_.GetCurrentOperatorIndex();
    // Obtain runtime registration (for builtin_code) from allocations when available.
    const SubgraphAllocations* allocations = interpreter_profiler->graph_.GetAllocations();
    const NodeAndRegistration* node_and_regs = nullptr;
    if (allocations != nullptr && subgraph_index >= 0) {
        node_and_regs = allocations[subgraph_index].node_and_registrations;
    }

    const NodeAndRegistration* node_and_registration = nullptr;
    //if (node_and_regs != nullptr && operator_num_in_model_ >= 0) { //op_index
    node_and_registration = &node_and_regs[operator_num_in_model_];
    //}

    const tflite::Operator* op = subgraph->operators()->Get(operator_num_in_model_);

    //if (node_and_registration != nullptr && node_and_registration->registration != nullptr) {
    tflite::BuiltinOperator builtin_code = static_cast<tflite::BuiltinOperator>(node_and_registration->registration->builtin_code);
#else //SL_BOARD_SI91X
    return false;
#endif // SL_BOARD_XXX

    if (builtin_code == tflite::BuiltinOperator_FULLY_CONNECTED) {
        calculate_fully_connected(op);
    } else if (builtin_code == tflite::BuiltinOperator_CONV_2D) {
        calculate_conv2d(op);
    } else if (builtin_code == tflite::BuiltinOperator_DEPTHWISE_CONV_2D) {
        calculate_depthwise_conv2d(op);
    } else if (builtin_code == tflite::BuiltinOperator_TRANSPOSE_CONV) {
        calculate_transpose_conv(op);
    } else if (builtin_code == tflite::BuiltinOperator_MAX_POOL_2D) {
        calculate_max_pool2d(op);
    } else if (builtin_code == tflite::BuiltinOperator_AVERAGE_POOL_2D) {
        calculate_average_pool2d(op);
    } else if (builtin_code == tflite::BuiltinOperator_SOFTMAX) {
        calculate_softmax(op);
    } else if (builtin_code == tflite::BuiltinOperator_ADD) {
        calculate_add(op);
    } else if (builtin_code == tflite::BuiltinOperator_MEAN) {
        calculate_mean(op);
    } else if (builtin_code == tflite::BuiltinOperator_RESIZE_NEAREST_NEIGHBOR) {
        calculate_resize_nearest_neighbor(op);
    } else if (builtin_code == tflite::BuiltinOperator_RELU ||
               builtin_code == tflite::BuiltinOperator_RELU6) {
        calculate_relu(op);
    } else if (builtin_code == tflite::BuiltinOperator_QUANTIZE) {
        calculate_quantize(op);
    } else if (builtin_code == tflite::BuiltinOperator_DEQUANTIZE) {
        calculate_dequantize(op);
    } else if (builtin_code == tflite::BuiltinOperator_PAD) {
        calculate_pad(op);
    } else if (builtin_code == tflite::BuiltinOperator_RESHAPE) {
        calculate_reshape(op);
    } else if (builtin_code == tflite::BuiltinOperator_MUL) {
        calculate_multiply(op);
    } else if (builtin_code == tflite::BuiltinOperator_SHAPE ||
               builtin_code == tflite::BuiltinOperator_STRIDED_SLICE ||
               builtin_code == tflite::BuiltinOperator_PACK ||
               builtin_code == tflite::BuiltinOperator_SPLIT ||
               builtin_code == tflite::BuiltinOperator_EXPAND_DIMS ||
               builtin_code == tflite::BuiltinOperator_CONCATENATION) {
        // ignore these layers for now
        return false;
    }

    return true;
}

} // namespace ml
} // namespace sl
