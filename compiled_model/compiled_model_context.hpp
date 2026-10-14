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

#include "tensorflow/lite/c/common.h"
#include "compiled_model_interface.hpp"
#include "compiled_model_paging_interface.hpp"
#include "compiled_model_dma_mgr.hpp"

namespace npu_toolkit
{
class CompiledModelPaging;
struct CompiledProgramInfo;


/**
 * @addtogroup compiled_model
 * @defgroup compiled_model_context Compiled Model Context
 * @{
 */



/**
 * Holds runtime information for executing a compiled model.
 */
class CompiledModelContext
{
public:



  /**
   * Create a CompiledModelContext as a "persistent"
   * buffer in the tensor arena.
   */
  static CompiledModelContext* create(
    TfLiteContext *context,
    const void* compiled_data
  );

  /**
   * Helper to return the model's CompiledModelContext
   * instance
   */
  template<typename ContextType>
  static CompiledModelContext* get(TfLiteContext *context)
  {
    auto acc_context = ContextType::get(context);
    return acc_context->compiled_context;
  }

 /**
   * Return the accelerator used by the layer with the given index.
   */
  CompiledAcceleratorId get_layer_accelerator(int index) const;

  /**
   * Initialize this CompiledModelContext.
   * This is called during TfliteMicroAccelerator:init()
   */

  bool init();
  /**
   * Load this CompiledModelContext.
   * This is called during TfliteMicroAccelerator:load()
   */

  bool load();
  /**
   * Deinitialize this CompiledModelContext.
   * This is called during TfliteMicroAccelerator:deinit()
   */

  void deinit();
  /**
   * Prepare this CompiledModelContext for the current layer's exeuction.
   */

  bool begin_layer();
  /**
   * Return the compiled accelerator register info
   * for the next program that should execute.
   */
  bool get_next_program(struct CompiledProgramInfo *info);

  /**
   * Wait for a necessary data to be ready before executing the next
   * accelerator program.
   */
  bool wait();

  /**
   * Release an data that was used by the last executed accelerator program.
   */
  void release();

  /**
   * Return the number of accelerator programs
   * required by the current layer.
   */
  uint16_t n_programs() const
  {
    return _layer_program_count;
  }


public:
  /**
   * Pointer to the model's TFLM context
   */
  TfLiteContext *_context = nullptr;
  /**
   * Pointer to the model's compiled data
   * stored in the .tflite's metadata.
   */
  const void* _compiled_data = nullptr;
  /**
   * Pointer to the CompiledModelPaging object (if used)
   */
  CompiledModelPaging* _paging = nullptr;
  /**
   * Point to the current compiled item.
   * At the beginning of each layer this is a
   * CompiledLayerConfig object. Then,
   * for each accelerator program, this is a CompressedProgramConfig.
   *
   * This points to compiled data in the .tflite's metadta.
   */
  const CompiledLinkedItem* _current_item = nullptr;
  /**
   * The number of accelerator programs used
   * by the current layer
   */
  uint16_t _layer_program_count = 0;

  /**
   * Class constructor
   */
  CompiledModelContext(
    TfLiteContext *context,
    const void* compiled_data
  );

  friend CompiledModelPaging;
};


/**
 * Contains information about a compiled accelerator program
 */
struct CompiledProgramInfo
{
  uint8_t n_register_groups;        /// The number of register groups used by this program
  const uint8_t* register_offsets;  /// List of register offsets. The first entry contains the number of offsets that follow for the group
  const uint32_t* register_values;  /// List of 32-bit register values
};


/**
 * @}
 */



} // namespace npu_toolkit