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
#include <cstdarg>

#include "tflite_micro_accelerator.hpp"


namespace npu_toolkit
{

/**
 * @addtogroup tflite_micro_model
 * @defgroup tflite_micro_accelerator_mvpv1
 * @{
 */


 /**
  * MVPv1 accelerator implementation
  */
class TfliteMicroMvpv1Accelerator : public TfliteMicroAccelerator
{
public:
  static TfliteMicroMvpv1Accelerator& instance()
  {
    return *static_cast<TfliteMicroMvpv1Accelerator*>(
      get_tflite_micro_accelerator()
    );
  }

  const char* name() const override;
  const char* description() const override;
  bool init(const void* flatbuffer) override;
  void deinit(TfLiteContext *context) override;
  TfliteMicroModelContext* create_context(
    TfLiteContext *context
  ) override;

  /**
   * Return if the model has been compiled
   */
  static bool is_model_compiled(TfLiteContext *context);

  /**
   * Return if the current ML model layer has pre-compiled data in the .tflite model file
  */
  static bool is_current_layer_compiled(TfLiteContext *context);

  /**
   * Prepare the accelerator for compiled kernel execution.
   *
   * This is invoked at the beginning of compiled_model_process_layer_with_accelerator()
   */
  void prepare_compiled_execution();

  /**
   * Execute the MVP program(s) for the current layer.
   *
   * This is invoked in compiled_model_process_layer_with_accelerator() after the accelerator
   * registers have been populated for the current program.
   */
 void execute_program();

  /**
   * Wait for the MVP program execution to complete by waiting for the appropriate interrupt(s).
   */
  void wait_for_idle();

  /**
   * Finalize the accelerator after compiled kernel execution.
   *
   * This is invoked at the end of compiled_model_process_layer_with_accelerator(),
   * after all MVP programs for the current layer have been executed.
   */
  void finalize_compiled_execution();


protected:
  TfliteMicroMvpv1Accelerator() = default;

  /**
   * This keeps track of the number of times the init/deinit functions have been called.
   * It increments when init is called and decrements when deinit is called.
   * If it is 0 when init is called, the accelerator is actually initialized.
   * If it decremented to 0 when deinit is called, the accelerator is actually deinitialized.
   */
  int32_t _ref_counter = 0;

  friend TfliteMicroAccelerator* get_tflite_micro_accelerator();
};

} // namespace npu_toolkit