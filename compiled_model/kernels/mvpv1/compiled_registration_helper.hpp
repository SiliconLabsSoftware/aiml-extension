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


namespace npu_toolkit
{

/**
 * Generates the following functions:
 * mvp_compiled_<kernel name>_init
 * mvp_compiled_<kernel name>_prepare
 * mvp_compiled_<kernel name>_invoke
 * mvp_compiled_<kernel name>_no_fallback_init
 * mvp_compiled_<kernel name>_no_fallback_prepare
 * mvp_compiled_<kernel name>_no_fallback_invoke
 *
 * These callbacks are called by the corresponding TFLMRegistration
 */
#define ADD_COMPILED_KERNEL_CALLBACKS(name, init_size, fallback_init_size, fallback_prepare, fallback_invoke) \
  void* mvp_compiled_ ## name ## _init(TfLiteContext* context, const char* buffer, size_t length) { \
    return compiled_registration_init( \
      context, \
      buffer, \
      length, \
      init_size, \
      fallback_init_size \
    ); \
  } \
  TfLiteStatus mvp_compiled_ ## name ## _prepare(TfLiteContext* context, TfLiteNode* node) { \
    return compiled_registration_prepare( \
      context, \
      node, \
      mvp_prepare, \
      fallback_prepare \
    ); \
  } \
  TfLiteStatus mvp_compiled_ ## name ## _invoke(TfLiteContext* context, TfLiteNode* node) { \
    return compiled_registration_invoke( \
      context, \
      node, \
      mvp_invoke, \
      fallback_invoke \
    ); \
  } \
  void* mvp_compiled_ ## name ## _no_fallback_init(TfLiteContext* context, const char* buffer, size_t length) { \
    return compiled_registration_init( \
      context, \
      buffer, \
      length, \
      init_size \
    ); \
  } \
  TfLiteStatus mvp_compiled_ ## name ## _no_fallback_prepare(TfLiteContext* context, TfLiteNode* node) { \
    return compiled_registration_prepare( \
      context, \
      node, \
      mvp_prepare \
    ); \
  } \
  TfLiteStatus mvp_compiled_ ## name ## _no_fallback_invoke(TfLiteContext* context, TfLiteNode* node) { \
    return compiled_registration_invoke( \
      context, \
      node, \
      mvp_invoke \
    ); \
  }


typedef TfLiteStatus (*KernelPrepare)(TfLiteContext* context, TfLiteNode* node);
typedef TfLiteStatus (*KernelInvoke)(TfLiteContext* context, TfLiteNode* node);


void* compiled_registration_init(
  TfLiteContext* context,
  const char* buffer,
  size_t length,
  int init_size,
  int fallback_init_size = 0
);

TfLiteStatus compiled_registration_prepare(
  TfLiteContext* context,
  TfLiteNode* node,
  KernelPrepare prepare,
  KernelPrepare fallback_prepare = nullptr
);

TfLiteStatus compiled_registration_invoke(
  TfLiteContext* context,
  TfLiteNode* node,
  KernelInvoke invoke,
  KernelInvoke fallback_invoke = nullptr
);


} // namespace npu_toolkit