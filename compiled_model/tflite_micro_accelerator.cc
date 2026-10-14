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
#include "tflite_micro_accelerator.hpp"
#include "tflite_micro_model_helper.hpp"
#include "tflite_micro_model.hpp"


namespace npu_toolkit
{

const char* TfliteMicroAccelerator::name() const
{
    return "unknown";
}

const char* TfliteMicroAccelerator::description() const
{
    return "unknown";
}

bool TfliteMicroAccelerator::init(const void* flatbuffer)
{
  // Each acclerator implemenation should override this
  return false;
}


static TfliteMicroAccelerator* _registered_accelerator = nullptr;
TfliteMicroAccelerator* register_tflite_micro_accelerator(TfliteMicroAccelerator* accelerator)
{
    _registered_accelerator = accelerator;
    return accelerator;
}

TfliteMicroAccelerator* get_registered_tflite_micro_accelerator()
{
    return _registered_accelerator;
}


} // namespace npu_toolkit