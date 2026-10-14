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
#pragma once
#include <cstdint>


#include "tensorflow/lite/micro/micro_interpreter.h"
#include "tensorflow/lite/micro/micro_op_resolver.h"

#include "tflite_micro_logger.hpp"
#include "tflite_model_parameters.hpp"
#include "tflite_micro_tensor.hpp"
#include "tflite_micro_accelerator.hpp"
#include "tflite_micro_model_context.hpp"



namespace npu_toolkit
{

/**
 * @defgroup tflite_micro_model TF-Lite Micro Model
 * @{
 */
class TfliteMicroModel
{
public:
  TfliteModelParameters parameters;

  /**
   * Default constructor
   */
  TfliteMicroModel() = default;

  /**
   * Cleanup any allocated data
   */
  ~TfliteMicroModel();

    /**
   * @brief Load model flatbuffer
   *
   * Load a model flatbuffer (.tflite).
   *
   * @note The provided `flatbuffer` and `runtime_buffer`
   * must persist for the life of this model object.
   *
   * @param flatbuffer Model flatbuffer (.tflite) binary data
   * @param op_resolver @ref tflite::MicroOpResolver with reigstered kernels
   * @param runtime_buffer Buffer to hold model working memory
   * @param runtime_buffer_size Size of the given runtime_buffer in bytes
   * @return true if model successfully loaded, false else
   */
  bool load(
    const void* flatbuffer,
    const tflite::MicroOpResolver& op_resolver,
    uint8_t *runtime_buffer,
    int32_t runtime_buffer_size
  );

  /**
   * @brief Load model with multiple runtime memory buffers
   *
   * Load a model flatbuffer (.tflite) with multiple runtime memory buffers.
   * The model must be pre-compiled to leverage the given memory buffers.
   *
   * @note The provided `flatbuffer` and `buffers` must persist for the life of this model object.
   *
   * @param flatbuffer Model flatbuffer (.tflite) binary data
   * @param op_resolver @ref tflite::MicroOpResolver with reigstered kernels
   * @param buffers List of buffers to hold the model's runtime buffers
   * @param buffer_sizes Size of each buffer
   * @param n_buffers Number of buffer provided
   * @return true if model successfully loaded, false else
   */
  bool load(
    const void* flatbuffer,
    const tflite::MicroOpResolver* op_resolver,
    uint8_t* const buffers[],
    const int32_t buffer_sizes[],
    int32_t n_buffers
  );

  /**
   * @brief Unload model
   *
   * Unload model and clean up and allocated resources
   */
  void unload();

  /**
   * @brief Return if a model is loaded
   *
   * @return Return true if a model was successfully loaded, false otherwise
   */
  bool is_loaded() const;

  /**
   * Reset the state to be what you would expect when the interpreter is first
  * created. i.e. after Init and Prepare is called for the very first time.
  */
  bool reset();

  /**
   * @brief Invoke model inference
   *
   * Execute the loaded model
   *
   * @return true if model executed successfully, false else
   */
  bool invoke();


  /**
   * @brief Return number of input tensors
   *
   * @return The number of model input tensors
   */
  unsigned n_inputs() const;

  /**
   * @brief Get input tensor
   *
   * Populate the provided @ref TfliteTensorView with the
   * details of the input tensor at the given index.
   *
   * @param index Optional, index of input tensor
   * @return @ref TfliteTensorView to populate with input tensor at `index`
   */
  TfliteTensorView* input(unsigned index = 0) const;

  /**
   * @brief Return number of output tensors
   *
   * @return The number of model output tensors
   */
  unsigned n_outputs() const;

  /**
   * @brief Get output tensor
   *
   * Populate the provided @ref TfliteTensorView with the
   * details of the output tensor at the given index.
   *
   * @param index Optional, index of output tensor
   * @return @ref TfliteTensorView to populate with output tensor at `index`
   */
  TfliteTensorView* output(unsigned index = 0) const;

  /**
   * @brief Find metadata with tag in flatbuffer
   *
   * Find metadata with the given `tag` in the model's flatubffer
   *
   * @param tag Tag to search for in flatbuffer's metadata
   * @param length Optional, pointer to hold length of metadata's binary data
   * @return Pointer to found metadata's buffer in flatbuffer, null if not found
   */
  const void* find_metadata(const char* tag, uint32_t* length = nullptr) const;

  /**
   * Return a pointer to .tflite flatbuffer loaded by the model
   */
  const void* flatbuffer() const;

  /**
   * Return a reference to the flatbuffer Model object
   */
  const tflite::Model* flatbuffer_model() const;

  /**
   * Return a pointer to the TfliteMicroInterpreter
   * used by the model
   * NOTE: The model must be loaded for this to return a valid pointer, otherwise it will return nullptr.
   */
  tflite::MicroInterpreter* interpreter() const;
  /**
   * Return a pointer to the tflite::MicroOpResolver
   * used by the model
   */
  const tflite::MicroOpResolver* ops_resolver() const;

  /**
   * Return a pointer to the TfliteContext of the MicroInterpreter instance
   * used by the model
   */
  TfLiteContext* tflite_context() const;

  /**
   * Return a pointer to the TfliteMicroAccelerator
   * used by the model.
   * NOTE: The model must be loaded for this to return a valid pointer, otherwise it will return nullptr.
   */
  TfliteMicroAccelerator* accelerator(bool allow_null=false) const;

  /**
   * Return a pointer to the tflite::MicroAllocator
   * used by the model.
   * NOTE: The model must be loaded for this to return a valid pointer, otherwise it will return nullptr.
   */
  tflite::MicroAllocator* allocator(bool allow_null=false) const;


  /**
   * Callback that is invoked AFTER the each layer of the model is invoked,
   * with information about the layer that was just executed.
   * This can be used for debugging, logging, or other purposes.
   *
   * @see set_layer_callback()
   */
  typedef TfLiteStatus (*TfliteMicroInvokeLayerCallback)(
    int index,
    const tflite::NodeAndRegistration& node_and_registration,
    TfLiteStatus status,
    void* arg
  );

  /**
   * Set a callback to be invoked AFTER the each layer of the model is invoked
  */
  void set_layer_callback(
    TfliteMicroInvokeLayerCallback callback,
    void* arg = nullptr
  );

  /**
   * Return the number of buffers used by the model
   *
   * @return The number of buffers used by the model
   */
  int n_buffers() const;

  /**
   * Return information about a buffer used by the model
   *
   * @param index Index of the buffer
   * @param ptr Pointer to hold the buffer's address
   * @param size Pointer to hold the buffer's size
   * @return true if buffer information was successfully retrieved, false otherwise
   */
  bool buffer_info(int index, uintptr_t* ptr, int32_t* size) const;


  /**
   * Return true if the given flatbuffer is correctly formatted,
   * False else
   */
  static bool verify_flatbuffer(const void* flatbuffer, int flatbuffer_len);

    /**
   * Return the GIT repository version
   * of Tensorflow-Lite Micro that this API uses.
   */
  static const char* tflite_micro_version();


protected:
  uint8_t _interpreter_buffer[sizeof(tflite::MicroInterpreter)];
  TfliteMicroAccelerator* _accelerator = nullptr;
  tflite::MicroAllocator* _allocator = nullptr;
  tflite::MicroInterpreter* _interpreter = nullptr;
  const tflite::MicroOpResolver* _ops_resolver = nullptr;
  const void* _flatbuffer = nullptr;
  int32_t* _buffer_sizes = nullptr;
  uintptr_t* _buffer_ptrs = nullptr;
  int8_t _n_buffers = 0;
  TfliteMicroInvokeLayerCallback _layer_callback = nullptr;
  void* _layer_callback_arg = nullptr;

  /**
   * Create a tflite::MicroInterpreter instance for the model.
   * This is virtual to allow for custom interpreter implementations.
   */
  virtual tflite::MicroInterpreter* create_interpreter();

  /**
   * Load the interpreter with the given flatbuffer and runtime buffer(s).
   * This is virtual to allow for custom loading implementations.
   */
  virtual bool load_interpreter(
    const void* flatbuffer,
    const tflite::MicroOpResolver* op_resolver,
    uint8_t* buffers[],
    const int32_t buffer_sizes[],
    int32_t n_buffers
  );

  /**
   * Create the micro allocator for the model.
   * This is virtual to allow for custom allocator implementations.
   */
  virtual bool create_allocator(
    const void* flatbuffer,
    uint8_t* const buffers[],
    const int32_t buffer_sizes[],
    int32_t n_buffers
  );

  friend tflite::MicroInterpreterGraph;
};


/**
 * Create the default MicroAllocator that comes with TFLM.
 * This does runtime memory planning.
 * If the compiled model uses the memory planner, then
 * the generated header will overide this implementation
 * and return null. This effectively removes the runtime
 * memory planner which can consume ~3kB of code space.
 */
tflite::MicroAllocator* tflite_micro_create_default_allocator(
  uint8_t* tensor_arena,
  int32_t tensor_arena_size
);

/**
 * @}
 */

} // namespace npu_toolkit