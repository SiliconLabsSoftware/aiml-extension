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

#include <cassert>
#include <algorithm>
#include <complex>


#include "em_device.h"
#include "tensorflow/lite/c/common.h"

// Embedded must use build flag: -mfp16-format=ieee
#if (defined(__ARM_FP16_FORMAT_IEEE) || defined(__ARM_FP16_FORMAT_ALTERNATIVE)) && !(defined(__APPLE__) && defined(__MACH__))
typedef __fp16 float16_t;

#else
#error "float16 support is not available on this platform. Please compile with -mfp16-format=ieee"
#endif

namespace npu_toolkit
{

typedef std::complex<float> cfloat_t;
typedef std::complex<float16_t> cfloat16_t;

// Selected values give range up to int32 but not larger, so that final_scale_factor doesn't overflow
//  power-of-two chosen so that adds 0 error during accumulation
// Note: could generate a value based on filter_count to maximize range or even handle extremely large filter counts
constexpr const float ACCUMULATOR_MULTIPLIER = 1 << 15;
constexpr const float ACCUMULATOR_SCALER =  1.0f / ACCUMULATOR_MULTIPLIER;

constexpr const float FP16_MIN = -65504.0f;
constexpr const float FP16_MAX = 65504.0f;
constexpr const float FP16_SMALLEST = 0.000000059605f;

constexpr const uint32_t MVP_FAULT_MASK = (
    MVP_IF_LOOPFAULT | MVP_IF_BUSERRFAULT  | \
    MVP_IF_BUSALIGNFAULT | MVP_IF_ALUFAULT | \
    MVP_IF_ARRAYFAULT
  );


TfLiteStatus populate_convolution_quantization_params(
  TfLiteContext* context,
  const TfLiteTensor* input,
  const TfLiteTensor* filter,
  const TfLiteTensor* output,
  float16_t* per_channel_scalers,
  int num_channels,
  float accumulator_multipler
);


TfLiteStatus allocate_output_multiplier_buffer(
  TfLiteContext* context,
  const TfLiteTensor* input,
  const TfLiteTensor* filters,
  const TfLiteTensor* output,
  int n_channels,
  float16_t** buffer
);


TfLiteStatus allocate_scaled_bias_tensor(
  TfLiteContext* context,
  const TfLiteTensor* bias,
  float16_t** scaled_tensor_ptr
);


template<typename T>
inline T clamp(T f, T min, T max)
{
    return std::min(std::max(f, min), max);
}

inline float16_t normalize_fp16(float f)
{
    return (float16_t)clamp(f, FP16_MIN, FP16_MAX);
}



} // namespace npu_toolkit