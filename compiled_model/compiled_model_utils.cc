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
#include "tflite_micro_model_helper.hpp"
#include "compiled_model_interface.hpp"

#if __has_include("_model_compiler_version.h")
#include "_model_compiler_version.h"
#endif

namespace npu_toolkit
{

const CompilerVersionData* compiled_model_retrieve_compiler_version(
    const void* flatbuffer
)
{
    return CompilerVersionData::create(
        TfliteMicroModelHelper::get_metadata_from_tflite_flatbuffer(
            flatbuffer, COMPILER_VERSION_TFLITE_TAG
        )
    );
}

bool compiled_model_validate_compiler_version(
    const void* flatbuffer,
    const char* expected_kernels_type,
    bool required
)
{
  #ifdef MODEL_COMPILER_VERSION
  const auto compiler_version = compiled_model_retrieve_compiler_version(flatbuffer);
  if(compiler_version == nullptr && required)
  {
    NPU_TOOLKIT_ERROR("The input .tflite model does not contain compiler version metadata.");
    return false;
  }

  if(expected_kernels_type != nullptr)
  {
    if(strcmp(compiler_version->kernels_type(), expected_kernels_type) != 0)
    {
      NPU_TOOLKIT_ERROR(
        "Incompatible model compiler kernels type. "
        "The model was compiled for kernels=%s, "
        "but the source code expects %s" ,
        compiler_version->kernels_type(),
        expected_kernels_type
      );
      return false;
    }
  }

  if(compiler_version->compiler_version() != MODEL_COMPILER_VERSION)
  {
    NPU_TOOLKIT_ERROR(
      "Incompatible model compiler version. "
      "The model was compiled with v%d.%d.%d, "
      "but the source code expects v" MODEL_COMPILER_VERSION_STR,
      compiler_version->compiler_version_major(),
      compiler_version->compiler_version_minor(),
      compiler_version->compiler_version_patch()
    );

    return false;
  }
  #endif

  return true;
}


const CompiledModelData* compiled_model_retrieve_compilation_data(
    TfLiteContext *context
)
{
    return CompiledModelData::create(
        TfliteMicroModelHelper::get_metadata_from_tflite_flatbuffer(
            context, COMPILED_MODEL_DATA_TFLITE_TAG
        )
    );
}


} // namespace npu_toolkit