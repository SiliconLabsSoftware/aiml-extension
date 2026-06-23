/***************************************************************************//**
 * @file sl_ml_profiler_interpreter.h
 * @brief Interpreter and profiler model wrapper declarations for the ML profiler.
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
#ifndef SL_ML_PROFILER_INTERPRETER_H
#define SL_ML_PROFILER_INTERPRETER_H

namespace sl {
namespace ml {
class SilabsProfiler;
} // namespace ml
} // namespace sl

// Macro to access private members - allows access to graph_
#define private public
#define protected public
#include "micro_interpreter.h"
#undef private
#undef protected

#include "micro_profiler_interface.h"
#include "tflite_micro_model.hpp"
#include "ml/tflite_micro_model/tflite_micro_model_helper.hpp"

using namespace npu_toolkit;

// Subtype of MicroInterpreter to access the TfLiteContext
class SLMicroInterpreter : public tflite::MicroInterpreter {
public:
  using MicroInterpreter::MicroInterpreter; // Inherit constructors

  const TfLiteEvalTensor* GetEvalTensor(int tensor_id) const
  {
    const TfLiteContext& ctx = context();
    return ctx.GetEvalTensor(&ctx, tensor_id);
  }
};

// Subtype of TfliteMicroModel
class SLTfliteMicroModel : public npu_toolkit::TfliteMicroModel {
public:
  using TfliteMicroModel::TfliteMicroModel; // Inherit constructors

  SLTfliteMicroModel(sl::ml::SilabsProfiler* profiler)
    : _custom_profiler(profiler) {}

  tflite::MicroInterpreter* create_interpreter() override;

private:
  sl::ml::SilabsProfiler* _custom_profiler;
};

#if (SL_ML_ENABLE_DYNAMIC_MODEL_LOAD == 1)
bool load_dynamic_tflite_micro_model(SLTfliteMicroModel& model);
#endif // SL_ML_ENABLE_DYNAMIC_MODEL_LOAD

#endif // SL_ML_PROFILER_INTERPRETER_H
