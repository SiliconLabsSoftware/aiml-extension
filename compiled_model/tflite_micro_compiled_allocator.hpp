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

#include "tensorflow/lite/micro/micro_allocator.h"
#include "compiled_memory_plan_interface.hpp"


namespace npu_toolkit
{

/**
 * @addtogroup tflite_micro_model
 * @defgroup tflite_micro_compiled_allocator
 * @{
 */


/**
 * The TfliteMicroCompiledAllocator inherits the TFLM MicroAllocator.
 *
 * It uses a pre-compiled memory plan, @ref CompiledMemoryPlan
 * to allocate the model's runtime buffers.
 *
 * The memory allocator allows for allocating buffers across
 * multiple memory regions (e.g. SRAM, PSRAM, DTCM)
*/
class TfliteMicroCompiledAllocator : public tflite::MicroAllocator
{
public:
  /** Instantiate the TfliteMicroCompiledModelAllocator
   *
   * The instantiate's buffer is stored at the beginning of buffers[0]
  */
  static TfliteMicroCompiledAllocator* create(
    const CompiledMemoryPlan* memory_plan,
    uint8_t* const buffers[],
    const int32_t buffer_sizes[],
    int32_t n_buffers
  );

  /**
   * Retrieve a CompiledMemoryPlan from the given
   * .tflite flatbuffer's metadata.
  */
  static const CompiledMemoryPlan* retrieve_memory_plan(
    const void* flatbuffer
  );

  /**
   * Allocate a planned-persistent buffer with the given size
  */
  void* allocate_planned_persistent_buffer(const char* tag, size_t size, bool* is_ref) override;

  /**
   * Get the base address of the given memory region
  */
  uint8_t* get_base_addr(int memory_region) const;

  /**
   * Ge the base address of the planned-persistent buffers
   * in the given memory region
  */
  uint8_t* get_planned_persistent_base_addr(int memory_region) const;

  /**
   * Ge the base address of the non-persistent buffers
   * in the given memory region
  */
  uint8_t* get_non_persistent_base_addr(int memory_region = 0) const;

  /**
   * Return the number of memory regions
  */
  uint8_t n_memory_regions() const;

private:

  /**
   * Buffer information
  */
  struct BufferInfo
  {
    uint8_t* base;     /// Base address of buffers
  };

  const CompiledMemoryPlan& _memory_plan;
  tflite::SingleArenaBufferAllocator* _base_allocator;
  BufferInfo* _buffers;
  int _next_planned_persistent_buffer_index;
  uint8_t* _primary_planned_persistent_buffer;


  TfliteMicroCompiledAllocator(
    const CompiledMemoryPlan* memory_plan,
    tflite::SingleArenaBufferAllocator* base_allocator,
    BufferInfo* buffers,
    uint8_t* primary_planned_persistent_buffer
  );

  /**
   * Override tflite::MicroAllocator:CommitStaticMemoryPlan.
   * Use the given DevNpuMemoryPlan to populate the model's buffers.
  */
  TfLiteStatus CommitStaticMemoryPlan(
    const tflite::Model* model,
    tflite::SubgraphAllocations* allocations,
    tflite::ScratchBufferHandle* scratch_buffer_handles
  ) override;

  /** Calculate the buffer's address at the given index */
  TfLiteStatus get_non_persistent_buffer_address(
    int index,
    int32_t buffer_size_bytes,
    uint8_t** buffer_address_ptr,
    int32_t memory_region_max_usage[]
  );

};

/**
 * @}
 */

} // namespace npu_toolkit