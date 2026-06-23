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

#include "app.h"
#include "FreeRTOS.h"
#include "task.h"

#include <cstdio>
#include <new>

#include "sl_fe_audio_instances.h"
#include "sl_fe_audio_si91x.h"
#include "recognize_commands.h"
#include "sl_ml_model_keyword_spotting_on_off_v3.h"
#include "tflite_micro_model.hpp"

static RecognizeCommands* command_recognizer = nullptr;
static uint8_t command_recognizer_instance_buffer[sizeof(RecognizeCommands)] alignas(alignof(RecognizeCommands));
static int32_t detected_timeout = 0;
static int32_t activity_timestamp = 0;
static uint8_t previous_score = 0;
static int32_t previous_score_timestamp = 0;
static int previous_result = 0;
int category_count = 0;

/** Block until the app task drains staged UDP and xTaskNotifyGive() completes the handshake.
 *  Matches task.h guidance: with xTaskNotifyGive (eIncrement), wait with ulTaskNotifyTake,
 *  not xTaskNotifyWait (see Silicon Labs simplicty_sdk FreeRTOS task.h ~2942–2945).
 *  When configUSE_TASK_NOTIFICATIONS is 0: vTaskResume + vTaskSuspend (no xTaskSuspend in FreeRTOS). */
static void voice_wait_app_udp_drained_freertos(void) {
    const TaskHandle_t app_task = static_cast<TaskHandle_t>(app_freertos_app_task_handle_ptr());
    const TaskHandle_t voice_task = static_cast<TaskHandle_t>(app_freertos_voice_task_handle_ptr());
    if (app_task == NULL || voice_task == NULL || xTaskGetCurrentTaskHandle() != voice_task) {
        return;
    }
#if (configUSE_TASK_NOTIFICATIONS == 1)
    (void)ulTaskNotifyTake(pdTRUE, portMAX_DELAY);
#else
    vTaskResume(app_task);
    vTaskSuspend(NULL);
#endif
}

void voice_control_light_task_cpp() {
    uint32_t prev_loop_timestamp = 0;
    led_turn_off(DETECTION_ON);
    printf("Starting Voice Control Light Application\n");

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

        /* Send UDP from app thread only: stage completed in handle_results(); wait here (not inside ML callbacks). */
        if (app_udp_notify_is_pending()) {
            voice_wait_app_udp_drained_freertos();
        }

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
    sl_status_t status =
        sl_ml_audio_feature_generation_fill_tensor(sl_ml_keyword_spotting_on_off_v3_model_handle.input_tensor(0));
    if (status != SL_STATUS_OK) {
        return SL_STATUS_FAIL;
    }

    // Run the model on the spectrogram input and make sure it succeeds.
    if (sl_ml_model_run(&sl_ml_keyword_spotting_on_off_v3_model_handle) != SL_STATUS_OK) {
        return SL_STATUS_FAIL;
    }

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
            sl_ml_keyword_spotting_on_off_v3_model_handle.output_tensor(0), current_timestamp, &result, &score, &is_new_command);

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

        printf("Detected class=%d label=%s score=%d @%ldms\n",
               result,
               label,
               (int)score,
               (long)current_time);

        char udp_msg[160];
        int n = snprintf(udp_msg,
                         sizeof(udp_msg),
                         "Detected class=%d label=%s score=%d @%ldms\n",
                         result,
                         label,
                         (int)score,
                         (long)current_time);
        if (n > 0) {
            size_t len = (size_t)n;
            if (len >= sizeof(udp_msg)) {
                len = sizeof(udp_msg) - 1U;
            }
            app_udp_notify_stage(udp_msg, len);
        }

        if (result == 0)

            led_turn_on(DETECTION_ON);

        else if (result == 1)

            led_turn_off(DETECTION_ON);

        activity_timestamp = 0;
        detected_timeout = current_time + 1200;
    } else if (detected_timeout != 0 && current_time >= detected_timeout) {
        detected_timeout = 0;
        previous_score = score;
        previous_result = result;
        previous_score_timestamp = current_time;
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
        const int16_t score_delta = (int16_t)((int)score - (int)previous_score);
        const int16_t abs_delta = (score_delta < 0) ? -score_delta : score_delta;
        const float diff = (time_delta > 0) ? (float)abs_delta / time_delta : 0.0f;

        previous_score = score;
        previous_score_timestamp = current_time;

        if (diff >= SENSITIVITY || (previous_result != result && score > 255 / 2)) {
            previous_result = result;
            activity_timestamp = current_time + 500;
        } else if (current_time >= activity_timestamp) {
            activity_timestamp = 0;
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
    printf("Loading model\n");
    if (sl_ml_model_init(&sl_ml_keyword_spotting_on_off_v3_model_handle) != SL_STATUS_OK) {
        printf("Failed to load model\n");
        return SL_STATUS_FAIL;
    }

    // Initialize the audio feature generation using the parameters
    // from the given .tflite model file
    if (!sl_ml_audio_feature_generation_load_model_settings(sl_ml_keyword_spotting_on_off_v3_model_handle.flatbuffer)) {
        printf("Failed to load audio feature generator parameters from .tflite model\n");
        return SL_STATUS_FAIL;
    }

    // Load the other application-specific model parameters from the .tflite
    if (!audio_classifier_config_load_model_settings(sl_ml_keyword_spotting_on_off_v3_model_handle.flatbuffer)) {
        printf("Failed to load app parameters from .tflite model\n");
        return SL_STATUS_FAIL;
    }

    const TfLiteTensor* input = sl_ml_keyword_spotting_on_off_v3_model_handle.input_tensor(0);
    const TfLiteTensor* output = sl_ml_keyword_spotting_on_off_v3_model_handle.output_tensor(0);

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
    if (sl_ml_audio_feature_generation_init(&sl_fe_audio_keyword_spotting_on_off_v3_cfg) != SL_STATUS_OK) {
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