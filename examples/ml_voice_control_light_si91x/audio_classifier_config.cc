
#include "audio_classifier_config.h"

#include "ml/tflite_micro_model/tflite_micro_logger.hpp"
#include "ml/tflite_micro_model/tflite_model_parameters.hpp"
#include "sl_ml_audio_feature_generation_config_si91x.h"
#include "tflite_micro_model.hpp"

extern "C" {

using namespace npu_toolkit;

int SL_TFLITE_MODEL_AVERAGE_WINDOW_DURATION_MS;
int SL_TFLITE_MODEL_DETECTION_THRESHOLD;
int SL_TFLITE_MODEL_SUPPRESSION_MS;
int SL_TFLITE_MODEL_MINIMUM_COUNT;
float SL_TFLITE_MODEL_SENSITIVITY;
int SL_TFLITE_MODEL_VERBOSE_MODEL_OUTPUT_LOGS;
int SL_TFLITE_MODEL_INFERENCE_INTERVAL_MS;
StringList SL_TFLITE_MODEL_CLASSES;
Int32List SL_TFLITE_DETECTION_THRESHOLD_LIST;
int SL_TFLITE_MODEL_BAUD_RATE = 0;

bool audio_classifier_config_load_model_settings(const void* tflite_flatbuffer) {
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
    model_parameters.get("sensitivity", SL_TFLITE_MODEL_SENSITIVITY);
    model_parameters.get("latency_ms", SL_TFLITE_MODEL_INFERENCE_INTERVAL_MS);
    model_parameters.get("dump_audio", SL_ML_AUDIO_FEATURE_GENERATION_DUMP_AUDIO);
    model_parameters.get("dump_raw_spectrograms", SL_ML_AUDIO_FEATURE_GENERATION_DUMP_RAW_SPECTROGRAM);
    model_parameters.get("dump_spectrograms", SL_ML_AUDIO_FEATURE_GENERATION_DUMP_QUANTIZED_SPECTROGRAM);
    model_parameters.get("verbose_model_output_logs", SL_TFLITE_MODEL_VERBOSE_MODEL_OUTPUT_LOGS);
    model_parameters.get("volume_gain", SL_ML_AUDIO_FEATURE_GENERATION_AUDIO_GAIN);

    // #ifdef AUDIO_CLASSIFIER_VERBOSE
    SL_TFLITE_MODEL_VERBOSE_MODEL_OUTPUT_LOGS = 0;
// #endif
#ifdef AUDIO_CLASSIFIER_DUMP_AUDIO
    SL_ML_AUDIO_FEATURE_GENERATION_DUMP_AUDIO = 1;
#endif
#ifdef AUDIO_CLASSIFIER_DUMP_SPECTROGRAMS
    SL_ML_AUDIO_FEATURE_GENERATION_DUMP_QUANTIZED_SPECTROGRAM = 1;
#endif
#ifdef AUDIO_CLASSIFIER_DUMP_RAW_SPECTROGRAMS
    SL_ML_AUDIO_FEATURE_GENERATION_DUMP_RAW_SPECTROGRAM = 1;
#endif

    if (model_parameters.get("detection_threshold_list", SL_TFLITE_DETECTION_THRESHOLD_LIST)) {
        if (SL_TFLITE_DETECTION_THRESHOLD_LIST.size() != SL_TFLITE_MODEL_CLASSES.size()) {
            printf(
                "The number of entries in the model parameter: 'detection_threshold_list' must match the number of "
                "classes\n");
            return false;
        }
    }

#ifndef __arm__
    // Ensure the loop interval is at least 10ms on Windows/Linux
    if (SL_TFLITE_MODEL_INFERENCE_INTERVAL_MS < 10) {
        SL_TFLITE_MODEL_INFERENCE_INTERVAL_MS = 10;
    }
#endif

    printf("Application settings:\n");
    printf("average_window_duration_ms=%d\n", SL_TFLITE_MODEL_AVERAGE_WINDOW_DURATION_MS);
    printf("detection_threshold=%d\n", SL_TFLITE_MODEL_DETECTION_THRESHOLD);
    printf("suppression_ms=%d\n", SL_TFLITE_MODEL_SUPPRESSION_MS);
    printf("minimum_count=%d\n", SL_TFLITE_MODEL_MINIMUM_COUNT);
    printf("sensitivity=%4.2f\n", SL_TFLITE_MODEL_SENSITIVITY);
    printf("verbose_model_output_logs=%d\n", SL_TFLITE_MODEL_VERBOSE_MODEL_OUTPUT_LOGS);
    printf("latency_ms=%d\n", SL_TFLITE_MODEL_INFERENCE_INTERVAL_MS);
    printf("volume_gain=%d\n", SL_ML_AUDIO_FEATURE_GENERATION_AUDIO_GAIN);
    printf("dump_audio=%d\n", SL_ML_AUDIO_FEATURE_GENERATION_DUMP_AUDIO);
    printf("dump_raw_spectrograms=%d\n", SL_ML_AUDIO_FEATURE_GENERATION_DUMP_RAW_SPECTROGRAM);
    printf("dump_spectrograms=%d\n", SL_ML_AUDIO_FEATURE_GENERATION_DUMP_QUANTIZED_SPECTROGRAM);

    return true;
}

}  // extern "C"
