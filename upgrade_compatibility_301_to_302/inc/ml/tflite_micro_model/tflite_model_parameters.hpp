/*******************************************************************************
 * @file tflite_model_parameters.hpp
 * @brief Deprecated redirect for aiml 3.0.1 `ml/tflite_micro_model/tflite_model_parameters.hpp`.
 *******************************************************************************
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

#pragma once

#pragma message("ml/tflite_micro_model/tflite_model_parameters.hpp is deprecated; use tflite_model_parameters.hpp. \
This header will be removed in a future release.")

/* Prefixed path from root_path `..` include so this shim does not include itself. */
#include "compiled_model/tflite_model_parameters.hpp"
