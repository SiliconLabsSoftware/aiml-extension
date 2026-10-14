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

#ifdef UINT32_WORKAROUND
typedef unsigned int uint32_t;
#endif

#include "compiled_model_paging_interface.hpp"


namespace npu_toolkit
{




struct DmaContext
{
  static const constexpr unsigned BUFFER_TYPE_WEIGHTS       = 0;
  static const constexpr unsigned BUFFER_TYPE_WEIGHTS2      = 1;
  static const constexpr unsigned BUFFER_TYPE_TRANSFER_LIST = 2;
  static const constexpr unsigned BUFFER_TYPE_COUNT         = 3;

  const uint32_t* src_buffer_base_addrs[BUFFER_TYPE_COUNT];
  uint32_t* paging_buffer;
  uint32_t* memory_region_addrs[4];
  uint32_t transfer_list_buffer_size_words;
  uint16_t transfer_list_buffer_offsets[2];
  bool should_trigger_transfers;
  uint8_t current_transfer_list_index;
  bool dual_weights_transfers_enabled;


  uint32_t transfer_list_buffer_size() const
  {
    return dual_weights_transfers_enabled ?
      transfer_list_buffer_size_words / 2 : transfer_list_buffer_size_words;
  }

  uint32_t current_transfer_list_buffer_offset() const
  {
    return transfer_list_buffer_offsets[current_transfer_list_index];
  }

  uint32_t* current_transfer_list_buffer() const
  {
    return paging_buffer + current_transfer_list_buffer_offset();
  }

  uint32_t current_transfer_list_buffer2_offset() const
  {
    return transfer_list_buffer_offsets[current_transfer_list_index] + transfer_list_buffer_size();
  }

  uint32_t* current_transfer_list_buffer2() const
  {
    return paging_buffer + current_transfer_list_buffer2_offset();
  }

  void update_current_transfer_list_index()
  {
    if(transfer_list_buffer_offsets[1] != 0)
    {
      current_transfer_list_index = (current_transfer_list_index + 1) % 2;
    }
  }
};

uint32_t dma_mgr_context_size(uint8_t flags);
bool dma_mgr_init(DmaContext* context, uint8_t flags);
void dma_mgr_deinit(DmaContext* context);
void dma_mgr_set_next_transfer_id(DmaContext* context, uint16_t id);
void dma_mgr_set_all_transfers_complete(DmaContext* context);
bool dma_mgr_wait(DmaContext* context);
void dma_mgr_trigger_next_transfer(DmaContext* context);
bool dma_mgr_start_transfers(DmaContext* context, const PagingConfig& config);


} // namespace npu_toolkit