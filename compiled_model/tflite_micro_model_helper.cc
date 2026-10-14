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
#include <new>
#include <cstdio>

#include "tensorflow/lite/schema/schema_generated.h"
#include "tflite_micro_model_helper.hpp"
#include "tflite_micro_logger.hpp"
#include "tflite_micro_model.hpp"


namespace npu_toolkit
{


void  TfliteMicroModelHelper_set_active_context(TfLiteContext* context);
TfLiteContext*  TfliteMicroModelHelper_get_active_context();



void TfliteMicroModelHelper::set_active_context(TfLiteContext* context)
{
  if(context != nullptr)
  {
    assert(TfliteMicroModelHelper_get_active_context() == nullptr);
  }

  TfliteMicroModelHelper_set_active_context(context);
}

TfLiteContext* TfliteMicroModelHelper::get_active_context(bool allow_null)
{
  auto context = TfliteMicroModelHelper_get_active_context();

  if(!allow_null)
  {
    assert(context != nullptr);
  }
  return context;
}


TfliteMicroModel* TfliteMicroModelHelper::tflite_micro_model(TfLiteContext* context)
{
  auto model_context = tflite_micro_model_context(context);
  return model_context->model();
}

const tflite::Model* TfliteMicroModelHelper::flatbuffer_model(TfLiteContext* context)
{
  auto model =  tflite_micro_model(context);
  if(model == nullptr)
  {
    return nullptr;
  }
  return tflite::GetModel(model->flatbuffer());
}

TfliteMicroModelContext* TfliteMicroModelHelper::tflite_micro_model_context(TfLiteContext* context)
{
  context = (context != nullptr) ? context : get_active_context(false);
  assert(context !=  nullptr);
  auto micro_context = tflite::GetMicroContext(context);
  auto model_context = reinterpret_cast<TfliteMicroModelContext*>(micro_context->external_context());
  return model_context;
}

tflite::MicroAllocator* TfliteMicroModelHelper::tflite_micro_model_allocator(TfLiteContext* context)
{
  auto model = tflite_micro_model(context);
  return model->allocator();
}

tflite::BuiltinOperator TfliteMicroModelHelper::current_layer_opcode(TfLiteContext* context)
{
  const auto model = tflite_micro_model(context);
  assert(model != nullptr);
  assert(model->is_loaded());
  const auto& interpreter = *model->interpreter();
  const auto& subgraph_allocation = interpreter.graph_.subgraph_allocations_[interpreter.graph_.current_subgraph_index_];
  const auto registration = subgraph_allocation.node_and_registrations[interpreter.graph_.current_operator_index_].registration;
  return (registration != nullptr) ? (tflite::BuiltinOperator)registration->builtin_code : (tflite::BuiltinOperator)-1;
}

int TfliteMicroModelHelper::current_layer_index(TfLiteContext* context)
{
  const auto model = tflite_micro_model(context);
  assert(model != nullptr);
  assert(model->is_loaded());
  const auto& interpreter = *model->interpreter();
  return interpreter.graph_.current_operator_index_;
}

const char* TfliteMicroModelHelper::current_layer_name(TfLiteContext* context)
{
  auto& model_context = *tflite_micro_model_context(context);
  return create_layer_name(
    current_layer_index(context),
    current_layer_opcode(context)
  );
}

const char* TfliteMicroModelHelper::opcode_to_str(tflite::BuiltinOperator opcode)
{
  if (opcode == tflite::BuiltinOperator_CUSTOM)
  {
    return "custom";
  }
  else
  {
    return tflite::EnumNameBuiltinOperator(opcode);
  }
}


const char* TfliteMicroModelHelper::create_layer_name(int layer_idx, tflite::BuiltinOperator opcode)
{
  static char op_name_buffer[128];
  snprintf(op_name_buffer, sizeof(op_name_buffer), "Op%d-%s", layer_idx, opcode_to_str(opcode));
  return op_name_buffer;
}


bool TfliteMicroModelHelper::verify_model_flatbuffer(const void* flatbuffer, int flatbuffer_length)
{
  flatbuffers::Verifier verifier((const uint8_t*)flatbuffer, flatbuffer_length);
  return tflite::VerifyModelBuffer(verifier);
}

const void* TfliteMicroModelHelper::get_metadata_from_tflite_flatbuffer(
  TfLiteContext* context,
  const char* tag,
  uint32_t* length
)
{
  return get_metadata_from_tflite_flatbuffer(
    tflite_micro_model(context)->flatbuffer(),
    tag,
    length
  );
}

const void* TfliteMicroModelHelper::get_metadata_from_tflite_flatbuffer(
  const void* tflite_flatbuffer,
  const char* tag,
  uint32_t* length
)
{
  if(tflite_flatbuffer == nullptr)
  {
    return nullptr;
  }

  return get_metadata_from_tflite_flatbuffer(
    tflite::GetModel(tflite_flatbuffer),
    tag,
    length
  );
}


const void* TfliteMicroModelHelper::get_metadata_from_tflite_flatbuffer(
    const tflite::Model *model,
    const char* tag,
    uint32_t* length
)
{
  if(length != nullptr)
  {
    *length = 0;
  }

  if(model == nullptr)
  {
    return nullptr;
  }

  const void* metadata_buffer = nullptr;

  const auto metadata_vector = model->metadata();
  if(metadata_vector == nullptr)
  {
    return nullptr;
  }

  const auto buffers_vector = model->buffers();
  if(buffers_vector == nullptr)
  {
    return nullptr;
  }

  for(auto meta : *metadata_vector)
  {
    if(meta == nullptr || meta->name() == nullptr)
    {
      return nullptr;
    }

    if(strcmp(meta->name()->c_str(), tag) == 0)
    {
      auto buffer_index = meta->buffer();
      auto buffer = buffers_vector->Get(buffer_index);
      if(buffer == nullptr)
      {
        return nullptr;
      }

      const auto buffer_data = buffer->data();
      if(buffer_data  == nullptr)
      {
        return nullptr;
      }

      metadata_buffer = buffer_data->Data();
      if(length != nullptr)
      {
        *length = buffer_data->size();
      }

      break;
    }
  }

  return metadata_buffer;
}


static TfLiteContext* TfliteMicroModelHelper_active_context = nullptr;

void TfliteMicroModelHelper_set_active_context(TfLiteContext* context)
{
  TfliteMicroModelHelper_active_context = context;
}

TfLiteContext* TfliteMicroModelHelper_get_active_context()
{
  return TfliteMicroModelHelper_active_context;
}


} // namespace npu_toolkit