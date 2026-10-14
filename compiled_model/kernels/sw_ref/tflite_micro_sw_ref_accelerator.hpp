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
 * @defgroup tflite_micro_accelerator_swf_ref
 * @{
 */


 /**
  * Software reference accelerator implementation
  */
class TfliteMicroSwRefAccelerator : public TfliteMicroAccelerator
{
public:
  const char* name() const override;
  const char* description() const override;
  bool init(const void* flatbuffer) override;

protected:
  TfliteMicroSwRefAccelerator() = default;
  friend TfliteMicroAccelerator* get_tflite_micro_accelerator();
};

/**
 * @}
 */

} // namespace npu_toolkit