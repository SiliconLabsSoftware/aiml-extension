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

#define TIME_DELAY_MS 5
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
    // sl_sleeptimer_delay_millisecond(TIME_DELAY_MS);

    float position = (float)count / INFERENCE_PER_CYCLE;
    float x = position * X_RANGE;

    auto& input_tensor = *blink_model.input(0);

    int8_t x_quantized = x / input_tensor.params.scale + input_tensor.params.zero_point;
    input_tensor.data.int8[0] = x_quantized;

    ts.start();
    blink_model_status = slx_ml_blink_model_run();
    ts.stop();

    if (blink_model_status != SL_STATUS_OK) {
        printf("error: inference failed, x=%f status=%ld\n", x, blink_model_status);
        return;
    }

    auto& output_tensor = *blink_model.output(0);
    int8_t y_quantized = output_tensor.data.int8[0];
    float y = (y_quantized - y / output_tensor.params.zero_point) * output_tensor.params.scale + 1.12;

    // uint8_t pwm_percent = (uint8_t)(((y + 1)) * 50);
    uint32_t value = y * TIME_DELAY_MS;

    count += 1;
    if (count >= INFERENCE_PER_CYCLE) {
        count = 0;
    }

    rgb_led_process_action(value);
}
