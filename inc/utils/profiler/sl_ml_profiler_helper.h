/***************************************************************************//**
 * @file sl_ml_profiler_helper.h
 * @brief Header file for helper functions and objects for ml profiler.
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
#ifndef SL_ML_PROFILER_HELPER_H
#define SL_ML_PROFILER_HELPER_H

#include "sl_ml_silabs_profiler.h"

#if defined(SL_BOARD_SI91X)
#include "micro_interpreter.h"
#include "micro_profiler_interface.h"
#include "tflite_micro_model.hpp"
#include "ml/tflite_micro_model/tflite_micro_model_helper.hpp"
using namespace npu_toolkit;

// Subtype of TfliteMicroModel
class SLTfliteMicroModel : public npu_toolkit::TfliteMicroModel {
public:
  using TfliteMicroModel::TfliteMicroModel; // Inherit constructors

  SLTfliteMicroModel(sl::ml::SilabsProfiler* profiler)
    : _custom_profiler(profiler) {}

  bool load_interpreter(
  const void* flatbuffer,
  const tflite::MicroOpResolver* op_resolver,
  uint8_t* buffers[],
  const int32_t buffer_sizes[],
  int32_t n_buffers
  ) override ;

private:
  sl::ml::SilabsProfiler* _custom_profiler;
  
};

#if (SL_ML_ENABLE_DYNAMIC_MODEL_LOAD == 1 )
bool load_dynamic_tflite_micro_model(SLTfliteMicroModel& model);
#endif //SL_ML_ENABLE_DYNAMIC_MODEL_LOAD
#endif //SL_BOARD_SI91X
#endif // SL_ML_PROFILER_HELPER_H
