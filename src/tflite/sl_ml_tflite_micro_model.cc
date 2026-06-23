/***************************************************************************/ /**
 * @file sl_ml_tflite_micro_model.cc
 * @brief Implements the embedded TensorFlow Lite Micro model lifecycle API
 * (``sl_ml_model_init``, ``sl_ml_model_run``, ``sl_ml_model_deinit``) declared in ``sl_ml_tflite_micro_model.h``.
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

#include "sl_ml_tflite_micro_model.h"

sl_status_t sl_ml_model_init(sl_ml_model_handle_t* handle)
{
  if (handle == nullptr || handle->flatbuffer == nullptr || handle->opcode_resolver == nullptr
      || handle->buffer_sizes == nullptr || handle->buffers == nullptr 
      || handle->flatbuffer_length <= 0 || handle->buffer_count <= 0) {
    return SL_STATUS_INVALID_PARAMETER;
  }

  if (handle->model.is_loaded()) {
    return SL_STATUS_OK;
  }

  ::npu_toolkit::register_tflite_micro_accelerator();
  const bool load_succeeded = handle->model.load(
    handle->flatbuffer,
    handle->opcode_resolver,
    handle->buffers,
    handle->buffer_sizes,
    static_cast<int>(handle->buffer_count)
  );
  return load_succeeded ? SL_STATUS_OK : SL_STATUS_FAIL;
}

sl_status_t sl_ml_model_run(sl_ml_model_handle_t* handle)
{
  if (handle == nullptr) {
    return SL_STATUS_INVALID_PARAMETER;
  }
  if (!handle->model.is_loaded()) {
    return SL_STATUS_NOT_INITIALIZED;
  }
  const bool invoke_succeeded = handle->model.invoke();
  return invoke_succeeded ? SL_STATUS_OK : SL_STATUS_FAIL;
}

sl_status_t sl_ml_model_deinit(sl_ml_model_handle_t* handle)
{
  if (handle == nullptr) {
    return SL_STATUS_INVALID_PARAMETER;
  }
  if (!handle->model.is_loaded()) {
    return SL_STATUS_NOT_INITIALIZED;
  }
  handle->model.unload();
  return SL_STATUS_OK;
}
