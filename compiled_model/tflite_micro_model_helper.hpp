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

#include "tensorflow/lite/schema/schema_generated.h"
#include "tensorflow/lite/micro/micro_allocator.h"
#include "tflite_micro_model_context.hpp"


namespace npu_toolkit
{

class TfliteMicroModel;
class TfliteMicroModelContext;

/**
 * @addtogroup tflite_micro_model
 * @defgroup tflite_micro_model_helper
 * @{
 */


/**
 * Helper class to manage/access information in a @ref TfliteMicroModel
 */
class TfliteMicroModelHelper
{
public:
    /**
     * Return a pointer to the model flatbuffer model instance
     *
     * @param context The model context, if null then use @ref get_active_context()
     * @return model flatbuffer model instance or nullptr if not available
     */
    static const tflite::Model* flatbuffer_model(TfLiteContext* context = nullptr);
    /**
     * Return a pointer to the @ref TfliteMicroModel instance
     *
     * @param context The model context, if null then use @ref get_active_context()
     * @return pointer to @ref TfliteMicroModel or nullptr if not available
     */
    static TfliteMicroModel* tflite_micro_model(TfLiteContext* context = nullptr);
    /**
     * Return a pointer to the @ref TfliteMicroModelContext instance
     *
     * @param context The model context, if null then use @ref get_active_context()
     * @return pointer to @ref TfliteMicroModelContext or nullptr if not available
     */
    static TfliteMicroModelContext* tflite_micro_model_context(TfLiteContext* context = nullptr);
    /**
     * Return a pointer to the @ref tflite::MicroAllocator instance
     *
     * @param context The model context, if null then use @ref get_active_context()
     * @return pointer to @ref tflite::MicroAllocator or nullptr if not available
     */
    static tflite::MicroAllocator* tflite_micro_model_allocator(TfLiteContext* context = nullptr);
    /**
     * Retrun the opcode of the active layer
     *
     * @param context The model context, if null then use @ref get_active_context()
     * @return The opcode of the active layer
     */
    static tflite::BuiltinOperator current_layer_opcode(TfLiteContext* context = nullptr);
    /**
     * Retrun the index of the active layer
     *
     * @param context The model context, if null then use @ref get_active_context()
     * @return The index of the active layer
     */
    static int current_layer_index(TfLiteContext* context = nullptr);
    /**
     * Retrun the name of the active layer
     *
     * @see create_layer_name()
     *
     * @param context The model context, if null then use @ref get_active_context()
     * @return The name of the active layer
     */
    static const char* current_layer_name(TfLiteContext* context = nullptr);

    /**
     * Set the context of the model that is actively executing
     *
     * @see @ref get_active_context()
     */
    static void set_active_context(TfLiteContext* context);
    /**
     * Return a pointer of the model context that is actively executing
     *
     * @see @ref set_active_context()
     */
    static TfLiteContext* get_active_context(bool allow_null = false);


    /**
     * Allocate a persistent buffer in the tensor arena
     */
    template<typename T = uint8_t>
    static  T* allocate_persistent_buffer(
      TfLiteContext* context,
      unsigned count
    )
    {
      if(context == nullptr) return nullptr;
      return reinterpret_cast<T*>(context->AllocatePersistentBuffer(context, sizeof(T) * count));
    }

    /**
     * Allocate a "planned" persistent buffer in the tensor arena.
     * This will be used by the model compiler for planning which memory region to store the buffer.
     */
    template<typename T = uint8_t>
    static T* allocate_planned_persistent_buffer(
      TfLiteContext* context,
      const char* tag,
      unsigned count,
      bool* is_ref = nullptr
    )
    {
      T* retval = nullptr;
      auto allocator = tflite_micro_model_allocator(context);
      if(is_ref != nullptr)
      {
        *is_ref = false;
      }

      if(allocator != nullptr)
      {
        retval = reinterpret_cast<T*>(allocator->allocate_planned_persistent_buffer(tag, count * sizeof(T), is_ref));
      }

      if(retval == nullptr)
      {
        retval = allocate_persistent_buffer<T>(context, count);
      }

      return retval;
    }

    /**
     * Allocate a scratch buffer in the tensor arena.
     */
    template<typename T = uint8_t>
    static TfLiteStatus allocate_scratch_buffer(
      TfLiteContext* context,
      unsigned count,
      int *buffer_index
    )
    {
        return context->RequestScratchBufferInArena(context, sizeof(T)*count, buffer_index);
    }

    /**
     * Return a scratch buffer
     */
    template<typename T = uint8_t>
    static T* get_scratch_buffer(TfLiteContext* context, int scratch_buffer_index)
    {
        return reinterpret_cast<T*>(context->GetScratchBuffer(context, scratch_buffer_index));
    }

    /**
     * Convert the given opcode to a human-readable string
     */
    static const char* opcode_to_str(tflite::BuiltinOperator opcode);

    /**
     * Create a layer name from the given index and opcode
     * This has the format: `op<index>-<opcode>`
     */
    static const char* create_layer_name(int layer_idx, tflite::BuiltinOperator opcode);

    /**
     * Return true if the given flatbuffer data is a valid tflite::Model
     */
    static bool verify_model_flatbuffer(const void* flatbuffer, int flatbuffer_length);

    /**
     * Retrieve metadata from the given flatbuffer
     */
    static const void* get_metadata_from_tflite_flatbuffer(
      const void* tflite_flatbuffer,
      const char* tag,
      uint32_t* length = nullptr
    );
    /**
     * Retrieve metadata from model with the given context
     */
    static const void* get_metadata_from_tflite_flatbuffer(
      TfLiteContext* context,
      const char* tag,
      uint32_t* length = nullptr
    );
    /**
     * Retrieve metadata from the given tflite model instance
     */
    static const void* get_metadata_from_tflite_flatbuffer(
      const tflite::Model *model,
      const char* tag,
      uint32_t* length = nullptr
    );
};

/**
 * Helper class to manage the context of the actively executing model.
 *
 * During the constructor this calls @ref TfliteMicroModelHelper::set_active_context()
 * and the destructor calls @ref TfliteMicroModelHelper::set_active_context()
 */
class TfliteMicroModelContextManager
{
public:
  /**
   * Constructor to set the active context
   * @see @ref TfliteMicroModelHelper::set_active_context()
   */
  TfliteMicroModelContextManager(TfLiteContext *context)
  {
    TfliteMicroModelHelper::set_active_context(context);
  }

  /**
   * Destructor to clear the active context
   * @see @ref TfliteMicroModelHelper::set_active_context()
   */
  ~TfliteMicroModelContextManager()
  {
    TfliteMicroModelHelper::set_active_context(nullptr);
  }
};


/**
 * Macro to allocate a persistent buffer in the tensor arena
 *
 * @see @ref TfliteMicroModelHelper::allocate_persistent_buffer()
 */
#define NPU_TOOLKIT_ALLOCATE_PERSISTENT_BUFFER(type, count) \
  ::npu_toolkit::TfliteMicroModelHelper::allocate_persistent_buffer<type>(context, count)

/**
 * Macro to allocate a "planned" persistent buffer in the tensor arena
 *
 * @see @ref TfliteMicroModelHelper::allocate_planned_persistent_buffer()
 */
#define NPU_TOOLKIT_ALLOCATE_PLANNED_PERSISTENT_BUFFER(tag, type, count, ...) \
  ::npu_toolkit::TfliteMicroModelHelper::allocate_planned_persistent_buffer<type>(context, tag, count, ## __VA_ARGS__)

/**
 * Macro to allocate a scratch buffer in the tensor arena
 *
 * @see @ref TfliteMicroModelHelper::allocate_scratch_buffer()
 */
#define NPU_TOOLKIT_ALLOCATE_SCRATCH_BUFFER(size_bytes, scratch_buffer_index) \
  ::npu_toolkit::TfliteMicroModelHelper::allocate_scratch_buffer(context, size_bytes, scratch_buffer_index)

/**
 * Macro to retrieve a scratch buffer
 *
 * @see @ref TfliteMicroModelHelper::get_scratch_buffer()
 */
#define NPU_TOOLKIT_GET_SCRATCH_BUFFER(type, scratch_buffer_index) \
  ::npu_toolkit::TfliteMicroModelHelper::get_scratch_buffer<type>(context, scratch_buffer_index)


/**
 * @}
 */

} // namespace npu_toolkit