#include "audio_classifier.h"

#include <new>

#include "recognize_commands.h"
#include "sl_ml_model_keyword_spotting_on_off_v3.h"
#include "tflite_micro_model.hpp"

using namespace npu_toolkit;

constexpr const int AUDIO_BUFFER_SIZE = 16000;
static int16_t audio_buffer[AUDIO_BUFFER_SIZE];
static RecognizeCommands* command_recognizer = nullptr;
static uint8_t command_recognizer_instance_buffer[sizeof(RecognizeCommands)] alignas(alignof(RecognizeCommands));
static int32_t detected_timeout = 0;
static int32_t activity_timestamp = 0;
static int32_t activity_toggle_timestamp = 0;
static uint8_t previous_score = 0;
static int32_t previous_score_timestamp = 0;
static int previous_result = 0;
int category_count = 0;

void audio_classifier_task() {
    uint32_t prev_loop_timestamp = 0;

    printf("Starting Audio Classifier\n");

    // Load the model
    if (load_model() != SL_STATUS_OK) {
        printf("Error while loading model\n");
        return;
    }

    while (1) {
        // Get the current time
        const uint32_t current_timestamp = sl_sleeptimer_tick_to_ms(sl_sleeptimer_get_tick_count());

        // If not enough time has passed since the last inference
        // then go back to sleep
        int elapsed = current_timestamp - prev_loop_timestamp;

        if (elapsed < INFERENCE_INTERVAL_MS) {
            sl_sleeptimer_delay_millisecond(INFERENCE_INTERVAL_MS - elapsed);
        }

        // Store the current timestamp before we run the audio feature generator
        // and do model inference
        command_recognizer->base_timestamp_ = current_timestamp;
        // Perform a word detection
        prev_loop_timestamp = current_timestamp;

        // Process the audio buffer
        sl_ml_audio_feature_generation_update_features();

        // Determine if we should run inference
        // If the activity detection block is disabled, then always run inference
        // If the activity detection block is enabled, then ensure there is activity before running inference
        const bool should_run_inference = (!SL_ML_FRONTEND_ACTIVITY_DETECTION_ENABLE ||
                                           (sl_ml_audio_feature_generation_activity_detected() == SL_STATUS_OK));

        if (should_run_inference) {
            // Execute the processed audio in the ML model
            if (run_inference() != SL_STATUS_OK) {
                break;
            }
        }

        // Process the ML model results
        // NOTE: We do this even if we didn't run inference.
        //       This way, the LEDs blink correctly
        process_output(should_run_inference);

    }  // main loop

    return;
}

/***************************************************************************/ /**
 * Run model inference
 *
 * Copies the currently available data from the feature_buffer into the input
 * tensor and runs inference, updating the global output tensor.
 *
 * @return
 *   SL_STATUS_OK on success, other value on failure.
 ******************************************************************************/
sl_status_t run_inference() {
    // Update model input tensor
    sl_status_t status = sl_ml_audio_feature_generation_fill_tensor(keyword_spotting_on_off_v3_model.input());
    if (status != SL_STATUS_OK) {
        return SL_STATUS_FAIL;
    }

    // Run the model on the spectrogram input and make sure it succeeds.
    if (slx_ml_keyword_spotting_on_off_v3_model_run() != SL_STATUS_OK) {
        return SL_STATUS_FAIL;
    }

    auto& output_tensor = *keyword_spotting_on_off_v3_model.output(0);

    return SL_STATUS_OK;
}

/***************************************************************************/ /**
 * Processes the output from the output tensor
 *
 * @return
 *   SL_STATUS_OK on success, other value on failure.
 ******************************************************************************/
sl_status_t process_output(const bool did_run_inference) {
    // Determine whether a command was recognized based on the output of inference
    uint8_t result = 0;
    uint8_t score = 0;
    bool is_new_command = false;
    sl_status_t status = SL_STATUS_OK;
    TfLiteStatus process_status = kTfLiteOk;
    const uint32_t current_timestamp = sl_sleeptimer_tick_to_ms(sl_sleeptimer_get_tick_count());

    if (did_run_inference)
        process_status = command_recognizer->ProcessLatestResults(
            keyword_spotting_on_off_v3_model.output(), current_timestamp, &result, &score, &is_new_command);

    if (process_status == kTfLiteOk) {
        handle_results(current_timestamp, result, score, is_new_command);
    } else {
        status = SL_STATUS_FAIL;
    }

    return status;
}

/***************************************************************************/ /**
 * Handle inference result
 *
 * This function is called whenever we have a succesfull inference result.
 *
 * @param current_time timestamp of the inference result.
 * @param result classification result, this is number >= 0.
 * @param score the score of the result. This is number represents the confidence
 *   of the result classification.
 * @param is_new_command true if the result is a new command, false otherwise.
 ******************************************************************************/
void handle_results(int32_t current_time, int result, uint8_t score, bool is_new_command) {
    const char* label = get_category_label(result);

    if (is_new_command) {
        // Reset the AFG internal state so we can detect a new keyword
        // NOTE: Alternatively, the "suppression" setting can be increased to add a delay
        //       until processing states again (this effectively clears the audio buffer)
        if (SUPPRESION_TIME_MS <= 1) {
            sl_ml_audio_feature_generation_reset();
        }

        printf("Detected class=%d label=%s score=%d @%ldms\n", result, label, score, current_time);

        led_turn_on(DETECTION_LED);
        led_turn_off(ACTIVITY_LED);
        activity_timestamp = 0;
        detected_timeout = current_time + 1200;
    } else if (detected_timeout != 0 && current_time >= detected_timeout) {
        detected_timeout = 0;
        previous_score = score;
        previous_result = result;
        previous_score_timestamp = current_time;
        led_turn_off(DETECTION_LED);
    }

    // If we're using the activity detection block,
    // then inference is only done when activity is detected
    // in the audio stream. In this case, we control
    // the LEDs based on static timeouts
    if (SL_ML_FRONTEND_ACTIVITY_DETECTION_ENABLE) {
        if (detected_timeout == 0 && score > 0 && previous_result != result) {
            activity_timestamp = current_time + 1000;
        } else if (current_time >= activity_timestamp) {
            activity_timestamp = 0;
            led_turn_off(ACTIVITY_LED);
        }

        if (activity_timestamp != 0) {
            if (current_time - activity_toggle_timestamp >= 100) {
                activity_toggle_timestamp = current_time;
                led_toggle(ACTIVITY_LED);
            }
        }
        return;
    }

    // If the activity detection block is NOT used,
    // then inference is also done a a specific interval.
    // In this case, we control the LEDs based on the scores
    // returned by the inference
    if (detected_timeout == 0) {
        if (previous_score == 0) {
            previous_result = result;
            previous_score = score;
            previous_score_timestamp = current_time;
            return;
        }

        // Calculate the rate of difference in score between the two last results
        const int32_t time_delta = current_time - previous_score_timestamp;
        const int8_t score_delta = (int8_t)(score - previous_score);
        const float diff = (time_delta > 0) ? std::fabs(score_delta) / time_delta : 0.0f;

        previous_score = score;
        previous_score_timestamp = current_time;

        if (diff >= SENSITIVITY || (previous_result != result && score > 255 / 2)) {
            previous_result = result;
            activity_timestamp = current_time + 500;
        } else if (current_time >= activity_timestamp) {
            activity_timestamp = 0;
            led_turn_off(ACTIVITY_LED);
        }

        if (activity_timestamp != 0) {
            if (current_time - activity_toggle_timestamp >= 100) {
                activity_toggle_timestamp = current_time;
                led_toggle(ACTIVITY_LED);
            }
        }
    }
}

/***************************************************************************/ /**
 * Get the label for a certain category/class
 *
 * @param index
 *   index of the category/class
 *
 * @return
 *   pointer to the label string. The label is "?" if no corresponding label
 *   was found.
 ******************************************************************************/
const char* get_category_label(int index) {
    if ((index >= 0) && (index < category_count)) {
        return CATEGORY_LABELS[index];
    } else {
        return "?";
    }
}

/***************************************************************************/ /**
 * Load the ML model and initialize the system
 *
 ******************************************************************************/
sl_status_t load_model() {
    // Register the accelerator if the TFLM lib was built with one
    auto accelerator = register_tflite_micro_accelerator();
    printf("Using accelerator: %s, %s\n", accelerator->name(), accelerator->description());

    printf("Loading model\n");
    if (slx_ml_keyword_spotting_on_off_v3_model_init() != SL_STATUS_OK) {
        printf("Failed to load model\n");
        return SL_STATUS_FAIL;
    }

    // Initialize the audio feature generation using the parameters
    // from the given .tflite model file
    if (!sl_ml_audio_feature_generation_load_model_settings(keyword_spotting_on_off_v3_model_flatbuffer)) {
        printf("Failed to load audio feature generator parameters from .tflite model\n");
        return SL_STATUS_FAIL;
    }

    // Load the other application-specific model parameters from the .tflite
    if (!audio_classifier_config_load_model_settings(keyword_spotting_on_off_v3_model_flatbuffer)) {
        printf("Failed to load app parameters from .tflite model\n");
        return SL_STATUS_FAIL;
    }

    const TfLiteTensor* input = keyword_spotting_on_off_v3_model.input();
    const TfLiteTensor* output = keyword_spotting_on_off_v3_model.output();

    // Validate model tensors
    if ((output->dims->size == 2) && (output->dims->data[0] == 1)) {
        category_count = output->dims->data[1];
    } else {
        printf(
            "Invalid output tensor shape\n"
            "expecting an output tensor of shape [1,x]\n"
            "where x is the number of classification results\n");
        return SL_STATUS_FAIL;
    }

    if (category_count != static_cast<int>(SL_TFLITE_MODEL_CLASSES.size())) {
        printf("Number of categories(%d) is not equal to the number of labels(%ld)\n",
               category_count,
               SL_TFLITE_MODEL_CLASSES.size());
        return SL_STATUS_FAIL;
    }

    if (!(input->type == kTfLiteInt8 || input->type == kTfLiteUInt16 || input->type == kTfLiteFloat32)) {
        printf(
            "Invalid input tensor type.\n"
            "Application requires input and output tensors to be of type int8, uint16, or float32.\n");
        return SL_STATUS_FAIL;
    }

    if (!(output->type == kTfLiteInt8 || output->type == kTfLiteFloat32)) {
        printf(
            "Invalid output tensor type.\n"
            "Application requires input and output tensors to be of type int8 or float32.\n");
        return SL_STATUS_FAIL;
    }

    // Initialize the audio feature generator
    if (sl_ml_audio_feature_generation_init_with_buffer(audio_buffer, AUDIO_BUFFER_SIZE) != SL_STATUS_OK) {
        printf("Failed to init audio feature generator\n");
        return SL_STATUS_FAIL;
    }

    // Instantiate CommandRecognizer
    command_recognizer = new (command_recognizer_instance_buffer) RecognizeCommands(SMOOTHING_WINDOW_DURATION_MS,
                                                                                    DETECTION_THRESHOLD,
                                                                                    SUPPRESION_TIME_MS,
                                                                                    MINIMUM_DETECTION_COUNT,
                                                                                    IGNORE_UNDERSCORE_LABELS);

    return SL_STATUS_OK;
}
