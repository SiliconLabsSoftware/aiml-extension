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
#include "em_device.h"
#include "tflite_micro_cmsis_accelerator.hpp"
#include "compiled_model.hpp"


namespace npu_toolkit
{



const char* TfliteMicroCmsisAccelerator::name() const
{
  return "cmsis";
}


const char* TfliteMicroCmsisAccelerator::description() const
{
  return "Select kernels optimized using ARM CMSIS-NN SIMD instructions";
}

bool TfliteMicroCmsisAccelerator::init(const void* flatbuffer)
{
  // Ensure this application's source code is compatible
  // with the ML compiler used to compile the given .tflite flatbuffer
  // Note that the compiler version is not strictly required since the kernel implementations are not compiled.
  if((flatbuffer != nullptr) && !compiled_model_validate_compiler_version(flatbuffer, name(), false))
  {
    return false;
  }
  return true;
}

__WEAK TfliteMicroAccelerator* get_tflite_micro_accelerator()
{
  static TfliteMicroCmsisAccelerator accelerator;
  return reinterpret_cast<TfliteMicroAccelerator*>(&accelerator);
}

__WEAK TfliteMicroAccelerator* register_tflite_micro_accelerator()
{
    return register_tflite_micro_accelerator(get_tflite_micro_accelerator());
}

} // namespace npu_toolkit