/***************************************************************************/ /**
 * @file sl_ml_tflite_micro_model.h
 * @brief Embedded TensorFlow Lite Micro model handle and lifecycle API (init, run, deinit).
 *
 * Declared by the \c machine_learning component. Implementation:
 * ``src/tflite/sl_ml_tflite_micro_model.cc``.
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

#include <cstddef>
#include "sl_status.h"

#if defined(SL_ML_ENABLE_SILABS_PROFILER) && !defined(SL_ML_MODEL_PROFILER)
#include "sl_ml_silabs_profiler.h"
#else
#include "tflite_micro_model.hpp"
#endif

/***************************************************************************//**
 * @addtogroup ml_tflite_micro_model
 * @{
 ******************************************************************************/

/**
 * @struct sl_ml_model_handle_t
 * @details
 * ### Model Handle Structure
 *
 * Runtime context for one TensorFlow Lite Micro model on an embedded target. Instantiate one
 * handle per deployed model.
 *
 * The handle holds the serialized model flatbuffer (@c flatbuffer), operator resolver
 * (@c opcode_resolver), and tensor arena memory (@c buffers and @c buffer_sizes). The embedded
 * interpreter wrapper (@c TfliteMicroModel, or @c SLTfliteMicroModel when profiling is enabled) is
 * default-constructed at creation; call @ref sl_ml_model_init before running inference.
 */
struct sl_ml_model_handle_t {
#if defined(SL_ML_ENABLE_SILABS_PROFILER) && !defined(SL_ML_MODEL_PROFILER)
  /** Profiler instance used for collecting runtime performance metrics. */
  sl::ml::SilabsProfiler profiler;

  /** Model instance with profiler support, wired to @c profiler above. */
  SLTfliteMicroModel model;
#else
  /** Embedded TFLM interpreter; default-constructed until @ref sl_ml_model_init succeeds. */
  npu_toolkit::TfliteMicroModel model;
#endif

  /** Pointer to the serialized model flatbuffer. */
  const uint8_t* flatbuffer;

  /** Length of @c flatbuffer in bytes. */
  int flatbuffer_length;

  /** Operator resolver that registers kernels required by the model. */
  const tflite::MicroOpResolver* opcode_resolver;

  /** Pointers to tensor arena buffers; parallel to @c buffer_sizes. */
  uint8_t** buffers;

  /** Size in bytes of each buffer in @c buffers. */
  const int32_t* buffer_sizes;

  /** Number of entries in @c buffers and @c buffer_sizes. */
  int32_t buffer_count;

  /** Byte alignment required for each tensor arena buffer (typically 4). */
  int32_t model_buffer_alignment;

  /** Linker section name for each buffer slot; parallel to @c buffers. May be NULL. */
  const char** model_buffer_sections;

  /**
   * @brief Return the input tensor at @p index.
   *
   * @details The model must be loaded successfully (@ref sl_ml_model_init).
   *
   * @param[in] index
   *    Zero-based input tensor index (0 to @c num_inputs - 1).
   *
   * @return
   *    Pointer to the input @c TfLiteTensor, or NULL if the model is not loaded, @p index is
   *    invalid, or the interpreter is unavailable.
   */
  TfLiteTensor* input_tensor(size_t index) const
  {
    if (!model.is_loaded()) {
      return nullptr;
    }
    tflite::MicroInterpreter* const ip = model.interpreter();
    return (ip != nullptr) ? ip->input_tensor(index) : nullptr;
  }

  /**
   * @brief Return the output tensor at @p index.
   *
   * @details The model must be loaded successfully (@ref sl_ml_model_init).
   *
   * @param[in] index
   *    Zero-based output tensor index (0 to @c num_outputs - 1).
   *
   * @return
   *    Pointer to the output @c TfLiteTensor, or NULL if the model is not loaded, @p index is
   *    invalid, or the interpreter is unavailable.
   */
  TfLiteTensor* output_tensor(size_t index) const
  {
    if (!model.is_loaded()) {
      return nullptr;
    }
    tflite::MicroInterpreter* const ip = model.interpreter();
    return (ip != nullptr) ? ip->output_tensor(index) : nullptr;
  }

  /**
   * @brief Initialize a model handle with flatbuffer, resolver, and tensor-buffer metadata.
   *
   * @details Default-constructs the embedded interpreter wrapper (and profiler when enabled).
   *    Call @ref sl_ml_model_init before @ref sl_ml_model_run.
   *
   * @param[in] flatbuffer_in
   *    Pointer to the serialized model flatbuffer; must not be NULL.
   * @param[in] flatbuffer_length_in
   *    Length of @p flatbuffer_in in bytes; must be greater than zero.
   * @param[in] opcode_resolver_in
   *    Operator resolver for kernels used by the model; must not be NULL.
   * @param[in] buffers_in
   *    Array of tensor arena buffers; must not be NULL.
   * @param[in] buffer_sizes_in
   *    Size in bytes of each buffer in @p buffers_in; must not be NULL.
   * @param[in] buffer_count_in
   *    Number of elements in @p buffers_in and @p buffer_sizes_in; must be greater than zero.
   * @param[in] model_buffer_alignment_in
   *    Required byte alignment for each tensor arena buffer (typically 4).
   * @param[in] model_buffer_sections_in
   *    Linker section name for each buffer slot; parallel to @p buffers_in. May be NULL.
   */
  sl_ml_model_handle_t(const uint8_t* flatbuffer_in,
                       int flatbuffer_length_in,
                       const tflite::MicroOpResolver* opcode_resolver_in,
                       uint8_t** buffers_in,
                       const int32_t* buffer_sizes_in,
                       int32_t buffer_count_in,
                       int32_t model_buffer_alignment_in,
                       const char** model_buffer_sections_in)
#if defined(SL_ML_ENABLE_SILABS_PROFILER) && !defined(SL_ML_MODEL_PROFILER)
    : profiler()
    , model(&profiler)
#else
    : model()
#endif
    , flatbuffer(flatbuffer_in)
    , flatbuffer_length(flatbuffer_length_in)
    , opcode_resolver(opcode_resolver_in)
    , buffers(buffers_in)
    , buffer_sizes(buffer_sizes_in)
    , buffer_count(buffer_count_in)
    , model_buffer_alignment(model_buffer_alignment_in)
    , model_buffer_sections(model_buffer_sections_in)
  {
  }
};

/***************************************************************************//**
 * @brief
 *    Load the model flatbuffer and initialize the interpreter for inference.
 *
 * @details
 *    If the model is already loaded, returns @ref SL_STATUS_OK without reloading the flatbuffer.
 *
 *    To reload the same @p handle, call @ref sl_ml_model_deinit first, then @ref sl_ml_model_init.
 *
 * @param[in] handle
 *    Pointer to an initialized handle structure; must not be NULL. @c flatbuffer,
 *    @c opcode_resolver, @c buffers, and @c buffer_sizes must also be set.
 *
 * @retval SL_STATUS_OK The model loaded successfully, or was already loaded.
 * @retval SL_STATUS_INVALID_PARAMETER @p handle, @c flatbuffer, @c opcode_resolver, @c buffers, or
 *    @c buffer_sizes is NULL, or @c flatbuffer_length or @c buffer_count is not positive.
 * @retval SL_STATUS_FAIL Interpreter setup or tensor allocation failed.
 *
 * @return
 *    @c sl_status_t indicating whether the model loaded successfully.
 ******************************************************************************/
sl_status_t sl_ml_model_init(sl_ml_model_handle_t* handle);

/***************************************************************************//**
 * @brief
 *    Run one inference pass on a loaded model.
 *
 * @param[in] handle
 *    Pointer to a loaded model handle; must not be NULL.
 *
 * @retval SL_STATUS_OK Inference completed successfully.
 * @retval SL_STATUS_INVALID_PARAMETER @p handle is NULL.
 * @retval SL_STATUS_NOT_INITIALIZED @ref sl_ml_model_init did not complete successfully, or
 *    @ref sl_ml_model_deinit has already been called.
 * @retval SL_STATUS_FAIL The TFLM @c invoke() call failed.
 *
 * @return
 *    @c sl_status_t indicating whether inference completed successfully.
 ******************************************************************************/
sl_status_t sl_ml_model_run(sl_ml_model_handle_t* handle);

/***************************************************************************//**
 * @brief
 *    Unload the model and release interpreter resources held by @p handle.
 *
 * @param[in] handle
 *    Pointer to the model handle to unload; must not be NULL.
 *
 * @retval SL_STATUS_OK The model was loaded and has been unloaded.
 * @retval SL_STATUS_NOT_INITIALIZED The model was not loaded; @c unload() is not invoked.
 * @retval SL_STATUS_INVALID_PARAMETER @p handle is NULL.
 *
 * @return
 *    @c sl_status_t indicating whether the model was unloaded successfully.
 ******************************************************************************/
sl_status_t sl_ml_model_deinit(sl_ml_model_handle_t* handle);

/***************************************************************************//**
 * @}
 ******************************************************************************/
