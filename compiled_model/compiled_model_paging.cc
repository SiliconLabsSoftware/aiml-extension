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
#include <cstring>
#include "tensorflow/lite/micro/micro_utils.h"
#include "tensorflow/lite/micro/memory_helpers.h"

#include "tflite_micro_compiled_allocator.hpp"
#include "tflite_micro_model_helper.hpp"
#include "tflite_micro_model.hpp"

#include "compiled_model_paging.hpp"
#include "compiled_model_paging_interface.hpp"
#include "compiled_model_dma_mgr.hpp"
#include "compiled_model_utils.hpp"

#define ENABLE_CPU_CYCLE_COUNTER() DWT->CTRL |= DWT_CTRL_CYCCNTENA_Msk
#define DISABLE_CPU_CYCLE_COUNTER() DWT->CTRL &= ~DWT_CTRL_CYCCNTENA_Msk

namespace npu_toolkit
{



CompiledModelPaging* CompiledModelPaging::create(
  TfLiteContext* context,
  const PagingConfig& paging_config
)
{
  const auto dma_context_size = dma_mgr_context_size(paging_config.flags());
  auto buffer = context->AllocatePersistentBuffer(context, sizeof(CompiledModelPaging));
  auto dma_context = context->AllocatePersistentBuffer(context, dma_context_size);
  assert(dma_context != nullptr);
  memset(dma_context, 0, dma_context_size);
  return new(buffer)CompiledModelPaging(dma_context);
}

CompiledModelPaging::CompiledModelPaging(void* dma_context)
{
  _dma_context = reinterpret_cast<DmaContext*>(dma_context);
}

bool CompiledModelPaging::init(
  TfLiteContext *context,
  const void* compiled_data
)
{
  const auto compiled_model_data = CompiledModelData::create(compiled_data);
  const auto& paging_config = *PagingConfig::create(compiled_model_data->paging_config_ptr());
  const auto transfer_list_ptr = (const uint32_t*)compiled_model_data->paging_transfer_list_ptr();

  auto model = TfliteMicroModelHelper::tflite_micro_model(context);
  auto allocator = reinterpret_cast<TfliteMicroCompiledAllocator*>(model->allocator());

  const auto paged_model_data = (const uint32_t*)TfliteMicroModelHelper::get_metadata_from_tflite_flatbuffer(
      context, PAGED_DATA_TFLITE_TAG
  );

  const auto flags = paging_config.flags();

  DEBUG_PRINTF("Initializing");

  memset(_dma_context, 0, dma_mgr_context_size(flags));

  _dma_context->should_trigger_transfers        = (flags & PagingConfig::FLAG_TRIGGER_TRANSFERS) != 0;
  _dma_context->dual_weights_transfers_enabled  = (flags & PagingConfig::FLAG_DUAL_WEIGHTS_PAGING_ENABLED) != 0;
  _dma_context->paging_buffer                   = (uint32_t*)allocator->get_base_addr(paging_config.paging_buffer_memory_region());
  for(int i = 0; i < allocator->n_memory_regions(); ++i)
  {
    _dma_context->memory_region_addrs[i] = (uint32_t*)allocator->get_base_addr(i);
    DEBUG_PRINTF("memory_region_addrs[%d]=%p", i, _dma_context->memory_region_addrs[i]);
  }

  _dma_context->transfer_list_buffer_offsets[0] = paging_config.transfer_list_buffer_offset0();
  _dma_context->transfer_list_buffer_offsets[1] = paging_config.transfer_list_buffer_offset1();
  _dma_context->transfer_list_buffer_size_words = paging_config.transfer_list_buffer_size_words();
  _dma_context->src_buffer_base_addrs[DmaContext::BUFFER_TYPE_WEIGHTS] = paged_model_data;
  _dma_context->src_buffer_base_addrs[DmaContext::BUFFER_TYPE_TRANSFER_LIST] =  transfer_list_ptr;
  _dma_context->src_buffer_base_addrs[DmaContext::BUFFER_TYPE_WEIGHTS2] = (paging_config.dual_weights_memory_region() != 0) ?
      (const uint32_t*)allocator->get_base_addr(paging_config.dual_weights_memory_region()) : nullptr;

  DEBUG_PRINTF("flags=0x%02X", flags);
  DEBUG_PRINTF("initial_next_transfer_id=%d", paging_config.initial_next_transfer_id());
  DEBUG_PRINTF("layer_config_buffer_offset0=0x%04X", paging_config.layer_config_buffer_offset0());
  DEBUG_PRINTF("transfer_list_buffer_offset0=0x%04X", paging_config.transfer_list_buffer_offset0());
  DEBUG_PRINTF("transfer_list_buffer_offset1=0x%04X", paging_config.transfer_list_buffer_offset1());
  DEBUG_PRINTF("transfer_list_buffer_size_words=%d", paging_config.transfer_list_buffer_size_words());
  DEBUG_PRINTF("weights1_src_addr=%p", paged_model_data);
  DEBUG_PRINTF("weights2_src_offset=0x%04X", paging_config.weights2_src_offset());
  DEBUG_PRINTF("weights2_count=%d", paging_config.weights2_count());
  DEBUG_PRINTF("paging_buffer_memory_region=%d", paging_config.paging_buffer_memory_region());
  DEBUG_PRINTF("dual_weights_memory_region=%d", paging_config.dual_weights_memory_region());

  if(!dma_mgr_init(_dma_context, flags))
  {
    return false;
  }

  dma_mgr_set_next_transfer_id(_dma_context, paging_config.initial_next_transfer_id());
  if(!dma_mgr_start_transfers(_dma_context, paging_config))
  {
    return false;
  }

  const auto layer_config_base_addr = _dma_context->paging_buffer + paging_config.layer_config_buffer_offset0();
  _current_item = reinterpret_cast<const PagingCompiledLinkedItem*>(layer_config_base_addr);

  return true;
}

bool CompiledModelPaging::load(TfLiteContext *context, const void* compiled_data)
{
  const auto compiled_model_data = CompiledModelData::create(compiled_data);
  const auto& paging_config = *PagingConfig::create(compiled_model_data->paging_config_ptr());
  const auto weights2_buffer = _dma_context->src_buffer_base_addrs[DmaContext::BUFFER_TYPE_WEIGHTS2];

  if(weights2_buffer != nullptr)
  {
    auto tensor_data_ptr          = (uint8_t*)(weights2_buffer + paging_config.weights2_count());
    const int16_t* tensor_indices = paging_config.copied_tensor_indices();

    for(const int16_t* tensor_indices = paging_config.copied_tensor_indices();
      *tensor_indices != -1;
      ++tensor_indices
    )
    {
      const auto tensor_index = *tensor_indices;
      auto tensor = context->GetEvalTensor(context, tensor_index);
      const auto length_bytes = tflite::EvalTensorBytes(tensor);
      DEBUG_PRINTF("Copying tensor %d, size=%d", tensor_index, length_bytes);
      memcpy((void*)tensor_data_ptr, tensor->data.raw, length_bytes);
      tensor->data.uint8 = tensor_data_ptr;
      tensor_data_ptr += tflite::AlignSizeUp(length_bytes, sizeof(uint32_t));
    }
  }

  return true;
}

void CompiledModelPaging::deinit()
{
  DEBUG_PRINTF("De-initialize");
  dma_mgr_deinit(_dma_context);
}

bool CompiledModelPaging::begin_layer(
  TfLiteContext *context,
  uint16_t* n_programs
)
{
  DEBUG_PRINTF("Layer: %s starting", TfliteMicroModelHelper::current_layer_name());

  if(!dma_mgr_wait(_dma_context))
  {
    return false;
  }

  const auto& layer_config = *reinterpret_cast<const PagingCompiledLayerConfig*>(_current_item);
  *n_programs = layer_config.n_programs();
  DEBUG_PRINTF("n_progs=%d", *n_programs);

  _layer_paging_enabled = layer_config.uses_paging();

  release();

  return true;
}

bool CompiledModelPaging::get_next_program(CompiledProgramInfo *info)
{
  if(!dma_mgr_wait(_dma_context))
  {
    return false;
  }

  const auto& prog = *reinterpret_cast<const PagingCompressedProgramConfig*>(_current_item);
  info->n_register_groups = prog.n_register_groups();
  info->register_offsets = prog.register_offsets();;
  info->register_values = prog.register_values();

  return true;
}

bool CompiledModelPaging::wait()
{
  // If the current layer has not been configured for paging
  // then just return
  if(!_layer_paging_enabled)
  {
    return true;
  }

  return dma_mgr_wait(_dma_context);
}


void CompiledModelPaging::release()
{
  auto& dma = *_dma_context;
  const auto item_config = _current_item->config();

  DEBUG_PRINTF("Current=%08X next=%08X (%d)",
    (uint32_t*)_current_item - dma.paging_buffer,
    (uint32_t*)_current_item->next_item() - dma.paging_buffer,
   _current_item->next_item_offset()
  );
  _current_item = _current_item->next_item();

  const auto trigger_next_transfer = item_config.trigger_next_transfer();
  const auto is_last_item = item_config.is_last_item();
  dma_mgr_set_next_transfer_id(_dma_context, item_config.next_transfer_id());

  #ifdef COMPILED_MODEL_PAGING_DEBUG_ENABLED
  static uint32_t config_id = 0;
  DEBUG_PRINTF("last config id=%d", ++config_id);
  if(is_last_item)
  {
    config_id = 0;
  }
  #endif // COMPILED_MODEL_PAGING_DEBUG_ENABLED

  if(is_last_item)
  {
    dma_mgr_set_all_transfers_complete(_dma_context);
  }

  if(trigger_next_transfer || (is_last_item && dma.should_trigger_transfers))
  {
    dma_mgr_trigger_next_transfer(_dma_context);
  }

}


} // namespace npu_toolkit