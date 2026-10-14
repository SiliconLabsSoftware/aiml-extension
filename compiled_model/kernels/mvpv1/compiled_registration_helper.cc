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
#include <cassert>
#include "tflite_micro_logger.hpp"
#include "compiled_registration_helper.hpp"
#include "tflite_micro_mvpv1_accelerator.hpp"


namespace npu_toolkit
{


void* compiled_registration_init(
  TfLiteContext* context,
  const char* buffer,
  size_t length,
  int init_size,
  int fallback_init_size
)
{
  assert(context->AllocatePersistentBuffer != nullptr);
  assert(TfliteMicroMvpv1Accelerator::is_model_compiled(context));

  int alloc_size = 0;

  // Is the current layer compiled (i.e. is it  supported by the accelerator)?
  if(TfliteMicroMvpv1Accelerator::is_current_layer_compiled(context))
  {
    // It is, so we use that acc impl init size
    alloc_size = init_size;
  }
  else
  {
    // Otherwise, the current layer is not supported,
    // So, use the fallback implementation's init size
    alloc_size = fallback_init_size;
  }

  // Allocate a buffer for the given size and return a pointer to it
  return (alloc_size > 0) ? context->AllocatePersistentBuffer(context, alloc_size) : nullptr;
}


TfLiteStatus compiled_registration_prepare(
  TfLiteContext* context,
  TfLiteNode* node,
  KernelPrepare prepare,
  KernelPrepare fallback_prepare
)
{
  // Is the current layer is compiled (i.e. is it supported by the accelerator)?
  if(TfliteMicroMvpv1Accelerator::is_current_layer_compiled(context))
  {
    // It is, so just use the mvp implementation
    return prepare(context, node);
  }
  // Otherwise use the fallback implementation if it's available
  else if(fallback_prepare != nullptr)
  {
    return fallback_prepare(context, node);
  }
  else
  {
    // Otherwise, return an error
    return kTfLiteError;
  }

}

TfLiteStatus compiled_registration_invoke(
  TfLiteContext* context,
  TfLiteNode* node,
  KernelInvoke invoke,
  KernelInvoke fallback_invoke
)
{
  // Is the current layer is compiled (i.e. is it supported by the accelerator)?
  if(TfliteMicroMvpv1Accelerator::is_current_layer_compiled(context))
  {
    // It is, so just use the mvp implementation
    return invoke(context, node);
  }
  // Otherwise use the fallback implementation if it's available
  else if(fallback_invoke != nullptr)
  {
    return fallback_invoke(context, node);
  }
  else
  {
    // Otherwise, return an error
    return kTfLiteError;
  }
}


} // namespace npu_toolkit