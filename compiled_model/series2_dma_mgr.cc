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
#include <cassert>
#include <algorithm>

#include "em_device.h"

#include "sl_core.h"
#include "sl_dma_manager.h"
#include "sl_dma_channel.h"
#include "sl_hal_ldma.h"

#include "compiled_model_dma_mgr.hpp"
#include "compiled_model_paging_interface.hpp"
#include "compiled_model_utils.hpp"


#define COMPILED_MODEL_RAM_ATTR __attribute__((section("text_application_ram")))
#define ENABLE_CPU_CYCLE_COUNTER() DWT->CTRL |= DWT_CTRL_CYCCNTENA_Msk
#define DISABLE_CPU_CYCLE_COUNTER() DWT->CTRL &= ~DWT_CTRL_CYCCNTENA_Msk




struct LDMA_Descriptor_TypeDef
{
  __IOM uint32_t             CTRL;                   /**< Control                                            */
  __IOM uint32_t             SRC;                    /**< Source Address                                     */
  __IOM uint32_t             DST;                    /**< Destination Address                                */
  __IOM uint32_t             LINK;                   /**< Link Address                                       */
};
static constexpr const unsigned LDMA_DESCRIPTOR_SIZE_WORD = (sizeof(LDMA_Descriptor_TypeDef)/sizeof(uint32_t));
static_assert(LDMA_DESCRIPTOR_SIZE_WORD == 4);

namespace npu_toolkit
{


static COMPILED_MODEL_OPTIMIZE_ATTR COMPILED_MODEL_RAM_ATTR void transfer_list_dma_irq_handler();
static COMPILED_MODEL_OPTIMIZE_ATTR COMPILED_MODEL_RAM_ATTR void transfers_dma_irq_handler();

struct _DmaContext : DmaContext
{
  uint32_t transfers_ch_mask;
  uint32_t transfer_list_ch_mask;
  const uint32_t* previous_transfer_list_dst_addr;

  volatile uint32_t last_executed_transfer_id;
  uint32_t next_transfer_id;
  volatile int8_t trigger_next_transfer;
  uint8_t transfers_ch;
  uint8_t transfer_list_ch;
};


uint32_t dma_mgr_context_size(uint8_t flags)
{
  return sizeof(_DmaContext);
}


bool dma_mgr_init(DmaContext* context, uint8_t flags)
{
  auto& ctx = *(_DmaContext*)context;


  if(sl_dma_manager_allocate_channel(nullptr, &ctx.transfers_ch) != SL_STATUS_OK)
  {
    goto exit_error;
  }
  if(sl_dma_manager_allocate_channel(nullptr, &ctx.transfer_list_ch) != SL_STATUS_OK)
  {
    goto exit_error;
  }
  if(sl_dma_manager_register_channel_irq_callback(nullptr, ctx.transfer_list_ch, transfer_list_dma_irq_handler) != SL_STATUS_OK)
  {
    goto exit_error;
  }
  if(sl_dma_manager_register_channel_user_data(nullptr, ctx.transfer_list_ch, context) != SL_STATUS_OK)
  {
    goto exit_error;
  }
  if(sl_dma_manager_register_channel_irq_callback(nullptr, ctx.transfers_ch, transfers_dma_irq_handler) != SL_STATUS_OK)
  {
    goto exit_error;
  }
  if(sl_dma_manager_register_channel_user_data(nullptr, ctx.transfers_ch, context) != SL_STATUS_OK)
  {
    goto exit_error;
  }

  ctx.transfers_ch_mask = (1 << ctx.transfers_ch);
  ctx.transfer_list_ch_mask = (1 << ctx.transfer_list_ch);
  ctx.last_executed_transfer_id = 0;
  ctx.next_transfer_id = 0;

  LDMA0->CHDONE_CLR = ctx.transfers_ch_mask | ctx.transfer_list_ch_mask;
  LDMA0->IF_CLR = ctx.transfers_ch_mask | ctx.transfer_list_ch_mask;
  LDMA0->IEN_SET = ctx.transfers_ch_mask | ctx.transfer_list_ch_mask;

  return true;

  exit_error:
  dma_mgr_deinit(context);
  return false;
}


void dma_mgr_deinit(DmaContext* context)
{
  auto& ctx = *(_DmaContext*)context;

  if(ctx.transfers_ch_mask)
  {
    LDMA0->CHDIS_SET = ctx.transfers_ch_mask;
    sl_dma_manager_free_channel(nullptr, ctx.transfers_ch);
    ctx.transfers_ch_mask = 0;
  }

  if(ctx.transfer_list_ch_mask)
  {
    LDMA0->CHDIS_SET = ctx.transfer_list_ch_mask;
    sl_dma_manager_free_channel(nullptr, ctx.transfer_list_ch);
    ctx.transfer_list_ch_mask = 0;
  }
}


void dma_mgr_set_next_transfer_id(DmaContext* context, uint16_t id)
{
  auto& ctx = *(_DmaContext*)context;
  DEBUG_PRINTF("Release: last_xfr=%d next_xfr=%d", ctx.last_executed_transfer_id, id);
  ctx.next_transfer_id = id;
}


void dma_mgr_set_all_transfers_complete(DmaContext* context)
{
  DEBUG_PRINTF("All transfers complete");
  auto& ctx = *(_DmaContext*)context;
  if(ctx.should_trigger_transfers)
  {
    ctx.last_executed_transfer_id = 0;
    LDMA0->CHDONE_SET = ctx.transfers_ch_mask;
  }
}



void dma_mgr_trigger_next_transfer(DmaContext* context)
{
  CORE_DECLARE_IRQ_STATE;
  auto& ctx = *(_DmaContext*)context;
  const uint32_t transfers_ch_mask = ctx.transfers_ch_mask;

  CORE_ENTER_CRITICAL();

  ++ctx.trigger_next_transfer;

  if(LDMA0->CHDONE & transfers_ch_mask)
  {
    LDMA0->IF_SET = transfers_ch_mask;
  }

  CORE_EXIT_CRITICAL();
}

bool dma_mgr_wait(DmaContext* context)
{
  const auto& ctx = *(const _DmaContext*)context;
  const auto next_transfer_id = ctx.next_transfer_id;

  if(ctx.last_executed_transfer_id < next_transfer_id)
  {
    do
    {
      CORE_DECLARE_IRQ_STATE;
      CORE_ENTER_CRITICAL();
      if(ctx.last_executed_transfer_id < next_transfer_id)
      {
        DISABLE_CPU_CYCLE_COUNTER();
        __WFI();
        ENABLE_CPU_CYCLE_COUNTER();
      }
      CORE_EXIT_CRITICAL();
    } while(ctx.last_executed_transfer_id < next_transfer_id);
  }

  return true;
}


static COMPILED_MODEL_OPTIMIZE_ATTR COMPILED_MODEL_RAM_ATTR void transfer_list_dma_irq_handler()
{
  uint8_t channel_nbr;
  void* user_data;
  sl_dma_manager_retrieve_current_channel_user_data(&channel_nbr, &user_data);
  auto& ctx = *(_DmaContext*)user_data;
  const auto transfer_list_src_base_addr    = ctx.src_buffer_base_addrs[DmaContext::BUFFER_TYPE_TRANSFER_LIST];
  const auto transfer_list_buffer_src_base  = ctx.previous_transfer_list_dst_addr;
  const auto transfer_list_buffer_start     = ctx.current_transfer_list_buffer();
  const auto transfer_list_buffer_end       = transfer_list_buffer_start + ctx.transfer_list_buffer_size();

  const auto next_transfer_list_xfr         = (const PagingTransferConfig*)transfer_list_buffer_src_base;
  const auto next_transfer_list_xfr_flags   = next_transfer_list_xfr->flags();
  const auto next_transfer_list_count       = next_transfer_list_xfr->count();
  DEBUG_PRINTF("Next transfer_list: %s", next_transfer_list_xfr->to_str());


  auto& ch = LDMA0->CH[ctx.transfer_list_ch];
  ch.SRC = (uintptr_t)(transfer_list_src_base_addr + next_transfer_list_xfr->src_offset());
  ch.DST = (uintptr_t)(ctx.paging_buffer + next_transfer_list_xfr->dst_offset());
  ch.CTRL = LDMA_CH_CTRL_STRUCTTYPE_TRANSFER |
    ((next_transfer_list_count-1) << _LDMA_CH_CTRL_XFERCNT_SHIFT) |
    LDMA_CH_CTRL_BLOCKSIZE_ALL |
    LDMA_CH_CTRL_REQMODE_ALL |
    LDMA_CH_CTRL_SRCINC_ONE |
    LDMA_CH_CTRL_DONEIEN |
    LDMA_CH_CTRL_SIZE_WORD |
    LDMA_CH_CTRL_DSTINC_ONE;
  LDMA0->CHDIS_SET = ctx.transfer_list_ch_mask;
  LDMA0->CHEN_SET = ctx.transfer_list_ch_mask;

  ctx.previous_transfer_list_dst_addr = (const uint32_t*)ch.DST;
  ctx.update_current_transfer_list_index();

  const uint32_t* xfr_ptr = transfer_list_buffer_src_base + PagingTransferConfig::LENGTH_WORDS;
  uint32_t* desc_ptr = (uint32_t*)transfer_list_buffer_start;
  while((xfr_ptr+PagingTransferConfig::LENGTH_WORDS) <= transfer_list_buffer_end)
  {
    auto xfr = (const PagingTransferConfig*)xfr_ptr;
    xfr_ptr += PagingTransferConfig::LENGTH_WORDS;

    DEBUG_PRINTF("xfr desc: %s desc=%04X %p", xfr->to_str(), (desc_ptr - ctx.paging_buffer), desc_ptr);
    const auto flags          = xfr->flags();
    const auto transfer_id    = xfr->transfer_id();
    const auto auto_trigger   = (flags & PagingTransferConfig::FLAG_SET_AUTO_TRIGGER) != 0;
    const auto link_to_next_xfr_list = (flags & PagingTransferConfig::FLAG_LINK_TO_NEXT_XFR_LIST) != 0;
    const auto dst_buffer     = ctx.memory_region_addrs[xfr->dst_memory_region_id()];


    auto& data_desc  = *(LDMA_Descriptor_TypeDef*)desc_ptr;
    desc_ptr += LDMA_DESCRIPTOR_SIZE_WORD;

    data_desc.SRC = (uintptr_t)(ctx.src_buffer_base_addrs[DmaContext::BUFFER_TYPE_WEIGHTS] + xfr->src_offset());
    data_desc.DST = (uintptr_t)(dst_buffer + xfr->dst_offset());
    data_desc.CTRL = \
      LDMA_CH_CTRL_STRUCTTYPE_TRANSFER |
      LDMA_CH_CTRL_STRUCTREQ |
      ((xfr->count()-1) << _LDMA_CH_CTRL_XFERCNT_SHIFT) |
      LDMA_CH_CTRL_BLOCKSIZE_UNIT1 |
      LDMA_CH_CTRL_REQMODE_ALL |
      LDMA_CH_CTRL_SRCINC_ONE |
      LDMA_CH_CTRL_SIZE_WORD |
      LDMA_CH_CTRL_DSTINC_ONE;
    data_desc.LINK =  \
      LDMA_CH_LINK_LINKMODE_ABSOLUTE |
      LDMA_CH_LINK_LINK |
      ((uintptr_t)desc_ptr & _LDMA_CH_LINK_LINKADDR_MASK);

    if(transfer_id != 0)
    {
      auto& sync_desc = *(LDMA_Descriptor_TypeDef*)desc_ptr;
      desc_ptr += LDMA_DESCRIPTOR_SIZE_WORD;

      const auto sync_link_addr = link_to_next_xfr_list ?
        (uintptr_t)ctx.current_transfer_list_buffer() :
        (uintptr_t)desc_ptr;

      sync_desc.SRC = transfer_id;
      sync_desc.DST = (uintptr_t)&ctx.last_executed_transfer_id;
      sync_desc.CTRL = \
        LDMA_CH_CTRL_STRUCTTYPE_WRITE |
        LDMA_CH_CTRL_REQMODE_ALL |
        LDMA_CH_CTRL_STRUCTREQ |
        LDMA_CH_CTRL_DONEIEN |
        LDMA_CH_CTRL_SIZE_WORD;
      sync_desc.LINK =  \
        LDMA_CH_LINK_LINKMODE_ABSOLUTE |
        (sync_link_addr & _LDMA_CH_LINK_LINKADDR_MASK);

      if(auto_trigger)
      {
        sync_desc.LINK |= LDMA_CH_LINK_LINK;
      }
    }
  }

  if(!(next_transfer_list_xfr_flags & PagingTransferConfig::FLAG_IS_ONE_TIME_TRANSFER))
  {
    auto& last_sync_desc = *(LDMA_Descriptor_TypeDef*)(desc_ptr-LDMA_DESCRIPTOR_SIZE_WORD);
    auto& wrap_transfer_list_desc = *(LDMA_Descriptor_TypeDef*)desc_ptr;
    wrap_transfer_list_desc.SRC = ctx.transfer_list_ch_mask;
    wrap_transfer_list_desc.DST = (uintptr_t)&LDMA0->SWREQ_SET;
    wrap_transfer_list_desc.CTRL = \
      LDMA_CH_CTRL_STRUCTTYPE_WRITE |
      LDMA_CH_CTRL_STRUCTREQ |
      LDMA_CH_CTRL_REQMODE_ALL |
      LDMA_CH_CTRL_DONEIEN |
      LDMA_CH_CTRL_SIZE_WORD;
    wrap_transfer_list_desc.LINK = last_sync_desc.LINK;
    last_sync_desc.LINK = \
      LDMA_CH_LINK_LINKMODE_ABSOLUTE | LDMA_CH_LINK_LINK |
      ((uintptr_t)&wrap_transfer_list_desc);

    DEBUG_PRINTF("Adding wrap desc=%04X link=%04X addr=%p prev_link_addr=%08X",
      ((uint32_t*)&wrap_transfer_list_desc - ctx.paging_buffer),
      ((uint32_t*)(wrap_transfer_list_desc.LINK & _LDMA_CH_LINK_LINKADDR_MASK) - ctx.paging_buffer),
      &wrap_transfer_list_desc,
      last_sync_desc.LINK
    );
  }
}

static COMPILED_MODEL_OPTIMIZE_ATTR COMPILED_MODEL_RAM_ATTR void transfers_dma_irq_handler()
{
  uint8_t channel_nbr;
  void* user_data;
  sl_dma_manager_retrieve_current_channel_user_data(&channel_nbr, &user_data);
  auto& ctx = *(_DmaContext*)user_data;
  const uint32_t transfers_ch_mask = ctx.transfers_ch_mask;

  if((ctx.trigger_next_transfer > 0) && (LDMA0->CHDONE & transfers_ch_mask))
  {
    --ctx.trigger_next_transfer;
    LDMA0->CHDONE_CLR = transfers_ch_mask;
    LDMA0->CHEN_SET = transfers_ch_mask;
    LDMA0->LINKLOAD_SET = transfers_ch_mask;
  }
}


static void wait_for_dma(const uint32_t ch_mask)
{
  do
  {
    CORE_DECLARE_IRQ_STATE;
    CORE_ENTER_CRITICAL();
    if(!(LDMA0->CHDONE & ch_mask))
    {
      DISABLE_CPU_CYCLE_COUNTER();
      __WFI();
      ENABLE_CPU_CYCLE_COUNTER();
    }
    CORE_EXIT_CRITICAL();
  } while(!(LDMA0->CHDONE & ch_mask));
  LDMA0->CHDONE_CLR = ch_mask;
}


static void copy_data(
  uint8_t ch,
  const uint32_t* src,
  uint32_t* dst,
  uint32_t count
)
{
  auto& ch_reg = LDMA0->CH[ch];

  LDMA0->IF_CLR = (1 << ch);
  LDMA0->IEN_SET = (1 << ch);

  while(count > 0)
  {
    auto xfr_count = std::min(count, (uint32_t)2048);
    DEBUG_PRINTF("copy_data: src=%p dst=%p cnt=%d", src, dst, xfr_count);

    ch_reg.SRC = (uintptr_t)src;
    ch_reg.DST = (uintptr_t)dst;
    ch_reg.LINK = 0;
    ch_reg.CTRL =
        LDMA_CH_CTRL_STRUCTTYPE_TRANSFER |
        LDMA_CH_CTRL_STRUCTREQ |
        ((xfr_count-1) << _LDMA_CH_CTRL_XFERCNT_SHIFT) |
        LDMA_CH_CTRL_BLOCKSIZE_UNIT1 |
        LDMA_CH_CTRL_REQMODE_ALL |
        LDMA_CH_CTRL_DONEIEN |
        LDMA_CH_CTRL_SRCINC_ONE |
        LDMA_CH_CTRL_SIZE_WORD |
        LDMA_CH_CTRL_DSTINC_ONE;

    LDMA0->CHDONE_CLR = (1 << ch);
    LDMA0->CHEN_SET = (1 << ch);
    LDMA0->SWREQ = (1 << ch);
    wait_for_dma((1 << ch));

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

  const auto n_transfers                      = config.n_transfers();
  const auto n_transfer_lists                 = config.n_transfer_lists();
  const auto weights_src_base_addr            = ctx.src_buffer_base_addrs[DmaContext::BUFFER_TYPE_WEIGHTS];
  const auto transfer_list_src_base_addr      = ctx.src_buffer_base_addrs[DmaContext::BUFFER_TYPE_TRANSFER_LIST];
  const auto first_list_xfr                   = (const PagingTransferConfig*)transfer_list_src_base_addr;
  const auto transfer_list_src_addr           = transfer_list_src_base_addr + first_list_xfr->src_offset();
  const auto transfer_list_dst_addr           = ctx.paging_buffer + first_list_xfr->dst_offset();
  const uint32_t transfer_list_count          = first_list_xfr->count();

  ctx.previous_transfer_list_dst_addr = transfer_list_dst_addr;

  DEBUG_PRINTF("n_transfers=%d n_transfer_lists=%d", n_transfers, n_transfer_lists);

  if(n_transfer_lists > 0)
  {
    DEBUG_PRINTF("Manually loading first transfer_list: %s", first_list_xfr->to_str());
    auto& ch = LDMA0->CH[ctx.transfer_list_ch];
    ch.SRC = (uintptr_t)transfer_list_src_addr;
    ch.DST = (uintptr_t)transfer_list_dst_addr;
    ch.CTRL = LDMA_CH_CTRL_STRUCTTYPE_TRANSFER |
      ((transfer_list_count-1) << _LDMA_CH_CTRL_XFERCNT_SHIFT) |
      LDMA_CH_CTRL_BLOCKSIZE_ALL |
      LDMA_CH_CTRL_REQMODE_ALL |
      LDMA_CH_CTRL_SRCINC_ONE |
      LDMA_CH_CTRL_DONEIEN |
      LDMA_CH_CTRL_SIZE_WORD |
      LDMA_CH_CTRL_DSTINC_ONE;
    LDMA0->CHDONE_CLR = ctx.transfer_list_ch_mask;
    LDMA0->CHEN_SET = ctx.transfer_list_ch_mask;
    LDMA0->SWREQ = ctx.transfer_list_ch_mask;

    wait_for_dma(ctx.transfer_list_ch_mask);
  }

  if(n_transfer_lists > 1)
  {
    DEBUG_PRINTF("Manually triggering next transfer_list");
    LDMA0->SWREQ_SET = ctx.transfer_list_ch_mask;
    wait_for_dma(ctx.transfer_list_ch_mask);
  }

  auto xfr_list = (const uint32_t*)ctx.src_buffer_base_addrs[DmaContext::BUFFER_TYPE_TRANSFER_LIST];
  for(int i = n_transfers-1; i >= 0; --i)
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

    ctx.last_executed_transfer_id = xfr.transfer_id();
  }

  if(ctx.should_trigger_transfers)
  {
    DEBUG_PRINTF("Manually triggering first transfer");
    auto first_transfer_desc = ctx.paging_buffer + ctx.transfer_list_buffer_offsets[0];
    ctx.last_executed_transfer_id = 0;
    auto& ch = LDMA0->CH[ctx.transfers_ch];
    ch.LINK = (uintptr_t)first_transfer_desc;
    LDMA0->CHEN_SET = ctx.transfers_ch_mask;
    LDMA0->LINKLOAD_SET = ctx.transfers_ch_mask;
  }

  return true;
}


} // namespace npu_toolkit


extern void sli_mvp_perfcnt_increment(unsigned id, uint32_t amount);

extern "C" COMPILED_MODEL_OPTIMIZE_ATTR COMPILED_MODEL_RAM_ATTR void MVP_IRQHandler()
{
  const uint32_t MVP_IEN = MVP->IEN;
  const uint32_t pending = MVP->IF & MVP_IEN;

  // Clear any interrupts flags
  MVP->IF_CLR = pending;

  #ifdef SL_ML_ENABLE_SILABS_PROFILER
  if(pending & MVP_IF_PROGDONE)
  {
    sli_mvp_perfcnt_increment(0, MVP->PERF[0].CNT);
    sli_mvp_perfcnt_increment(1, MVP->PERF[1].CNT);
  }
  if(pending & MVP_IF_PERFCNT0)
  {
    sli_mvp_perfcnt_increment(0, _MVP_PERFCNT_COUNT_MASK + 1);
  }
  if(pending & MVP_IF_PERFCNT1)
  {
    sli_mvp_perfcnt_increment(1, _MVP_PERFCNT_COUNT_MASK + 1);
  }
  #endif // SL_ML_ENABLE_SILABS_PROFILER
}