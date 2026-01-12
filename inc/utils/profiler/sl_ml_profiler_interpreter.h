/***************************************************************************//**
 * @file sl_ml_profiler_interpreter.h
 * @brief Header file for interpreter declaration.
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

#if defined(SL_BOARD_SI91X)
// Macro to access private members - allows access to graph_
#define private public
#define protected public
#include "micro_interpreter.h"
#undef private
#undef protected
#include "micro_profiler_interface.h"
#include "tflite_micro_model.hpp"
#include "ml/tflite_micro_model/tflite_micro_model_helper.hpp"
#endif

#if defined(SL_BOARD_EFX)
#include "sl_tflite_micro_init.h"
//#include "sl_tflite_micro_model.h"
#endif
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
#endif //
