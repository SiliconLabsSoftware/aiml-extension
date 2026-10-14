/***************************************************************************//**
 * @file micro_ops_no_fallback.h
 * @brief Allows for using the accelerated kernels without falling back to the reference implementations
 *******************************************************************************
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
#pragma once

#include "tensorflow/lite/c/common.h"
#include "tensorflow/lite/micro/micro_common.h"

// This supplements:
// tensorflow/lite/micro/kernels/micro_ops.h
//
// It allows for using the accelerated kernels without falling back to the reference implementations.
// This allows for reducing code space by not linking in unused kernel implementations.
//
// Forward declaration of all micro op kernel registration methods. These
// registrations are included with the standard `BuiltinOpResolver`.
//
// This header is particularly useful in cases where only a subset of ops are
// needed. In such cases, the client can selectively add only the registrations
// their model requires, using a custom `(Micro)MutableOpResolver`. Selective
// registration in turn allows the linker to strip unused kernels.

namespace tflite {

TFLMRegistration Register_ADD_no_fallback();
TFLMRegistration Register_AVERAGE_POOL_2D_no_fallback();
TFLMRegistration Register_BATCH_MATMUL_no_fallback();
TFLMRegistration Register_CONCATENATION_no_fallback();
TFLMRegistration Register_CONV_2D_no_fallback();
TFLMRegistration Register_DEPTHWISE_CONV_2D_no_fallback();
TFLMRegistration Register_FULLY_CONNECTED_no_fallback();
TFLMRegistration Register_MAXIMUM_no_fallback();
TFLMRegistration Register_MAX_POOL_2D_no_fallback();
TFLMRegistration Register_MEAN_no_fallback();
TFLMRegistration Register_MINIMUM_no_fallback();
TFLMRegistration Register_MUL_no_fallback();
TFLMRegistration Register_PAD_no_fallback();
TFLMRegistration Register_PADV2_no_fallback();
TFLMRegistration Register_RESHAPE_no_fallback();
TFLMRegistration Register_SQUARED_DIFFERENCE_no_fallback();
TFLMRegistration Register_SUB_no_fallback();
TFLMRegistration Register_TRANSPOSE_no_fallback();
TFLMRegistration Register_TRANSPOSE_CONV_no_fallback();

}  // namespace tflite


