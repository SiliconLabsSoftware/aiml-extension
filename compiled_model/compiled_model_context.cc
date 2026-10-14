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
#include "tflite_micro_model_helper.hpp"
#include "tflite_micro_compiled_allocator.hpp"
#include "tflite_micro_model.hpp"
#include "compiled_model_context.hpp"
#include "compiled_model_paging.hpp"


namespace npu_toolkit
{

CompiledModelContext* CompiledModelContext::create(
  TfLiteContext *context,
  const void* compiled_data
)
{
  auto buffer = context->AllocatePersistentBuffer(
    context,
    sizeof(CompiledModelContext)
  );

  if(buffer == nullptr)
  {
    return nullptr;
  }

  return new(buffer)CompiledModelContext(
    context,
    compiled_data
  );
}

CompiledModelContext::CompiledModelContext(
  TfLiteContext *context,
  const void* compiled_data
) :
  _context(context),
  _compiled_data(compiled_data),
  _paging(nullptr)
{
}

bool CompiledModelContext::init()
{
  auto tflm_model = TfliteMicroModelHelper::tflite_micro_model(_context);
  auto allocator = reinterpret_cast<TfliteMicroCompiledAllocator*>(tflm_model->allocator());
  const auto compiled_model_data = CompiledModelData::create(_compiled_data);
  const auto paging_config = compiled_model_data->paging_config_ptr();

  if(paging_config != nullptr)
  {
    auto compiled_paging = CompiledModelPaging::create(_context, *PagingConfig::create(paging_config));
    if(compiled_paging == nullptr)
    {
      return false;
    }
    if(!compiled_paging->init(
      _context,
      _compiled_data
    ))
    {
      return false;
    }

    _paging = compiled_paging;
  }
  else
  {
    _current_item = (const CompiledLinkedItem*)compiled_model_data->layer_config_ptr();
  }

  return true;
}

bool CompiledModelContext::load()
{
  if(_paging != nullptr)
  {
    return _paging->load(_context, _compiled_data);
  }
  return true;
}

void CompiledModelContext::deinit()
{
  if(_paging != nullptr)
  {
    _paging->deinit();
    _paging = nullptr;
  }
}

CompiledAcceleratorId CompiledModelContext::get_layer_accelerator(int index) const
{
  const auto compiled_model_data = CompiledModelData::create(_compiled_data);
  const auto layer_accelerators = compiled_model_data->layer_accelerators();
  assert(index < compiled_model_data->n_layers());
  return layer_accelerators[index];
}


bool CompiledModelContext::begin_layer()
{
  if(_paging != nullptr)
  {
    if(!_paging->begin_layer(_context, &_layer_program_count))
    {
      return false;
    }
  }
  else
  {
    auto layer_config = reinterpret_cast<const CompiledLayerConfig*>(_current_item);
    assert(layer_config  != nullptr);
    _layer_program_count = layer_config->n_programs();
    _current_item = _current_item->next_item();
  }

  return true;
}

bool CompiledModelContext::get_next_program(CompiledProgramInfo *info)
{
  if(_paging != nullptr)
  {
    if(!_paging->get_next_program(info))
    {
      return false;
    }
  }
  else
  {
    auto prog = reinterpret_cast<const CompressedProgramConfig*>(_current_item);
    _current_item = _current_item->next_item();
    info->n_register_groups = prog->n_register_groups();
    info->register_offsets = prog->register_offsets();
    info->register_values = prog->register_values();
  }



  return true;
}

bool CompiledModelContext::wait()
{
  if(_paging != nullptr)
  {
    return _paging->wait();
  }
  else
  {
    return true;
  }
}


void CompiledModelContext::release()
{
  if(_paging != nullptr)
  {
    _paging->release();
  }
}

} // namespace npu_toolkit