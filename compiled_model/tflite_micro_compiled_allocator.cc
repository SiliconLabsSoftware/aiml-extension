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
#include <algorithm>

#include "tensorflow/lite/micro/micro_allocation_info.h"
#include "tensorflow/lite/micro/memory_helpers.h"
#include "tensorflow/lite/micro/micro_arena_constants.h"
#include "tensorflow/lite/micro/memory_planner/non_persistent_buffer_planner_shim.h"
#include "tensorflow/lite/micro/memory_planner/greedy_memory_planner.h"
#include "tensorflow/lite/kernels/internal/compatibility.h"


#include "tflite_micro_logger.hpp"
#include "tflite_micro_compiled_allocator.hpp"
#include "tflite_micro_model_helper.hpp"

namespace npu_toolkit
{
using namespace tflite;


const CompiledMemoryPlan* TfliteMicroCompiledAllocator::retrieve_memory_plan(
  const void* flatbuffer
)
{
  // Retrieve the pre-compiled memory plan from the .tflite model's metadata
  const auto memory_plan_data = TfliteMicroModelHelper::get_metadata_from_tflite_flatbuffer(
    flatbuffer,
    COMPILED_MEMORY_PLAN_TFLITE_TAG
  );

  if(memory_plan_data == nullptr)
  {
    return nullptr;
  }

  return CompiledMemoryPlan::create(memory_plan_data);
}

TfliteMicroCompiledAllocator* TfliteMicroCompiledAllocator::create(
  const CompiledMemoryPlan* memory_plan,
  uint8_t* const buffers[],
  const int32_t buffer_sizes[],
  int32_t n_buffers
)
{
  NPU_TOOLKIT_DEBUG(
    "Memory plan:\n"
    "  Region count: %d\n"
    "  Non-persistent buffer count: %d\n"
    "  Planned-persistent buffers count: %d",
    memory_plan->n_memory_regions(),
    memory_plan->n_non_persistent_buffers(),
    memory_plan->n_planned_persistent_buffers()
    );

  // Ensure the number of buffers given matches the number of memory regions in the memory plan
  if(n_buffers != memory_plan->n_memory_regions())
  {
    NPU_TOOLKIT_ERROR(
      "The given number of buffers (%d) does not match the expected memory regions (%d)",
      n_buffers, memory_plan->n_memory_regions()
    );
    return nullptr;
  }

  // Buffer[0] is used as the "base_memory_allocator".
  // This is used to allocate buffers for all other the other object instances.
  // It is also used for any persistent data and temporary allocations.
  uint8_t* base_buffer = buffers[0];
  uint8_t* primary_planned_persistent_buffer = nullptr;

  // Ensure the base_buffer's base address is properly aligned
  uint8_t* aligned_base_buffer = AlignPointerUp(base_buffer, MicroArenaBufferAlignment());
  size_t aligned_base_buffer_size = base_buffer + buffer_sizes[0] - aligned_base_buffer;

  // Create the base memory allocator
  auto base_memory_allocator = SingleArenaBufferAllocator::Create(aligned_base_buffer, aligned_base_buffer_size);

  // Create an array to store the points to each memory regions' buffer allocator
  auto buffer_info = reinterpret_cast<BufferInfo*>(
    base_memory_allocator->AllocatePersistentBuffer(
      memory_plan->n_memory_regions() * sizeof(BufferInfo),
      MicroArenaBufferAlignment()
    )
  );

  if(buffer_info == nullptr)
  {
    NPU_TOOLKIT_ERROR("Failed to allocate BufferInfo list");
    return nullptr;
  }

  // Populate the other memory regions' memory allocators
  for(int i = 0; i < memory_plan->n_memory_regions(); ++i)
  {
    const auto& region = *memory_plan->memory_region(i);

    if((int32_t)region.size() > buffer_sizes[i])
    {
      NPU_TOOLKIT_ERROR("Memory region-%d buffer not large enough (required=%d, provided=%d)",
        i, region.size(), buffer_sizes[i]
      );
      return nullptr;
    }

    if(i == 0)
    {
      primary_planned_persistent_buffer = base_memory_allocator->AllocatePersistentBuffer(
         region.planned_persistent_size(),
         MicroArenaBufferAlignment()
      );
      if(primary_planned_persistent_buffer == nullptr)
      {
        NPU_TOOLKIT_ERROR("Failed to allocate primary planned-persistent buffer");
        return nullptr;
      }
      buffer_info[i].base = base_memory_allocator->GetOverlayMemoryAddress();
    }
    else
    {
      buffer_info[i].base = buffers[i];
    }
  }

  // Create a buffer to store the TfliteMicroCompiledAllocator instance
  auto allocator_instance_buffer = base_memory_allocator->AllocatePersistentBuffer(
    sizeof(TfliteMicroCompiledAllocator),
    alignof(TfliteMicroCompiledAllocator)
  );
  if(allocator_instance_buffer == nullptr)
  {
    NPU_TOOLKIT_ERROR("Failed to allocate TfliteMicroCompiledAllocator instance buffer");
    return nullptr;
  }

  // Instantiate the TfliteMicroCompiledAllocator
  return new(allocator_instance_buffer)TfliteMicroCompiledAllocator(
    memory_plan,
    base_memory_allocator,
    buffer_info,
    primary_planned_persistent_buffer
  );
}

TfliteMicroCompiledAllocator::TfliteMicroCompiledAllocator(
  const CompiledMemoryPlan* memory_plan,
  tflite::SingleArenaBufferAllocator* base_allocator,
  BufferInfo* buffers,
  uint8_t* primary_planned_persistent_buffer
) :
 tflite::MicroAllocator(base_allocator, nullptr),
  _memory_plan(*memory_plan),
  _base_allocator(base_allocator),
  _buffers(buffers),
  // The planned-persistent buffers start after the non-persistent buffers
  _next_planned_persistent_buffer_index(memory_plan->n_non_persistent_buffers()),
  _primary_planned_persistent_buffer(primary_planned_persistent_buffer)
{
}

void* TfliteMicroCompiledAllocator::allocate_planned_persistent_buffer(
  const char* tag,
  size_t size,
  bool* is_ref
)
{
  if(_next_planned_persistent_buffer_index >= (int)_memory_plan.n_buffers())
  {
    NPU_TOOLKIT_ERROR("Planned-persistent buffer overflow");
    return nullptr;
  }

  const auto buffer_offset = _memory_plan.buffer_offset(_next_planned_persistent_buffer_index++);

  if(!buffer_offset.is_valid())
  {
    NPU_TOOLKIT_ERROR("Invalid offline offset");
    return nullptr;
  }

  if(buffer_offset.memory_region_id >= _memory_plan.n_memory_regions())
  {
    NPU_TOOLKIT_ERROR("Invalid memory region index");
    return nullptr;
  }

  if(is_ref != nullptr)
  {
    *is_ref = buffer_offset.is_ref;
  }

  // memory_region_id == 0 is the primary tensor arena.
  // This region has a specially allocated buffer
  // (which was allocated inside the primary tensor arena)
  // for the "planned persistent" buffers.
  // IF the current buffer_offset IS:
  // - In the primary tensor arena memory region
  // - Is NOT a reference
  // THEN return the primary tensor arena planned persistent buffer.
  // ELSE use the memory region's base address:
  // - For the primary tensor arena, this is the non-persistent buffers
  // - For other memory regions, the non-persistent and planned persistent have the same base address
  auto buffer_base = (buffer_offset.memory_region_id == 0 && !buffer_offset.is_ref) ?
    _primary_planned_persistent_buffer : _buffers[buffer_offset.memory_region_id].base;
  auto ptr = buffer_base + buffer_offset.offset;


  return ptr;
}

uint8_t TfliteMicroCompiledAllocator::n_memory_regions() const
{
  return _memory_plan.n_memory_regions();
}

uint8_t* TfliteMicroCompiledAllocator::get_base_addr(int memory_region) const
{
  if(memory_region >= _memory_plan.n_memory_regions())
  {
    NPU_TOOLKIT_ERROR("Invalid memory region index");
    return nullptr;
  }

  return _buffers[memory_region].base;
}

uint8_t* TfliteMicroCompiledAllocator::get_planned_persistent_base_addr(int memory_region) const
{
  if(memory_region >= _memory_plan.n_memory_regions())
  {
    NPU_TOOLKIT_ERROR("Invalid memory region index");
    return nullptr;
  }

  return (memory_region == 0) ? _primary_planned_persistent_buffer : _buffers[memory_region].base;
}


uint8_t* TfliteMicroCompiledAllocator::get_non_persistent_base_addr(int memory_region) const
{
  if(memory_region >= _memory_plan.n_memory_regions())
  {
    NPU_TOOLKIT_ERROR("Invalid memory region index");
    return nullptr;
  }

  return _buffers[memory_region].base;
}


TfLiteStatus TfliteMicroCompiledAllocator::get_non_persistent_buffer_address(
  int index,
  int32_t buffer_size_bytes,
  uint8_t** buffer_address_ptr,
  int32_t memory_region_max_usage[]
)
{
  *buffer_address_ptr = nullptr;

  if(index >= _memory_plan.n_non_persistent_buffers())
  {
    NPU_TOOLKIT_ERROR("Invalid offline offset count");
    return kTfLiteError;
  }

  const auto buffer_offset = _memory_plan.buffer_offset(index);

  if(!buffer_offset.is_valid())
  {
    NPU_TOOLKIT_ERROR("Invalid offline offset");
    return kTfLiteError;
  }

  if(buffer_offset.memory_region_id >= _memory_plan.n_memory_regions())
  {
    NPU_TOOLKIT_ERROR("Invalid memory region index");
    return kTfLiteError;
  }

  uint8_t* buffer_base_address = get_non_persistent_base_addr(buffer_offset.memory_region_id);

  // Calculate the buffer runtime address
  *buffer_address_ptr = buffer_base_address + buffer_offset.offset;

  if(memory_region_max_usage != nullptr)
  {
    const int32_t required_size =
      tflite::AlignSizeUp(buffer_offset.offset + buffer_size_bytes, MicroArenaBufferAlignment());
    // For bookkeeping, also keep track of the largest size required by the buffer's memory region
    memory_region_max_usage[buffer_offset.memory_region_id] =
      std::max(memory_region_max_usage[buffer_offset.memory_region_id], required_size);
    // NPU_TOOLKIT_DEBUG("Tensor-%d: region=%d offset=%X size=%d",
    //   index,
    //   buffer_offset.memory_region_id,
    //   buffer_offset.offset,
    //   required_size
    // );
  }

  return kTfLiteOk;
}


TfLiteStatus TfliteMicroCompiledAllocator::CommitStaticMemoryPlan(
  const tflite::Model* model,
  tflite::SubgraphAllocations* allocations,
  tflite::ScratchBufferHandle* scratch_buffer_handles
)
{
  const constexpr int MAX_MEMORY_REGIONS = 8;
  const int n_regions = _memory_plan.n_memory_regions();
  if(n_regions > MAX_MEMORY_REGIONS)
  {
    return kTfLiteError;
  }

  int32_t memory_region_max_usage[MAX_MEMORY_REGIONS] = {0};
  int buffer_index = 0;
  internal::ScratchBufferRequest* scratch_buffer_requests = GetScratchBufferRequests();

  // Iterate through each subgraph
  for (size_t subgraph_idx = 0; subgraph_idx < model->subgraphs()->size(); subgraph_idx++)
  {
    const SubGraph* subgraph = model->subgraphs()->Get(subgraph_idx);
    const auto tensors = subgraph->tensors();
    const int subgraph_tensor_count = tensors->size();
    TfLiteEvalTensor* eval_tensors = allocations[subgraph_idx].tensors;

    NPU_TOOLKIT_CHECK(subgraph != nullptr);

    // Allocate the subgraph's variable tensors
    // TODO: Add support for specifying variables in offline planning
    TF_LITE_ENSURE_STATUS(
      AllocateVariables(subgraph, allocations[subgraph_idx].tensors, nullptr)
    );

    // Iterate through each tensor in the subgraph
    for (int i = 0; i < subgraph_tensor_count; ++i)
    {
      size_t tensor_size_bytes;
      const auto tensor = tensors->Get(i);
      auto eval_tensor = &eval_tensors[i];

      TF_LITE_ENSURE_STATUS(
        TfLiteEvalTensorByteLength(eval_tensor, &tensor_size_bytes)
      );

      // Calculate this tensor's buffer address (if necessary)
      if(eval_tensor->data.raw == nullptr &&
         !tensor->is_variable() &&
         tensor_size_bytes > 0
      )
      {
        TF_LITE_ENSURE_STATUS(
          get_non_persistent_buffer_address(
            buffer_index,
            tensor_size_bytes,
            &eval_tensor->data.uint8,
            memory_region_max_usage
          )
        );
        buffer_index += 1;
      }
    }
  }

  // Iterate through each scratch buffer
  // and populate it buffer address
  for(int i = 0 ; i < (int)scratch_buffer_request_count_; ++i)
  {
    TF_LITE_ENSURE_STATUS(
      get_non_persistent_buffer_address(
        buffer_index,
        scratch_buffer_requests[i].bytes,
        &scratch_buffer_handles[i].data,
        memory_region_max_usage
      )
    );
    buffer_index += 1;
  }


  TF_LITE_ENSURE_STATUS(
    non_persistent_buffer_allocator_->ResetTempAllocations()
  );
  TF_LITE_ENSURE_STATUS(
    non_persistent_buffer_allocator_->DeallocateResizableBuffer(scratch_buffer_head_)
  );

  // Ensure each provided memory region is large enough
  // to store the tensors in the pre-compiled memory plan
  for(int i = 0; i < (int)_memory_plan.n_memory_regions(); ++i)
  {
    if(i == 0)
    {
      if(_base_allocator->ReserveNonPersistentOverlayMemory(
          memory_region_max_usage[i],
          MicroArenaBufferAlignment()
        ) != kTfLiteOk)
      {
        NPU_TOOLKIT_ERROR("Not enough room in memory region: %d\n", i);
        return kTfLiteError;
      }
    }
    else
    {
      if(memory_region_max_usage[i] > (int)_memory_plan.non_persistent_buffer_size(i))
      {
        NPU_TOOLKIT_ERROR("Not enough room in memory region: %d (%d > %d)\n",
          i, memory_region_max_usage[i], _memory_plan.non_persistent_buffer_size(i)
        );
        return kTfLiteError;
      }
    }
  }

  return kTfLiteOk;
}

} // namespace npu_toolkit