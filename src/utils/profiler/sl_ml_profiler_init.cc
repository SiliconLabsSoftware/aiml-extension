/***************************************************************************//**
 * @file
 *   sl_ml_profiler_init.cc
 * @brief
 *   Model profiler initialization and session bootstrap helpers.
 *******************************************************************************
 * # License
 * <b>Copyright 2025 Silicon Laboratories Inc. www.silabs.com</b>
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
#include <cstdint>
#include <cstdio>
#include "sl_sleeptimer.h"
#include "sl_ml_profiler_debug_channel.h"
#include "sl_status.h"

#include "sl_ml_profiler_init.h"
#include "sl_ml_profiler_config.h"
#if (SL_ML_ENABLE_PROFILER_DEBUG_MSG != 1)
#if defined(SL_BOARD_EFX)
#include "sl_iostream.h"
#include "sl_iostream_handles.h"
extern sl_iostream_t *sl_iostream_vcom_handle;
#endif // SL_BOARD_EFX
#endif //SL_ML_ENABLE_PROFILER_DEBUG_MSG

/***************************************************************************//**
 * @brief
 *   Initializes variables for the model profiler and emits initial debug stream
 *   packets (when enabled).
 ******************************************************************************/
static void init(void)
{
  sl_status_t st = sl_sleeptimer_init();
  assert(st == SL_STATUS_OK);

#if (SL_ML_ENABLE_PROFILER_DEBUG_MSG == 1)
  sl_ml_profiler_debug_init();
#else // SL_ML_ENABLE_PROFILER_DEBUG_MSG
#if defined(SL_BOARD_EFX)
  //extern sl_iostream_t *sl_iostream_uart_handle;
  sl_iostream_set_default(sl_iostream_vcom_handle);
#endif //SL_BOARD_EFX
#endif // SL_ML_ENABLE_PROFILER_DEBUG_MSG
}

/***************************************************************************//**
 * @brief
 *   C linkage entry point for the model profiler initialization.
 ******************************************************************************/
extern "C" void sl_ml_profiler_init(void)
{
  init();
}
