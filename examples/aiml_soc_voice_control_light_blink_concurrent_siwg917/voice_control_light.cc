/***************************************************************************//**
 * @file
 * @brief voice control light
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

#include "voice_control_light.h"

#include <cmath>
#include <cinttypes>
#include <new>

#include "sl_fe_audio_instances.h"
#include "sl_fe_audio_si91x.h"
#include "sl_ml_model_blink.h"
#include "sl_ml_model_keyword_spotting_on_off_v3.h"
#include "recognize_commands.h"
#include "tflite_micro_model.hpp"

// Handles keyword recognition smoothing and suppression.
static RecognizeCommands* command_recognizer = nullptr;
static uint8_t command_recognizer_instance_buffer[sizeof(RecognizeCommands)] alignas(alignof(RecognizeCommands));
static int32_t detected_timeout = 0;
static int32_t activity_timestamp = 0;
static int32_t activity_toggle_timestamp = 0;
static uint8_t previous_score = 0;
static int32_t previous_score_timestamp = 0;
static int previous_result = 0;
static uint8_t last_voice_command = 0xFFu;
int category_count = 0;

// Use a visible software PWM frame for the sine-driven LED animation.
#define TIME_DELAY_MS 10
#define INFERENCE_PER_CYCLE (1000 / TIME_DELAY_MS)
#define X_RANGE (2.f * 3.14159265359f)
#define SINE_LOOPS 3

static bool run_sine_blink(void);

static void led_pwm(float brightness01)
{
    led_all_off();
    if (brightness01 <= 0.10f) {
        sl_sleeptimer_delay_millisecond(TIME_DELAY_MS);
        return;
    }

    const uint32_t on_ms = (uint32_t)((TIME_DELAY_MS * brightness01) + 0.5f);
    const uint32_t off_ms = TIME_DELAY_MS - on_ms;

    if (on_ms > 0U) {
        led_turn_on(DETECTION_ON);
        sl_sleeptimer_delay_millisecond(on_ms);
        led_turn_off(DETECTION_ON);
    }
    if (off_ms > 0U) {
        sl_sleeptimer_delay_millisecond(off_ms);
    }
}

static bool run_sine_blink(void)
{
    if (!sl_ml_blink_model_handle.model.is_loaded()) {
        printf("Blink: blink model is not loaded\n");
        return false;
    }

    for (int loop = 0; loop < SINE_LOOPS; ++loop) {
        for (int step = 0; step < INFERENCE_PER_CYCLE; ++step) {
            auto& in_tensor = *sl_ml_blink_model_handle.model.input(0);
            const float x = ((float)step / (float)INFERENCE_PER_CYCLE) * X_RANGE;
            in_tensor.data.int8[0] = (int8_t)(x / in_tensor.params.scale + in_tensor.params.zero_point);

            if (sl_ml_model_run(&sl_ml_blink_model_handle) != SL_STATUS_OK) {
                led_all_off();
                return false;
            }

            auto& out_tensor = *sl_ml_blink_model_handle.model.output(0);
            const float y =
                (out_tensor.data.int8[0] - out_tensor.params.zero_point) * out_tensor.params.scale;
            float b = (y + 1.0f) * 0.5f;
            if (b < 0.f) {
                b = 0.f;
            } else if (b > 1.f) {
                b = 1.f;
            }

            led_pwm(b);
        }
    }

    led_detection_on();
    return true;
}

void voice_control_light_task() {
    uint32_t prev_loop_timestamp = 0;
    led_all_off();
    printf("Starting Voice Control Light + Blink Application\n");
    printf("Model load mode: concurrent\n");

    if (load_model() != SL_STATUS_OK) {
        printf("Error while loading model\n");
        return;
    }

    while (1) {
        const uint32_t current_timestamp = sl_sleeptimer_tick_to_ms(sl_sleeptimer_get_tick_count());

        // Wait until the next keyword inference interval.
        int elapsed = current_timestamp - prev_loop_timestamp;

        if (elapsed < INFERENCE_INTERVAL_MS) {
            sl_sleeptimer_delay_millisecond(INFERENCE_INTERVAL_MS - elapsed);
        }

        // Timestamp the audio features before keyword inference.
        command_recognizer->base_timestamp_ = current_timestamp;
        // Process one keyword detection window.
        prev_loop_timestamp = current_timestamp;

        // Update the audio feature buffer.
        sl_ml_audio_feature_generation_update_features();

        // Run inference when activity detection is disabled or speech activity is present.
        const bool should_run_inference = (!SL_ML_FRONTEND_ACTIVITY_DETECTION_ENABLE ||
                                           (sl_ml_audio_feature_generation_activity_detected() == SL_STATUS_OK));

        if (should_run_inference) {
            // Execute keyword spotting on the processed audio features.
            if (run_inference() != SL_STATUS_OK) {
                break;
            }
        }

        // Process the latest model result, preserving timeout-driven LED behavior.
        process_output(should_run_inference);

    }  // main loop

    return;
}

/***************************************************************************/ /**
 * Run keyword spotting inference.
 *
 * Copies the latest audio features into the keyword spotting input tensor and
 * runs inference, updating the model output tensor.
 *
 * @return
 *   SL_STATUS_OK on success, other value on failure.
 ******************************************************************************/
sl_status_t run_inference() {
    // Fill the keyword model input tensor from the latest audio features.
    sl_status_t status =
        sl_ml_audio_feature_generation_fill_tensor(sl_ml_keyword_spotting_on_off_v3_model_handle.input_tensor(0));
    if (status != SL_STATUS_OK) {
        return SL_STATUS_FAIL;
    }

    // Run keyword spotting on the spectrogram input.
    if (sl_ml_model_run(&sl_ml_keyword_spotting_on_off_v3_model_handle) != SL_STATUS_OK) {
        return SL_STATUS_FAIL;
    }

    return SL_STATUS_OK;
}

/***************************************************************************/ /**
 * Process keyword spotting output.
 *
 * @return
 *   SL_STATUS_OK on success, other value on failure.
 ******************************************************************************/
sl_status_t process_output(const bool did_run_inference) {
    // Determine whether a spoken command was recognized.
    uint8_t result = 0;
    uint8_t score = 0;
    bool is_new_command = false;
    sl_status_t status = SL_STATUS_OK;
    TfLiteStatus process_status = kTfLiteOk;
    const uint32_t current_timestamp = sl_sleeptimer_tick_to_ms(sl_sleeptimer_get_tick_count());

    if (did_run_inference)
        process_status = command_recognizer->ProcessLatestResults(
            sl_ml_keyword_spotting_on_off_v3_model_handle.output_tensor(0), current_timestamp, &result, &score, &is_new_command);

    if (process_status == kTfLiteOk) {
        handle_results(current_timestamp, result, score, is_new_command);
    } else {
        status = SL_STATUS_FAIL;
    }

    return status;
}

/***************************************************************************/ /**
 * Handle a filtered keyword spotting result.
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
        // Reset the audio frontend when suppression is configured for immediate retriggering.
        if (SUPPRESION_TIME_MS <= 1) {
            sl_ml_audio_feature_generation_reset();
        }

        printf("Detected class=%d label=%s score=%" PRIu8 " @%" PRId32 "ms\n", result, label, score, current_time);

        if (result == 0) {
            if ((last_voice_command != 0u) && run_sine_blink()) {
                last_voice_command = 0u;
            }
        } else if (result == 1) {
            led_all_off();
            last_voice_command = 1u;
        }

        activity_timestamp = 0;
        detected_timeout = current_time + 1200;
    } else if (detected_timeout != 0 && current_time >= detected_timeout) {
        detected_timeout = 0;
        previous_score = score;
        previous_result = result;
        previous_score_timestamp = current_time;
    }

    // With activity detection enabled, use static timeouts between inference windows.
    if (SL_ML_FRONTEND_ACTIVITY_DETECTION_ENABLE) {
        if (detected_timeout == 0 && score > 0 && previous_result != result) {
            activity_timestamp = current_time + 1000;
        } else if (current_time >= activity_timestamp) {
            activity_timestamp = 0;
        }

        if (activity_timestamp != 0) {
            if (current_time - activity_toggle_timestamp >= 100) {
                activity_toggle_timestamp = current_time;
            }
        }
        return;
    }

    // Without activity detection, use score changes at the fixed inference interval.
    if (detected_timeout == 0) {
        if (previous_score == 0) {
            previous_result = result;
            previous_score = score;
            previous_score_timestamp = current_time;
            return;
        }

        // Calculate the score change rate between the two latest results.
        const int32_t time_delta = current_time - previous_score_timestamp;
        const int8_t score_delta = (int8_t)(score - previous_score);
        const float diff =
          (time_delta > 0) ? std::fabs(static_cast<float>(score_delta)) / time_delta : 0.0f;

        previous_score = score;
        previous_score_timestamp = current_time;

        if (diff >= SENSITIVITY || (previous_result != result && score > 255 / 2)) {
            previous_result = result;
            activity_timestamp = current_time + 500;
        } else if (current_time >= activity_timestamp) {
            activity_timestamp = 0;
            //  led_turn_off(ACTIVITY_LED);
        }

        if (activity_timestamp != 0) {
            if (current_time - activity_toggle_timestamp >= 100) {
                activity_toggle_timestamp = current_time;
                //  led_toggle(ACTIVITY_LED);
            }
        }
    }
}

/***************************************************************************/ /**
 * Get the label for a model output category.
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
    }
    return "?";
}

/***************************************************************************/ /**
 * Load both ML models and initialize audio processing.
 *
 ******************************************************************************/

sl_status_t load_model() {
    printf("Loading model\n");
    if (sl_ml_model_init(&sl_ml_keyword_spotting_on_off_v3_model_handle) != SL_STATUS_OK) {
        printf("Failed to load model\n");
        return SL_STATUS_FAIL;
    }

    // Initialize audio feature generation from the keyword model metadata.
    if (!sl_ml_audio_feature_generation_load_model_settings(sl_ml_keyword_spotting_on_off_v3_model_handle.flatbuffer)) {
        printf("Failed to load audio feature generator parameters from .tflite model\n");
        return SL_STATUS_FAIL;
    }

    // Load application-specific keyword detection parameters.
    if (!audio_classifier_config_load_model_settings(sl_ml_keyword_spotting_on_off_v3_model_handle.flatbuffer)) {
        printf("Failed to load app parameters from .tflite model\n");
        return SL_STATUS_FAIL;
    }

    const TfLiteTensor* input = sl_ml_keyword_spotting_on_off_v3_model_handle.input_tensor(0);
    const TfLiteTensor* output = sl_ml_keyword_spotting_on_off_v3_model_handle.output_tensor(0);

    // Validate model tensor shapes and types.
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

    // Start the audio feature generator.
    if (sl_ml_audio_feature_generation_init(&sl_fe_audio_keyword_spotting_on_off_v3_cfg) != SL_STATUS_OK) {
        printf("Failed to init audio feature generator\n");
        return SL_STATUS_FAIL;
    }

    // Instantiate the command recognizer in static storage.
    command_recognizer = new (command_recognizer_instance_buffer) RecognizeCommands(SMOOTHING_WINDOW_DURATION_MS,
                                                                                    DETECTION_THRESHOLD,
                                                                                    SUPPRESION_TIME_MS,
                                                                                    MINIMUM_DETECTION_COUNT,
                                                                                    IGNORE_UNDERSCORE_LABELS);

    // Keep the sine blink model initialized for concurrent model execution.
    if (sl_ml_model_init(&sl_ml_blink_model_handle) != SL_STATUS_OK) {
        printf("Failed to load blink model\n");
        return SL_STATUS_FAIL;
    }

    return SL_STATUS_OK;
}
