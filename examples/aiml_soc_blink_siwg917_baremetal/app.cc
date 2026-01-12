/***************************************************************************/ /**
* @file app.c
* @brief Top level application functions
*******************************************************************************
* # License
* <b>Copyright 2024 Silicon Laboratories Inc. www.silabs.com</b>
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
#include "app.h"

#include <stdio.h>

#include "rgb_led.h"
#include "sl_ml_model_blink.h"
#include "sl_sleeptimer.h"
#include "timestamp.h"

static int count = 0;

#define INFERENCE_PER_CYCLE (1000 / TIME_DELAY_MS)
#define X_RANGE (2.f * 3.14159265359f)

/*Delay for PWM simulation*/
#ifndef TICK_DELAY
    #define TICK_DELAY 30
#endif

#ifndef PULSE_PERIOD
    #define PULSE_PERIOD (TICK_DELAY * 0xFF)
#endif

// static void handle_output(timestamp& ms, float x, float y);
// static uint32_t pwm_percentagetoticks(uint8_t percent);

/*******************************************************************************
 * Initialize application.
 ******************************************************************************/
void app_init(void) {
    printf("Starting ML Blink Application\n");

    rgb_led_init();

    blink_model_status = slx_ml_blink_model_init();
    if (blink_model_status != SL_STATUS_OK) {
        printf("error: Failed to load model\n");
        for (;;)
            ;
    } else {
        printf("Model loaded successfully\n");
        printf("Rest of the output will only print if an error occurs\n");
    }
}

/*******************************************************************************
 * App ticking function.
 ******************************************************************************/
void app_process_action(void) {
    if (blink_model_status != SL_STATUS_OK) {
        printf("error: blink model is not initialized\n");
        return;
    }

    timestamp ts;

    float position = (float)count / INFERENCE_PER_CYCLE;
    float x = position * X_RANGE;

    auto& input_tensor = *blink_model.input(0);

    int8_t x_quantized = x / input_tensor.params.scale + input_tensor.params.zero_point;
    input_tensor.data.int8[0] = x_quantized;

    ts.start();
    blink_model_status = slx_ml_blink_model_run();
    ts.stop();

    if (blink_model_status != SL_STATUS_OK) {
        printf("error: inference failed, x=%f status=%d\n", x, (int)blink_model_status);
        return;
    }

    auto& output_tensor = *blink_model.output(0);
    int8_t y_quantized = output_tensor.data.int8[0];
    float y = (y_quantized - output_tensor.params.zero_point) * output_tensor.params.scale;

    float brightness = (y + 1.0f) * 0.5f;  // Map [-1, +1] to [0, 1]
    uint32_t value = (uint32_t)(TIME_DELAY_MS * (1.0f - brightness));
  
    count += 1;
    if (count >= INFERENCE_PER_CYCLE) {
        count = 0;
    }

    //printf("x=%f y=%f (%.3f ms)\n", x, y, ts.duration_ms());
    rgb_led_process_action(value);
}
