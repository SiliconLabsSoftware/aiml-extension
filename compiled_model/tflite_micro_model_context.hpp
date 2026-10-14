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

#include "tensorflow/lite/core/c/common.h"
#include "tensorflow/lite/micro/micro_context.h"



namespace npu_toolkit
{
class TfliteMicroModel;



/**
 * @addtogroup tflite_micro_model
 * @defgroup tflite_micro_model_context
 * @{
 */

 /**
  * This holds information used by @ref TfliteMicroModel
  *
  * The basic reference list is:
  * @verbatim TfLiteContext.impl_ -> tflite::MicroContext.external_context() -> TfliteMicroModelContext @endverbatim
  *
  * This has virtual functions that are typically overridden by the accelerator contexts
  */
class TfliteMicroModelContext
{
public:
  /**
   * Allocate a persistent buffer in the tensor arena and create a TfliteMicroModelContext instance
   */
  static TfliteMicroModelContext* create(TfLiteContext *context);

  /**
   * Initialize this context with the given model
   *
   * @return true if initialization was successful, false else
   */
  virtual bool init(TfliteMicroModel* model);

  /**
   * De-initialize this context
   */
  virtual void deinit();

  /**
   * Load this context
   *
   * @return true if loading was successful, false else
   */
  virtual bool load();

  /**
   * Return the associated @ref TfliteMicroModel
   */
  TfliteMicroModel* model() const;

  /**
   * Helper method to get a reference to the @ref TfliteMicroModelContext
   * associated with the given TfLiteContext.
   */
  static TfliteMicroModelContext* get(TfLiteContext* context);


protected:

  /**
   * The associated @ref TfliteMicroModel
   */
  TfliteMicroModel* _model = nullptr;

  /**
   * Default constructor
   */
  TfliteMicroModelContext() = default;

};

/**
 * @}
 */




} // namespace npu_toolkit