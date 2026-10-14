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
#include "compiled_model_context.hpp"
#include "compiled_model.hpp"
#include "tflite_micro_model.hpp"
#include "tflite_micro_mvpv1_accelerator.hpp"
#include "tflite_micro_mvpv1_context.hpp"


namespace npu_toolkit
{



bool TfliteMicroMvpv1Context::init(TfliteMicroModel* model)
{
  if(!TfliteMicroModelContext::init(model))
  {
    return false;
  }

  auto tfl_context  = model->tflite_context();
  const void* compiled_data = compiled_model_retrieve_compilation_data(tfl_context);

  if(compiled_data == nullptr)
  {
    return true;
  }

  auto compiled_context = CompiledModelContext::create(
    tfl_context,
    compiled_data
  );
  if(compiled_context == nullptr)
  {
    return false;
  }

  if(!compiled_context->init())
  {
    return false;
  }

  this->compiled_context = compiled_context;

  return true;
}

bool TfliteMicroMvpv1Context::load()
{
  auto mvpv1_context = TfliteMicroMvpv1Context::get(_model->tflite_context());
  if(mvpv1_context->compiled_context != nullptr)
  {
    if(!mvpv1_context->compiled_context->load())
    {
      return false;
    }
  }

  return true;
}


} // namespace npu_toolkit