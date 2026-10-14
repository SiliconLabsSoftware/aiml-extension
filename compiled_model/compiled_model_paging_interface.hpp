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
#include <cstdio>

#include "compiled_model_interface.hpp"


namespace npu_toolkit
{


/**
 * @defgroup compiled_model_paging_interface Compiled Model Paging Interface
 * @{
 */



/**
 * This is the .tflite metadata tag to store the paged weights data.
 */
constexpr const char PAGED_DATA_TFLITE_TAG[] = "sl_paged_data_v1";

/**
 * DMA transfer configuration
 *
*/
struct PagingTransferConfig
{
  /**
   * The number of bytes used by this struct
   */
  static const constexpr unsigned LENGTH_BYTES              = 12;
  /**
   * The number of 32-bit words used by this struct
   */
  static const constexpr unsigned LENGTH_WORDS              = 3;


  /**
   * Flag to indicate if this transfer's buffer is only transfers once on startup.
   * i.e. If the data only needs to be copied once and persists
   * for subsequent invocations.
   */
  static const constexpr unsigned FLAG_IS_ONE_TIME_TRANSFER = (1 << 0);
  /**
   * Flag to indicate if this transfer should automatically
   * trigger the next linked DMA.
   */
  static const constexpr unsigned FLAG_SET_AUTO_TRIGGER     = (1 << 1);
  /**
   * Flag to indicate if this is the last transfer in the
   * current paged transfer list, and it should link to the
   * other paged transfer list (i.e. ping-pong to the other transfer list buffer).
   */
  static const constexpr unsigned FLAG_LINK_TO_NEXT_XFR_LIST= (1 << 2);
  /**
   * Flag to indicate that this transfer uses dual weights paging.
   * i.e. Part of the weights are copied from flash and part are copied from PSRAM.
   */
  static const constexpr unsigned FLAG_IS_DUAL_TRANSFER     = (1 << 3);
  /**
   * Flag indicating if the buffer is compressed.
   */
  static const constexpr unsigned FLAG_COMPRESSION_ENABLED  = (1 << 4);


  /**
   * Return a pointer to this struct's underlying data
   */
  const uint8_t* data(int offset = 0) const
  {
    return reinterpret_cast<const uint8_t*>(this) + offset;
  }

   /**
   * The offset, in 32-bit words, from the beginning of
   * this transfer's source buffer.
   * e.g.
   * src_addr = source_buffers[type] + src_offset()
   */
  uint32_t src_offset() const
  {
    const auto v = *(const uint32_t*)data(0);
    return (v & 0x00FFFFFF); // first 24-bits contain the offset
  }

  /**
   * Flags specific to this transfers
   */
  uint8_t flags() const
  {
    return *data(3);
  }

  /**
   * The offset, in 32-bit words, from the beginning
   * of the specified memory region.
   * dst_addr = <memory region base address> + dst_offset()
   */
  uint32_t dst_offset() const
  {
    const auto v = *(const uint32_t*)data(4);
    return (v & 0x00FFFFFF); // first 24-bits contain the offset
  }

  /**
   * ID of memory region that will store the transferred data
   */
  uint8_t dst_memory_region_id() const
  {
    return *data(7);
  }

  /**
   * The number of 32-bit words to transfer
   */
  uint16_t count() const
  {
    return *(const uint16_t*)data(8);
  }

  /**
   * A unique ID of this transfer
   */
  uint16_t transfer_id() const
  {
    return *(const uint16_t*)data(10);
  }

  /**
   * Convert this transfer to human-readable string
   */
  const char* to_str(char *buf = nullptr) const
  {
    static char buffer[128];
    buf = (buf == nullptr) ? buffer : buf;
    snprintf(buf, sizeof(buffer), "id=%03d flgs=%02X cnt=%4d src=%06X dst=%04X mem=%d",
      transfer_id(), flags(), count(), (unsigned int)src_offset(), (unsigned int)dst_offset(), (unsigned int)dst_memory_region_id());
    return buf;
  }

};



/**
 * Paging Configuration
 *
 * This is used during CompiledModelPagig::init()
 * to initialize the page library.
*/
struct PagingConfig
{

  /**
   * Flag indicating that the DMA transfers should be triggered
   * after initialization.
   */
  static const constexpr unsigned FLAG_TRIGGER_TRANSFERS     = (1 << 0);
  /**
   * Flag indicating that dual weights paging is enabled
   */
  static const constexpr unsigned FLAG_DUAL_WEIGHTS_PAGING_ENABLED = (1 << 1);
  /**
   * Flag indicating if weights compression is enabled
   */
  static const constexpr unsigned FLAG_COMPRESSION_ENABLED = (1 << 2);

 /**
   * Create a PagingConfig instance from the compiled data
   */
  static const PagingConfig* create(const void* p)
  {
    return reinterpret_cast<const PagingConfig*>(p);
  }

  /**
   * Return a pointer to this object's underlying data
   */
  const uint8_t* data(int offset = 0) const
  {
    return reinterpret_cast<const uint8_t*>(this) + offset;
  }

  /**
   * Return the memory region ID that contains
   * the paging buffer. This is determined by
   * the compiler's memory planner and obtained
   * from TfliteMicroCompiledAllocator.
   *
   */
  uint8_t paging_buffer_memory_region() const
  {
    return *data(0) & 0x0F;
  }

  /**
   * Return the memory region ID that contains
   * the dual weights buffer. This is determined by
   *  the compiler's memory planner and obtained
   * from TfliteMicroCompiledAllocator.
   */
  uint8_t dual_weights_memory_region() const
  {
    return (*data(0) >> 4);
  }

  /**
   * Flags specific to this PagingConfig
   */
  uint8_t flags() const
  {
    return *data(1);
  }

  /**
   * The number of "transfer list" used
   * by this compiled model.
   * A "transfer list" is a list of PagingTransferConfig
   */
  uint16_t n_transfer_lists() const
  {
    return *(const uint16_t*)data(2);
  }

  /**
   * The total number of transfers used by
   * this compiled model.
   * i.e. The total number of PagingTransferConfig
   */
  uint16_t n_transfers() const
  {
    return *(const uint16_t*)data(4);
  }

  /**
   * The size, in 32-bit words, of the ping-pong buffers in the paging buffer
   * that stores the "transfer lists".
   *
   */
  uint16_t transfer_list_buffer_size_words() const
  {
    return (*(const uint16_t*)data(6));
  }

  /**
   * The initial value of the transfer ID
   * to wait on before executing.
   */
  uint16_t initial_next_transfer_id() const
  {
    return *(const uint16_t*)data(8);
  }

  /**
   * The offset, in 32-bit words, to
   * layer_config buffer0 in the paging_buffer. This is a ping-pong
   * buffer to stored the paged compiled layer configs.
   */
  uint16_t layer_config_buffer_offset0() const
  {
    return *(const uint16_t*)data(10);
  }

  /**
   * The offset, in 32-bit words, to
   * transfer_list buffer0 in the paging_buffer. This is a ping-pong
   * buffer to stored the paged transfer_lists.
   */
  uint16_t transfer_list_buffer_offset0() const
  {
    return *(const uint16_t*)data(12);
  }

  /**
   * The offset, in 32-bit words, to
   * transfer_list buffer1 in the paging_buffer. This is a ping-pong
   * buffer to stored the paged transfer_lists.
   */
  uint16_t transfer_list_buffer_offset1() const
  {
    return *(const uint16_t*)data(14);
  }

  /**
   * The offset, in 32-bit words, to
   * the "weights2" data in the "paging_data" metadata buffer.
   * All of the weights data is stored in the PAGED_DATA_TFLITE_TAG
   * metadata buffer which typically resides in flash.
   * Also is this buffer are the weights that should reside in PSRAM.
   * This is the offset in the flash buffer that should copied to the PSRAM
   * on startup.
   */
  uint32_t weights2_src_offset() const
  {
    return *(const uint32_t*)data(16);
  }

  /**
   * The number of 32-bit words that are in the "weigths2" buffer.
   */
  uint32_t weights2_count() const
  {
    return *(const uint32_t*)data(20);
  }

  /**
   * List of tensor indices that should be copied from
   * the .tflite's memory (typically flash) to PSRAM.
   */
  const int16_t* copied_tensor_indices() const
  {
    return (const int16_t*)data(24);
  }


};


/**
 * Helper struct to hold information
 * about a linked item.
 * This is used by:
 * @ref PagingCompressedProgramConfig and @ref PagingCompiledLayerConfig
 */
struct PagingCompiledLinkedItemConfig
{
  /**
   * Flag that indicates if the current item
   * should trigger the next DMA.
   */
  static const constexpr uint16_t FLAG_TRIGGER_NEXT_TRANSFER = (1 << 14);
  /**
   * Flag indicating if this is the last item.
   * i.e. If this is the last accelerator program that needs to execute.
   */
  static const constexpr uint16_t FLAG_IS_LAST_ITEM = (1 << 15);
  /**
   * 14-bit mask to contain the NEXT transfer ID
   * that we must wait on before we can continue execution.
   */
  static const constexpr uint16_t NEXT_TRANSFER_ID_MASK = 0x3FFF;

  /**
   * Stores the transfer_id and flags
   */
  const uint16_t value;

  /**
   * Constructor
   */
  PagingCompiledLinkedItemConfig(const uint16_t v) : value(v){}

  /**
   * Overloaded comparison operator
   */
  const PagingCompiledLinkedItemConfig operator=(const PagingCompiledLinkedItemConfig& other)
  {
    return PagingCompiledLinkedItemConfig(other.value);
  }

  /**
   * The ID of the NEXT transfer that we must wait on
   * before execution can continue.
   */
  uint16_t next_transfer_id() const
  {
    return value & NEXT_TRANSFER_ID_MASK;
  }

  /**
   * Flag indicating that this compiled item
   * should trigger the next DMA transfer to execute.
   */
  bool trigger_next_transfer() const
  {
    return (value & FLAG_TRIGGER_NEXT_TRANSFER) != 0;
  }

  /**
   * Flag indicating if this is the last item.
   * i.e. If this is the last accelerator program that needs to execute.
   */
  bool is_last_item() const
  {
    return (value & FLAG_IS_LAST_ITEM) != 0;
  }

};


/**
 * Base struct for paged, compiled linked items.
 * This is a parent class of:
 * @ref PagingCompressedProgramConfig and @ref PagingCompiledLayerConfig
 *
 * The underlying data has the format:
 * <int32:  next_item_word_offset>
 * <uint16: linked item config>
 * <... data ...>
 */
struct PagingCompiledLinkedItem : CompiledLinkedItem
{
  /**
   * Item configuration. See @ref PagingCompiledLinkedItemConfig
   */
  PagingCompiledLinkedItemConfig config() const
  {
    return PagingCompiledLinkedItemConfig(*(const uint16_t*)(base() + sizeof(int32_t)));
  }

  /**
   * Pointer to underlying data
   */
  const uint8_t* data() const
  {
    return base() + sizeof(int32_t) + sizeof(uint16_t);
  }

  /**
   * Pointer to underlying data with offset
   */
  const uint8_t* data(uint32_t offset) const
  {
    return base() + sizeof(int32_t) + sizeof(uint16_t) + offset;
  }

  /**
   * Pointer to next PagingCompiledLinkedItem
   */
  const PagingCompiledLinkedItem* next_item() const
  {
    return reinterpret_cast<const PagingCompiledLinkedItem*>((const uint32_t*)this + this->next_item_offset());
  }
};


/**
 * Information about a compiled accelerator program.
 * This is similar to @ref CompressedProgramConfig but it also
 * supports paging.
 *
 * The underlying data has the format:
 * <int32   : next_item_word_offset>
 * <uint16  : linked item config>
 * <uint16  : byte offset to register values>
 * <uint8   : number of register groups>
 * <uint8*  : list of register offsets>
 * <uint32* : list of 32-bit register values>
 */
struct PagingCompressedProgramConfig : PagingCompiledLinkedItem
{

  /**
   * Byte offset to the list of register values.
   * See the register_values() function below.
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
   * Pointer to list of 32-bit register values
   */
  const uint32_t* register_values() const
  {
    return (const uint32_t*)data(register_values_offset());
  }
};

/**
 * Paging configuration for an individual model layer
 *
 * This is similar to @ref CompiledLayerConfig but it also
 * supports paging.
 *
 * The underlying data has the format:
 * <int32   : next_item_word_offset>
 * <uint16  : linked item config>
 * <uint16  : number of accelerator programs used by this layer>
 * <uint8   : 1 if layer uses paging, 0 else>
*/
struct PagingCompiledLayerConfig : PagingCompiledLinkedItem
{
  /**
   * The number of accelerator programs used by this layer
   */
  uint16_t n_programs() const
  {
    return *(const uint16_t*)data(0);
  }
  /**
   * Return true if this layer uses paging
   */
  bool uses_paging() const
  {
    return data(2) != 0;
  }

};

/**
 * @}
 */


} // namepsace npu_toolkit