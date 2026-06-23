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

#if (SL_ML_ENABLE_DYNAMIC_MODEL_LOAD == 1)

#define CUSTOM_OP_RESOLVER_EN (0)

#if (CUSTOM_OP_RESOLVER_EN)
#include "sl_tflite_micro_all_opcode_resolver.h"
#else
#include "ml/third_party/tflm/all_ops_resolver.h"
#endif

uint8_t primary_tensor_arena_buffer_[SL_ML_PROFILER_PRIMARY_TENSOR_ARENA_SIZE] __attribute__((section(".bss"))) __attribute__((aligned(4)));
uint8_t weights_paging_buffer_[SL_ML_PROFILER_MODEL_PAGING_BUFFER_SIZE] __attribute__((section(".bss"))) __attribute__((aligned(4)));//ml_model_cache_buffer
const int32_t model_buffer_sizes_[2] = { 
  SL_ML_PROFILER_PRIMARY_TENSOR_ARENA_SIZE, 
  SL_ML_PROFILER_MODEL_PAGING_BUFFER_SIZE 
};
uint8_t* model_buffers_[2] = { 
  primary_tensor_arena_buffer_,
   weights_paging_buffer_ 
  };

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
  const int32_t model_buffer_count_ = 2;

  ::npu_toolkit::register_tflite_micro_accelerator();
  const bool ok = model.load(
    flatbuffer_ptr,
    &opcode_resolver,
    model_buffers_,
    model_buffer_sizes_,
    model_buffer_count_
  );
  return ok;
}

#endif // SL_ML_ENABLE_DYNAMIC_MODEL_LOAD

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
