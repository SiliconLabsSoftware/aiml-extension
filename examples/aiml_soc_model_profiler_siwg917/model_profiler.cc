/***************************************************************************//**
 * @file model_profiler.cc
 * @brief TFLM model profiler.
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
#include "ml/platform/ml_clock_helper.h"
#include "sl_sleeptimer.h"
#include "sl_ml_model_soc_model_profiler.h"
#include "sl_ml_mvp_counter.h"
#include "sl_nn_util.h"

#if defined(SL_COMPONENT_CATALOG_PRESENT)
#include "sl_component_catalog.h"
#endif

#include "ml/third_party/tflm/micro_profiler_interface.h"
#include "tflite_micro_model.hpp"
#include "model_profiler.h"
#include "energy_profiling_utils.hpp"

#include <cinttypes>
#include <cstdio>
#include <cstring>

//#define SL_PRINT_MATRIX_DATA                 // Harvest matrix data while profiling.
//#define SL_PRINT_MATRIX_DATA_HUMAN_READABLE

#if defined(__arm__)
static inline void dwt_enable_cyccnt_portable(void)
{
  constexpr uintptr_t kCoreDebugDemcrAddr = 0xE000EDFCUL;
  constexpr uintptr_t kDwtCtrlAddr = 0xE0001000UL;
  volatile uint32_t *const demcr = reinterpret_cast<volatile uint32_t *>(kCoreDebugDemcrAddr);
  volatile uint32_t *const dwt_ctrl = reinterpret_cast<volatile uint32_t *>(kDwtCtrlAddr);
  *demcr |= (1u << 24); // TRCENA
  *dwt_ctrl |= 1u;      // CYCCNTENA
}

static inline uint32_t dwt_read_cyccnt_portable(void)
{
  constexpr uintptr_t kDwtCyccntAddr = 0xE0001004UL;
  return *reinterpret_cast<volatile uint32_t *>(kDwtCyccntAddr);
}
#endif

#if defined(TIMER0)
#include "em_cmu.h"
#include "em_timer.h"
/***************************************************************************//**
 * Same pattern as sl_ml_profiler_session_metrics.cc: clock + init TIMER before
 * TIMER_CounterGet(). Override if TIMER0 is used elsewhere (e.g. use TIMER1).
 ******************************************************************************/
#ifndef SL_MODEL_PROFILER_TIMER_REF
#define SL_MODEL_PROFILER_TIMER_REF    TIMER0
#endif
#ifndef SL_MODEL_PROFILER_TIMER_CLOCK
#define SL_MODEL_PROFILER_TIMER_CLOCK  cmuClock_TIMER0
#endif

static void sli_model_profiler_hw_timer_ensure_running(void)
{
  static bool initialized = false;
  if (initialized) {
    return;
  }
  CMU_ClockEnable(SL_MODEL_PROFILER_TIMER_CLOCK, true);

  TIMER_Init_TypeDef init = TIMER_INIT_DEFAULT;
  TIMER_Init(SL_MODEL_PROFILER_TIMER_REF, &init);
  TIMER_TopSet(SL_MODEL_PROFILER_TIMER_REF, 0x00FFFFFFUL);
  TIMER_Enable(SL_MODEL_PROFILER_TIMER_REF, true);

  initialized = true;
}
#endif // TIMER0



namespace {

uint32_t cpu_freq;
#if defined(TIMER0)
uint32_t hw_timer_freq_hz = 0;
#endif

// Subtype of MicroInterpreter to access the TfLiteContext (for optional tensor harvest).
class SLMicroInterpreter : public tflite::MicroInterpreter {
public:
  using MicroInterpreter::MicroInterpreter; // Inherit constructors

  const TfLiteEvalTensor *GetEvalTensor(int tensor_id) const
  {
    const TfLiteContext &ctx = context();
    return ctx.GetEvalTensor(&ctx, tensor_id);
  }
};

const tflite::Model *model      = nullptr;
SLMicroInterpreter *interpreter = nullptr;

// Print uint32_t with thousand separators.
void sli_ui32_print_ts(uint32_t n)
{
  if (n < 1000) {
    printf("%" PRIu32, n);
    return;
  }
  sli_ui32_print_ts(n / 1000);
  printf(",%03" PRIu32, n % 1000U);
}

void sli_print_ui32(const char *lead_str, uint32_t n, const char *lag_str)
{
  printf("%s", lead_str);
  sli_ui32_print_ts(n);
  printf("%s", lag_str);
}

/** @brief Print duration from sleeptimer tick count. */
static void sli_print_wall_time_total(const char *str, uint64_t wall_ticks)
{
  uint32_t hz = sl_sleeptimer_get_timer_frequency();
  if (hz == 0U) {
    printf("%s(n/a)\n", str);
    return;
  }
  double duration = (double)wall_ticks / (double)hz;
  if (duration > 1.0) {
    printf("%s%.02f s\n", str, duration);
  } else if (duration > 0.001) {
    printf("%s%.02f ms\n", str, duration * 1000.0);
  } else if (duration > 0.000001) {
    printf("%s%.02f us\n", str, duration * 1000000.0);
  } else {
    printf("%s0.00 s\n", str);
  }
}

#if defined(SL_PRINT_MATRIX_DATA)
#include "sl_nn_util.h"
/***************************************************************************//**
 *
 *  Memory layout of 3x3x3 (HEIGHT x WIDTH x DEPTH) matrix.
 *
 ******************************************************************************/
void print_matrix_3d_u8(int height, int width, int depth, const uint8_t *data)
{
  uint8_t val;

#if defined(SL_PRINT_MATRIX_DATA_HUMAN_READABLE)
  for (int ch = 0; ch < depth; ch++) {
    printf("//   Channel = %d,  Y = 0..%d,  X = 0..%d ->\n", ch, height - 1, width - 1);
    for (int y = 0; y < height; y++) {
      for (int x = 0; x < width; x++) {
        int index = sli_nn_calc_offset_nhwc(height, width, depth, 0, x, y, ch);
        val = data[index];
        printf("0x%02X, ", val);
      }
      printf("\n");
    }
  }
#else
  for (int y = 0; y < height; y++) {
    printf("//   Y = %d,  X = 0..%d,  Channel = 0..%d ->\n", y, width - 1, depth - 1);
    for (int x = 0; x < width; x++) {
      for (int ch = 0; ch < depth; ch++) {
        int index = sli_nn_calc_offset_nhwc(height, width, depth, 0, x, y, ch);
        val = data[index];
        printf("0x%02X, ", val);
      }
      printf("\n");
    }
  }
#endif
}

void harvest_input_matrix(int operation_index)
{
  const tflite::Operator *op = model->subgraphs()->Get(0)->operators()->Get(operation_index);
  int tensor_index = op->inputs()->Get(0);
  const TfLiteEvalTensor *tensor = interpreter->GetEvalTensor(tensor_index);

  if ((tensor->type == kTfLiteInt8) && (tensor->dims->size == 4)) {
    const int8_t *in_data = tensor->data.int8;
    int height = tensor->dims->data[1];
    int width  = tensor->dims->data[2];
    int depth  = tensor->dims->data[3];
    printf("Input tensor dimensions: %dx%dx%dx%d\n", tensor->dims->data[0],
           height, width, depth);
    print_matrix_3d_u8(height, width, depth, reinterpret_cast<const uint8_t *>(in_data));
  }
}

void harvest_output_matrix(int operation_index)
{
  const tflite::Operator *op = model->subgraphs()->Get(0)->operators()->Get(operation_index);
  int tensor_index = op->outputs()->Get(0);
  const TfLiteEvalTensor *tensor = interpreter->GetEvalTensor(tensor_index);

  if ((tensor->type == kTfLiteInt8) && (tensor->dims->size == 4)) {
    const int8_t *out_data = tensor->data.int8;
    int height = tensor->dims->data[1];
    int width  = tensor->dims->data[2];
    int depth  = tensor->dims->data[3];
    printf("Output tensor dimensions: %dx%dx%dx%d\n", tensor->dims->data[0],
           height, width, depth);
    print_matrix_3d_u8(height, width, depth, reinterpret_cast<const uint8_t *>(out_data));
  }
}
#endif

//  interpreter construction
class ProfilerTfliteMicroModel : public npu_toolkit::TfliteMicroModel {
public:
  explicit ProfilerTfliteMicroModel(tflite::MicroProfilerInterface *profiler) : profiler_(profiler) {}

  tflite::MicroInterpreter *create_interpreter() override
  {
    return new (_interpreter_buffer) SLMicroInterpreter(
      flatbuffer_model(),
      *ops_resolver(),
      allocator(),
      nullptr,
      profiler_);
  }

private:
  tflite::MicroProfilerInterface *profiler_;
};

// Subclass tflite::MicroProfiler to get Invoke() hooks from model layers.
class SlProfiler : public tflite::MicroProfilerInterface {
public:
  SlProfiler() : cpu_cycles(0), total_cpu_cycles_(0), total_mvp_instructions_(0), total_mvp_stall_cycles_(0), operation_index_(0),
    wall_tick_start_(0U), total_wall_ticks_(0U)
  {
  }

  ~SlProfiler() override = default;

  /** Clear session totals (e.g. between energy-profiling inference passes). */
  void reset_metrics()
  {
    cpu_cycles = 0;
    total_cpu_cycles_ = 0;
    total_mvp_instructions_ = 0;
    total_mvp_stall_cycles_ = 0;
    operation_index_ = 0;
    wall_tick_start_ = 0;
    total_wall_ticks_ = 0;
  }

  uint32_t BeginEvent(const char *tag) override
  {
    printf("--------------------------------------------\n");
    printf("%d: %s\n", operation_index_, tag);
#if defined(SL_PRINT_MATRIX_DATA)
    harvest_input_matrix(operation_index_);
#endif

    sli_ml_mvp_perfcnt_reset_all();
    sli_ml_mvp_progcnt_reset();

    wall_tick_start_ = sl_sleeptimer_get_tick_count64();
#if defined(__arm__)
    cpu_cycles = dwt_read_cyccnt_portable();
#else
    cpu_cycles = 0;
#endif
    return 0;
  }

  void EndEvent(uint32_t event_handle) override
  {
    uint32_t mvp_instructions;
    uint32_t mvp_stall_cycles;

    (void)event_handle;
#if defined(__arm__)
    cpu_cycles = dwt_read_cyccnt_portable() - cpu_cycles;
#else
    cpu_cycles = 0;
#endif
    total_cpu_cycles_ += cpu_cycles;
    {
      uint64_t wall_end = sl_sleeptimer_get_tick_count64();
      uint64_t wd = wall_end - wall_tick_start_;
      total_wall_ticks_ += wd;
      sli_print_wall_time_total("Execution time:   ", wd);
    }
    sli_print_ui32("CPU cycle count:   ", cpu_cycles, "\n");

    mvp_instructions = sli_ml_mvp_perfcnt_get(0);
    if (mvp_instructions > 0) {
      mvp_stall_cycles = sli_ml_mvp_perfcnt_get(1);

      total_mvp_instructions_ += mvp_instructions;
      total_mvp_stall_cycles_ += mvp_stall_cycles;

      sli_print_ui32("MVP instructions:  ", mvp_instructions, "\n");
      sli_print_ui32("MVP stall cycles:  ", mvp_stall_cycles, "\n");
    }

#if defined(SL_PRINT_MATRIX_DATA)
    harvest_output_matrix(operation_index_);
#endif
    operation_index_++;
  }

  uint32_t total_cpu_cycles(void)
  {
    return total_cpu_cycles_;
  }
  uint32_t total_mvp_instructions(void)
  {
    return total_mvp_instructions_;
  }
  uint32_t total_mvp_stall_cycles(void)
  {
    return total_mvp_stall_cycles_;
  }
  uint32_t operation_index(void)
  {
    return operation_index_;
  }

  uint64_t total_wall_ticks(void)
  {
    return total_wall_ticks_;
  }

private:
  uint32_t cpu_cycles;
  uint32_t total_cpu_cycles_;
  uint32_t total_mvp_instructions_;
  uint32_t total_mvp_stall_cycles_;
  int operation_index_;
  uint64_t wall_tick_start_;
  uint64_t total_wall_ticks_;
  TF_LITE_REMOVE_VIRTUAL_DELETE
};

SlProfiler profiler;
static ProfilerTfliteMicroModel g_profiler_model(&profiler);

}  // namespace

// Initialize a TFLM model.
bool model_profiler_init(void)
{
  printf("\n--------------------------------------------\n");
  printf("Model profiler.\n");

  sl_sleeptimer_init();

  ml_configure_clocks_to_max_rate();
  cpu_freq = ml_get_cpu_clock_frequency();
  printf("CPU core frequency: %.1f MHz\n", (float)cpu_freq / 1000000.0);
  printf("Sleeptimer frequency: %lu Hz\n",
         (unsigned long)sl_sleeptimer_get_timer_frequency());

#if defined(TIMER0)
  sli_model_profiler_hw_timer_ensure_running();
  hw_timer_freq_hz = CMU_ClockFreqGet(SL_MODEL_PROFILER_TIMER_CLOCK);
  printf("HW TIMER frequency: %.1f MHz (TIMER_CounterGet for op timing)\n",
         (float)hw_timer_freq_hz / 1000000.0f);
#endif

  printf("--------------------------------------------\n");
  printf("Loading model.\n");

  model = tflite::GetModel(sl_ml_soc_model_profiler_model_handle.flatbuffer);
  if (model->version() != TFLITE_SCHEMA_VERSION) {
    printf("Model provided is schema version %" PRIu32 ", only version "
           "%d is supported.",
           model->version(), TFLITE_SCHEMA_VERSION);
    return false;
  }
  if (model->description() != nullptr) {
    const flatbuffers::String *desc = model->description();
    printf("%s\n", desc->c_str());
  }

  size_t arena_total = 0;
  for (int32_t i = 0; i < sl_ml_soc_model_profiler_model_handle.buffer_count; ++i) {
    arena_total += static_cast<size_t>(sl_ml_soc_model_profiler_model_handle.buffer_sizes[i]);
  }
  sli_print_ui32("\nTFLM model tensor buffer total: ", static_cast<uint32_t>(arena_total), "\n");

  ::npu_toolkit::register_tflite_micro_accelerator();
  if (!g_profiler_model.load(sl_ml_soc_model_profiler_model_handle.flatbuffer,
                             sl_ml_soc_model_profiler_model_handle.opcode_resolver,
                             sl_ml_soc_model_profiler_model_handle.buffers,
                             sl_ml_soc_model_profiler_model_handle.buffer_sizes,
                             sl_ml_soc_model_profiler_model_handle.buffer_count)) {
    printf("Could not load model (TfliteMicroModel::load failed)!\n");
    return false;
  }

  interpreter = static_cast<SLMicroInterpreter *>(g_profiler_model.interpreter());

  printf("\n--------------------------------------------\n");
  printf("Starting model profiler.\n");

#if defined(__arm__)
  dwt_enable_cyccnt_portable();
#endif

  ENERGY_PROFILING_INIT();

  return true;
}

// Run one inference on the TFLM model.
void model_profiler_process_action(void)
{
  TfLiteTensor *input_tensor = interpreter->input(0);
  if (input_tensor == nullptr || input_tensor->data.data == nullptr) {
    printf("No input tensor\n");
    return;
  }

  bool invoke_ok = true;
  for (int i = 0; i < 2; ++i) {
    profiler.reset_metrics();
    memset(input_tensor->data.data, 0, input_tensor->bytes);

    if (interpreter->Invoke() != kTfLiteOk) {
      printf("Model invocation failed !\n");
      invoke_ok = false;
      break;
    }

    ENERGY_PROFILING_INFERENCE_DELAY();
  }

  if (!invoke_ok) {
    return;
  }

  printf("--------------------------------------------\n");
  printf("--------------------------------------------\n");
  printf("Profiling summary:\n");
  sli_print_wall_time_total("Total execution time (all ops): ", profiler.total_wall_ticks());
  sli_print_ui32("Total CPU cycle count:  ", profiler.total_cpu_cycles(), "\n");

  auto total_mvp_instructions = profiler.total_mvp_instructions();
  if (total_mvp_instructions > 0) {
    sli_print_ui32("Total MVP instructions: ", total_mvp_instructions, "\n");
    sli_print_ui32("Total MVP stall cycles: ", profiler.total_mvp_stall_cycles(), "\n");
  }
  printf("--------------------------------------------\n");
  printf("--------------------------------------------\n");

  printf("Model profiling done.\n");
}
