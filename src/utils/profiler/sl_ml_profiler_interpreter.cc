/***************************************************************************//**
 * @file sl_ml_profiler_interpreter.cc
 * @brief Interpreter and profiler model wrapper implementation for the ML profiler.
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
#include "sl_ml_profiler_interpreter.h"
#include "sl_ml_silabs_profiler.h"
#include "sl_ml_profiler_config.h"
#include "tflite_micro_compiled_allocator.hpp"

#include <cinttypes>
#if (SL_ML_ENABLE_DYNAMIC_BUFFER_ALLOCATION == 1)
#include <climits>
#include <cstdlib>
#endif

#if (SL_ML_ENABLE_DYNAMIC_MODEL_LOAD == 1)

#define CUSTOM_OP_RESOLVER_EN (0)

#if (CUSTOM_OP_RESOLVER_EN)
#include "sl_tflite_micro_all_opcode_resolver.h"
#else
#include "all_ops_resolver.h"
#endif

#if (SL_ML_ENABLE_DYNAMIC_BUFFER_ALLOCATION == 1)

static constexpr int32_t k_max_profiler_model_buffers = 8;
static constexpr size_t k_model_buffer_alignment = 16;
// The compiled memory plan only covers the model's own buffers. The allocator
// instance and the TFLM persistent (tail) allocations also come out of region 0,
// so that region needs headroom on top of the planned size.
static constexpr int32_t k_model_buffer_headroom = SL_ML_PROFILER_MODEL_BUFFER_HEADROOM_SIZE;
static constexpr int32_t k_max_model_buffer_headroom_retries = 4;

static uint8_t* g_model_buffer_bases[k_max_profiler_model_buffers] = {};
static uint8_t* g_model_buffers[k_max_profiler_model_buffers] = {};
static int32_t g_model_buffer_sizes[k_max_profiler_model_buffers] = {};
static int32_t g_model_buffer_count = 0;
static SLTfliteMicroModel* g_loaded_dynamic_model = nullptr;

static int32_t model_buffer_count_from_flatbuffer(const CompiledMemoryPlan* memory_plan)
{
  if (memory_plan == nullptr) {
    return 1;
  }

  return static_cast<int32_t>(memory_plan->n_memory_regions());
}

static bool model_buffer_sizes_from_flatbuffer(const CompiledMemoryPlan* memory_plan,
                                               int32_t buffer_count,
                                               int32_t* buffer_sizes)
{
  if (buffer_count <= 0 || buffer_sizes == nullptr) {
    return false;
  }

  if (memory_plan == nullptr) {
    buffer_sizes[0] = SL_ML_PROFILER_PRIMARY_TENSOR_ARENA_SIZE;
    return buffer_sizes[0] > 0;
  }

  for (int32_t i = 0; i < buffer_count; ++i) {
    const uint32_t region_size = memory_plan->memory_region_size(i);
    if (region_size == 0u || region_size > static_cast<uint32_t>(INT32_MAX)) {
      std::printf("[ml] Invalid memory region %" PRId32 " size %" PRIu32 "\n", i, region_size);
      return false;
    }
    buffer_sizes[i] = static_cast<int32_t>(region_size);
  }

  return true;
}

static uint8_t* allocate_aligned_model_buffer(size_t arena_size, uint8_t** aligned_buffer)
{
  arena_size = (arena_size + k_model_buffer_alignment - 1u) & ~(k_model_buffer_alignment - 1u);
  uint8_t* buffer_base = static_cast<uint8_t*>(std::malloc(arena_size + k_model_buffer_alignment));
  if (buffer_base == nullptr) {
    return nullptr;
  }

  *aligned_buffer = reinterpret_cast<uint8_t*>(
    (reinterpret_cast<uintptr_t>(buffer_base) + k_model_buffer_alignment - 1u)
    & ~(k_model_buffer_alignment - 1u));
  return buffer_base;
}

static void free_allocated_model_buffers(void)
{
  for (int32_t i = 0; i < g_model_buffer_count; ++i) {
    std::free(g_model_buffer_bases[i]);
    g_model_buffer_bases[i] = nullptr;
    g_model_buffers[i] = nullptr;
    g_model_buffer_sizes[i] = 0;
  }
  g_model_buffer_count = 0;
}

void release_dynamic_tflite_micro_model(void)
{
  // Do not call `unload()` here. Session end runs from `EndEvent`, so the
  // interpreter is still on the stack. Drop the model pointer and free heap
  // buffers; process exit follows immediately after the session packets.
  g_loaded_dynamic_model = nullptr;
  free_allocated_model_buffers();
}

#else // SL_ML_ENABLE_DYNAMIC_BUFFER_ALLOCATION == 0

uint8_t primary_tensor_arena_buffer_[SL_ML_PROFILER_PRIMARY_TENSOR_ARENA_SIZE] __attribute__((section(".bss"))) __attribute__((aligned(4)));
uint8_t weights_paging_buffer_[SL_ML_PROFILER_MODEL_PAGING_BUFFER_SIZE] __attribute__((section(".bss"))) __attribute__((aligned(4)));
const int32_t model_buffer_sizes_[2] = {
  SL_ML_PROFILER_PRIMARY_TENSOR_ARENA_SIZE,
  SL_ML_PROFILER_MODEL_PAGING_BUFFER_SIZE
};
uint8_t* model_buffers_[2] = {
  primary_tensor_arena_buffer_,
  weights_paging_buffer_
};

static constexpr int32_t k_profiler_model_buffer_count =
  sizeof(model_buffer_sizes_) / sizeof(model_buffer_sizes_[0]);

static int32_t model_buffer_count_from_flatbuffer(const void* flatbuffer_ptr)
{
  const CompiledMemoryPlan* memory_plan =
    TfliteMicroCompiledAllocator::retrieve_memory_plan(flatbuffer_ptr);

  if (memory_plan == nullptr) {
    return 1;
  }

  return memory_plan->n_memory_regions();
}

#endif // SL_ML_ENABLE_DYNAMIC_BUFFER_ALLOCATION

// Model is loaded dynamically, use the model base address here to get the model data
// Adjust the address according to your memory map and linker script
// SL_ML_PROFILER_DEBUG_MODEL_BASE_ADDR is defined in slcp project file as a preprocessor macro
// For the SiWG917M111M (your module) J‑Link’s device table shows: kb.segger.com
// M4 application flash base: 0x08201000
// M4 application flash size: 2044 KB = 0x001FF000 bytes
// So M4’s flash region is:
//    Start (header): 0x08201000
//    End (exclusive): 0x08201000 + 0x001FF000 = 0x08400000
// You want to reserve 1 MB = 0x00100000 at the very end of that region:
// End of M4 flash (exclusive): 0x08400000
// 1 MB region size:            0x00100000
// Model start = 0x08400000 - 0x00100000 = 0x08300000
static constexpr uintptr_t PRIMARY_MODEL_BASE_ADDR = static_cast<uintptr_t>(SL_ML_PROFILER_DEBUG_MODEL_BASE_ADDR); // 0x8351000UL

static inline bool has_tfl3_identifier(const uint8_t* ptr)
{
  // FlatBuffer file identifier for TFLite models is "TFL3" at bytes [4..7]
  return ptr[4] == 'T' && ptr[5] == 'F' && ptr[6] == 'L' && ptr[7] == '3';
}

static inline const uint8_t* select_model_flatbuffer_ptr()
{
  const uint8_t* primary = reinterpret_cast<const uint8_t*>(PRIMARY_MODEL_BASE_ADDR);
  return primary;
}

#if (CUSTOM_OP_RESOLVER_EN)
static tflite::MicroOpResolver &sl_tflite_micro_opcode_resolver()
{
  SL_TFLITE_MICRO_OPCODE_RESOLVER(opcode_resolver);
  return opcode_resolver;
}
#endif

bool load_dynamic_tflite_micro_model(SLTfliteMicroModel& model)
{
#if (CUSTOM_OP_RESOLVER_EN)
  tflite::MicroOpResolver &opcode_resolver = sl_tflite_micro_opcode_resolver();
#else
  // AllOpsResolver registers every built-in op; constructor cost is large on MCU and
  // looks like a hang under the debugger. Use a function-local static so it runs once.
  static tflite::AllOpsResolver opcode_resolver;
#endif
    // Select a plausible flash base for the model; if both regions look blank,
  // bail out early to avoid undefined behavior when no dynamic model is present yet.
  const uint8_t* flatbuffer_ptr = select_model_flatbuffer_ptr();
  // Basic validity check for a TFLite flatbuffer before proceeding
  if (!has_tfl3_identifier(flatbuffer_ptr)) {
    std::printf("[ml] Invalid TFLite flatbuffer header at 0x%08lX (missing TFL3)\n", (unsigned long)flatbuffer_ptr);
    return false;
  }

#if (SL_ML_ENABLE_DYNAMIC_BUFFER_ALLOCATION == 1)
  const CompiledMemoryPlan* memory_plan =
    TfliteMicroCompiledAllocator::retrieve_memory_plan(flatbuffer_ptr);
  const int32_t model_buffer_count = model_buffer_count_from_flatbuffer(memory_plan);

  if (model_buffer_count <= 0 || model_buffer_count > k_max_profiler_model_buffers) {
    std::printf(
      "[ml] Model requires %" PRId32 " buffers but profiler firmware supports %" PRId32 "\n",
      model_buffer_count,
      k_max_profiler_model_buffers);
    return false;
  }

  if (g_loaded_dynamic_model != nullptr) {
    g_loaded_dynamic_model->unload();
    g_loaded_dynamic_model = nullptr;
  }
  free_allocated_model_buffers();

  int32_t planned_buffer_sizes[k_max_profiler_model_buffers] = {};
  if (!model_buffer_sizes_from_flatbuffer(memory_plan, model_buffer_count, planned_buffer_sizes)) {
    return false;
  }

  // The exact allocator overhead is not part of the plan, so start with the
  // configured headroom and grow it if the allocator reports it ran out of room.
  int32_t headroom = k_model_buffer_headroom;
  for (int32_t attempt = 0; attempt <= k_max_model_buffer_headroom_retries; ++attempt) {
    g_model_buffer_count = model_buffer_count;
    for (int32_t i = 0; i < model_buffer_count; ++i) {
      g_model_buffer_sizes[i] = planned_buffer_sizes[i] + ((i == 0) ? headroom : 0);
      g_model_buffer_bases[i] = allocate_aligned_model_buffer(
        static_cast<size_t>(g_model_buffer_sizes[i]),
        &g_model_buffers[i]);
      if (g_model_buffer_bases[i] == nullptr) {
        std::printf(
          "[ml] Failed to allocate %" PRId32 " bytes for model buffer %" PRId32 "\n",
          g_model_buffer_sizes[i],
          i);
        free_allocated_model_buffers();
        return false;
      }
      std::printf(
        "[ml] Allocated model buffer %" PRId32 ": %" PRId32 " bytes\n",
        i,
        g_model_buffer_sizes[i]);
    }

    ::npu_toolkit::register_tflite_micro_accelerator();

    if (model.load(
          flatbuffer_ptr,
          &opcode_resolver,
          g_model_buffers,
          g_model_buffer_sizes,
          model_buffer_count)) {
      g_loaded_dynamic_model = &model;
      return true;
    }

    free_allocated_model_buffers();
    headroom *= 2;
    if (attempt < k_max_model_buffer_headroom_retries) {
      std::printf(
        "[ml] Retrying model load with %" PRId32 " bytes of buffer headroom\n",
        headroom);
    }
  }

  return false;
#else // SL_ML_ENABLE_DYNAMIC_BUFFER_ALLOCATION == 0
  const int32_t model_buffer_count = model_buffer_count_from_flatbuffer(flatbuffer_ptr);

  if (model_buffer_count <= 0 || model_buffer_count > k_profiler_model_buffer_count) {
    std::printf(
      "[ml] Model requires %" PRId32 " buffers but profiler firmware provides %" PRId32 "\n",
      model_buffer_count,
      k_profiler_model_buffer_count);
    return false;
  }

  ::npu_toolkit::register_tflite_micro_accelerator();

  return model.load(
    flatbuffer_ptr,
    &opcode_resolver,
    model_buffers_,
    model_buffer_sizes_,
    model_buffer_count);
#endif // SL_ML_ENABLE_DYNAMIC_BUFFER_ALLOCATION
}

#endif // SL_ML_ENABLE_DYNAMIC_MODEL_LOAD

#if (SL_ML_ENABLE_DYNAMIC_BUFFER_ALLOCATION == 1) && (SL_ML_ENABLE_DYNAMIC_MODEL_LOAD != 1)
void release_dynamic_tflite_micro_model(void)
{
}
#endif

tflite::MicroInterpreter* SLTfliteMicroModel::create_interpreter()
{
  auto* interpreter = new (_interpreter_buffer) SLMicroInterpreter(
    flatbuffer_model(),
    *ops_resolver(),
    allocator(),
    nullptr,
    _custom_profiler);

  _custom_profiler->model_profiler = flatbuffer_model();
  _custom_profiler->interpreter_profiler = interpreter;

  return interpreter;
}
