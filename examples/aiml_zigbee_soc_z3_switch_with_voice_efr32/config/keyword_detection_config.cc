/***************************************************************************//**
 * @file
 * @brief keyword detection config
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


#include "keyword_detection_config.h"

#include "ml/tflite_micro_model/tflite_model_parameters.hpp"

extern "C" {

using namespace npu_toolkit;

int SL_TFLITE_MODEL_AVERAGE_WINDOW_DURATION_MS;
int SL_TFLITE_MODEL_DETECTION_THRESHOLD;
int SL_TFLITE_MODEL_SUPPRESSION_MS;
int SL_TFLITE_MODEL_MINIMUM_COUNT;
int SL_TFLITE_MODEL_VERBOSE_MODEL_OUTPUT_LOGS;
int SL_TFLITE_MODEL_INFERENCE_INTERVAL_MS;
StringList SL_TFLITE_MODEL_CLASSES;
int SL_TFLITE_MODEL_BAUD_RATE = 0;

bool keyword_detection_config_load_model_settings(const void* tflite_flatbuffer) {
    TfliteModelParameters model_parameters;

    if (!TfliteModelParameters::load_from_tflite_flatbuffer(tflite_flatbuffer, model_parameters)) {
        return false;
    }

    if (!model_parameters.get("classes", SL_TFLITE_MODEL_CLASSES)) {
        printf(".tflite does not define a \"classes\" parameter\n");
        return false;
    }
    model_parameters.get("average_window_duration_ms", SL_TFLITE_MODEL_AVERAGE_WINDOW_DURATION_MS);
    model_parameters.get("suppression_ms", SL_TFLITE_MODEL_SUPPRESSION_MS);
    model_parameters.get("detection_threshold", SL_TFLITE_MODEL_DETECTION_THRESHOLD);
    model_parameters.get("minimum_count", SL_TFLITE_MODEL_MINIMUM_COUNT);
    model_parameters.get("latency_ms", SL_TFLITE_MODEL_INFERENCE_INTERVAL_MS);
    model_parameters.get("verbose_model_output_logs", SL_TFLITE_MODEL_VERBOSE_MODEL_OUTPUT_LOGS);

#ifdef AUDIO_CLASSIFIER_VERBOSE
    SL_TFLITE_MODEL_VERBOSE_MODEL_OUTPUT_LOGS = 1;
#endif

    printf("Keyword detection settings:\n");
    printf("average_window_duration_ms=%d\n", SL_TFLITE_MODEL_AVERAGE_WINDOW_DURATION_MS);
    printf("detection_threshold=%d\n", SL_TFLITE_MODEL_DETECTION_THRESHOLD);
    printf("suppression_ms=%d\n", SL_TFLITE_MODEL_SUPPRESSION_MS);
    printf("minimum_count=%d\n", SL_TFLITE_MODEL_MINIMUM_COUNT);
    printf("verbose_model_output_logs=%d\n", SL_TFLITE_MODEL_VERBOSE_MODEL_OUTPUT_LOGS);
    printf("latency_ms=%d\n", SL_TFLITE_MODEL_INFERENCE_INTERVAL_MS);

    return true;
}

}  // extern "C"
