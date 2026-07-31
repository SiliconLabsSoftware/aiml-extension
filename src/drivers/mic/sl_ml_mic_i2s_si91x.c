/***************************************************************************//**
 * @file
 * @brief I2S microphone driver
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
#include <stddef.h>
#include <stdint.h>
#include <inttypes.h>
#include <string.h>
#include <assert.h>
#include <stdio.h>
#include "sl_ml_mic_i2s_si91x.h"
#include "RTE_Device_917.h"
#include "rsi_i2s.h"
#include "sl_si91x_i2s.h"
#include "sl_si91x_dma.h"
#include "rsi_udma_wrapper.h"
#include "rsi_rom_udma_wrapper.h"
#include "sl_si91x_i2s_config.h"


#ifndef MIN
#define MIN(a, b) ((a) < (b) ? (a) : (b))
#endif

extern UDMA_RESOURCES UDMA0_Resources;
extern UDMA_Channel_Info udma0_chnl_info[32];
extern RSI_UDMA_HANDLE_T udmaHandle0;



#if defined(SL_I2S0_PERIPHERAL)
#define I2S_INSTANCE            0
#define I2S_PERIPHERAL          SL_I2S0_PERIPHERAL
#define UDMA_RX_CH              RTE_I2S0_CHNL_UDMA_RX_CH
#elif defined(SL_ULP_I2S_PERIPHERAL)
#define I2S_INSTANCE            1
#define I2S_PERIPHERAL          I2S1
#define UDMA_RX_CH              RTE_I2S1_CHNL_UDMA_RX_CH
#if !defined(ULP_I2S_SCLK_LOC)
#error "ULP I2S selected but pin_config.h is missing ULP_I2S pin defines. " \
       "See readme.md 'Required: pin_config.h for ULP I2S' section."
#endif
#else
#error "Either SL_I2S0_PERIPHERAL or SL_ULP_I2S_PERIPHERAL must be defined"
#endif


#define N_FRAMES_PER_CALLBACK (1024/2) 

typedef enum { RECEIVE_DATA, WAIT_STATE, INVALID_STATE } transfer_state_t;

static struct
{

  sl_i2s_handle_t handle;

  struct
  {
    int16_t*                      base;
    int16_t*                      end;
    int16_t*                      ptr;
  } sample_buffer;

  int16_t dma_buffer[N_FRAMES_PER_CALLBACK*2];

  uint32_t                        sample_rate;
  sl_mic_buffer_ready_callback_t  callback;

  uint8_t                         n_channels;
  bool                            initialized;
  bool                            is_streaming;
  uint32_t                        transfer_count;
  transfer_state_t                state;
  volatile bool                   receive_complete;

} mic_context =
{
  .initialized = false,
  .is_streaming = false,
  .transfer_count = 0,
  .state = RECEIVE_DATA,
  .receive_complete = false
};


void i2s_event_handler2(uint32_t event);
sl_status_t sl_ml_mic_process_action(void);
#if defined(SL_I2S0_PERIPHERAL)
static sl_status_t i2s_receive_data_i2s0(void);
static void i2s_dma_callback(uint32_t event, uint32_t ch)
{
  (void)ch;
  i2s_event_handler2(event);
}
#endif


/***************************************************************************//**
 *    Initializes the microphone
 ******************************************************************************/
sl_status_t sl_ml_mic_init(uint32_t sample_rate, uint8_t n_channels)
{
  sl_status_t status;

  if (!(n_channels == 1 || n_channels == 2)) {
    return SL_STATUS_INVALID_PARAMETER;
  }
  if (mic_context.initialized) {
    return SL_STATUS_INVALID_STATE;
  }

  status = sl_si91x_i2s_init(
    I2S_INSTANCE,
    &mic_context.handle
  );
  if(status != SL_STATUS_OK)
  {
    return status;
  }

  status = sl_si91x_i2s_configure_power_mode(
    mic_context.handle,
    SL_I2S_FULL_POWER
  );
  if(status != SL_STATUS_OK)
  {
    return status;
  }

  // Register event callback handler
  status = sl_si91x_i2s_register_event_callback(mic_context.handle, i2s_event_handler2);
  if(status != SL_STATUS_OK)
  {
    return status;
  }

  /* Driver parameters */
  mic_context.n_channels = n_channels;
  mic_context.sample_rate = sample_rate;
  mic_context.initialized = true;


  return SL_STATUS_OK;
}

/***************************************************************************//**
 *    De-initializes the microphone
 ******************************************************************************/
sl_status_t sl_ml_mic_deinit(void)
{
  /* Stop sampling */
  sl_status_t status = sl_ml_mic_stop();
  if (status != SL_STATUS_OK) {
    return status;
  }
  status = sl_si91x_i2s_deinit_v2(mic_context.handle);
  if (status != SL_STATUS_OK) {
    return status;
  }
  mic_context.initialized = false;
  return SL_STATUS_OK;
}


/***************************************************************************//**
 *    Start streaming
 ******************************************************************************/
sl_status_t sl_ml_mic_start_streaming(
  void *buffer,
  uint32_t n_frames,
  sl_mic_buffer_ready_callback_t callback
)
{
  sl_status_t status;


  if (!mic_context.initialized) {
    return SL_STATUS_NOT_INITIALIZED;
  }

  if (mic_context.is_streaming) {
    return SL_STATUS_INVALID_STATE;
  }

  mic_context.callback = callback;

  // Store buffer info for potential future use
  const uint32_t sample_length = n_frames * mic_context.n_channels;
  mic_context.sample_buffer.base = (int16_t*)buffer;
  mic_context.sample_buffer.ptr = mic_context.sample_buffer.base;
  mic_context.sample_buffer.end = mic_context.sample_buffer.ptr +  sample_length*2;

  sl_i2s_xfer_config_t i2s_xfer_config;
  i2s_xfer_config.mode          = SL_I2S_MASTER;
  i2s_xfer_config.protocol      = SL_I2S_PROTOCOL;
  #if defined(SL_I2S0_PERIPHERAL)
  i2s_xfer_config.resolution    = SL_I2S_RESOLUTION_16;
  i2s_xfer_config.sampling_rate = mic_context.sample_rate;
  #elif defined(SL_ULP_I2S_PERIPHERAL)
  i2s_xfer_config.resolution    = SL_ULP_I2S_RESOLUTION;
  i2s_xfer_config.sampling_rate = mic_context.sample_rate;
  #endif
  i2s_xfer_config.sync          = SL_I2S_ASYNC;
  i2s_xfer_config.data_size     = SL_I2S_DATA_SIZE16;
  i2s_xfer_config.transfer_type = SL_MIC_ICS43434_RECEIVE;


  status = sl_si91x_i2s_config_transmit_receive(
    mic_context.handle,
    &i2s_xfer_config
  );

  if (status != SL_STATUS_OK) {
    printf("ERROR: I2S config failed with status: %" PRIu32 "\n", (uint32_t)status);
    return status;
  }

  printf("SUCCESS: I2S configuration completed\n");

  #if defined(SL_I2S0_PERIPHERAL)
  // For I2S0, configure interrupt masks 
  I2S_PERIPHERAL->CHANNEL_CONFIG[0].I2S_IMR &= ~F_RXDAM;
  I2S_PERIPHERAL->CHANNEL_CONFIG[0].I2S_IMR |= F_RXFOM;

  #elif defined(SL_ULP_I2S_PERIPHERAL)
  // For ULP I2S, no manual interrupt configuration needed (handled by driver)
  #endif

  #if defined(SL_I2S0_PERIPHERAL)
  status = i2s_receive_data_i2s0();
  #elif defined(SL_ULP_I2S_PERIPHERAL)
  status = SL_STATUS_OK;
  #endif

  if (status == SL_STATUS_OK) {
    mic_context.is_streaming = true;
    mic_context.transfer_count = 0;
    mic_context.state = RECEIVE_DATA;
    mic_context.receive_complete = false;
  } else {
    printf("ERROR: ULP I2S receive start failed with status: %" PRIu32 "\n", (uint32_t)status);
  }

  return status;
}

/***************************************************************************//**
 *    Process action - state machine
 ******************************************************************************/

#if defined(SL_ULP_I2S_PERIPHERAL)
// Each DMA transfer is 1024 samples at 16kHz = 64ms.
// At typical polling rate (~100us/call), 5000 polls ≈ 500ms >> 64ms.
#define WAIT_STATE_TIMEOUT_MS  5000
static uint32_t wait_state_polls = 0;

static void process_received_data(void)
{
  if (!mic_context.is_streaming || mic_context.callback == NULL) {
    return;
  }

  int16_t* ptr = mic_context.sample_buffer.ptr;
  const int16_t* src = mic_context.dma_buffer;
  const int length_to_end = (int)(mic_context.sample_buffer.end - mic_context.sample_buffer.ptr);
  const int chunk_size = MIN(N_FRAMES_PER_CALLBACK * 2, length_to_end);
  const int remaining_size = (N_FRAMES_PER_CALLBACK * 2) - chunk_size;

  memcpy(ptr, src, chunk_size * sizeof(int16_t));
  mic_context.callback(ptr, chunk_size);

  if (remaining_size > 0) {
    memcpy(mic_context.sample_buffer.base, src + chunk_size, remaining_size * sizeof(int16_t));
    mic_context.callback(mic_context.sample_buffer.base, remaining_size);
    mic_context.sample_buffer.ptr = mic_context.sample_buffer.base + remaining_size;
  } else {
    mic_context.sample_buffer.ptr = ptr + chunk_size;
    if (mic_context.sample_buffer.ptr >= mic_context.sample_buffer.end) {
      mic_context.sample_buffer.ptr = mic_context.sample_buffer.base;
    }
  }
}

static sl_status_t start_ulp_receive(void)
{
  sl_status_t status = sl_si91x_i2s_receive_data(
    mic_context.handle,
    (uint16_t *)mic_context.dma_buffer,
    N_FRAMES_PER_CALLBACK * 2
  );
  if (status != SL_STATUS_OK) {
    return status;
  }
  mic_context.state = WAIT_STATE;
  wait_state_polls = 0;
  return SL_STATUS_OK;
}
#endif /* SL_ULP_I2S_PERIPHERAL */

sl_status_t sl_ml_mic_process_action(void)
{
  if (!mic_context.is_streaming) {
    return SL_STATUS_INVALID_STATE;
  }

#if defined(SL_I2S0_PERIPHERAL)
  /* I2S0 uses UDMA completion in i2s_event_handler; do not use the HAL receive path here. */
  return SL_STATUS_OK;
#elif defined(SL_ULP_I2S_PERIPHERAL)

  switch (mic_context.state) {
    case RECEIVE_DATA: {
      sl_status_t status = start_ulp_receive();
      if (status != SL_STATUS_OK) {
        printf("ERROR: Failed to start I2S receive, status: %" PRIu32 "\n", (uint32_t)status);
        mic_context.state = INVALID_STATE;
        return status;
      }
      break;
    }

    case WAIT_STATE:
      if (mic_context.receive_complete) {
        process_received_data();

        sl_si91x_i2s_end_transfer(mic_context.handle, SL_I2S_RECEIVE_ABORT);
        mic_context.receive_complete = false;

        sl_status_t status = start_ulp_receive();
        if (status != SL_STATUS_OK) {
          printf("ERROR: Failed to restart I2S receive, status: %" PRIu32 "\n", (uint32_t)status);
          mic_context.state = RECEIVE_DATA;
        }
      } else if (++wait_state_polls > WAIT_STATE_TIMEOUT_MS) {
        printf("WARNING: ULP I2S DMA timeout, restarting\n");
        sl_si91x_i2s_end_transfer(mic_context.handle, SL_I2S_RECEIVE_ABORT);
        mic_context.receive_complete = false;
        mic_context.state = RECEIVE_DATA;
      }
      break;

    case INVALID_STATE:
      sl_si91x_i2s_end_transfer(mic_context.handle, SL_I2S_RECEIVE_ABORT);
      mic_context.receive_complete = false;
      mic_context.state = RECEIVE_DATA;
      break;

    default:
      mic_context.state = RECEIVE_DATA;
      break;
  }

  return SL_STATUS_OK;

#endif /* SL_ULP_I2S_PERIPHERAL */
}


/***************************************************************************//**
 *    Stops the microphone
 ******************************************************************************/
sl_status_t sl_ml_mic_stop(void)
{
  if (!mic_context.is_streaming) {
    return SL_STATUS_INVALID_STATE;
  }

  #if defined(SL_I2S0_PERIPHERAL)
  // For I2S0, disable DMA and I2S peripheral directly
  // Disable I2S peripheral
  if (I2S_PERIPHERAL->I2S_CER_b.CLKEN) {
    I2S_PERIPHERAL->I2S_CER_b.CLKEN = DISABLE;
    I2S_PERIPHERAL->I2S_IRER_b.RXEN = 0x0;
    I2S_PERIPHERAL->CHANNEL_CONFIG[0].I2S_RER_b.RXCHEN = 0x0;
  }

  // Disable DMA channel
  UDMAx_ChannelDisable(UDMA_RX_CH, &UDMA0_Resources, udmaHandle0);
  #elif defined(SL_ULP_I2S_PERIPHERAL)

  sl_si91x_i2s_end_transfer(mic_context.handle, SL_I2S_RECEIVE_ABORT);
  #endif

  mic_context.is_streaming = false;

  return SL_STATUS_OK;
}



void i2s_event_handler2(uint32_t event)
{
  (void)event;
  #if defined(SL_I2S0_PERIPHERAL)
  if (mic_context.is_streaming && mic_context.callback != NULL) {
    int16_t* ptr = mic_context.sample_buffer.ptr;
    const int16_t* src = mic_context.dma_buffer;
    const int length_to_end = (int)(mic_context.sample_buffer.end - mic_context.sample_buffer.ptr);
    const int chunk_size = MIN(N_FRAMES_PER_CALLBACK * 2, length_to_end);
    const int remaining_size = (N_FRAMES_PER_CALLBACK * 2) - chunk_size;

    memcpy(ptr, src, chunk_size * sizeof(int16_t));
    mic_context.callback(ptr, chunk_size);

    if (remaining_size > 0) {
      memcpy(mic_context.sample_buffer.base, src + chunk_size, remaining_size * sizeof(int16_t));
      mic_context.callback(mic_context.sample_buffer.base, remaining_size);
      mic_context.sample_buffer.ptr = mic_context.sample_buffer.base + remaining_size;
    } else {
      mic_context.sample_buffer.ptr = ptr + chunk_size;
      if (mic_context.sample_buffer.ptr >= mic_context.sample_buffer.end) {
        mic_context.sample_buffer.ptr = mic_context.sample_buffer.base;
      }
    }
  }

  // Restart DMA for I2S0
  if (mic_context.is_streaming) {
    i2s_receive_data_i2s0();
  }

  #elif defined(SL_ULP_I2S_PERIPHERAL)
  if (event == SL_I2S_RECEIVE_COMPLETE) {
    mic_context.transfer_count++;
    mic_context.receive_complete = true;
  }
  #endif
}


/***************************************************************************//**
 * @brief Internal: Start a DMA receive for I2S0
 * 
 * Configures and enables the DMA channel for I2S0 RX.
 * Returns SL_STATUS_OK on success, SL_STATUS_FAIL on error.
 ******************************************************************************/
#if defined(SL_I2S0_PERIPHERAL)
static sl_status_t i2s_receive_data_i2s0()
{
  RSI_UDMA_CHA_CONFIG_DATA_T dma_control =
  {
    .transferType = UDMA_MODE_BASIC,
    .nextBurst = 0,
    .rPower = ARBSIZE_1,
    .totalNumOfDMATrans = (N_FRAMES_PER_CALLBACK*2)-1, // Number of 16-bit transfers per callback
    .srcSize = SRC_SIZE_16,
    .srcInc  = SRC_INC_NONE,
    .dstSize = DST_SIZE_16,
    .dstInc  = DST_INC_16
  };

  // DMA channel configuration
  RSI_UDMA_CHA_CFG_T chnl_cfg =
  {
    .altStruct = 0,
    .burstReq  = 1,
    .channelPrioHigh = UDMA0_CHNL_PRIO_LVL,
    .dmaCh           = UDMA_RX_CH, //changed from RTE_I2S0_CHNL_UDMA_RX_CH to UDMA_RX_CH
    .periAck   = 0,
    .periphReq = 0,
    .reqMask   = 0
  };

  int stat;

  // Configure DMA channel for I2S RX
  stat = UDMAx_ChannelConfigure(
    &UDMA0_Resources,
    UDMA_RX_CH, //changed from RTE_I2S0_CHNL_UDMA_RX_CH to UDMA_RX_CH
    (uint32_t)&(I2S_PERIPHERAL->I2S_RXDMA),
    (uint32_t)mic_context.dma_buffer,
    N_FRAMES_PER_CALLBACK * 2,
    dma_control,
    &chnl_cfg,
    i2s_dma_callback,
    udma0_chnl_info,
    udmaHandle0
  );
  if(stat == -1)
  {
    assert(!"Failed to config dma");
    return SL_STATUS_FAIL;
  }

  // Enable DMA channel
  stat = UDMAx_ChannelEnable(
    UDMA_RX_CH, //changed from RTE_I2S0_CHNL_UDMA_RX_CH to UDMA_RX_CH
    &UDMA0_Resources,
    udmaHandle0
  );
  if(stat == -1)
  {
    assert(!"Failed to enable dma ch");
    return SL_STATUS_FAIL;
  }

  // Enable DMA controller
  stat = UDMAx_DMAEnable(
    &UDMA0_Resources,
    udmaHandle0
  );
  if(stat == -1)
  {
    assert(!"Failed to enable dma");
    return SL_STATUS_FAIL;
  }

  // Enable I2S peripheral if not already enabled
  if(!I2S_PERIPHERAL->I2S_CER_b.CLKEN)
  {
    I2S_PERIPHERAL->CHANNEL_CONFIG[0].I2S_RFCR_b.RXCHDT = 1; //CHANGED FROM I2S0 TO I2S_PERIPHERAL
    I2S_PERIPHERAL->CHANNEL_CONFIG[0].I2S_RER_b.RXCHEN = 0x1;
    I2S_PERIPHERAL->I2S_IRER_b.RXEN = 0x1;
    I2S_PERIPHERAL->I2S_CER_b.CLKEN = ENABLE;
  }

  return SL_STATUS_OK;
}
#endif