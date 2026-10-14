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


#include <cstdint>
#include "compiled_model_utils.hpp"


namespace npu_toolkit
{

/**
 * @defgroup compiled_model_interface Compiled Model Interface
 * @{
 */

/**
 * The tag used to store the compiled model data stored
 * in a .tflite's metadata.
 *
 * Refer to the corresponding Python:
 * npu_toolkit.compiler.common.embedded_interface.CompiledModelData
 */
constexpr const char COMPILED_MODEL_DATA_TFLITE_TAG[] = "sl_model_data_v1";

/**
 * The tag used to store the compiler version data stored
 * in a .tflite's metadata.
 *
 * Refer to the corresponding Python:
 * npu_toolkit.compiler.common.embedded_interface.CompilerVersionData
 */
constexpr const char COMPILER_VERSION_TFLITE_TAG[] = "sl_model_compiler_ver_v1";


/**
 * Contains information about the ML compiler used to compile the ML model.
 * This is used at runtime to ensure the built application's source code
 * is compatible with the compiled model.
 */
struct CompilerVersionData
{
  /**
   * The model compiler's version as an integer:
   * <major> << 24 | <minor> << 16 | <patch>
   */
  uint32_t compiler_version() const
  {
    return *reinterpret_cast<const uint32_t*>(data(0));
  }

  /**
   * The compiler's major version
   */
  uint8_t compiler_version_major() const
  {
    return compiler_version() >> 24;
  }

    /**
   * The compiler's minor version
   */
  uint8_t compiler_version_minor() const
  {
    return (compiler_version() >> 16) & 0xFF;
  }

    /**
   * The compiler's patch version
   */
  uint16_t compiler_version_patch() const
  {
    return compiler_version() & 0xFFFF;
  }

  /**
   * The kernels used for model compilation
   */
  const char* kernels_type() const
  {
    return reinterpret_cast<const char*>(data(4));
  }

  /**
   * Wrap the given bytes with this data struct
   */
  static const CompilerVersionData* create(const void* p)
  {
    return reinterpret_cast<const CompilerVersionData*>(p);
  }

  /**
   * Return pointer to underlying data
   */
  const uint8_t* data(uint32_t offset) const
  {
    return ((const uint8_t*)this) + offset;
  }
};


/**
 * An id used to indicate which accelerator
 * is used by the CompiledLayerConfig
 *
 * Refer to the corresponding Python:
 * npu_toolkit.compiler.common.embedded_interface.CompiledAcceleratorId
*/
enum class CompiledAcceleratorId : int8_t
{
  NoAccelerator = 0, //!< No accelerator used by layer
  Mvp = 1, //!< MVPv1 or 2
  MvpTse = 2, //!< MVPv2 + TSE
  Gce = 3, //!< GCE
};


/**
 * A generic wrapper for compiled data structures
 * that are linked together.
 *
 * Refer to the corresponding Python:
 * npu_toolkit.compiler.common.embedded_interface_common.CompiledLinkedItem
 *
 * The underlying data has the format:
 * <int32: relative offset>
 * <... data ...>
 */

struct CompiledLinkedItem
{
  /**
   * The relative, 32-bit offset
   * to the next item in the linked list.
   */
  int32_t next_item_offset() const
  {
    return *(const int32_t*)this;
  }

  /**
   * Pointer to the underlying data (including the offset)
   */
  const uint8_t* base() const
  {
    return (const uint8_t*)this;
  }

  /**
   *  Pointer to the underlying data (NOT including the offset)
   */
  const uint8_t* data() const
  {
    return base() + sizeof(int32_t);
  }

  /**
   *  Pointer to the underlying data (NOT including the offset)
   */
  const uint8_t* data(uint32_t offset) const
  {
    return base() + sizeof(int32_t) + offset;
  }

  /**
   *  Return the next item in the linked list
   */
  const CompiledLinkedItem* next_item() const
  {
    return reinterpret_cast<const CompiledLinkedItem*>((const uint32_t*)this + this->next_item_offset());
  }

};


/**
 * Contains the compressed program info for a single program:
 *
 * This has the format:
 * <int32  : 32-bit offset to next link item>
 * <uint16 : byte offset to register values>
 * <uint8  : # register groups>
 * <uint8* : register offsets>
 * <uint32*: 32-bit register values>
 *
 * Where <register offsets> has the format:
 * <# registers group0><group0 offsets><# registers group1><group1 offsets>...
 *
 * <register values>  has format:
 * <group0 32-bit register values><group1 32-bit register values>...
 *
 * Refer to the corresponding Python:
 * npu_toolkit.compiler.register_compressor.embedded_interface.CompressedProgramConfig
*/
struct CompressedProgramConfig : CompiledLinkedItem
{
  /**
   * Byte offset to 32-bit register values
   */
  uint16_t register_values_offset() const
  {
    return *(const uint16_t*)data(0);
  }

  /**
   * The number of register groups
   */
  uint8_t n_register_groups() const
  {
    return *data(2);
  }

  /**
   * Pointer to the register offsets
   */
  const uint8_t* register_offsets() const
  {
    return data(3);
  }

  /**
   * Pointer to 32-bit register values
   */
  const uint32_t* register_values() const
  {
    return (const uint32_t*)data(register_values_offset());
  }

  /**
   * Return the MVP array ID encoded into the register offset
   * This is used when populating the array base addresses
   */
  static inline uint8_t get_array_id_from_register_offset(uint8_t offset)
  {
    return offset & 0x0F;
  }

  /**
   * The default memory region ID when no specific memory region is configured
   */
  static const constexpr uint8_t DEFAULT_MEMORY_REGION_ID = 0x07;
  /**
   * Flag indicating an alternate memory region ID
   */
  static const constexpr uint8_t ALTERNATE_MEMORY_REGION_ID_FLAG = 0x08;
  /**
   * Mask for valid memory region ID bits, i.e. IDs 0-7 are valid
   */
  static const constexpr uint8_t MEMORY_REGION_ID_MASK = 0x07;

  /**
   * Return the memory region id from the register offset
   *  This is used when populating the array base addresses
   */
  static inline uint8_t get_memory_region_id_from_register_offset(uint8_t offset)
  {
    return (offset >> 4);
  }

};


/**
 * Contains all the compressed program info for a single model layer
 *
 * The underlying data has the format:
 * <int32   : next_item_word_offset>
 * <uint16  : number of accelerator programs used by this layer>
 *
 * Refer to the corresponding Python:
 * npu_toolkit.compiler.common.embedded_interface.CompiledLayerConfig
*/
struct CompiledLayerConfig : CompiledLinkedItem
{
  /**
   * The number of accelerator programs used by this layer
  */
  uint16_t n_programs() const
  {
    return *(const uint16_t*)data(0);
  }
};


/**
 * Contains all compilation data for a model
 *

 * Refer to the corresponding Python:
 * npu_toolkit.compiler.common.embedded_interface.CompiledModelData
*/
struct CompiledModelData
{
  /**
   * Wrap the given bytes with this data struct
   */
  static const CompiledModelData* create(const void* p)
  {
    return reinterpret_cast<const CompiledModelData*>(p);
  }

  /**
   * Byte offset to the layer_config.
   * This is a linked list of CompiledLayerConfig
   * and CompressedProgramConfig items.
   *
   * NOTE: This is 0 if paging is enabled.
   */
  uint32_t layer_config_offset() const
  {
    return *(const uint32_t*)data(0);
  }

  /**
   * Byte offset to the PagingConfig
   * or 0 if not used.
   */
  uint32_t paging_config_offset() const
  {
    return *(const uint32_t*)data(4);
  }

  /**
   *  Byte offset to list of PagingTransferConfig
   * or 0 if not used.
   */
  uint32_t paging_transfer_list_offset() const
  {
    return *(const uint32_t*)data(8);
  }

  /*
   * The number of layers in the model
  */
 uint16_t n_layers() const
 {
   return *(const uint16_t*)data(12);
 }

  /**
   * Return a list of CompiledAcceleratorId,
   * which indicates the accelerator used by each layer of the model.
  */
  const CompiledAcceleratorId* layer_accelerators() const
  {
    return reinterpret_cast<const CompiledAcceleratorId*>(data(14));
  }

  /**
   * Return a pointer to linked list of CompiledLayerConfig
   * and CompressedProgramConfig items.
   * This is a nullptr if paging is enabled.
  */
  const uint8_t* layer_config_ptr() const
  {
    return data(layer_config_offset());
  }

  /**
   * Return a pointer to the PagingConfig data
   * if available, nullptr otherwise
  */
  const uint8_t* paging_config_ptr() const
  {
    const auto offset = paging_config_offset();
    return (offset > 0) ? data(offset) : nullptr;
  }

  /**
   * Return a pointer to list of PagingTransferConfig
   * if available, nullptr otherwise
  */
  const uint8_t* paging_transfer_list_ptr() const
  {
    const auto offset = paging_transfer_list_offset();
    return (offset > 0) ? data(offset) : nullptr;
  }

  /**
   * Return pointer to underlying data
   */
  const uint8_t* data() const
  {
    return (const uint8_t*)this;
  }

  /**
   * Return pointer to underlying data
   */
  const uint8_t* data(uint32_t offset) const
  {
    return data() + offset;
  }
};

/**
 * @}
 */

} // namespace npu_toolkit