/***************************************************************************//**
 * @file
 * @brief Inertial Measurement Unit driver
 *******************************************************************************
 * # License
 * <b>Copyright 2020 Silicon Laboratories Inc. www.silabs.com</b>
 *******************************************************************************
 *
 * SPDX-License-Identifier: Zlib
 *
 * The licensor of this software is Silicon Laboratories Inc.
 *
 * This software is provided 'as-is', without any express or implied
 * warranty. In no event will the authors be held liable for any damages
 * arising from the use of this software.
 *
 * Permission is granted to anyone to use this software for any purpose,
 * including commercial applications, and to alter it and redistribute it
 * freely, subject to the following restrictions:
 *
 * 1. The origin of this software must not be misrepresented; you must not
 *    claim that you wrote the original software. If you use this software
 *    in a product, an acknowledgment in the product documentation would be
 *    appreciated but is not required.
 * 2. Altered source versions must be plainly marked as such, and must not be
 *    misrepresented as being the original software.
 * 3. This notice may not be removed or altered from any source distribution.
 *
 ******************************************************************************/

#include <stdint.h>
#include <stdbool.h>
#include <stdio.h>
#include "sl_si91x_icm40627.h"
#include "sl_si91x_imu.h"
#include "sl_sleeptimer.h"
#include "sl_si91x_driver_gpio.h"
#include "sl_si91x_ssi.h"
#include "rsi_rom_clks.h"
#include "sl_si91x_clock_manager.h"

/** @cond DO_NOT_INCLUDE_WITH_DOXYGEN */

// IMU state and configuration
static uint8_t IMU_state = IMU_STATE_DISABLED; /**< IMU state variable */
static float sensorsSampleRate;                /**< Sensors sample rate */
static sl_ssi_handle_t ssi_driver_handle = NULL; /**< SSI driver handle */

// Data ready statistics
static uint32_t IMU_isDataReadyQueryCount = 0; /**< Total data ready queries */
static uint32_t IMU_isDataReadyTrueCount = 0;  /**< Queries when data is ready */

// Sensor fusion object
static sl_imu_sensor_fusion_t fuseObj;         /**< Structure to store sensor fusion data */

/** @endcond */

/*******************************************************************************
 **************************   GLOBAL FUNCTIONS   *******************************
 ******************************************************************************/

/**
 * @brief Initializes and calibrates the IMU
 * @param[in] handle SSI driver handle
 * @return SL_STATUS_OK on success, error code on failure
 */
sl_status_t sl_imu_init(sl_ssi_handle_t handle)
{
  float gyroBiasScaled[3];
  float accelBiasScaled[3];
  sl_status_t status = SL_STATUS_OK;

  ssi_driver_handle = handle;

  IMU_state = IMU_STATE_INITIALIZING;
  sl_imu_fuse_new(&fuseObj);

  // Initialize acc/gyro driver
  status = sl_si91x_icm40627_init(ssi_driver_handle);
  if (status != SL_STATUS_OK) {
    goto cleanup;
  }

  // Gyro/accel calibration
  IMU_state = IMU_STATE_CALIBRATING;
  status = sl_si91x_icm40627_calibrate_accel_and_gyro(ssi_driver_handle, accelBiasScaled, gyroBiasScaled);
  if (status != SL_STATUS_OK) {
    goto cleanup;
  }

cleanup:
  if (status != SL_STATUS_OK) {
    sl_si91x_icm40627_deinit();
    IMU_state = IMU_STATE_DISABLED;
  }

  return status;
}

/**
 * @brief De-initializes the IMU chip
 * @return SL_STATUS_OK on success, error code on failure
 */
sl_status_t sl_imu_deinit(void)
{
  sl_status_t status;

  IMU_state = IMU_STATE_DISABLED;
  status = sl_si91x_icm40627_deinit();

  return status;
}

/**
 * @brief Configures the IMU with specified sample rate
 * @param[in] sampleRate Desired sample rate in Hz
 * @param[in] handle SSI driver handle
 */
void sl_imu_configure(float sampleRate, sl_ssi_handle_t handle)
{
  uint32_t itStatus;

  ssi_driver_handle = handle;

  // Set IMU state
  IMU_state = IMU_STATE_INITIALIZING;

  // Enable accelerometer sensor
  sl_si91x_icm40627_enable_sensor(ssi_driver_handle, true, true, false);

  // Set sample rate
  sensorsSampleRate = sl_si91x_icm40627_set_sample_rate(ssi_driver_handle, sampleRate);

  // Filter bandwidth: 8kHz for accel, 12.5Hz for gyro
  sl_si91x_icm40627_set_accel_bandwidth(ssi_driver_handle, SL_ICM40627_ACCEL_BW_8000HZ);
  sl_si91x_icm40627_set_gyro_bandwidth(ssi_driver_handle, SL_ICM40627_GYRO_BW_12_5HZ);

  // Accelerometer: 2G full scale
  sl_si91x_icm40627_set_accel_full_scale(ssi_driver_handle, SL_ICM40627_ACCEL_FULLSCALE_2G);

  // Gyroscope: 250 degrees per sec full scale
  sl_si91x_icm40627_set_gyro_full_scale(ssi_driver_handle, SL_ICM40627_GYRO_FULLSCALE_250DPS);

  sl_sleeptimer_delay_millisecond(50);

  // Enable the raw data ready interrupt
  sl_si91x_icm40627_enable_interrupt(ssi_driver_handle, true, false);

  // Clear the interrupts
  sl_si91x_icm40627_read_interrupt_status(ssi_driver_handle, &itStatus);

  // IMU fuse config & setup
  sl_imu_fuse_accelerometer_set_sample_rate(&fuseObj, sensorsSampleRate);
  sl_imu_fuse_gyro_set_sample_rate(&fuseObj, sensorsSampleRate);

  IMU_state = IMU_STATE_READY;
  sl_imu_fuse_reset(&fuseObj);
}

/**
 * @brief Returns current IMU state
 * @return Current IMU state
 */
uint8_t sl_imu_get_state(void)
{
  return IMU_state;
}

/**
 * @brief Retrieves the processed acceleration data
 * @param[out] avec Array to store acceleration data (x, y, z)
 */
void sl_imu_get_acceleration(int16_t avec[3])
{
  if (fuseObj.aAccumulatorCount > 0) {
    avec[0] = (int16_t)(1000.0f * fuseObj.aAccumulator[0] / fuseObj.aAccumulatorCount);
    avec[1] = (int16_t)(1000.0f * fuseObj.aAccumulator[1] / fuseObj.aAccumulatorCount);
    avec[2] = (int16_t)(1000.0f * fuseObj.aAccumulator[2] / fuseObj.aAccumulatorCount);

    // Reset accumulator
    fuseObj.aAccumulator[0] = 0;
    fuseObj.aAccumulator[1] = 0;
    fuseObj.aAccumulator[2] = 0;
    fuseObj.aAccumulatorCount = 0;
  } else {
    avec[0] = 0;
    avec[1] = 0;
    avec[2] = 0;
  }
}

/**
 * @brief Retrieves the processed orientation data
 * @param[out] ovec Array to store orientation data (x, y, z) in degrees * 100
 */
void sl_imu_get_orientation(int16_t ovec[3])
{
  ovec[0] = (int16_t)(100.0f * (float)IMU_RAD_TO_DEG_FACTOR * fuseObj.orientation[0]);
  ovec[1] = (int16_t)(100.0f * (float)IMU_RAD_TO_DEG_FACTOR * fuseObj.orientation[1]);
  ovec[2] = (int16_t)(100.0f * (float)IMU_RAD_TO_DEG_FACTOR * fuseObj.orientation[2]);
}

/**
 * @brief Retrieves the processed gyroscope data
 * @param[out] gvec Array to store gyroscope data (x, y, z)
 */
void sl_imu_get_gyro(int16_t gvec[3])
{
  gvec[0] = (int16_t)(100.0f * fuseObj.gVector[0]);
  gvec[1] = (int16_t)(100.0f * fuseObj.gVector[1]);
  gvec[2] = (int16_t)(100.0f * fuseObj.gVector[2]);
}

/**
 * @brief Gets a new set of data from the accel and gyro sensor and updates the fusion calculation
 */
void sl_imu_update(void)
{
  sl_imu_fuse_update(&fuseObj);
}

/**
 * @brief Resets the fusion calculation
 */
void sl_imu_reset(void)
{
  sl_imu_fuse_reset(&fuseObj);
}

/**
 * @brief Retrieves the raw acceleration data from the IMU
 * @param[out] avec Array to store raw acceleration data (x, y, z)
 */
void sl_imu_get_acceleration_raw_data(float avec[3])
{
  if (IMU_state != IMU_STATE_READY) {
    avec[0] = 0;
    avec[1] = 0;
    avec[2] = 0;
    return;
  }

  sl_si91x_icm40627_get_accel_data(ssi_driver_handle, avec);
}

/**
 * @brief Retrieves the processed gyroscope correction angles
 * @param[out] acorr Array to store gyroscope correction angles (x, y, z)
 */
void sl_imu_get_gyro_correction_angles(float acorr[3])
{
  acorr[0] = fuseObj.angleCorrection[0];
  acorr[1] = fuseObj.angleCorrection[1];
  acorr[2] = fuseObj.angleCorrection[2];
}

/**
 * @brief Retrieves the raw gyroscope data from the IMU
 * @param[out] gvec Array to store raw gyroscope data (x, y, z)
 */
void sl_imu_get_gyro_raw_data(float gvec[3])
{
  if (IMU_state != IMU_STATE_READY) {
    gvec[0] = 0;
    gvec[1] = 0;
    gvec[2] = 0;
    return;
  }

  sl_si91x_icm40627_get_gyro_data(ssi_driver_handle, gvec);
}

/**
 * @brief Checks if there is new accel/gyro data available for read
 * @param[in] handle SSI driver handle
 * @return true if data is ready, false otherwise
 */
bool sl_imu_is_data_ready(sl_ssi_handle_t handle)
{
  bool ready;

  ssi_driver_handle = handle;

  if (IMU_state != IMU_STATE_READY) {
    return false;
  }

  ready = sl_si91x_icm40627_is_data_ready(ssi_driver_handle);
  IMU_isDataReadyQueryCount++;

  if (ready) {
    IMU_isDataReadyTrueCount++;
  }

  return ready;
}

/**
 * @brief Performs gyroscope calibration to cancel gyro bias
 * @param[in] handle SSI driver handle
 * @return SL_STATUS_OK on success, error code on failure
 */
sl_status_t sl_imu_calibrate_gyro(sl_ssi_handle_t handle)
{
  sl_status_t status = SL_STATUS_OK;

  ssi_driver_handle = handle;

  // Disable interrupt
  sl_si91x_icm40627_enable_interrupt(ssi_driver_handle, false, false);

  sl_imu_deinit();
  status = sl_imu_init(ssi_driver_handle);

  // Restart regular sampling
  sl_imu_configure(sensorsSampleRate, ssi_driver_handle);

  return status;
}
