/***************************************************************************//**
 * @file sl_ml_mvp_counter.h
 * @brief MVP performance counter helpers for the profiler (RUN / STALL).
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
#ifndef SL_ML_MVP_COUNTER_H
#define SL_ML_MVP_COUNTER_H

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/** Number of MVP performance counters tracked by the profiler (RUN, STALL). */
#define SL_ML_MVP_NUM_PERF_COUNTERS 2

void sli_ml_mvp_perfcnt_reset_all(void);
void sli_ml_mvp_progcnt_reset(void);
uint32_t sli_ml_mvp_perfcnt_get(int counter_id);

#ifdef __cplusplus
}
#endif

namespace npu_toolkit {
void sli_mvp_perfcnt_increment(unsigned int id, unsigned long amount);
} // namespace npu_toolkit

/** C++ linkage; matches generated `extern void` forward decl in C++ TUs. */
#ifdef __cplusplus
void sli_mvp_perfcnt_increment(unsigned id, uint32_t amount);
#endif

#endif // SL_ML_MVP_COUNTER_H
