/***************************************************************************//**
 * @file sl_ml_profiler_helper.cc
 * @brief Module with helper functions and objects for ml profiler.
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
#include "sl_ml_profiler_helper.h"
#if defined(SL_BOARD_SI91X)
#include "sl_ml_profiler_interpreter.h"
#include "sl_ml_profiler_config.h"

#if (SL_ML_ENABLE_DYNAMIC_MODEL_LOAD == 1 )

#define CUSTOM_OP_RESOLVER_EN (0)

#if (CUSTOM_OP_RESOLVER_EN)
#include "sl_tflite_micro_all_opcode_resolver.h"
#else
#include "ml/third_party/tflm/all_ops_resolver.h"
#endif

uint8_t PrimaryTensorArena_buffer_[PRIMARY_TENSOR_ARENA_SIZE_SI91X] __attribute__((section(".bss"))) __attribute__((aligned(4)));
uint8_t ModelCache_buffer_[MODEL_CACHE_SIZE_SI91X] __attribute__((section(".ml_model_cache_buffer"))) __attribute__((aligned(4)));
const int32_t model_buffer_sizes_[2] = { PRIMARY_TENSOR_ARENA_SIZE_SI91X, MODEL_CACHE_SIZE_SI91X };
uint8_t* model_buffers_[2] = { PrimaryTensorArena_buffer_, ModelCache_buffer_ };

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
  tflite::AllOpsResolver opcode_resolver;
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
  return model.load(
    flatbuffer_ptr,
    &opcode_resolver,
    model_buffers_,
    model_buffer_sizes_,
    model_buffer_count_
  );
}

#endif // SL_ML_ENABLE_DYNAMIC_MODEL_LOAD


bool SLTfliteMicroModel::load_interpreter(
	const void* flatbuffer,
	const tflite::MicroOpResolver* op_resolver,
	uint8_t* buffers[],
	const int32_t buffer_sizes[],
	int32_t n_buffers
	)
	{
	TfliteMicroModelContext* model_context = nullptr;
	auto tflite_model = tflite::GetModel(flatbuffer);

	_ops_resolver = op_resolver;
	_flatbuffer = flatbuffer;

	// Register the accelerator that was built with this application
	_accelerator = get_registered_tflite_micro_accelerator();
	if(_accelerator == nullptr)
	{
		NPU_TOOLKIT_ISSUE_MODEL_FATAL_ERROR_(_status, "No accelerator registered");
		return false;
	}

	#ifdef TFLITE_MICRO_RECORDER_ENABLED
	// (Re)initialize the recorder if necessary
	if(_recorder != nullptr)
	{
		_recorder->init();
	}
	#endif

	/*#ifdef TFLITE_MICRO_PROFILER_ENABLED
	if(_profiler != nullptr)
	{
		_profiler->reset();
	}
	#endif*/


	// Initialize the accelerator
	if(!_accelerator->init())
	{
		NPU_TOOLKIT_ISSUE_MODEL_FATAL_ERROR_(_status, "Failed to initialize the accelerator");
		_accelerator = nullptr;
		return false;
	}

	// Create the buffer allocator
	if(!create_allocator(
		flatbuffer,
		buffers,
		buffer_sizes,
		n_buffers
	))
	{
		NPU_TOOLKIT_ISSUE_MODEL_WARNING_(_status, "Failed to create allocator");
		unload();
		return false;
	}

	// Create the MicroInterpreter instance
	_interpreter = new(_interpreter_buffer)tflite::MicroInterpreter(
		tflite_model,
		*op_resolver,
		_allocator,
		nullptr,
		_custom_profiler
	);

	// Set the profiler's interpreter and model pointers
	//_custom_profiler->interpreter_profiler = &_interpreter;
	_custom_profiler->model_profiler = tflite_model;
	_custom_profiler->interpreter_profiler = (SLMicroInterpreter* )_interpreter;

	auto context = &_interpreter->context_;
	TfliteMicroModelContextManager context_manager(context);

	_status.set_op_error_reporter(context);

	model_context = _accelerator->create_context(context);
	if(model_context == nullptr)
	{
		NPU_TOOLKIT_ISSUE_MODEL_WARNING_(_status, "Failed to create TfliteMicroModelContext");
		unload();
		return false;
	}

	// Initialize the model context
	if(!model_context->init(this))
	{
		NPU_TOOLKIT_ISSUE_MODEL_WARNING_(_status, "Failed to init model context");
		unload();
		return false;
	}

	_n_buffers = n_buffers;
	_buffer_sizes = (int32_t*)_allocator->AllocatePersistentBuffer(sizeof(int32_t)*n_buffers);
	for(int i = 0; i < n_buffers; ++i)
	{
		_buffer_sizes[i] = buffer_sizes[i];
	}

	// Allocate the model
	bool retval = true;
	if(_interpreter->AllocateTensors() == kTfLiteOk)
	{
		if(!model_context->load())
		{
		NPU_TOOLKIT_ISSUE_MODEL_WARNING_(_status, "Failed to load model context");
		unload();
		return false;
		}
	}
	else
	{
		unload();
		retval = false;
	}

	return retval;
	}
#endif
