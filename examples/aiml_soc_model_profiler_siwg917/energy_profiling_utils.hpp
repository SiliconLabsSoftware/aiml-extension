/***************************************************************************//**
 * @file
 * @brief energy profiling utils
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

#ifdef SL_ML_ENABLE_ENERGY_PROFILING

    #ifndef __arm__
        #error Energy profiling only supported on embedded devices
    #endif

    #include "sl_sleeptimer.h"

    #ifndef MODEL_PROFILER_ENERGY_PROFILING_HEADER_HOLD_TIME_MS
        #define MODEL_PROFILER_ENERGY_PROFILING_HEADER_HOLD_TIME_MS 15
    #endif
    #ifndef MODEL_PROFILER_ENERGY_PROFILING_HEADER_TOGGLE_COUNT
        #define MODEL_PROFILER_ENERGY_PROFILING_HEADER_TOGGLE_COUNT 50
    #endif
    #ifndef MODEL_PROFILER_ENERGY_PROFILING_DELAY_MS
        #define MODEL_PROFILER_ENERGY_PROFILING_DELAY_MS (MODEL_PROFILER_ENERGY_PROFILING_HEADER_HOLD_TIME_MS + 10)
    #endif

static void sleep_ms(int time_ms) {
    sl_sleeptimer_timer_handle_t timer;
    volatile bool done = false;
    sl_sleeptimer_start_timer_ms(
        &timer,
        time_ms,
        [](sl_sleeptimer_timer_handle_t*, void* arg) {
            auto s = reinterpret_cast<bool*>(arg);
            *s = true;
        },
        (void*)&done,
        0,
        0);
    while (!done) {
        __WFE();
    }
}

static void spin_ms(int time_ms) {
    volatile bool done = false;
    sl_sleeptimer_timer_handle_t delay_timer;

    sl_sleeptimer_start_timer_ms(
        &delay_timer,
        time_ms,
        [](sl_sleeptimer_timer_handle_t*, void* arg) {
            auto s = reinterpret_cast<bool*>(arg);
            *s = true;
        },
        (void*)&done,
        0,
        0);
    while (!done) {
        __NOP();
    }
}

static void energy_profiling_init() {
    for (int i = 0; i < MODEL_PROFILER_ENERGY_PROFILING_HEADER_TOGGLE_COUNT; ++i) {
        sleep_ms(MODEL_PROFILER_ENERGY_PROFILING_HEADER_HOLD_TIME_MS);
        spin_ms(MODEL_PROFILER_ENERGY_PROFILING_HEADER_HOLD_TIME_MS);
    }

    sleep_ms(MODEL_PROFILER_ENERGY_PROFILING_HEADER_HOLD_TIME_MS);
}

    #define ENERGY_PROFILING_INIT() energy_profiling_init()
    #define ENERGY_PROFILING_INFERENCE_DELAY() sleep_ms(MODEL_PROFILER_ENERGY_PROFILING_DELAY_MS);

#else  // SL_ML_ENABLE_ENERGY_PROFILING

    #define ENERGY_PROFILING_INIT()
    #define ENERGY_PROFILING_INFERENCE_DELAY()

#endif  // SL_ML_ENABLE_ENERGY_PROFILING
