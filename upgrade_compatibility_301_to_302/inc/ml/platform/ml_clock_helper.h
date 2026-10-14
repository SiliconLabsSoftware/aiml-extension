/*******************************************************************************
 * @file ml_clock_helper.h
 * @brief Deprecated compatibility stub for aiml <=3.0.1 `ml/platform/ml_clock_helper.h`.
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
#pragma once
#include <stdbool.h>
#include <stdint.h>
#if defined(ML_UPGRADE_COMPATIBILITY_EFR32)
#include "em_cmu.h"
#elif defined(ML_UPGRADE_COMPATIBILITY_SIWG917)
#include "system_si91x.h"
#endif
#pragma message("ml/platform/ml_clock_helper.h is deprecated; remove it from your project. \
ml_configure_clocks_to_max_rate() and ml_get_cpu_clock_frequency() are deprecated; remove calls. \
This header will be removed in a future release.")
#ifdef __cplusplus
extern "C" {
#endif
/**
 * @deprecated No-op stub for upgraded <=3.0.1 apps; remove calls to this API.
 */
static inline bool ml_configure_clocks_to_max_rate(void)
{
  return true;
}
/**
 * @deprecated Compatibility stub for upgraded <=3.0.1 apps; remove calls to this API.
 */
static inline uint32_t ml_get_cpu_clock_frequency(void)
{
#if defined(ML_UPGRADE_COMPATIBILITY_EFR32)
  return CMU_ClockFreqGet(cmuClock_CORE);
#elif defined(ML_UPGRADE_COMPATIBILITY_SIWG917)
  return system_clocks.soc_clock;
#else
  return 0;
#endif
}
#ifdef __cplusplus
}
#endif
