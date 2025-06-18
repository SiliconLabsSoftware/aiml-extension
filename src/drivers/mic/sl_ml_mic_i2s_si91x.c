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
#include <stdlib.h>
#include <assert.h>
#include <stdio.h>
#include <math.h>
#include "sl_ml_mic_i2s_si91x.h"
#include "RTE_Device_917.h"
#include "rsi_i2s.h"
#include "sl_si91x_i2s.h"
#include "sl_si91x_dma.h"
#include "rsi_udma_wrapper.h"
#include "rsi_rom_udma_wrapper.h"


extern UDMA_RESOURCES UDMA0_Resources;
extern UDMA_Channel_Info udma0_chnl_info[32];
extern RSI_UDMA_HANDLE_T udmaHandle0;

#define I2S_INSTANCE            0         // I2S instance

#ifndef MIN
#define MIN(x,y)  ((x) < (y) ? (x) : (y))
#endif
#ifndef MAX
#define MAX(x,y)  ((x) > (y) ? (x) : (y))
#endif


#define N_FRAMES_PER_CALLBACK (1024/2)

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
  bool initialized;
  bool is_streaming;

} mic_context =
{
  .initialized = false,
  .is_streaming = false
};

static sl_status_t i2s_receive_data();
void i2s_event_handler(uint32_t event, uint32_t ch);
void i2s_event_handler2(uint32_t event);


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
  sl_ml_mic_stop();
  mic_context.initialized = false;
  sl_si91x_i2s_deinit(&mic_context.handle);

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

  const uint32_t sample_length = n_frames * mic_context.n_channels;
  mic_context.sample_buffer.base = (int16_t*)buffer;
  mic_context.sample_buffer.ptr = mic_context.sample_buffer.base;
  mic_context.sample_buffer.end = mic_context.sample_buffer.ptr + sample_length*2;

  sl_i2s_xfer_config_t i2s_xfer_config;
  i2s_xfer_config.mode          = SL_I2S_MASTER;
  i2s_xfer_config.protocol      = SL_I2S_PROTOCOL;
  i2s_xfer_config.resolution    = SL_I2S_RESOLUTION_16;
  i2s_xfer_config.sampling_rate = mic_context.sample_rate;
  i2s_xfer_config.sync          = SL_I2S_ASYNC;
  i2s_xfer_config.data_size     = SL_I2S_DATA_SIZE16;
  i2s_xfer_config.transfer_type = SL_MIC_ICS43434_RECEIVE;

  status = sl_si91x_i2s_config_transmit_receive(
    mic_context.handle,
    &i2s_xfer_config
  );
  I2S0->CHANNEL_CONFIG[0].I2S_IMR &= ~F_RXDAM;
  I2S0->CHANNEL_CONFIG[0].I2S_IMR |= F_RXFOM;

  if (status != SL_STATUS_OK) {
    return status;
  }

  status = i2s_receive_data();

  return SL_STATUS_OK;
}


/***************************************************************************//**
 *    Stops the microphone
 ******************************************************************************/
sl_status_t sl_ml_mic_stop(void)
{
  sl_si91x_i2s_end_transfer(mic_context.handle, SL_I2S_SEND_ABORT);
  mic_context.is_streaming = false;

  return SL_STATUS_OK;
}


void i2s_event_handler2(uint32_t event)
{
  i2s_event_handler(0, 0);
}

void i2s_event_handler(uint32_t event, uint32_t ch)
{

  if(mic_context.n_channels == 2||mic_context.n_channels == 1)
    {
    int16_t* ptr = mic_context.sample_buffer.ptr;
    const int16_t* src = mic_context.dma_buffer;
    int16_t* dst = mic_context.sample_buffer.ptr;
    const int length_to_end = (int)(mic_context.sample_buffer.end - mic_context.sample_buffer.ptr);
    const int chunk_size = MIN(N_FRAMES_PER_CALLBACK*2, length_to_end);
    const int remaining_size = (N_FRAMES_PER_CALLBACK*2) - length_to_end;

    for(int i = 0; i < chunk_size; ++i)
    {
      *dst++ = *src++;

    }

    if(mic_context.callback != NULL)
    {
      mic_context.callback(ptr, chunk_size);
    }

    if(remaining_size >= 0)
    {
      dst = mic_context.sample_buffer.base;
      for(int i = 0; i < remaining_size; ++i)
      {
        *dst++ = *src++;
      }

      if(mic_context.callback != NULL)
      {
        mic_context.callback(mic_context.sample_buffer.base, remaining_size);
      }
    }
    mic_context.sample_buffer.ptr = dst;
  }
  else
  {
    assert(!"> 2 channels not supported");
  }

  i2s_receive_data();
}

static sl_status_t i2s_receive_data()
{
  RSI_UDMA_CHA_CONFIG_DATA_T dma_control =
  {
    .transferType = UDMA_MODE_BASIC,
    .nextBurst = 0,
    .rPower = ARBSIZE_1,
    .totalNumOfDMATrans = (N_FRAMES_PER_CALLBACK*2)-1,
    .srcSize = SRC_SIZE_16,
    .srcInc  = SRC_INC_NONE,
    .dstSize = DST_SIZE_16,
    .dstInc  = DST_INC_16
  };
  RSI_UDMA_CHA_CFG_T chnl_cfg =
  {
    .altStruct = 0,
    .burstReq  = 1,
    .channelPrioHigh = UDMA0_CHNL_PRIO_LVL,
    .dmaCh           = RTE_I2S0_CHNL_UDMA_RX_CH,
    .periAck   = 0,
    .periphReq = 0,
    .reqMask   = 0
  };
  int stat;

  stat = UDMAx_ChannelConfigure(
    &UDMA0_Resources,
    RTE_I2S0_CHNL_UDMA_RX_CH,
    (uint32_t)&(I2S0->I2S_RXDMA),
    (uint32_t)mic_context.dma_buffer,
    N_FRAMES_PER_CALLBACK*2,
    dma_control,
    &chnl_cfg,
    i2s_event_handler,
    udma0_chnl_info,
    udmaHandle0
  );
  if(stat == -1)
  {
    assert(!"Failed to config dma");
    return SL_STATUS_FAIL;
  }

  UDMAx_ChannelEnable(
    RTE_I2S0_CHNL_UDMA_RX_CH,
    &UDMA0_Resources,
    udmaHandle0
  );
  if(stat == -1)
  {
    assert(!"Failed to enable dma ch");
    return SL_STATUS_FAIL;
  }

  UDMAx_DMAEnable(
    &UDMA0_Resources,
    udmaHandle0
  );
  if(stat == -1)
  {
    assert(!"Failed to enable dma");
    return SL_STATUS_FAIL;
  }
  if(!I2S0->I2S_CER_b.CLKEN)
  {
    I2S0->CHANNEL_CONFIG[0].I2S_RFCR_b.RXCHDT = 1;
    I2S0->CHANNEL_CONFIG[0].I2S_RER_b.RXCHEN = 0x1;
    I2S0->I2S_IRER_b.RXEN = 0x1;
    I2S0->I2S_CER_b.CLKEN = ENABLE;
  }

  return SL_STATUS_OK;
}
