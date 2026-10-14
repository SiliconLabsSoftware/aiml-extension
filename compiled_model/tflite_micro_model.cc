/*******************************************************************************
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

#include "tflite_micro_model_config.h"
#include "sl_common.h"
#include "tensorflow/lite/micro/version.h"
#include "tflite_micro_model.hpp"
#include "tflite_micro_model_helper.hpp"
#include "tflite_micro_model_context.hpp"
#include "tflite_micro_compiled_allocator.hpp"
#include "compiled_model.hpp"


namespace npu_toolkit
{

TfliteMicroModel::~TfliteMicroModel()
{
  unload();
}


bool TfliteMicroModel::load(
  const void* flatbuffer,
  const tflite::MicroOpResolver& op_resolver,
  uint8_t *runtime_buffer,
  int32_t runtime_buffer_size
)
{
  if(runtime_buffer == nullptr || runtime_buffer_size <= 0)
  {
    NPU_TOOLKIT_ERROR("Null runtime buffer argument");
    return false;
  }

  uint8_t* local_buffers[1] = {runtime_buffer};
  const int32_t local_buffer_sizes[1] = {runtime_buffer_size};

  return load(flatbuffer, &op_resolver, local_buffers, local_buffer_sizes, 1);
}

bool TfliteMicroModel::load(
  const void* flatbuffer,
  const tflite::MicroOpResolver* op_resolver,
  uint8_t* const buffers[],
  const int32_t buffer_sizes[],
  int32_t n_buffers
)
{
  constexpr int MAX_BUFFERS = 8;
  if(flatbuffer == nullptr)
  {
      NPU_TOOLKIT_ERROR("Null flatbuffer argument");
      return false;
  }
  if(n_buffers <= 0 || n_buffers > MAX_BUFFERS || buffers == nullptr || buffer_sizes == nullptr)
  {
      NPU_TOOLKIT_ERROR("Invalid buffer argument");
      return false;
  }

  uint8_t* local_buffers[MAX_BUFFERS];
  int32_t local_buffer_sizes[MAX_BUFFERS];

  for(int i = 0; i < n_buffers; ++i)
  {
    if(buffers[i] == nullptr || buffer_sizes[i] <= 0)
    {
      NPU_TOOLKIT_ERROR("Null buffer argument at index %d", i);
      return false;
    }

    local_buffers[i] = buffers[i];
    local_buffer_sizes[i] = buffer_sizes[i];
  }

  // Load any parameters added to the flatbuffer metadata
  TfliteModelParameters::load_from_tflite_flatbuffer(flatbuffer, parameters);

  // Load the TFLM interpreter
  if(!load_interpreter(
      flatbuffer,
      op_resolver,
      local_buffers,
      local_buffer_sizes,
      n_buffers
  ))
  {
    NPU_TOOLKIT_ERROR("Failed to load model with runtime buffer size: %d", local_buffer_sizes[0]);
    unload();
    return false;
  }

  return true;
}

void TfliteMicroModel::unload()
{
  _flatbuffer = nullptr;
  _ops_resolver = nullptr;
  _allocator = nullptr;
  _buffer_sizes = nullptr;
  _n_buffers = 0;
  parameters.unload();

  if(_accelerator != nullptr)
  {
    _accelerator->deinit(tflite_context());
    _accelerator = nullptr;
  }

  auto tflite_context = this->tflite_context();
  if(tflite_context != nullptr)
  {
    auto model_context = TfliteMicroModelHelper::tflite_micro_model_context(tflite_context);
    if(model_context != nullptr)
    {
      model_context->deinit();
    }
  }

  if(_interpreter != nullptr)
  {
    _interpreter->tflite::MicroInterpreter::~MicroInterpreter();
    _interpreter = nullptr;
  }
}


bool TfliteMicroModel::reset()
{
  if(!is_loaded())
  {
    return false;
  }

  return _interpreter->Reset() == kTfLiteOk;
}

bool TfliteMicroModel::invoke()
{
  bool retval;
  TfliteMicroModelContextManager context_manager(tflite_context());
  auto accelerator = get_registered_tflite_micro_accelerator();

  if(!is_loaded())
  {
    NPU_TOOLKIT_ERROR("Model not loaded");
    return false;
  }

  retval = (_interpreter->Invoke() == kTfLiteOk);

  return retval;
}


unsigned TfliteMicroModel::n_inputs() const
{
  if(!is_loaded())
  {
    NPU_TOOLKIT_ERROR("Model not loaded");
    return false;
  }

  return _interpreter->inputs_size();
}

TfliteTensorView* TfliteMicroModel::input(unsigned index) const
{
  if(!is_loaded())
  {
    NPU_TOOLKIT_ERROR("Model not loaded");
    return nullptr;
  }

  return reinterpret_cast<TfliteTensorView*>(_interpreter->input(index));
}

unsigned TfliteMicroModel::n_outputs() const
{
  if(!is_loaded())
  {
    NPU_TOOLKIT_ERROR("Model not loaded");
    return false;
  }

  return _interpreter->outputs_size();
}

TfliteTensorView* TfliteMicroModel::output(unsigned index) const
{
  if(!is_loaded())
  {
    NPU_TOOLKIT_ERROR("Model not loaded");
    return nullptr;
  }

  return reinterpret_cast<TfliteTensorView*>(_interpreter->output(index));
}


const void* TfliteMicroModel::flatbuffer() const
{
  return _flatbuffer;
}

const tflite::Model* TfliteMicroModel::flatbuffer_model() const
{
  assert(_flatbuffer != nullptr);
  return (_flatbuffer != nullptr) ? tflite::GetModel(_flatbuffer) : nullptr;
}

tflite::MicroInterpreter* TfliteMicroModel::interpreter() const
{
  assert(_interpreter != nullptr);
  return _interpreter;
}

const tflite::MicroOpResolver* TfliteMicroModel::ops_resolver() const
{
  assert(_ops_resolver != nullptr);
  return _ops_resolver;
}

TfLiteContext* TfliteMicroModel::tflite_context() const
{
  return (_interpreter != nullptr) ? &_interpreter->context_ : nullptr;
}

TfliteMicroAccelerator* TfliteMicroModel::accelerator(bool allow_null) const
{
  assert(allow_null || _accelerator != nullptr);
  return _accelerator;
}

tflite::MicroAllocator* TfliteMicroModel::allocator(bool allow_null) const
{
  assert(allow_null || _allocator != nullptr);
  return _allocator;
}


void TfliteMicroModel::set_layer_callback(
  TfliteMicroInvokeLayerCallback callback,
  void* arg
)
{
  _layer_callback = callback;
  _layer_callback_arg = arg;
}


int TfliteMicroModel::n_buffers() const
{
  return _n_buffers;
}

bool TfliteMicroModel::buffer_info(int index, uintptr_t* ptr, int32_t* size) const
{
  if(index < 0 || index >= _n_buffers)
  {
    return false;
  }
  if(ptr != nullptr)
  {
    *ptr = _buffer_ptrs[index];
  }
  if(size != nullptr)
  {
    *size = _buffer_sizes[index];
  }
  return true;
}

bool TfliteMicroModel::is_loaded() const
{
  return _interpreter != nullptr;
}


const void* TfliteMicroModel::find_metadata(const char* tag, uint32_t* length) const
{
  return TfliteMicroModelHelper::get_metadata_from_tflite_flatbuffer(
    _flatbuffer,
    tag,
    length
  );
}

const char* TfliteMicroModel::tflite_micro_version()
{
  return TFLITE_MICRO_VERSION;
}


bool TfliteMicroModel::verify_flatbuffer(const void* flatbuffer, int flatbuffer_len)
{
  if(flatbuffer == nullptr || flatbuffer_len <= 0)
  {
    return false;
  }

  ::flatbuffers::Verifier verifier(reinterpret_cast<const uint8_t*>(flatbuffer), flatbuffer_len);
  return tflite::VerifyModelBuffer(verifier);
}




bool TfliteMicroModel::load_interpreter(
  const void* flatbuffer,
  const tflite::MicroOpResolver* op_resolver,
  uint8_t* buffers[],
  const int32_t buffer_sizes[],
  int32_t n_buffers
)
{
  TfliteMicroModelContext* model_context = nullptr;

  _ops_resolver = op_resolver;
  _flatbuffer = flatbuffer;

  // Register the accelerator that was built with this application
  _accelerator = get_registered_tflite_micro_accelerator();
  if(_accelerator == nullptr)
  {
    NPU_TOOLKIT_ERROR("No accelerator registered");
    return false;
  }

  // Initialize the accelerator
  if(!_accelerator->init(flatbuffer))
  {
    NPU_TOOLKIT_ERROR("Failed to initialize the accelerator");
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
    NPU_TOOLKIT_ERROR("Failed to create allocator");
    unload();
    return false;
  }

  // Store the buffer info in this TfliteMicroModel instance
  _n_buffers = n_buffers;
  _buffer_ptrs = (uintptr_t*)_allocator->AllocatePersistentBuffer((sizeof(int32_t) + sizeof(uintptr_t))*n_buffers);
  _buffer_sizes = (int32_t*)(_buffer_ptrs + n_buffers);
  for(int i = 0; i < n_buffers; ++i)
  {
    _buffer_ptrs[i] = (uintptr_t)buffers[i];
    _buffer_sizes[i] = buffer_sizes[i];
  }

  // Create the MicroInterpreter instance
  _interpreter = create_interpreter();
  auto context = &_interpreter->context_;
  TfliteMicroModelContextManager context_manager(context);


  // Create an accelerator-specific context
  model_context = _accelerator->create_context(context);
  if(model_context == nullptr)
  {
    NPU_TOOLKIT_ERROR("Failed to create TfliteMicroModelContext");
    unload();
    return false;
  }

  // Initialize the model context
  if(!model_context->init(this))
  {
    NPU_TOOLKIT_ERROR("Failed to init model context");
    unload();
    return false;
  }

  // Allocate the model and all its tensors
  bool retval = true;
  if(_interpreter->AllocateTensors() == kTfLiteOk)
  {
    if(!model_context->load())
    {
      NPU_TOOLKIT_ERROR("Failed to load model context");
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

tflite::MicroInterpreter* TfliteMicroModel::create_interpreter()
{
  return new(_interpreter_buffer)tflite::MicroInterpreter(
    flatbuffer_model(),
    *ops_resolver(),
    allocator()
  );
}

bool TfliteMicroModel::create_allocator(
  const void* flatbuffer,
  uint8_t* const buffers[],
  const int32_t buffer_sizes[],
  int32_t n_buffers
)
{
  // Attempt to retrieve a compiled memory plan in the metadata of the given .tflite
  auto memory_plan = TfliteMicroCompiledAllocator::retrieve_memory_plan(flatbuffer);

  // If a compiled memory plan is available
  // then create the allocator instance
  if(memory_plan != nullptr)
  {
    // Ensure this application's source code is compatible
    // with the ML compiler used to compile the given .tflite flatbuffer
    if(!compiled_model_validate_compiler_version(flatbuffer, _accelerator->name(), true))
    {
      return false;
    }

    auto compiled_allocator = TfliteMicroCompiledAllocator::create(
      memory_plan,
      buffers,
      buffer_sizes,
      n_buffers
    );

    if(compiled_allocator == nullptr)
    {
      return false;
    }

    _allocator = reinterpret_cast<tflite::MicroAllocator*>(compiled_allocator);
    return true;
  }

  // No compiled memory plan is available
  // Then try to create the default allocate
  //which typically uses the TFLM online memory planner
  _allocator = tflite_micro_create_default_allocator(buffers[0], buffer_sizes[0]);

  // If create_default_allocator() returns null,
  // then the generated model header likely override TfliteMicroModel::create_default_allocator()
  // to return null. This is done to force offline memory planning.
  // This is useful, as it removes the code required to do online memory planning
  // which can consume ~3k of code space.

  if(_allocator == nullptr)
  {
    NPU_TOOLKIT_ERROR("Offline memory planning is required for this build");
    return false;
  }

  return true;
}

SL_WEAK tflite::MicroAllocator* tflite_micro_create_default_allocator(
  uint8_t* tensor_arena,
  int32_t tensor_arena_size
)
{
  // Use the default MicroAllocator
  // which uses runtime memory planning
  return tflite::MicroAllocator::Create(
    tensor_arena,
    tensor_arena_size
  );
}

} // namespace npu_toolkit