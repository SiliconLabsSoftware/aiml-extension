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

#include <functional>
#include "tensorflow/lite/micro/kernels/kernel_util.h"
#include "compiled_model_interface.hpp"
#include "compiled_model_paging_interface.hpp"


namespace npu_toolkit
{


/**
 * @defgroup compiled_model Compiled Model
 * @{
 */

 /**
 * Retrieve the model compiler version from the .tflite model's metadata
*/
const CompilerVersionData* compiled_model_retrieve_compiler_version(
    const void* flatbuffer
);

/**
 * Retrieve the CompilerVersionData from the given .tflite
 * and ensure the compiler version used to compiled the .tflite
 * matches this application's source code.
 */
bool compiled_model_validate_compiler_version(
    const void* flatbuffer,
    const char* expected_kernels_type = nullptr,
    bool required = true
);

/**
 * Retrieve the compilation data from the .tflite model's metadata
*/
const CompiledModelData* compiled_model_retrieve_compilation_data(
    TfLiteContext *context
);


/**
 * Execute the pre-compiled data for the current model layer on the accelerator.
 *
 * This is called by each accelerator kernel's "compiled" implementation.
 * It iterates through the pre-compiled data for the current ML model layer.

 * @param context TF-Lite context
 * @param program_callback Optional callback to be invoked for each accelerator program used by current layer
 *                         This callback is invoked BEFORE the accelerator registers are populated for the current program
*/
TfLiteStatus compiled_model_process_layer_with_accelerator(
    TfLiteContext *context,
    void (*program_callback)(void*, int) = nullptr,
    void* program_callback_arg = nullptr
);

/**
 * @}
 */


} // namespace npu_toolkit