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
#include <algorithm>
#include <cstring>
#include <cassert>

#if __has_include("sl_core.h")
#include "sl_core.h"
#else
#include "em_core.h"
#endif

extern "C" {
#include "rsi_rom_udma.h"
#include "rsi_ulpss_clk.h"
#include "rsi_rom_ulpss_clk.h"
}
#include "sl_si91x_dma.h"
#include "si91x_mvp.h"

#include "compiled_model_dma_mgr.hpp"
#include "compiled_model_utils.hpp"
#include "compiled_model_paging_interface.hpp"


#ifndef COMPILED_MODEL_PAGING_MAX_MODEL_COUNT
#define COMPILED_MODEL_PAGING_MAX_MODEL_COUNT 4
#endif


#define DMA_INSTANCE0              0              // DMA0 Instance
#define ULP_DMA_INSTANCE           1              // ULP_DMA Instance
#define ENABLE_CPU_CYCLE_COUNTER() DWT->CTRL |= DWT_CTRL_CYCCNTENA_Msk
#define DISABLE_CPU_CYCLE_COUNTER() DWT->CTRL &= ~DWT_CTRL_CYCCNTENA_Msk

#define LINKED_TRANSFERS_COMPLETE_IF  MVP_IF_PERFCNT0
#define PROCESS_TRANSFER_LIST_IF      MVP_IF_PERFCNT1
#define SINGLE_TRANSFER_COMPLETE_IF   MVP_IF_PROGDONE
#define COMPILED_MODEL_RAM_ATTR __attribute__((section(".ram")))


namespace npu_toolkit
{

static constexpr const unsigned DESCRIPTOR_SIZE_WORDS = (sizeof(RSI_UDMA_DESC_T)/sizeof(uint32_t));
static uint32_t PROCESS_TRANSFER_LIST_IF_BUFFER = PROCESS_TRANSFER_LIST_IF;
static uint32_t SINGLE_TRANSFER_COMPLETE_IF_BUFFER = SINGLE_TRANSFER_COMPLETE_IF;
static uint32_t LINKED_TRANSFERS_COMPLETE_IF_BUFFER = LINKED_TRANSFERS_COMPLETE_IF|SINGLE_TRANSFER_COMPLETE_IF;


static const RSI_UDMA_CHA_CONFIG_DATA_T CH_LINKED_DATA_CFG =
{
  UDMA_MODE_MEM_SCATTER_GATHER|UDMA_MODE_ALT_SELECT,
  NEXT_BURST_DISABLE,
  0,
  ARBSIZE_1,
  SOURCE_PROTECT_CONTROL_DISABLE,
  DESTINATION_PROTECT_CONTROL_DISABLE,
  SRC_SIZE_32,
  SRC_INC_32,
  DST_SIZE_32,
  DST_INC_32
};

static const RSI_UDMA_CHA_CONFIG_DATA_T CH_LAST_SYNC_CFG =
{
  UDMA_MODE_AUTO,
  NEXT_BURST_DISABLE,
  0,
  ARBSIZE_1,
  SOURCE_PROTECT_CONTROL_DISABLE,
  DESTINATION_PROTECT_CONTROL_DISABLE,
  SRC_SIZE_32,
  SRC_INC_NONE,
  DST_SIZE_32,
  DST_INC_NONE
};

static const RSI_UDMA_CHA_CONFIG_DATA_T CH_LINKED_SYNC_CFG =
{
  UDMA_MODE_MEM_SCATTER_GATHER|UDMA_MODE_ALT_SELECT,
  NEXT_BURST_DISABLE,
  0,
  ARBSIZE_1,
  SOURCE_PROTECT_CONTROL_DISABLE,
  DESTINATION_PROTECT_CONTROL_DISABLE,
  SRC_SIZE_32,
  SRC_INC_NONE,
  DST_SIZE_32,
  DST_INC_NONE
};


struct _DmaContext;

static struct
{
  _DmaContext* contexts[COMPILED_MODEL_PAGING_MAX_MODEL_COUNT];
  uint8_t n_contexts;

  bool add(_DmaContext* ctx)
  {
    if(n_contexts == COMPILED_MODEL_PAGING_MAX_MODEL_COUNT)
    {
      assert(!"DmaContext list overflow");
      return false;
    }
    contexts[n_contexts++] = ctx;
    return true;
  }

  void remove(_DmaContext* ctx)
  {
    for(int i = 0; i < n_contexts; ++i)
    {
      if(contexts[i] == ctx)
      {
        for(int j = i; j < n_contexts-1; ++j)
        {
          contexts[j] = contexts[j+1];
        }
        --n_contexts;
        return;
      }
    }
  }

} dma_contexts;


struct ChannelInfo
{
  static constexpr const uint32_t LAST_TRANSFER_ID_SHIFT = 0;
  static constexpr const uint32_t LAST_TRANSFER_ID_MASK = 0x3FFF;
  static constexpr const uint32_t LINK_OFFSET_SHIFT = 14;
  static constexpr const uint32_t LINK_OFFSET_MASK = 0x3FFF;;
  static constexpr const uint32_t FLAG_TRANSFER_LIST_COMPLETE = uint32_t(1 << 29);
  static constexpr const uint32_t FLAG_LINKED_TRANSFER_COMPLETE = uint32_t(1 << 30);
  static constexpr const uint32_t FLAG_PROCESS_TRANSFER_LIST = uint32_t(1 << 31);

  RSI_UDMA_T* DMA;
  uint32_t mask;
  uint8_t ch;

  inline RSI_UDMA_DESC_T* hw_desc() const
  {
    return &((RSI_UDMA_DESC_T*)DMA->CTRL_BASE_PTR)[ch];
  }

  inline RSI_UDMA_DESC_T* hw_alt_desc() const
  {
    const unsigned CH_ALT_SELECT = (DMA == UDMA0) ? UDMA_ALT_SELECT : UDMA_ULPALT_SELECT;
    return &((RSI_UDMA_DESC_T*)DMA->CTRL_BASE_PTR)[ch + CH_ALT_SELECT];
  }

  inline uint32_t spare_reg() const
  {
    return hw_alt_desc()->Spare;
  }

  void init(RSI_UDMA_T* DMA, uint8_t ch)
  {
    this->DMA = DMA;
    this->ch = ch-1;
    this->mask = (1 << this->ch);
    hw_alt_desc()->Spare = 0;
  }


  inline uint16_t last_transfer_executed() const
  {
    return (spare_reg() >> LAST_TRANSFER_ID_SHIFT) & LAST_TRANSFER_ID_MASK;
  }

  inline void clear_last_transfer_executed() const
  {
    hw_alt_desc()->Spare &= ~(LAST_TRANSFER_ID_MASK << LAST_TRANSFER_ID_SHIFT);
  }

  inline uint16_t link_offset() const
  {
    return (spare_reg() >> LINK_OFFSET_SHIFT) & LINK_OFFSET_MASK;
  }

  inline bool transfer_list_complete() const
  {
    return (spare_reg() & FLAG_TRANSFER_LIST_COMPLETE) != 0;
  }

  inline void clear_transfer_list_complete() const
  {
    hw_alt_desc()->Spare &= ~FLAG_TRANSFER_LIST_COMPLETE;
  }

  inline bool linked_transfer_complete() const
  {
    return (spare_reg() & FLAG_LINKED_TRANSFER_COMPLETE) != 0;
  }

  inline void clear_linked_transfer_complete() const
  {
    hw_alt_desc()->Spare &= ~FLAG_LINKED_TRANSFER_COMPLETE;
  }

  inline bool process_transfer_list() const
  {
    return (spare_reg() & FLAG_PROCESS_TRANSFER_LIST) != 0;
  }

  inline void clear_process_transfer_list() const
  {
    hw_alt_desc()->Spare &= ~FLAG_PROCESS_TRANSFER_LIST;
  }

  static inline uint32_t create_spare_reg_value(
    uint32_t id,
    uint32_t link_offset,
    uint32_t flags = 0
  )
  {
    return  (id << LAST_TRANSFER_ID_SHIFT) | (link_offset << LINK_OFFSET_SHIFT) | flags;
  }

  inline void init_scatter_gather_transfer(
    const void* desc_src,
    int8_t n_desc = -1,
    bool start = false
  )
  {
    auto desc = (const RSI_UDMA_DESC_T*)desc_src;

    if(n_desc == -1)
    {
      n_desc = 1;
      for(const RSI_UDMA_DESC_T* desc_ptr = desc;
          desc_ptr->vsUDMAChaConfigData1.transferType != UDMA_MODE_AUTO;
          desc_ptr = &desc_ptr[1])
      {
        ++n_desc;
      }
    }

    const RSI_UDMA_CHA_CONFIG_DATA_T ch_cfg =
    {
      UDMA_MODE_MEM_SCATTER_GATHER,
      NEXT_BURST_DISABLE,
      (uint32_t)((n_desc*4)-1),
      ARBSIZE_4,
      SOURCE_PROTECT_CONTROL_DISABLE,
      DESTINATION_PROTECT_CONTROL_DISABLE,
      SRC_SIZE_32,
      SRC_INC_32,
      DST_SIZE_32,
      DST_INC_32
    };

    auto hw_desc = this->hw_desc();
    hw_desc->pSrcEndAddr = (void*)&desc[n_desc-1].Spare;
    hw_desc->pDstEndAddr = &hw_alt_desc()->Spare;

    auto cfg_ptr = (uint32_t*)&hw_desc->vsUDMAChaConfigData1;
    auto ch_cfg_ptr = (const uint32_t*)&ch_cfg;
    *cfg_ptr = *ch_cfg_ptr;

    DMA->DMA_CFG_b.MASTER_ENABLE = 1;
    DMA->CHNL_PRI_ALT_CLR = mask;
    DMA->CHNL_REQ_MASK_CLR = mask;
    if(start)
    {
      DMA->CHNL_ENABLE_SET = mask;
      DMA->CHNL_SW_REQUEST |= mask;
    }
  }
};

struct _DmaContext : DmaContext
{
  const uint32_t* previous_transfer_list_dst_addr;

  ChannelInfo transfers_ch;
  ChannelInfo transfers2_ch;
  ChannelInfo transfer_list_ch;
  RSI_UDMA_DESC_T transfer_list_desc[2];

  uint32_t next_transfer_id;
  volatile bool trigger_next_transfer1;
  volatile bool trigger_next_transfer2;
  uint8_t context_index;
};

static inline void populate_descriptor(
  RSI_UDMA_DESC_T* desc,
  const void *src,
  const void *dst,
  const uint32_t count,
  const RSI_UDMA_CHA_CONFIG_DATA_T& cfg,
  uint32_t spare = 0
)
{
  auto cfg_ptr = (uint32_t*)&desc->vsUDMAChaConfigData1;
  *cfg_ptr = *(const uint32_t*)&cfg;
  desc->pSrcEndAddr =(void*) (((const uint32_t*)src) + count - 1);
  desc->pDstEndAddr  = (void*) (((const uint32_t*)dst) + count - 1);
  desc->vsUDMAChaConfigData1.totalNumOfDMATrans = count - 1;
  desc->Spare = spare;
}




uint32_t dma_mgr_context_size(uint8_t flags)
{
  return sizeof(_DmaContext);
}


bool dma_mgr_init(DmaContext* context, uint8_t flags)
{
  uint32_t ch;
  auto& ctx = *(_DmaContext*)context;
  sl_dma_init_t dma_init = { DMA_INSTANCE0 };

  if(dma_contexts.n_contexts == 0)
  {
    DEBUG_PRINTF("Enabling MVP");
    NVIC_EnableIRQ(MVP_IRQn);
    MVP->IF_CLR = SINGLE_TRANSFER_COMPLETE_IF|LINKED_TRANSFERS_COMPLETE_IF|PROCESS_TRANSFER_LIST_IF;
    MVP->IEN = LINKED_TRANSFERS_COMPLETE_IF|PROCESS_TRANSFER_LIST_IF;
  }


  if(!dma_contexts.add(&ctx))
  {
    return false;
  }

  if(sl_si91x_dma_init(&dma_init))
  {
    return false;
  }

  ch = 0;
  if(sl_si91x_dma_allocate_channel(DMA_INSTANCE0, &ch, 0) != SL_STATUS_OK)
  {
    return false;
  }
  ctx.transfer_list_ch.init(UDMA0, ch);
  DEBUG_PRINTF("transfer_list_ch: ch=%d (RSI_UDMA_DESC_T*)0x%08X (RSI_UDMA_DESC_T*)0x%08X",
    ctx.transfer_list_ch.ch, ctx.transfer_list_ch.hw_desc(), ctx.transfer_list_ch.hw_alt_desc());


  if(context->dual_weights_transfers_enabled)
  {
    sl_dma_init_t dma_init = { ULP_DMA_INSTANCE };


    if(sl_si91x_dma_init(&dma_init))
    {
      return false;
    }

    RSI_ULPSS_ClockConfig(M4CLK, ENABLE, 0, 0);
    RSI_ULPSS_UlpProcClkConfig(ULPCLK, ULP_PROC_SOC_CLK, 0, 0);

    ch = 0;
    if(sl_si91x_dma_allocate_channel(ULP_DMA_INSTANCE, &ch, 0) != SL_STATUS_OK)
    {
      return false;
    }
    ctx.transfers_ch.init(UDMA1, ch);
    DEBUG_PRINTF("transfers1_ch: ch=%d, (RSI_UDMA_DESC_T*)0x%08X (RSI_UDMA_DESC_T*)0x%08X",
      ctx.transfers_ch.ch, ctx.transfers_ch.hw_desc(), ctx.transfers_ch.hw_alt_desc());

    ch = 0;
    if(sl_si91x_dma_allocate_channel(DMA_INSTANCE0, &ch, 0) != SL_STATUS_OK)
    {
      return false;
    }
    ctx.transfers2_ch.init(UDMA0, ch);
    DEBUG_PRINTF("transfers2_ch:  ch=%d, (RSI_UDMA_DESC_T*)0x%08X (RSI_UDMA_DESC_T*)0x%08X",
      ctx.transfers2_ch.ch, ctx.transfers2_ch.hw_desc(), ctx.transfers2_ch.hw_alt_desc());
  }
  else
  {
    ch = 0;
    if(sl_si91x_dma_allocate_channel(DMA_INSTANCE0, &ch, 0) != SL_STATUS_OK)
    {
      return false;
    }
    ctx.transfers_ch.init(UDMA0, ch);
    DEBUG_PRINTF("transfers_ch: ch=%d (RSI_UDMA_DESC_T*)0x%08X (RSI_UDMA_DESC_T*)0x%08X",
      ctx.transfers_ch.ch, ctx.transfers_ch.hw_desc(), ctx.transfers_ch.hw_alt_desc());

  }

  ctx.next_transfer_id = 0;

  populate_descriptor(
    &ctx.transfer_list_desc[0],
    nullptr,
    nullptr,
    1,
    CH_LINKED_DATA_CFG
  );

  populate_descriptor(
    &ctx.transfer_list_desc[1],
    &PROCESS_TRANSFER_LIST_IF_BUFFER,
    (void*)&MVP->IF_SET,
    1,
    CH_LAST_SYNC_CFG
  );


  return true;
}


void dma_mgr_deinit(DmaContext* context)
{
  auto& ctx = *(_DmaContext*)context;
  dma_contexts.remove(&ctx);
  if(dma_contexts.n_contexts == 0)
  {
    MVP->IEN_CLR = LINKED_TRANSFERS_COMPLETE_IF|PROCESS_TRANSFER_LIST_IF;
  }

  if(ctx.transfers_ch.mask)
  {
    sl_si91x_dma_deallocate_channel(DMA_INSTANCE0, ctx.transfers_ch.ch+1);
    ctx.transfers_ch.mask = 0;
  }

  if(ctx.transfers2_ch.mask)
  {
    sl_si91x_dma_deallocate_channel(ULP_DMA_INSTANCE, ctx.transfers2_ch.ch+1);
    ctx.transfers2_ch.mask = 0;
  }

  if(ctx.transfer_list_ch.mask)
  {
    sl_si91x_dma_deallocate_channel(DMA_INSTANCE0, ctx.transfer_list_ch.ch+1);
    ctx.transfer_list_ch.mask = 0;
  }

}


void dma_mgr_set_next_transfer_id(DmaContext* context, uint16_t id)
{
  auto& ctx = *(_DmaContext*)context;
  DEBUG_PRINTF("Release: last_xfr=%d next_xfr=%d", ctx.transfers_ch.last_transfer_executed(), id);
  ctx.next_transfer_id = id;
}


void dma_mgr_set_all_transfers_complete(DmaContext* context)
{
  DEBUG_PRINTF("All transfers complete");
  auto& ctx = *(_DmaContext*)context;

  if(ctx.should_trigger_transfers)
  {
    ctx.transfers_ch.clear_last_transfer_executed();

    if(ctx.dual_weights_transfers_enabled)
    {
      ctx.transfers2_ch.clear_last_transfer_executed();
    }
  }

}


void dma_mgr_trigger_next_transfer(DmaContext* context)
{
  auto& ctx = *(_DmaContext*)context;
  MVP->IEN_CLR = LINKED_TRANSFERS_COMPLETE_IF;

  ctx.trigger_next_transfer1 = true;

  if(ctx.transfers_ch.linked_transfer_complete())
  {
    MVP->IF_SET = LINKED_TRANSFERS_COMPLETE_IF;
  }

  if(ctx.dual_weights_transfers_enabled)
  {
    ctx.trigger_next_transfer2 = true;
    if(ctx.transfers2_ch.linked_transfer_complete())
    {
      MVP->IF_SET = LINKED_TRANSFERS_COMPLETE_IF;
    }
  }

  MVP->IEN_SET = LINKED_TRANSFERS_COMPLETE_IF;
}


bool dma_mgr_wait(DmaContext* context)
{
  const auto& ctx = *(const _DmaContext*)context;
  const auto next_transfer_id = ctx.next_transfer_id;
  assert((MVP->IEN & SINGLE_TRANSFER_COMPLETE_IF) != 0);

  if(ctx.dual_weights_transfers_enabled)
  {
    if((ctx.transfers_ch.last_transfer_executed() < next_transfer_id) || \
       (ctx.transfers2_ch.last_transfer_executed() < next_transfer_id))
    {
      do
      {
        CORE_DECLARE_IRQ_STATE;
        CORE_ENTER_CRITICAL();
        if((ctx.transfers_ch.last_transfer_executed() < next_transfer_id) || \
           (ctx.transfers2_ch.last_transfer_executed() < next_transfer_id))
        {
          DISABLE_CPU_CYCLE_COUNTER();
          __WFI();
          ENABLE_CPU_CYCLE_COUNTER();
        }
        CORE_EXIT_CRITICAL();
      } while((ctx.transfers_ch.last_transfer_executed() < next_transfer_id) || \
              (ctx.transfers2_ch.last_transfer_executed() < next_transfer_id));
    }
  }
  else
  {
    if(ctx.transfers_ch.last_transfer_executed() < next_transfer_id)
    {
      do
      {
        CORE_DECLARE_IRQ_STATE;
        CORE_ENTER_CRITICAL();
        if(ctx.transfers_ch.last_transfer_executed() < next_transfer_id)
        {
          DISABLE_CPU_CYCLE_COUNTER();
          __WFI();
          ENABLE_CPU_CYCLE_COUNTER();
        }
        CORE_EXIT_CRITICAL();
      } while(ctx.transfers_ch.last_transfer_executed() < next_transfer_id);
    }
  }

  DEBUG_PRINTF("Ready: last_xfr=%d next_xfr=%d",
    ctx.transfers_ch.last_transfer_executed(), next_transfer_id);

  return true;
}



static inline COMPILED_MODEL_OPTIMIZE_ATTR COMPILED_MODEL_RAM_ATTR void process_transfer_list(_DmaContext& ctx)
{
  const auto transfer_list_src_base_addr    = ctx.src_buffer_base_addrs[DmaContext::BUFFER_TYPE_TRANSFER_LIST];
  const auto transfer_list_buffer_src_base  = ctx.previous_transfer_list_dst_addr;
  const auto transfer_list_buffer_start     = ctx.current_transfer_list_buffer();
  const auto transfer_list_buffer_end       = transfer_list_buffer_start + ctx.transfer_list_buffer_size();

  const auto next_transfer_list_xfr         = (const PagingTransferConfig*)transfer_list_buffer_src_base;
  const auto next_transfer_list_xfr_flags   = next_transfer_list_xfr->flags();
  const auto next_transfer_list_count       = next_transfer_list_xfr->count();
  const auto next_transfer_list_src         = transfer_list_src_base_addr + next_transfer_list_xfr->src_offset();
  const auto next_transfer_list_dst         = ctx.paging_buffer + next_transfer_list_xfr->dst_offset();
  DEBUG_PRINTF("Next transfer_list: %s", next_transfer_list_xfr->to_str());

  auto& transfer_list_data_desc             = ctx.transfer_list_desc[0];
  transfer_list_data_desc.pSrcEndAddr       = (void*)(next_transfer_list_src + next_transfer_list_count - 1);
  transfer_list_data_desc.pDstEndAddr       = (void*)(next_transfer_list_dst + next_transfer_list_count - 1);
  transfer_list_data_desc.vsUDMAChaConfigData1.totalNumOfDMATrans = next_transfer_list_count - 1;
  ctx.transfer_list_desc[1].Spare           = next_transfer_list_xfr->transfer_id() | ChannelInfo::FLAG_PROCESS_TRANSFER_LIST;

  ctx.transfer_list_ch.init_scatter_gather_transfer(ctx.transfer_list_desc, 2);

  const uint32_t* xfr_ptr = transfer_list_buffer_src_base + PagingTransferConfig::LENGTH_WORDS;
  const uint32_t* desc_ptr = transfer_list_buffer_start;
  const uint32_t* desc_ptr2 = ctx.dual_weights_transfers_enabled ? ctx.current_transfer_list_buffer2() : nullptr;

  ctx.previous_transfer_list_dst_addr = next_transfer_list_dst;
  ctx.update_current_transfer_list_index();


  while((xfr_ptr+PagingTransferConfig::LENGTH_WORDS) <= transfer_list_buffer_end)
  {
    const auto xfr = (const PagingTransferConfig*)xfr_ptr;
    xfr_ptr += PagingTransferConfig::LENGTH_WORDS;

    const auto flags        = xfr->flags();
    const auto auto_trigger = (flags & PagingTransferConfig::FLAG_SET_AUTO_TRIGGER) != 0;
    const auto link_to_next_xfr_list = (flags & PagingTransferConfig::FLAG_LINK_TO_NEXT_XFR_LIST) != 0;
    const auto dual_xfr     = (flags & PagingTransferConfig::FLAG_IS_DUAL_TRANSFER) != 0;
    const auto dst_buffer   = ctx.memory_region_addrs[xfr->dst_memory_region_id()];
    const auto transfer_id  = xfr->transfer_id();

    auto data_desc          = (RSI_UDMA_DESC_T*)desc_ptr;
    desc_ptr                += DESCRIPTOR_SIZE_WORDS;

    const auto src          = ctx.src_buffer_base_addrs[DmaContext::BUFFER_TYPE_WEIGHTS] + xfr->src_offset();
    const auto dst          = dst_buffer + xfr->dst_offset();
    const auto count        = xfr->count();

    DEBUG_PRINTF("xfr  desc: %s desc=%04X src=%p dst=%p", xfr->to_str(), ((uint32_t*)data_desc - ctx.paging_buffer), src, dst);

    populate_descriptor(
      data_desc,
      src,
      dst,
      count,
      CH_LINKED_DATA_CFG
    );

    if(dual_xfr)
    {
      const auto xfr2 = (const PagingTransferConfig*)xfr_ptr;
      xfr_ptr += PagingTransferConfig::LENGTH_WORDS;

      const auto src2     = ctx.src_buffer_base_addrs[DmaContext::BUFFER_TYPE_WEIGHTS2] + xfr2->src_offset();
      const auto dst2     = dst_buffer + xfr2->dst_offset();
      const auto count2   = xfr2->count();
      auto data_desc2     = (RSI_UDMA_DESC_T*)desc_ptr2;
      desc_ptr2 += DESCRIPTOR_SIZE_WORDS;

      DEBUG_PRINTF("xfr2 desc: %s desc=%04X src=%p dst=%p", xfr2->to_str(), ((uint32_t*)data_desc2 - ctx.paging_buffer), src2, dst2);

      populate_descriptor(
        data_desc2,
        src2,
        dst2,
        count2,
        CH_LINKED_DATA_CFG
      );
    }

    if(transfer_id != 0)
    {
      auto sync_desc = (RSI_UDMA_DESC_T*)desc_ptr;
      desc_ptr += DESCRIPTOR_SIZE_WORDS;

      const auto sync_link_offset = link_to_next_xfr_list ?
        (uint32_t)ctx.current_transfer_list_buffer_offset() :
        (uint32_t)(desc_ptr - ctx.paging_buffer);

      populate_descriptor(
        sync_desc,
        auto_trigger ? &SINGLE_TRANSFER_COMPLETE_IF_BUFFER : &LINKED_TRANSFERS_COMPLETE_IF_BUFFER,
        (void*)&MVP->IF_SET,
        1,
        auto_trigger ? CH_LINKED_SYNC_CFG : CH_LAST_SYNC_CFG,
        ChannelInfo::create_spare_reg_value(
          transfer_id,
          sync_link_offset,
          auto_trigger ? 0 : ChannelInfo::FLAG_LINKED_TRANSFER_COMPLETE
        )
      );

      if(desc_ptr2 != nullptr)
      {
        auto sync_desc2 = (RSI_UDMA_DESC_T*)desc_ptr2;
        desc_ptr2 += DESCRIPTOR_SIZE_WORDS;

        const auto sync_link_offset = link_to_next_xfr_list ?
          (uint32_t)ctx.current_transfer_list_buffer2_offset() :
          (uint32_t)(desc_ptr2 - ctx.paging_buffer);

        populate_descriptor(
          sync_desc2,
          auto_trigger ? &SINGLE_TRANSFER_COMPLETE_IF_BUFFER : &LINKED_TRANSFERS_COMPLETE_IF_BUFFER,
          (void*)&MVP->IF_SET,
          1,
          auto_trigger ? CH_LINKED_SYNC_CFG : CH_LAST_SYNC_CFG,
          ChannelInfo::create_spare_reg_value(
            transfer_id,
            sync_link_offset,
            auto_trigger ? 0 : ChannelInfo::FLAG_LINKED_TRANSFER_COMPLETE
          )
        );
      }
    }
  }

  if(!(next_transfer_list_xfr_flags & PagingTransferConfig::FLAG_IS_ONE_TIME_TRANSFER))
  {
    auto last_sync_desc = (RSI_UDMA_DESC_T*)(desc_ptr-DESCRIPTOR_SIZE_WORDS);
    last_sync_desc->Spare |= ChannelInfo::FLAG_TRANSFER_LIST_COMPLETE;
    DEBUG_PRINTF("Setting xfr_list complete flag desc=%04X", ((uint32_t*)last_sync_desc - ctx.paging_buffer));
    if(desc_ptr2 != nullptr)
    {
      auto last_sync_desc2 = (RSI_UDMA_DESC_T*)(desc_ptr2-DESCRIPTOR_SIZE_WORDS);
      last_sync_desc2->Spare |= ChannelInfo::FLAG_TRANSFER_LIST_COMPLETE;
      DEBUG_PRINTF("Setting xfr_list complete flag desc=%04X", ((uint32_t*)last_sync_desc2 - ctx.paging_buffer));
    }
  }
}




static void wait_for_dma(const ChannelInfo& ch)
{
  MVP->IEN_SET = SINGLE_TRANSFER_COMPLETE_IF;
  do
  {
    CORE_DECLARE_IRQ_STATE;
    CORE_ENTER_CRITICAL();
    if((ch.DMA->CHANNEL_STATUS_REG & ch.mask))
    {
      DISABLE_CPU_CYCLE_COUNTER();
      __WFI();
      ENABLE_CPU_CYCLE_COUNTER();
    }
    CORE_EXIT_CRITICAL();
  } while((ch.DMA->CHANNEL_STATUS_REG & ch.mask));
  MVP->IEN_CLR = SINGLE_TRANSFER_COMPLETE_IF;
}

static void copy_data(
  ChannelInfo& ch,
  const uint32_t* src,
  uint32_t* dst,
  uint32_t count
)
{
    while(count > 0)
    {
      auto xfr_count = std::min(count, (uint32_t)1024);
      DEBUG_PRINTF("copy_data: src=%p dst=%p cnt=%d", src, dst, xfr_count);

      RSI_UDMA_DESC_T desc[2];
      populate_descriptor(
        &desc[0],
        src,
        dst,
        xfr_count,
        CH_LINKED_DATA_CFG
      );
      populate_descriptor(
        &desc[1],
        &SINGLE_TRANSFER_COMPLETE_IF_BUFFER,
        (void*)&MVP->IF_SET,
        1,
        CH_LAST_SYNC_CFG,
        0
      );
      ch.init_scatter_gather_transfer(&desc, 2, true);
      wait_for_dma(ch);

      count -= xfr_count;
      src += xfr_count;
      dst += xfr_count;
    }

}


bool dma_mgr_start_transfers(
  DmaContext* context,
  const PagingConfig& config
)
{
  auto& ctx = *(_DmaContext*)context;

  const auto n_transfers                  = config.n_transfers();
  const auto n_transfer_lists             = config.n_transfer_lists();
  const auto weights_src_base_addr        = ctx.src_buffer_base_addrs[DmaContext::BUFFER_TYPE_WEIGHTS];
  const auto transfer_list_src_base_addr  = ctx.src_buffer_base_addrs[DmaContext::BUFFER_TYPE_TRANSFER_LIST];
  const auto first_list_xfr               = (const PagingTransferConfig*)transfer_list_src_base_addr;
  const auto transfer_list_src_addr       = transfer_list_src_base_addr + first_list_xfr->src_offset();
  const auto transfer_list_dst_addr       = ctx.paging_buffer + first_list_xfr->dst_offset();
  const uint32_t transfer_list_count      = first_list_xfr->count();

  ctx.previous_transfer_list_dst_addr     = transfer_list_dst_addr;

  auto& transfer_list_data_desc           = ctx.transfer_list_desc[0];
  transfer_list_data_desc.pSrcEndAddr     = (void*)(transfer_list_src_addr + transfer_list_count - 1);
  transfer_list_data_desc.pDstEndAddr     = (void*)(transfer_list_dst_addr + transfer_list_count - 1);
  transfer_list_data_desc.vsUDMAChaConfigData1.totalNumOfDMATrans = transfer_list_count - 1;
  ctx.transfer_list_desc[1].Spare         = first_list_xfr->transfer_id() | ChannelInfo::FLAG_PROCESS_TRANSFER_LIST;

  DEBUG_PRINTF("n_transfers=%d n_transfer_lists=%d", n_transfers, n_transfer_lists);

  if(n_transfer_lists > 0)
  {
    DEBUG_PRINTF("Manually loading first transfer_list: %s", first_list_xfr->to_str());
    ctx.transfer_list_ch.init_scatter_gather_transfer(ctx.transfer_list_desc, 2, true);
    wait_for_dma(ctx.transfer_list_ch);
  }

  if(n_transfer_lists > 1)
  {
    DEBUG_PRINTF("Manually triggering next transfer_list");
    ctx.transfer_list_ch.DMA->CHNL_ENABLE_SET = ctx.transfer_list_ch.mask;
    wait_for_dma(ctx.transfer_list_ch);
  }


  auto xfr_list = (const uint32_t*)ctx.src_buffer_base_addrs[DmaContext::BUFFER_TYPE_TRANSFER_LIST];
  // Copy the one-time transfers (which are at the end of the list)
  for(int i = n_transfers-1; i >=0 ; --i)
  {
    auto& xfr = *(const PagingTransferConfig*)&xfr_list[i*PagingTransferConfig::LENGTH_WORDS];
    const auto flags = xfr.flags();

    if(!(flags & PagingTransferConfig::FLAG_IS_ONE_TIME_TRANSFER))
    {
      break;
    }

    DEBUG_PRINTF("Transferring one-time: %s", xfr.to_str());

    copy_data(
      ctx.transfers_ch,
      weights_src_base_addr + xfr.src_offset(),
      ctx.paging_buffer + xfr.dst_offset(),
      xfr.count()
    );
  }

  if(ctx.dual_weights_transfers_enabled)
  {
    copy_data(
      ctx.transfers_ch,
      weights_src_base_addr + config.weights2_src_offset(),
      (uint32_t*)ctx.src_buffer_base_addrs[DmaContext::BUFFER_TYPE_WEIGHTS2],
      config.weights2_count()
    );
  }

  if(ctx.should_trigger_transfers)
  {
    DEBUG_PRINTF("Manually triggering first transfer");

    auto first_transfer_desc = (RSI_UDMA_DESC_T*)(ctx.paging_buffer + ctx.transfer_list_buffer_offsets[0]);
    ctx.transfers_ch.init_scatter_gather_transfer(first_transfer_desc, -1, true);

    if(ctx.dual_weights_transfers_enabled)
    {
      auto first_transfer_desc = (RSI_UDMA_DESC_T*)(ctx.paging_buffer + ctx.transfer_list_buffer_offsets[0] + ctx.transfer_list_buffer_size());
      ctx.transfers2_ch.init_scatter_gather_transfer(
        first_transfer_desc,
        -1,
        true
      );
    }
  }

  return true;
}


} // namespace npu_toolkit


extern void sli_mvp_perfcnt_increment(unsigned id, uint32_t amount);

extern "C" void COMPILED_MODEL_OPTIMIZE_ATTR COMPILED_MODEL_RAM_ATTR IRQ062_Handler() // MVP IRQ
{
  using namespace npu_toolkit;
  const uint32_t MVP_IEN = MVP->IEN;
  const uint32_t pending_if = MVP->IF & MVP_IEN;
  MVP->IF_CLR = pending_if;

  #ifdef SL_ML_ENABLE_SILABS_PROFILER
  if(pending_if & MVP_IF_PROGDONE)
  {
    sli_mvp_perfcnt_increment(0, MVP->PERF[0].CNT);
    sli_mvp_perfcnt_increment(1, MVP->PERF[1].CNT);

    // NOTE: The MVP_IF_PERFCNT0 and MVP_IF_PERFCNT1 flags
    //       are used by PROCESS_TRANSFER_LIST_IF and LINKED_TRANSFERS_COMPLETE_IF
    //       The linked DMA descriptor manually sets these flags.
    //       This is done for efficience purposes.
    //       The MVP perfcnts go up to 16M, so it's highly
    //       unlikely a real-world model will require an MVP
    //       program that exceeds this count and trigger this interrupt from an actual overflow.
  }
  #endif // SL_ML_ENABLE_SILABS_PROFILER

  if(!(pending_if & (PROCESS_TRANSFER_LIST_IF|LINKED_TRANSFERS_COMPLETE_IF)))
  {
    return;
  }

  const auto n_contexts = dma_contexts.n_contexts;
  for(int i = 0; i < n_contexts; ++i)
  {
     auto& ctx = *dma_contexts.contexts[i];

    if(pending_if & PROCESS_TRANSFER_LIST_IF)
    {
      if(ctx.transfer_list_ch.process_transfer_list())
      {
        ctx.transfer_list_ch.clear_process_transfer_list();
        process_transfer_list(ctx);
      }
    }

    if(pending_if & LINKED_TRANSFERS_COMPLETE_IF)
    {
      if(ctx.dual_weights_transfers_enabled)
      {
        if(ctx.transfers_ch.transfer_list_complete() &&
          ctx.transfers2_ch.transfer_list_complete())
        {
          ctx.transfers_ch.clear_transfer_list_complete();
          ctx.transfers2_ch.clear_transfer_list_complete();
          ctx.transfer_list_ch.DMA->CHNL_ENABLE_SET = ctx.transfer_list_ch.mask;
          DEBUG_PRINTF("Trigger transfer_list");
        }
      }
      else
      {
        if(ctx.transfers_ch.transfer_list_complete())
        {
          ctx.transfers_ch.clear_transfer_list_complete();
          ctx.transfer_list_ch.DMA->CHNL_ENABLE_SET = ctx.transfer_list_ch.mask;
          DEBUG_PRINTF("Trigger transfer_list");
        }
      }

      if(ctx.trigger_next_transfer1 && \
         ctx.transfers_ch.linked_transfer_complete() && \
        !ctx.transfers_ch.transfer_list_complete()
      )
      {
        assert((ctx.transfers_ch.DMA->CHANNEL_STATUS_REG & ctx.transfers_ch.mask) == 0);
        ctx.trigger_next_transfer1 = false;
        ctx.transfers_ch.clear_linked_transfer_complete();
        const auto link_offset = ctx.transfers_ch.link_offset();
        auto base_desc = (RSI_UDMA_DESC_T*)(ctx.paging_buffer + link_offset);
        DEBUG_PRINTF("Start transfer1 desc=%04X addr=%p", link_offset, base_desc);

        ctx.transfers_ch.init_scatter_gather_transfer(
          base_desc,
          -1,
          true
        );
      }

      if(ctx.trigger_next_transfer2 && \
         ctx.transfers2_ch.linked_transfer_complete() && \
        !ctx.transfers2_ch.transfer_list_complete()
      )
      {
        assert((ctx.transfers2_ch.DMA->CHANNEL_STATUS_REG & ctx.transfers2_ch.mask) == 0);
        ctx.trigger_next_transfer2 = false;
        ctx.transfers2_ch.clear_linked_transfer_complete();

        const auto link_offset = ctx.transfers2_ch.link_offset();
        auto base_desc = (const RSI_UDMA_DESC_T*)(ctx.paging_buffer + link_offset);
        DEBUG_PRINTF("Start transfer2 desc=%04X addr=%p", link_offset, base_desc);

        ctx.transfers2_ch.init_scatter_gather_transfer(
          base_desc,
          -1,
          true
        );
      }
    }
  }
}