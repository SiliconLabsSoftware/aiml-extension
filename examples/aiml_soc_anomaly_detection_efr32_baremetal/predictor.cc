/***************************************************************************//**
 * @file
 * @brief Averaging and evaluation of model output for prediction of gesture
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
#include "predictor.h"
#include "constants.h"
#include <cstdio>

// State for the averaging algorithm we're using.
static model_output_t history[PREDICTION_HISTORY_LEN] = {};
static int history_index = 0;
static int suppression_count = 0;
static int previous_prediction = NO_SCENARIO;

int predict_scenario(const model_output_t* output)
{
  // Record latest in circular buffer
  history[history_index] = *output;
  if (++history_index >= PREDICTION_HISTORY_LEN) history_index = 0;

  // Average scores over history
  int   max_idx = -1;
  float max_avg = 0.0f;

  for (int i = 0; i < SCENARIO_COUNT; ++i) {
    float sum = 0.0f;
    for (int j = 0; j < PREDICTION_HISTORY_LEN; ++j) {
      sum += history[j].scenario[i];
    }
    float avg = sum / (float)PREDICTION_HISTORY_LEN;
    if (max_idx < 0 || avg > max_avg) {
      max_idx = i;
      max_avg = avg;
    }
  }

#if DEBUG_LOGGING
  printf("avg top: %d @ %f\n", max_idx, max_avg);
#endif

  // Cooldown window to avoid rapid repeats
  if (suppression_count > 0) --suppression_count;

  // If confidence too low or still cooling down on same class → suppress
  if (max_idx < 0 ||
      max_avg < DETECTION_THRESHOLD ||
      (max_idx == previous_prediction && suppression_count > 0)) {
    return NO_SCENARIO;
  }

  // Accept new scenario
  suppression_count = PREDICTION_SUPPRESSION;
  previous_prediction = max_idx;
  return max_idx;
}
