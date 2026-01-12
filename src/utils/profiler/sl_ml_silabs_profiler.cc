/***************************************************************************//**
 * @file sl_ml_silabs_profiler.cc
 * @brief Silabs model profiler class.
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
#if defined(SL_COMPONENT_CATALOG_PRESENT)
    #include "sl_component_catalog.h"
#endif

#include "sl_ml_silabs_profiler.h"
#include <stdlib.h>
#include "em_device.h"
#include <inttypes.h>

namespace sl {
namespace ml {
/***************************************************************************//**
*  @brief Default constructor for SilabsProfiler.
*
*  Initializes internal counters and state used to accumulate profiling
*  statistics (CPU cycles, MVP counts and operation index). Does not
*  allocate or create any interpreter or arena.
******************************************************************************/
SilabsProfiler::SilabsProfiler()
    : operation_index_(0)
    , number_of_inferences_(0)
    , number_operators_model_(0)
    , operator_num_in_model_(0) {}


/***************************************************************************//**
*  @brief Destructor for SilabsProfiler.
*
*  Default destructor; if InitInterpreter() created an owned interpreter
*  it will be destroyed here via the unique_ptr.
******************************************************************************/
SilabsProfiler::~SilabsProfiler() = default;
/***************************************************************************//**
*  @brief Called by the MicroInterpreter at the start of an operation.
*
*  Records a timestamp and prints a simple header for the upcoming
*  operation. Optionally harvests input matrices if enabled.
*
*  @param tag Human-readable name for the profiled operation.
*  @return an event handle (unused, currently always 0).
******************************************************************************/
uint32_t SilabsProfiler::BeginEvent(const char* tag) {

    op_name_ = tag;
    
    reset_profiler_metrics();
    update_start_event_info();
    // Prepare counter track info to debug channel
    emit_counter_packet_info();
    // Prepare start event info to debug channel
    emit_start_event_info();

    event_counter_begin();
    return 0;
}
/***************************************************************************//**
*  @brief Called by the MicroInterpreter at the end of an operation.
*
*  Computes the elapsed CPU cycles since BeginEvent(), accumulates
*  per-run and total statistics, and prints MVP-related counters when
*  available. Optionally harvests output matrices if enabled.
*
*  @param event_handle Handle returned from BeginEvent (ignored).
******************************************************************************/
void SilabsProfiler::EndEvent(uint32_t event_handle) {

    // ******** do not move this line ***************
    event_counter_end();
    // Prepare end event info to debug channel
    // This should be called immediately.
    emit_end_event_info();
    // Prepare counter track info to debug channel
    emit_counter_packet_info();
    update_end_session_info();
    operation_index_++;
}

} // namespace ml
} // namespace sl
