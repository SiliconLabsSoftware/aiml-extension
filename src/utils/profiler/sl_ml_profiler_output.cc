#include "sl_ml_silabs_profiler.h"
#include "sl_ml_profiler_debug_channel.h"

namespace sl {
namespace ml {
#if (SL_ML_ENABLE_PROFILER_DEBUG_MSG)
// =============================== INIT INFO ==========================================
/***************************************************************************//**
 * @brief
 *   Send unnamed top-level tracks (one core, one counter) to establish the
 *   track hierarchy roots.
 ******************************************************************************/
void SilabsProfiler::emit_parent_tracks(void)
{
  // Start Processor core track without name (root)
  sl_ml_profiler_start_track_info_t core = {};
  core.has_parent = false;
  core.is_counter = false;
  core.processor_id = PARENT_TRACK_PROCESSOR_CORES; // Parent track for processor cores
  sl_ml_profiler_core_start_track_write(&core);

  // Start Counter track without name (root)
  sl_ml_profiler_start_track_info_t counter = {};
  counter.has_parent = false;
  counter.is_counter = true;
  counter.processor_id = PARENT_TRACK_PERFORMANCE_COUNTER; // Parent track for performance counters
  sl_ml_profiler_counter_start_track_write(&counter);
}

/***************************************************************************//**
 * @brief
 *   Start processor-specific core track(s) with names.
 * @note
 *   Extend this function if additional processor cores are supported.
 ******************************************************************************/
void SilabsProfiler::emit_processor_core_start_track_packets()
{
  // Start Processor core track with name (ARM Cortex-M33 example)
  sl_ml_profiler_start_track_info_t mcu_info = {};
  mcu_info.processor_id = map_mcu_to_processor_id(start_session_info.mcu_id);
  mcu_info.has_parent   = true;
  mcu_info.is_counter   = false;
  sl_ml_profiler_core_start_track_write(&mcu_info);
  // Start Accelerator core track with name (MVP example)
  sl_ml_profiler_start_track_info_t acc_info = {};
  acc_info.processor_id = map_accelerator_to_processor_id(start_session_info.accel_id);
  acc_info.has_parent   = true;
  acc_info.is_counter   = false;
  sl_ml_profiler_core_start_track_write(&acc_info);
}
/***************************************************************************//**
*  @brief Send processor core end track info to debug channel.
*  Fills and sends the end track info structure to the debug channel.
*******************************************************************************/
void SilabsProfiler::emit_processor_core_end_track_packets()
{
    // End Processor core track with name (ARM Cortex-M33 example)
    sl_ml_profiler_end_track_info_t mcu_info = {};
    mcu_info.processor_id = map_mcu_to_processor_id(start_session_info.mcu_id);
    mcu_info.has_parent   = true;
    mcu_info.is_counter   = false;
    sl_ml_profiler_core_end_track_write(&mcu_info);
    // End Accelerator core track with name (MVP example)
    sl_ml_profiler_end_track_info_t acc_info = {};
    acc_info.processor_id = map_accelerator_to_processor_id(start_session_info.accel_id);
    acc_info.has_parent   = true;
    acc_info.is_counter   = false;
    sl_ml_profiler_core_end_track_write(&acc_info);
    // End of parent core track
    sl_ml_profiler_end_track_info_t parent = {};
    parent.has_parent = false;
    parent.is_counter = false;
    parent.processor_id = PARENT_TRACK_PROCESSOR_CORES; // Parent track for processor cores
    sl_ml_profiler_core_end_track_write(&parent);
    // End of top level core track
}
/***************************************************************************//**
 * @brief
 *   Start named counter tracks (CPU frequency, energy consumption, memory).
 ******************************************************************************/
void SilabsProfiler::emit_counter_start_track_packets()
{
  // CPU frequency (MHz)
  sl_ml_profiler_start_track_info_t freq = {};
  freq.has_parent   = true;
  freq.is_counter   = true;
  freq.processor_id = COUNTER_TRACK_CPU_FREQ; // CPU frequency counter track start
  freq.counter_unit = SL_ML_COUNTER_UNIT_MHZ;
  sl_ml_profiler_counter_start_track_write(&freq);

#ifdef SL_ML_ENABLE_ENERGY_PROFILING
  // Energy consumption (Joules)
  sl_ml_profiler_start_track_info_t energy = {};
  energy.has_parent   = true;
  energy.is_counter   = true;
  energy.processor_id = COUNTER_TRACK_ENERGY; // Energy counter track start
  energy.counter_unit = SL_ML_COUNTER_UNIT_JOULES;
  sl_ml_profiler_counter_start_track_write(&energy);
#endif // SL_ML_ENABLE_ENERGY_PROFILING

  // Memory usage (KB)
  sl_ml_profiler_start_track_info_t memory = {};
  memory.has_parent   = true;
  memory.is_counter   = true;
  memory.processor_id = COUNTER_TRACK_RAM_USAGE; // RAM usage counter track start
  memory.counter_unit = SL_ML_COUNTER_UNIT_KB;
  sl_ml_profiler_counter_start_track_write(&memory);
}
/***************************************************************************//**
*  @brief Send counter end track info to debug channel.
*  Fills and sends the end track info structure to the debug channel.
*******************************************************************************/
void SilabsProfiler::emit_counter_end_track_packets()
{
    // CPU frequency (MHz)
    sl_ml_profiler_end_track_info_t freq = {};
    freq.has_parent   = true;
    freq.is_counter   = true;
    freq.processor_id = COUNTER_TRACK_CPU_FREQ; // CPU frequency counter track start
    sl_ml_profiler_counter_end_track_write(&freq);

#ifdef SL_ML_ENABLE_ENERGY_PROFILING
    // Energy consumption (Joules)
    sl_ml_profiler_end_track_info_t energy = {};
    energy.has_parent   = true;
    energy.is_counter   = true;
    energy.processor_id = COUNTER_TRACK_ENERGY; // Energy counter track start
    sl_ml_profiler_counter_end_track_write(&energy);
#endif // SL_ML_ENABLE_ENERGY_PROFILING

    // Memory usage (KB)
    sl_ml_profiler_end_track_info_t memory = {};
    memory.has_parent   = true;
    memory.is_counter   = true;
    memory.processor_id = COUNTER_TRACK_RAM_USAGE; // RAM usage counter track start
    sl_ml_profiler_counter_end_track_write(&memory);

    // End of parent counter track
    sl_ml_profiler_end_track_info_t parent = {};
    parent.has_parent = false;
    parent.is_counter = true;
    parent.processor_id = PARENT_TRACK_PERFORMANCE_COUNTER; // Parent track for performance counters
    sl_ml_profiler_counter_end_track_write(&parent);
}
#endif //SL_ML_ENABLE_PROFILER_DEBUG_MSG
// ============================ SESSION INFO ==========================================
/***************************************************************************//**
 * @brief
 *   Send start session info to debug channel.
 *   Fills and sends the start session info structure to the debug channel.
 ******************************************************************************/
void SilabsProfiler::emit_start_session_info() {
#if (SL_ML_ENABLE_PROFILER_DEBUG_MSG)
    sl_ml_profiler_start_session_write(&start_session_info);
    emit_parent_tracks();
    emit_processor_core_start_track_packets();
    emit_counter_start_track_packets();
#else
    // Console-only fallback
    std::printf("\n=== SESSION START ===\n");
    std::printf("  Board Name   : %s\n", start_session_info.board_name);
    std::printf("  OPN          : %s\n", start_session_info.opn);
    std::printf("  Part Family  : %s\n", start_session_info.part_family);
    std::printf("  Model Name   : %s\n", start_session_info.model_name);
    std::printf("  Flash (KB)   : %u\n", static_cast<unsigned>(start_session_info.flash_kb));
    std::printf("  RAM (KB)     : %u\n", static_cast<unsigned>(start_session_info.ram_kb));
    std::printf("  Arena (KB)   : %u\n", static_cast<unsigned>(start_session_info.arena_kb));
    std::printf("  MCU          : %s\n", map_mcu_id(start_session_info.mcu_id));
    std::printf("  Accelerator  : %s\n\n", map_accelerator_id(start_session_info.accel_id));
#endif
}
/***************************************************************************//**
*  @brief Send end session info to debug channel.
*  Fills and sends the end session info structure to the debug channel.
*******************************************************************************/
void SilabsProfiler::emit_end_session_info()
{
#if (SL_ML_ENABLE_PROFILER_DEBUG_MSG)
    sl_ml_profiler_end_session_write(&end_session_info);
#else // SL_ML_ENABLE_PROFILER_DEBUG_MSG
    printf("\n=== SESSION END ===\n");
    this->sli_print_ui32("  Total CPU Cycles    : ", end_session_info.total_num_cycles, "\n");
    this->sli_print_ui32("  Total CPU Stalls    : ", end_session_info.total_num_stalls, "\n");
    this->sli_print_ui32("  Total Acc Cycles    : ", end_session_info.total_acc_cycles, "\n");
    this->sli_print_ui32("  Total Acc Stalls    : ", end_session_info.total_acc_stalls, "\n");
    this->sli_print_ui32("  Total Operations    : ", end_session_info.total_num_ops, "\n");
    this->sli_print_ui32("  Total MACs          : ", end_session_info.total_num_macs, "\n");
    printf("  Flash Used (KB)    : %lu\n", end_session_info.total_flash_used_kb);
    printf("  RAM Used (KB)      : %lu\n", end_session_info.total_ram_used_kb);
    printf("  Mean MAC/Cycle     : %.4f\n", end_session_info.mean_mac_per_cycle);
    printf("  Mean Inference (ms): %.4f\n", end_session_info.mean_inference_time_ms);
#ifdef SL_ML_ENABLE_ENERGY_PROFILING
    printf("  Energy (Joules)    : %.6f\n\n", end_session_info.energy_joules);
#endif // SL_ML_ENABLE_ENERGY_PROFILING

#endif // SL_ML_ENABLE_PROFILER_DEBUG_MSG
}
// ============================= COUNTER PACKET INFO ==========================================
/* ***************************************************************************//**
*  @brief Send counter packet info to debug channel.
*
*  Populates and sends the counter packet info structure to the debug channel:
*  - CPU frequency (MHz)
*  - Energy (J) – placeholder value pending measurement integration
*  - Memory usage (KiB)
*
*  @see get_memory_utilization_kb()
*  @see get_cpu_freq()
*  @see estimate_active_current_A()
*  @see energyJ_from_cycles()
*******************************************************************************/
void SilabsProfiler::emit_counter_packet_info()
{
  // Update max RAM usage in KB for the session info
  float current_ram_kb = get_memory_utilization_kb();
  if (end_session_info.total_ram_used_kb < current_ram_kb) {
      end_session_info.total_ram_used_kb = static_cast<uint32_t>(current_ram_kb);
  }
#if (SL_ML_ENABLE_PROFILER_DEBUG_MSG)
  sl_ml_profiler_counter_pkt_info_t counter_pkt_info;

  // CPU frequency (MHz)
  counter_pkt_info.counter_value = get_cpu_freq() / 1'000'000.0f;
  counter_pkt_info.counter_unit  = SL_ML_COUNTER_UNIT_MHZ;
  sl_ml_profiler_counter_pkt_write(&counter_pkt_info);

#ifdef SL_ML_ENABLE_ENERGY_PROFILING
  // Energy (J) – placeholder until a real measurement is wired in
  counter_pkt_info.counter_unit  = SL_ML_COUNTER_UNIT_JOULES;
  counter_pkt_info.counter_value = 0.0f; // energyJ_from_cycles(cycles); // TODO
  sl_ml_profiler_counter_pkt_write(&counter_pkt_info);
#endif // SL_ML_ENABLE_ENERGY_PROFILING

  // Memory (KiB)
  counter_pkt_info.counter_unit  = SL_ML_COUNTER_UNIT_KB;
  counter_pkt_info.counter_value = static_cast<float>(current_ram_kb);
  sl_ml_profiler_counter_pkt_write(&counter_pkt_info);
#else
  // Print values when debug channel is disabled
  std::printf("\n=== COUNTER ===\n");
  std::printf("  CPU Frequency (MHz): %.2f\n", get_cpu_freq() / 1'000'000.0f);
#ifdef SL_ML_ENABLE_ENERGY_PROFILING
  std::printf("  Energy (J)       : %.6f\n", 0.0f);
#endif // SL_ML_ENABLE_ENERGY_PROFILING
  std::printf("  Memory (KiB)     : %.4f\n\n", current_ram_kb);
#endif
}
// ============================== EVENT INFO ==================================================
/* ***************************************************************************//**
*  @brief Send start event info to debug channel.
*  Fills and sends the start event info structure to the debug channel.
*******************************************************************************/
void SilabsProfiler::emit_start_event_info()
{
#if (SL_ML_ENABLE_PROFILER_DEBUG_MSG)
    sl_ml_profiler_start_event_write(&event_info);
#else // SL_ML_ENABLE_PROFILER_DEBUG_MSG
    printf("\n=== EVENT START: %s ===\n", event_info.function_name);
    printf("  Line Number      : %d\n", -1);
    printf("  Processor ID     : %s\n", map_mcu_id(start_session_info.mcu_id));
    printf("  Input Shape      : ");
    for (int i = 0; i < event_info.input_shape.n_dimensions; i++) {
        printf("%dx", event_info.input_shape.shape[i]);
    }
    printf("\n");
    printf("  Output Shape     : ");
    for (int i = 0; i < event_info.output_shape.n_dimensions; i++) {
        printf("%dx", event_info.output_shape.shape[i]);
    }
    printf("\n");
    printf("  Operator Number  : %d\n", operator_num_in_model_);
#endif //SL_ML_ENABLE_PROFILER_DEBUG_MSG
}
/***************************************************************************//**
*  @brief prints a 64bit unsigned integer (uint64_t) with thousands separators
******************************************************************************/
void SilabsProfiler::sli_ui32_print_ts(uint32_t n) {
    if (n < 1000) {
        printf("%lu", n);
        return;
    }
    sli_ui32_print_ts(n / 1000);
    printf(",%03lu", n % 1000);
}

/***************************************************************************//**
*  @brief prints a 64bit unsigned integer (uint64_t) with text
*
*  Uses sli_ui32_print_ts to format the integer.
******************************************************************************/
void SilabsProfiler::sli_print_ui32(const char* lead_str, uint32_t n, const char* lag_str) {
    printf(lead_str);
    sli_ui32_print_ts(n);
    printf(lag_str);
}

/***************************************************************************//**
*  @brief Convert cycle count to human-readable time and print it.
*
*  Converts cycle counts to seconds/ms/us using the stored cpu_freq_
*  and prints a formatted message.
******************************************************************************/
void SilabsProfiler::sli_print_time(const char* str, uint32_t time) {
    cpu_freq_ = get_cpu_freq();
    printf("  CPU core frequency: %.3f MHz\n", (float)cpu_freq_ / 1000000.0);

    float duration = (float)time / (float)cpu_freq_;
    if (duration > 1.0) {
        printf("%s%.03f s\n", str, duration);
    } else if (duration > 0.001) {
        printf("%s%.03f ms\n", str, duration * 1000.0);
    } else if (duration > 0.000001) {
        printf("%s%.03f us\n", str, duration * 1000000.0);
    }
}
/***************************************************************************
*  @brief Send end event info to debug channel.
*  Fills and sends the end event info structure to the debug channel.
****************************************************************************/
void SilabsProfiler::emit_end_event_info()
{

#if (SL_ML_ENABLE_PROFILER_DEBUG_MSG)
    sl_ml_profiler_end_event_write(&event_info);
#else // SL_ML_ENABLE_PROFILER_DEBUG_MSG
    printf("\n=== EVENT END: %s ===\n", event_info.function_name);
    this->sli_print_ui32("  CPU Cycles         : ", event_info.num_cpu_cycles, "\n");
    this->sli_print_ui32("  CPU Stalls         : ", event_info.num_cpu_stalls, "\n");
//#if defined(SL_CATALOG_MVP_PRESENT)
    this->sli_print_ui32("  MVP Cycles         : ", event_info.num_mvp_cycles, "\n");
    this->sli_print_ui32("  MVP Stalls         : ", event_info.num_mvp_stalls, "\n");
//#endif
    printf("  CPU Utilization   : %.2f%%\n", event_info.cpu_util_percent);
    printf("  Clock Rate (Hz)    : %.2f\n", event_info.clock_rate_hz);
    printf("  MAC/Cycle          : %.4f\n", event_info.mac_per_cycle);
#ifdef SL_ML_ENABLE_ENERGY_PROFILING
    printf("  Energy (Joules)    : %.6f\n", event_info.energy_joules);
#endif // SL_ML_ENABLE_ENERGY_PROFILING
    //printf("  Line Number        : %d\n", event_info.line_number);
    printf("  Processor ID       : %s\n", map_processor_id(event_info.processor_id));
    this->sli_print_time("  Execution Time     : ", event_info.num_cpu_cycles);
    printf("\n");
//#endif
#endif // SL_ML_ENABLE_PROFILER_DEBUG_MSG
}

} // namespace ml
} // namespace sl
