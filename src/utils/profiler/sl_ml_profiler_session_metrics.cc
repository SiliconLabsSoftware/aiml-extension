/***************************************************************************//**
 * @file sl_ml_profiler_counter_metrics.cc
 * @brief Computes counter metrics.
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
#include "sl_ml_silabs_profiler.h"
#include <cstddef>
#include <cstdint>
#include <cstdio>  // printf in non-debug builds
#include <stdint.h>
// If you use Silicon Labs sleeptimer:
// If you don't have sleeptimer, use your always-on timer instead and
// provide equivalents for WALL_TICKS_NOW() and WALL_TICKS_PER_SEC().
#define WALL_TICKS_NOW()        sl_sleeptimer_get_tick_count64()
#define WALL_TICKS_PER_SEC()    sl_sleeptimer_get_timer_frequency()

/***************************************************************************//**
 * @brief Linker symbols declared as arrays for memory map(addresses).
 ******************************************************************************/
extern char __data_start__[];
extern char __data_end__[];
extern char __bss_start__[];
extern char __bss_end__[];

extern char __HeapBase[];    // start of heap
extern char __HeapLimit[];   // end of heap (end of RAM)

extern char __StackTop[];    // initial SP (top of stack)
extern char __StackLimit[];  // stack low limit

#if defined(SL_BOARD_SI91X)
extern char __rom_start[];
extern char __rom_length[];
#endif

static char *heap_curr = __HeapBase;
/***************************************************************************//**
 * @brief Flash memory map constants.
 ******************************************************************************/
#if defined(SL_BOARD_SI91X)
#define FLASH_BASE    (uintptr_t)__rom_start
#define FLASH_LENGTH  (uintptr_t)__rom_length
#endif

#if defined(SL_BOARD_EFX)
//#define FLASH_BASE    ((uintptr_t)SCB->VTOR)
#define FLASH_LENGTH  ((size_t)SYSTEM_GetFlashSize() * 1024)
#endif

#define SECTOR_SIZE   (32)
#define ERASED_WORD   (0xFFFFFFFFu)

#ifdef SL_ML_ENABLE_ENERGY_PROFILING
/***************************************************************************//**
 * @brief Electric model constants used for energy measurment(TODO : Not used now).
 ******************************************************************************/
constexpr float UA_PER_MHZ_ACTIVE = 49.1f; // microamps per MHz (active)
constexpr float STATIC_MA         = 0.0f;  // baseline static current (mA)
constexpr float VOLTAGE_V         = 3.3f;  // supply voltage (V)
#endif // SL_ML_ENABLE_ENERGY_PROFILING



/***************************************************************************//**
 * @brief SI91X MVP performance counter access (RUN and STALL).
 *
 * Configures and reads MVP counters on SI91X:
 * - Counter0: RUN cycles
 * - Counter1: STALL cycles
 ******************************************************************************/
#if (!defined(SL_CATALOG_MVP_PRESENT))
static inline void sli_mvp_perfcnt_reset_all() {
  // Clear and configure counters to measure RUN and STALL
  // Disable counters while configuring
  MVP->CFG_CLR = MVP_CFG_PERFCNTEN;
  // Select PERF0 = RUN, PERF1 = STALL
  MVP->CFG_CLR = (_MVP_CFG_PERF0CNTSEL_MASK | _MVP_CFG_PERF1CNTSEL_MASK);
  MVP->CFG_SET = (uint32_t)MVP_CFG_PERF0CNTSEL_RUN;
  MVP->CFG_SET = (uint32_t)MVP_CFG_PERF1CNTSEL_STALL;
  // Enable performance counters
  MVP->CFG_SET = MVP_CFG_PERFCNTEN;
}
/***************************************************************************//**
 * @brief Reset MVP cycles and program count.
 ******************************************************************************/
static inline void sli_mvp_progcnt_reset() {
  // Reset sequence: briefly disable and re-enable perf counters
  MVP->CFG_CLR = MVP_CFG_PERFCNTEN;
  MVP->CFG_SET = MVP_CFG_PERFCNTEN;
}

/***************************************************************************//**
 * @brief provides instrunctions/stalls count for MVP accelerator.
 *
 * @return mvp cycles count.
 ******************************************************************************/
static inline uint32_t sli_mvp_perfcnt_get(int counter_id) {
  // Return the selected counter value
  if (counter_id == 0) {
    return (uint32_t)MVP->PERF[0].CNT;
  } else if (counter_id == 1) {
    return (uint32_t)MVP->PERF[1].CNT;
  }
  return 0u;
}
/***************************************************************************//**
 * @brief provides program count for MVP accelerator.
 *
 * @return mvp program count.
 ******************************************************************************/
static inline uint32_t sli_mvp_progcnt_get() {
  // SI91X does not expose a separate program counter; return PERF0 as proxy
  return (uint32_t)MVP->PERF[0].CNT;
}
#endif

namespace sl {
namespace ml {
//====================================RESET/INIT METRICS============================
  /***************************************************************************//**
 * @brief Reset per-op calculators and session info via the callee.
 *
 * @return true on success.
 ******************************************************************************/
bool SilabsProfiler::reset_profiler_metrics()
{
    // get subgraph and operator
    if (!model_profiler) return false;
    // set values to 0
    memset(&event_info, 0, sizeof(sl_ml_profiler_event_info_t));

    const tflite::SubGraph* subgraph = model_profiler->subgraphs()->Get(0);

    number_operators_model_ = subgraph->operators()->size(); // stored in profiler class member
    if (number_operators_model_ != 0 ){
        operator_num_in_model_ = operation_index_%number_operators_model_; // stored in profiler class member
    }
    // reset session info for evry sample
    // For every sample reset counters to 0 except enargy and mean inference
    if (operator_num_in_model_ == 0)
    {
        end_session_info.total_num_cycles      = 0;
        end_session_info.total_num_stalls      = 0;
        end_session_info.total_acc_cycles      = 0;
        end_session_info.total_acc_stalls      = 0;
        end_session_info.total_num_ops         = 0;
        end_session_info.total_num_macs        = 0;
        end_session_info.total_flash_used_kb   = 0;
        end_session_info.total_ram_used_kb     = 0;
        end_session_info.mean_mac_per_cycle    = 0.0f;
        end_session_info.total_num_cycles      = 0;
    }
    // Reset energy and inference when session start only
    if (operation_index_ == 0)
    {
        end_session_info.mean_inference_time_ms = 0.0f;
#ifdef SL_ML_ENABLE_ENERGY_PROFILING
        end_session_info.energy_joules         = 0.0f;
#endif // SL_ML_ENABLE_ENERGY_PROFILING
        // Emit start session info to debug channel
        start_session_info = {};
        update_start_session_info(&start_session_info);
        emit_start_session_info();
    }

    return true;
}
//====================================START SESSION INFO=======================

/***************************************************************************//**
 * @brief
 *   Derive a human-readable core name from the CPUID register.
 *
 * @return
 *   "Cortex-M4", "Cortex-M33", or a generic "Cortex-M" fallback.
 ******************************************************************************/
const char* SilabsProfiler::core_name_from_cpuid(void)
{
  // CMSIS: SCB->CPUID holds implementer/part number for the core.
  // PartNo field = bits [15:4].
  //   0xC24 -> Cortex-M4
  //   0xD21 -> Cortex-M33
  uint32_t cpuid  = SCB->CPUID;
  uint32_t partno = (cpuid >> 4) & 0xFFFu;

  switch (partno) {
    case 0xC24: return "Cortex-M4";
    case 0xD21: return "Cortex-M33";
    default:    return "Cortex-M"; // Fallback (unknown M-profile)
  }
}
/***************************************************************************//**
 * @brief
 *   Resolve MCU core name using compile-time hints when available.
 *
 * @return
 *   Core name string (e.g., "Cortex-M33").
 ******************************************************************************/
sl_ml_mcu_id_t SilabsProfiler::get_mcu_id(void)
{
#if defined(__CORTEX_M)
  // Prefer compile-time if available (CMSIS defines __CORTEX_M = 4 or 33)
#  if   (__CORTEX_M == 33U)
  return SL_ML_MCU_ID_ARM_CORTEX_M33;//"Cortex-M33";
#  elif (__CORTEX_M == 4U)
  return SL_ML_MCU_ID_ARM_CORTEX_M4;//"Cortex-M4";
#  else
  return SL_ML_MCU_ID_INVALID;//core_name_from_cpuid();
#  endif
#else
  return SL_ML_MCU_ID_INVALID;//core_name_from_cpuid();
#endif
}
/***************************************************************************//**
 * @brief
 *   Read MVP IP version from whichever alias is visible (TZ/non-TZ).
 *
 * @return
 *   IPVERSION register value, or 0 if MVP is not present/visible.
 ******************************************************************************/
uint32_t SilabsProfiler::read_mvp_ipversion(void)
{
  // On TrustZone-enabled Series 2 parts, MVP may be aliased as MVP_NS or MVP_S.
#if defined(MVP_NS)
  return MVP_NS->IPVERSION;
#elif defined(MVP_S)
  return MVP_S->IPVERSION;
#elif defined(MVP)
  return MVP->IPVERSION;
#else
  return 0u; // No MVP in this device / image
#endif
}
/***************************************************************************//**
 * @brief
 *   Produce a friendly accelerator name based on the MVP IP version.
 *
 * @return
 *   "mvp1" if absent/unknown.
 ******************************************************************************/
sl_ml_accelerator_id_t SilabsProfiler::get_accelerator_id(void)
{
  // If there's no MVP peripheral accessible, report "none".
#if !(defined(MVP) || defined(MVP_NS) || defined(MVP_S))
  return SL_ML_ACCELERATOR_ID_INVALID; //"none";
#else
  // Silicon Labs peripherals expose an IPVERSION register whose value is the IP version.
  uint32_t v = read_mvp_ipversion();
  if (v == 0u)          return SL_ML_ACCELERATOR_ID_INVALID; // not present/visible
  uint32_t major = v & 0xFFu;          // defensive: use low 8 bits
  switch (major) {
    case 1u:  return SL_ML_ACCELERATOR_ID_MVPv1;// "mvp1";
    default:  return SL_ML_ACCELERATOR_ID_MVPv1;           // newer/unknown version
  }
#endif
}
/***************************************************************************//**
 * @brief
 *   Check if a flash sector is blank (erased).
 * @param sector_addr
 *   Start address of the sector to check.
 * @return
 *   true if all words in the sector are 0xFFFFFFFF, false otherwise.
 ******************************************************************************/
bool SilabsProfiler::sector_is_blank(uint32_t sector_addr)
{
  for (uint32_t off = 0; off < SECTOR_SIZE; off += 4u) {
    uint32_t v = *(volatile const uint32_t *)(sector_addr + off);
    if (v != ERASED_WORD) {
      return false;
    }
  }
  return true;
}
/***************************************************************************//**
 * @brief
 *   Calculate used flash bytes by scanning sectors.
 * @return
 *   Number of used bytes in flash.
 ******************************************************************************/
uint32_t SilabsProfiler::flash_used_kb()
{
  uint32_t used = 0;
  for (uint32_t a = FLASH_BASE; a < FLASH_BASE + FLASH_LENGTH; a += SECTOR_SIZE) {
    if (!sector_is_blank(a)) {
      used += SECTOR_SIZE;
    }
  }
  return (used / 1024);
}
/***************************************************************************//**
 * @brief
 *   Populate the start-session info structure with defaults and, when known,
 *   board-specific values.
 *
 * @param[out] info
 *   Structure to fill.
 ******************************************************************************/
void SilabsProfiler::update_start_session_info(sl_ml_profiler_start_session_info_t* info)
{
  // Initialize defaults
  *info = {};
#ifdef SL_BOARD_NAME
  info->board_name  = SL_BOARD_NAME;
#else
  info->board_name  = "NA";
#endif
  info->model_name  = "NA";
  info->flash_kb    = 0;
  info->ram_kb      = 0;
  info->arena_kb    = static_cast<uint16_t>(interpreter_profiler->arena_used_bytes() / 1024.0f);
  // mcu ID
  info->mcu_id      = get_mcu_id();
  // accelarator name
  info->accel_id    = get_accelerator_id();

#if defined(SL_BOARD_EFX)
  info->opn         = PART_NUMBER;
   //(defined(_SILICON_LABS_32B_SERIES) && defined(_SILICON_LABS_32B_SERIES_2_CONFIG))
   //std::string part_family = std::to_string(_SILICON_LABS_32B_SERIES) +std::to_string(_SILICON_LABS_32B_SERIES_2_CONFIG);
   //info->part_family = part_family.c_str();
  info->part_family = "EFR32";
  info->model_name  = "NA";
  info->flash_kb    = SYSTEM_GetFlashSize();
  info->ram_kb      = SYSTEM_GetSRAMSize();
#endif


#if defined(SL_BOARD_SI91X)
  info->opn         = "SIW91X";
  info->part_family = "SIW91X";
  uintptr_t rom_size  = (uintptr_t)__rom_length;
  info->flash_kb    = (uint16_t)(rom_size / 1024);

  uintptr_t ram_base = (uintptr_t)__data_start__;
  uintptr_t ram_top  = (uintptr_t)__HeapLimit;
  info->ram_kb   = (ram_top - ram_base) / 1024;
#endif

}
//======================================END SESSION METRICS==================================
/***************************************************************************//**
 * @brief Update session metrics and update session_info via the callee.
 *
 * Called once per operator. Selects the appropriate `calculate_*` implementation.
 *
 * @return true on success.
 ******************************************************************************/
void SilabsProfiler::update_end_session_info()
{
    end_session_info.total_num_cycles += event_info.num_cpu_cycles;
    end_session_info.total_num_stalls += event_info.num_cpu_stalls;
    end_session_info.total_acc_cycles += event_info.num_mvp_cycles;
    end_session_info.total_acc_stalls += event_info.num_mvp_stalls;
    end_session_info.total_num_ops += event_info.ops;
    end_session_info.total_num_macs += event_info.macs;
    //end_session_info.total_ram_used_kb = 0; updated in emit_counter_packet_info()
    end_session_info.mean_mac_per_cycle += event_info.mac_per_cycle;
    //end_session_info.mean_inference_time_ms = inference_time_ms_;
#ifdef SL_ML_ENABLE_ENERGY_PROFILING
    end_session_info.energy_joules += event_info.energy_joules;
#endif // SL_ML_ENABLE_ENERGY_PROFILING
    end_session_info.total_num_cycles += event_info.num_cpu_cycles;
    // Check if end of inference
    if (number_operators_model_-1 == operator_num_in_model_) {
        number_of_inferences_++;
        // Finalize averages for the inference
        if (number_of_inferences_ == SL_NUMBER_OF_SAMPLES_TO_PROCESS) {
            end_session_info.total_flash_used_kb = flash_used_kb();
            end_session_info.mean_inference_time_ms = inference_time_ms_ / static_cast<float>(SL_NUMBER_OF_SAMPLES_TO_PROCESS);
#ifdef SL_ML_ENABLE_ENERGY_PROFILING
            end_session_info.energy_joules = end_session_info.energy_joules /
                                            static_cast<float>(SL_NUMBER_OF_SAMPLES_TO_PROCESS);
#endif // SL_ML_ENABLE_ENERGY_PROFILING
            end_session_info.mean_mac_per_cycle = end_session_info.mean_mac_per_cycle /
                        (static_cast<float>(SL_NUMBER_OF_SAMPLES_TO_PROCESS) * (static_cast<float>(number_operators_model_)));

            // prepare end track info to debug channel
#if (SL_ML_ENABLE_PROFILER_DEBUG_MSG)
            emit_counter_end_track_packets();
            emit_processor_core_end_track_packets();
#endif //SL_ML_ENABLE_PROFILER_DEBUG_MSG
            // Prepare end session info to debug channel
            emit_end_session_info();
            exit(1);
        }
    }
}

//====================================COUNTER INFO=================================
#if 1
/***************************************************************************//**
 * @brief Bytes used by the .data segment.
 ******************************************************************************/
size_t SilabsProfiler::data_usage_bytes() {
  // return (size_t)(__bss_end__ - __data_start__);
  return static_cast<std::size_t>(__data_end__ - __data_start__);
}
/***************************************************************************//**
 * @brief Bytes used by the .bss segment.
 ******************************************************************************/
size_t SilabsProfiler::bss_usage_bytes() {
  return static_cast<std::size_t>(__bss_end__ - __bss_start__);
}
/***************************************************************************//**
 * @brief Bytes used by the heap (program break).
 ******************************************************************************/
std::size_t SilabsProfiler::heap_usage_bytes() {
  return (size_t)(heap_curr - __HeapBase);
}
/***************************************************************************//**
 * @brief Bytes used by the stack (current SP to top).
 * @note Uses compiler-specific inline assembly to read SP when available.
 ******************************************************************************/
std::size_t SilabsProfiler::stack_usage_bytes() {
#if defined(__GNUC__) || defined(__clang__)
  register char* sp asm("sp");
#else // __GNUC__ //  __clang__
  uint32_t sp = __get_MSP();  // current stack pointer
#endif// __GNUC__ //  __clang__
  if (sp >= __StackTop) return 0;
  return static_cast<std::size_t>(__StackTop - sp);
}
#endif
/***************************************************************************//**
 * @brief Total RAM utilization in KiB (data + bss + heap + stack).
 * @return Total KiB.
 * @see send_counter_packet_info()
 ******************************************************************************/
float SilabsProfiler::get_memory_utilization_kb() {
  std::size_t total_b =
      data_usage_bytes()
    + bss_usage_bytes()
    + heap_usage_bytes()
    + stack_usage_bytes();

  return (total_b / 1024.0f); // integer KiB
}

/***************************************************************************//**
 * @brief Get current CPU frequency (Hz).
 * @return Frequency in Hz.
 ******************************************************************************/
std::uint32_t SilabsProfiler::get_cpu_freq() {
  std::uint32_t cpu_freq_hz = 0;
#if defined(SL_BOARD_EFX)
  cpu_freq_hz = CMU_ClockFreqGet(cmuClock_CORE);   // Core (CPU) clock
#endif
#if defined(SL_BOARD_SI91X)
  cpu_freq_hz = ml_get_cpu_clock_frequency();
#endif
  return cpu_freq_hz;
}

#ifdef SL_ML_ENABLE_ENERGY_PROFILING
/***************************************************************************//**
 * @brief Estimate active current (A) from CPU frequency.
 * @param cpu_hz CPU frequency in Hz.
 * @return Estimated active current in amperes.
 ******************************************************************************/
float SilabsProfiler::estimate_active_current_A(std::uint32_t cpu_hz) {
  const float cpu_ma = (UA_PER_MHZ_ACTIVE * (cpu_hz / 1e6f)) / 1000.0f;
  return (cpu_ma + STATIC_MA) / 1000.0f;
}
#endif // SL_ML_ENABLE_ENERGY_PROFILING

#ifdef SL_ML_ENABLE_ENERGY_PROFILING
/***************************************************************************//**
 * @brief Convert cycles to energy (J).
 * @param delta_cycles Elapsed cycles.
 * @return Energy in joules.
 ******************************************************************************/
float SilabsProfiler::energyJ_from_cycles(std::uint32_t delta_cycles) {
  const std::uint32_t cpu_hz  = get_cpu_freq();
  const float         hz_safe = cpu_hz ? static_cast<float>(cpu_hz) : 1.0f;
  const float         duration_s = static_cast<float>(delta_cycles) / hz_safe;
  return duration_s * estimate_active_current_A(cpu_hz) * VOLTAGE_V;
}
#endif // SL_ML_ENABLE_ENERGY_PROFILING

//=================================END EVENT INFO==================================
/***************************************************************************//**
 * @brief Enable DWT CYCCNT if not already enabled.
 ******************************************************************************/
void SilabsProfiler::dwt_enable_cyccnt(void)
{
#ifdef __arm__
  // Enable trace block (needed for DWT)
  CoreDebug->DEMCR |= CoreDebug_DEMCR_TRCENA_Msk;

  // Start the cycle counter if it's not already running
  if ((DWT->CTRL & DWT_CTRL_CYCCNTENA_Msk) == 0u) {
    DWT->CYCCNT = 0u;
    DWT->CTRL  |= DWT_CTRL_CYCCNTENA_Msk;
  }
#endif
}
/***************************************************************************//**
 * @brief Get CPU cycles.
 ******************************************************************************/
uint32_t SilabsProfiler::get_cpu_cycles()
{
#ifdef __arm__
    auto DWT_CYCCNT_REG ((const volatile uint32_t*)0xE0001004UL); // DWT->CYCCNT
    auto CORE_DEBUG_DEMCR = ((volatile uint32_t*)0xE000EDFC); // CoreDebug->DEMCR
    *CORE_DEBUG_DEMCR |= CoreDebug_DEMCR_TRCENA_Msk;
    return *DWT_CYCCNT_REG;
#else
    return 0; // CPU cycles on Windows/Linux don't make sense, so just return 0
#endif
}
/***************************************************************************//**
 * @brief Call at beginEvent().
 ******************************************************************************/
void SilabsProfiler::event_counter_begin()
{
  dwt_enable_cyccnt();
//#if defined(SL_CATALOG_MVP_PRESENT)
    sli_mvp_perfcnt_reset_all();
    sli_mvp_progcnt_reset();
//#endif
    busy_cycles = get_cpu_cycles(); // DWT->CYCCNT;
    wall_ticks   = WALL_TICKS_NOW();
}

/***************************************************************************//**
 * @brief Call at endEvent(); returns CPU utilization percentage in [0, 100].
 ******************************************************************************/
void SilabsProfiler::event_counter_end()
{
  uint32_t cyccnt_end   = get_cpu_cycles(); //DWT->CYCCNT;
  uint32_t wall_end     = WALL_TICKS_NOW();
//#if defined(SL_CATALOG_MVP_PRESENT)
    mvp_instructions = sli_mvp_perfcnt_get(0);
    if (mvp_instructions > 0) {
        mvp_stall_cycles = sli_mvp_perfcnt_get(1);
        mvp_programs = sli_mvp_progcnt_get();
    }
//#endif

  // Unsigned subtraction handles CYCCNT wraparound (mod 2^32)
  busy_cycles  = (uint32_t)(cyccnt_end - busy_cycles);
  wall_ticks   = (wall_end   - wall_ticks);

  uint32_t busy_cpu_cycles = busy_cycles-mvp_instructions;//+mvp_stall_cycles;
  float cpu_pct;

  uint32_t cpu_freq = get_cpu_freq();
  float busy_s = (float)busy_cpu_cycles / cpu_freq;
  if (wall_ticks == 0u) {
     cpu_pct =  0.0f;
  }
  else{
    float wall_s = (float)wall_ticks / WALL_TICKS_PER_SEC();
    cpu_pct = (busy_s / wall_s) * 100.0;
    if (cpu_pct < 0.0)   cpu_pct = 0.0;
    if (cpu_pct > 100.0) cpu_pct = 100.0;
  }

  event_info.num_cpu_cycles = busy_cpu_cycles;
  event_info.num_cpu_stalls = 0; // TODO
//#if defined(SL_CATALOG_MVP_PRESENT)
  event_info.num_mvp_cycles = mvp_instructions;
  event_info.num_mvp_stalls = mvp_stall_cycles;
//#endif
  event_info.cpu_util_percent = cpu_pct;
  event_info.clock_rate_hz = (float)cpu_freq;
    // Per cycle MACs
  event_info.mac_per_cycle = event_info.macs / (float)busy_cycles;;
#ifdef SL_ML_ENABLE_ENERGY_PROFILING
  event_info.energy_joules = 0.0; //energyJ_from_cycles(busy_cycles); // TODO
#endif // SL_ML_ENABLE_ENERGY_PROFILING
  inference_time_ms_ += ((float)busy_cycles / cpu_freq) * 1000.0f;
}

} // namespace ml
} // namespace sl
