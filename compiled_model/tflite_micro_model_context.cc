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
#include "tensorflow/lite/micro/micro_context.h"
#include "tflite_micro_model.hpp"


namespace npu_toolkit
{


TfliteMicroModelContext* TfliteMicroModelContext::create(TfLiteContext *context)
{
  auto buffer = context->AllocatePersistentBuffer(
    context,
    sizeof(TfliteMicroModelContext)
  );

  if(buffer == nullptr)
  {
    return nullptr;
  }

  return new(buffer)TfliteMicroModelContext();
}

bool TfliteMicroModelContext::init(TfliteMicroModel* model)
{
  _model = model;
  auto micro_context = tflite::GetMicroContext(model->tflite_context());
  assert(micro_context != nullptr);
  micro_context->set_external_context(this);
  return true;
}

void TfliteMicroModelContext::deinit()
{
  auto micro_context = tflite::GetMicroContext(_model->tflite_context());
  assert(micro_context != nullptr);
  micro_context->set_external_context(nullptr);
  _model = nullptr;
}


bool TfliteMicroModelContext::load()
{
  return true;
}

TfliteMicroModel* TfliteMicroModelContext::model() const
{
  assert(_model != nullptr);
  return _model;
}

TfliteMicroModelContext* TfliteMicroModelContext::get(TfLiteContext* context)
{
  auto micro_context = tflite::GetMicroContext(context);
  assert(micro_context != nullptr);
  auto model_context = reinterpret_cast<TfliteMicroModelContext*>(micro_context->external_context());
  assert(model_context != nullptr);
  return model_context;
}


} // namespace npu_toolkit