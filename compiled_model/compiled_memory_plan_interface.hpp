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

#include <cassert>
#include <cstdint>

#include "compiled_model_utils.hpp"



namespace npu_toolkit
{

  /**
 * @addtogroup tflite_micro_model
 * @defgroup compiled_memory_plan_interface
 * @{
 */



/**
 * This is the .tflite model meta tag used to store the compiled memory plan
 */
constexpr const char COMPILED_MEMORY_PLAN_TFLITE_TAG[] = "sl_memory_plan_v1";


/**
 * Memory Plan Buffer Offset
 *
 * @see @ref :py:class:npu_toolkit.compiler.CompiledMemoryPlanBufferOffset
*/
#pragma pack(push, 1)
struct CompiledModelMemoryPlanBufferOffset
{
  /** The byte offset from the beginning of the corresponding memory region */
  const uint32_t offset : 27;
  /** The index of the buffer's memory region */
  const uint32_t memory_region_id : 4;
  /** Flag indicating if this buffer is a reference to another memory region.
   * Typically, this is used by a "planned persistent buffer" that references
   * non-persistent memory
   */
  const uint32_t is_ref : 1;


  bool is_valid() const
  {
    return ((int32_t)offset) >= 0 && ((int32_t)memory_region_id) >= 0;
  }
};
#pragma pack(pop)


/**
 * A memory region used by the memory plan
 *
 * @see @ref :py:class:npu_toolkit.compiler.CompiledMemoryRegion
 */
struct CompiledModelMemoryRegion
{
  /** Size of the packed struct in bytes */
  static constexpr const int SIZE_BYTES = 8;


  static const CompiledModelMemoryRegion* create(const void* p)
  {
    return reinterpret_cast<const CompiledModelMemoryRegion*>(p);
  }

  const uint8_t* data(int offset = 0) const
  {
    return reinterpret_cast<const uint8_t*>(this) + offset;
  }

  /**
   * The size, in bytes, used by the non-persistent buffers
  */
  uint32_t non_persistent_size() const
  {
    return bytes_to_uint32(data(0));
  }

  /**
   * The size, in bytes, used by the planned-persistent buffers
  */
  uint32_t planned_persistent_size() const
  {
    return bytes_to_uint32(data(4));
  }

  /**
   * The total size, in bytes, of this memory region
  */
  uint32_t size() const
  {
    return non_persistent_size() + planned_persistent_size();
  }

};

/**
 * Memory Plan
 *
 * This provides an API for the pre-compiled
 * memory plan that is optionally available in
 * a model's metadata.
 *
 * @see @ref :py:class:npu_toolkit.compiler.CompiledMemoryPlan
*/
struct CompiledMemoryPlan
{
  static const CompiledMemoryPlan* create(const void* p)
  {
    return reinterpret_cast<const CompiledMemoryPlan*>(p);
  }

  const uint8_t* data(int offset = 0) const
  {
    return reinterpret_cast<const uint8_t*>(this) + offset;
  }


  /**
   * The total number of non-persistent buffers
  */
  uint16_t n_non_persistent_buffers() const
  {
    return bytes_to_uint16(data(0));
  }

  /**
   * The total number of planned-persistent buffers
  */
  uint16_t n_planned_persistent_buffers() const
  {
    return bytes_to_uint16(data(2));
  }

 /**
   * The total number of buffers
  */
  uint32_t n_buffers() const
  {
    return n_non_persistent_buffers() + n_planned_persistent_buffers();
  }

  /**
   * The number of memory regions
  */
  uint8_t n_memory_regions() const
  {
    return *data(4);
  }

  /**
   * Return the list of CompiledModelMemoryRegion
  */
  const CompiledModelMemoryRegion* memory_regions() const
  {
    // The memory regions structs are 32-bit aligned, hence they start at offset 8
    return reinterpret_cast<const CompiledModelMemoryRegion*>(data(8));
  }


  /**
   * Return a specific memory region
  */
  const CompiledModelMemoryRegion* memory_region(int index) const
  {
    assert(index < n_memory_regions());
    const int offset = 8 + index * CompiledModelMemoryRegion::SIZE_BYTES;
    return reinterpret_cast<const CompiledModelMemoryRegion*>(data(offset));
  }


  /**
   * This size, in bytes, of a specific memory region's non-persistent buffers
  */
  uint32_t non_persistent_buffer_size(int index) const
  {
    const auto region = memory_region(index);
    return region->non_persistent_size();
  }

  /**
   * This size, in bytes, of a specific memory region's planned-persistent buffers
  */
  uint32_t planned_persistent_buffer_size(int index) const
  {
    const auto region = memory_region(index);
    return region->planned_persistent_size();
  }

  /**
   * This size, in bytes, of a specific memory region's buffers
   */
  uint32_t memory_region_size(int index) const
  {
    const auto region = memory_region(index);
    return region->size();
  }

  /**
   * Return the list of buffer offsets
  */
  const CompiledModelMemoryPlanBufferOffset* buffer_offsets() const
  {
    // The memory regions structs are 32-bit aligned, hence they start at offset 8
    const int offset = 8 + n_memory_regions()*CompiledModelMemoryRegion::SIZE_BYTES;
    return reinterpret_cast<const CompiledModelMemoryPlanBufferOffset*>(data(offset));
  }

  /**
   * Return a specific buffer offset
  */
  const CompiledModelMemoryPlanBufferOffset buffer_offset(int index) const
  {
    return buffer_offsets()[index];
  }

};

/**
 * @}
 */

} // namespace npu_toolkit