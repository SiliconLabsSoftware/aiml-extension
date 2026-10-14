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


#include "tflite_micro_model_context.hpp"
#include "tflite_micro_model_helper.hpp"


namespace npu_toolkit
{

class CompiledModelContext;

class TfliteMicroMvpv1Context : public TfliteMicroModelContext
{
public:

  static TfliteMicroMvpv1Context* create(TfLiteContext *context)
  {
    auto buffer = context->AllocatePersistentBuffer(context, sizeof(TfliteMicroMvpv1Context));
    if(buffer == nullptr)
    {
      return nullptr;
    }
    return new(buffer)TfliteMicroMvpv1Context();
  }

  static TfliteMicroMvpv1Context* get(TfLiteContext* context = nullptr)
  {
    auto model_context = TfliteMicroModelHelper::tflite_micro_model_context(context);
    return reinterpret_cast<TfliteMicroMvpv1Context*>(model_context);
  }

  bool init(TfliteMicroModel* model) override;
  bool load() override;

  CompiledModelContext* compiled_context = nullptr;
  uintptr_t array_base_addresses[5];
  uintptr_t array_base_addresses2[5];
  void (*update_array_base_addresses_callback)(TfliteMicroMvpv1Context&) = nullptr;
  int program_counter = 0;

protected:
  TfliteMicroMvpv1Context() = default;

};



} // namespace npu_toolkit