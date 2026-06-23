/***************************************************************************//**
 * @file sl_ml_mvp_counter.cc
 * @brief MVP performance counter implementation for the profiler.
 *******************************************************************************
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
#include "sl_ml_mvp_counter.h"

#include "em_device.h"

namespace {
constexpr int kNumPerfCnt = SL_ML_MVP_NUM_PERF_COUNTERS;
} // namespace

static volatile uint32_t perfcnt[kNumPerfCnt] = { 0 };

extern "C" {

/***************************************************************************//**
 * @brief SI91X MVP performance counter access (RUN and STALL).
 *
 * Configures and reads MVP counters on SI91X:
 * - Counter0: RUN cycles
 * - Counter1: STALL cycles
 ******************************************************************************/
void sli_ml_mvp_perfcnt_reset_all(void)
{
  // Clear and configure counters to measure RUN and STALL
  // Disable counters while configuring
  MVP->CFG_CLR = MVP_CFG_PERFCNTEN;
  // Select PERF0 = RUN, PERF1 = STALL
  MVP->CFG_CLR = (_MVP_CFG_PERF0CNTSEL_MASK | _MVP_CFG_PERF1CNTSEL_MASK);
  MVP->CFG_SET = (uint32_t)MVP_CFG_PERF0CNTSEL_RUN;
  MVP->CFG_SET = (uint32_t)MVP_CFG_PERF1CNTSEL_STALL;
  // Enable performance counters
  MVP->CFG_SET = MVP_CFG_PERFCNTEN;
  perfcnt[0] = 0;
  perfcnt[1] = 0;
}

/***************************************************************************//**
 * @brief Reset MVP cycles and program count.
 ******************************************************************************/
void sli_ml_mvp_progcnt_reset(void)
{
  // Reset sequence: briefly disable and re-enable perf counters
  MVP->CFG_CLR = MVP_CFG_PERFCNTEN;
  MVP->CFG_SET = MVP_CFG_PERFCNTEN;
}

/***************************************************************************//**
 * @brief Provides instruction / stall counts for the MVP accelerator.
 *
 * @return MVP counter value for the requested counter id.
 ******************************************************************************/
uint32_t sli_ml_mvp_perfcnt_get(int counter_id)
{
  if (counter_id == 0) {
    return perfcnt[0];
  }
  if (counter_id == 1) {
    return perfcnt[1];
  }
  return 0u;
}

} // extern "C"


namespace npu_toolkit {
void sli_mvp_perfcnt_increment(unsigned int id, unsigned long amount)
{
  if (id < static_cast<unsigned int>(kNumPerfCnt)) {
    perfcnt[id] += static_cast<uint32_t>(amount);
  }
}
} // namespace npu_toolkit

void sli_mvp_perfcnt_increment(unsigned id, uint32_t amount)
{
  npu_toolkit::sli_mvp_perfcnt_increment(id, static_cast<unsigned long>(amount));
}

