/***************************************************************************//**
 * @file
 * @brief Constants for use in the sample application
 *******************************************************************************
 * # License
 * <b>Copyright 2022 Silicon Laboratories Inc. www.silabs.com</b>
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

#ifndef CONSTANTS_H
#define CONSTANTS_H

// The expected accelerometer data sample frequency
#define IMU_FREQ      25
#define IMU_CHANNELS   9

// LEDs are active for this amount of time before they are turned off
#define TOGGLE_DELAY_MS       2000

// Inference it triggered by a periodic timer, this configuration is the time
// between each trigger
#define INFERENCE_PERIOD_MS    100
#define SCENARIO_COUNT  8

// Length of the accelerator input sequence expected by the model
#define SEQUENCE_LENGTH         100
// What gestures are supported.
#define NO_ANOMALY_TILT            5
#define NO_ANOMALY_YAW             4
#define ANOMALY_JOLT_DROP          3
#define NO_ANOMALY_MAGNETOMETER    6
#define ANOMALY_PICK_UP             2
#define ANOMALY_SHAKING_VIRBATING  1
#define NO_ANOMALY_STATIONERY      7
#define ANOMALY_TILTING            0

#define SCENARIO_COUNT  8
// These control the sensitivity of the detection algorithm. If you're seeing
// too many false positives or not enough true positives, you can try tweaking
// these thresholds. Often, increasing the size of the training set will give
// more robust results though, so consider retraining if you are seeing poor
// predictions.
#define DETECTION_THRESHOLD   0.25f
#define PREDICTION_HISTORY_LEN   3
#define PREDICTION_SUPPRESSION  15
static const float MEAN[IMU_CHANNELS] = {
    227.69133f ,  -667.4827f  , -3059.248f   ,  -314.01733f ,
          -2405.7383f  ,    -9.485798f,  -225.23122f ,   347.62112f ,
           -772.58777f
};

static const float STD[IMU_CHANNELS] = {
    2053.2402f, 1779.2793f, 7346.2764f, 3125.351f , 9199.504f ,  403.066f ,
            688.0491f,  556.1538f, 2188.2158f
};
#endif // CONSTANTS_H
