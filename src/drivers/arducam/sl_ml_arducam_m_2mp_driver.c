/***************************************************************************//**
 * @file
 * @brief Silicon Labs Arducam camera peripheral driver.
 *******************************************************************************
 * # License
 * <b>Copyright 2025 Silicon Laboratories Inc. www.silabs.com</b>
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
#include "em_device.h"
#include "em_cmu.h"
#include "em_gpio.h"
#include "em_usart.h"
#include "sl_dma_manager.h"
#include "sl_dma_channel.h"

#include "sl_ml_arducam.h"
#include "sl_ml_arducam_m_2mp_driver.h"

static sl_dma_channel_handle_t tx_handle;
static sl_dma_channel_handle_t rx_handle;
static volatile bool is_first_dma_call;
static volatile bool tx_done = false;
static volatile bool dma_error = false;

/* DMA channel indices may be 0; track successful sl_dma_channel_init instead of channel != 0. */
static bool s_dma_rx_channel_in_use;
static bool s_dma_tx_channel_in_use;

static void sli_on_dma_rx_complete_irq_handler(sl_dma_channel_handle_t *handle,
                                                void *user_data,
                                                bool error,
                                                bool aborted);
#define SL_CONCAT(A, B, C) A ## B ## C
/* Generate the cmu clock symbol based on instance. */
#define I2C_CLOCK(N) SL_CONCAT(cmuClock_I2C, N, )
#define I2C_PORT(N) SL_CONCAT(I2C, N, )

#define USART_CLOCK(N) SL_CONCAT(cmuClock_USART, N, )
#define USART_PORT(N) SL_CONCAT(USART, N, )
#define USART_RX_DMA_SIGNAL(N) SL_CONCAT(SL_DMA_SIGNAL_USART, N, _RXDATAV)
#define USART_TX_DMA_SIGNAL(N) SL_CONCAT(SL_DMA_SIGNAL_USART, N, _TXBL)

#if defined(_SILICON_LABS_32B_SERIES_2)
#define ENABLE_SPI_INTERRUPT() LDMA->IEN_SET = (1 << arducam_context.dma_rx_channel)
#define DISABLE_SPI_INTERRUPT() LDMA->IEN_CLR = (1 << arducam_context.dma_rx_channel)
#else 
#define ENABLE_SPI_INTERRUPT() LDMA->IEN |= (1 << arducam_context.dma_rx_channel)
#define DISABLE_SPI_INTERRUPT() LDMA->IEN &= ~(1 << arducam_context.dma_rx_channel)
#endif

static void tx_cb(sl_dma_channel_handle_t *handle, void *user_data,
                   bool error, bool aborted)
{
  (void)handle;
  (void)user_data;

  if (!error && !aborted) {
    tx_done = true;
  } else {
    dma_error = true;
  }
}
/***************************************************************************//**
 *  Driver function to initialize arducam camera
 ******************************************************************************/
sl_status_t slx_ml_arducam_driver_init(const arducam_config_t* config)
{
    sl_status_t status;

    CMU_ClockEnable(cmuClock_GPIO, true);
#if defined(_CMU_HFPERCLKEN0_MASK)
    CMU_ClockEnable(cmuClock_HFPER, true);
#endif

    // ---------------------------------------------------
    // Initialize SPI interface
    // ---------------------------------------------------
    CMU_ClockEnable(USART_CLOCK(ARDUCAM_USART_PERIPHERAL_NO), true);

    // Configure GPIO mode
    GPIO_PinModeSet(ARDUCAM_USART_CLK_PORT, ARDUCAM_USART_CLK_PIN, gpioModePushPull, 0); // US0_CLK is push pull
    GPIO_PinModeSet(ARDUCAM_USART_CS_PORT,  ARDUCAM_USART_CS_PIN,  gpioModePushPull, 1); // US0_CS is push pull
    GPIO_PinModeSet(ARDUCAM_USART_TX_PORT,  ARDUCAM_USART_TX_PIN,  gpioModePushPull, 1); // US0_TX (MOSI) is push pull
    GPIO_PinModeSet(ARDUCAM_USART_RX_PORT,  ARDUCAM_USART_RX_PIN,  gpioModeInput, 1);    // US0_RX (MISO) is input

    USART_InitSync_TypeDef usart_config = USART_INITSYNC_DEFAULT;
    usart_config.master       = true;            // master mode
    usart_config.baudrate     = 1000000;         // CLK freq is 1MHz 
                                                 // NOTE: This can technically go up to 8M but can be unreliable depending on the hardware setup
    usart_config.autoCsEnable = true;            // CS pin controlled by hardware, not firmware
    usart_config.clockMode    = usartClockMode0; // clock idle low, sample on rising/first edge
    usart_config.msbf         = true;            // send MSB first
    usart_config.enable       = usartDisable;    // making sure USART isn't enabled until we set it up
    USART_InitSync(USART_PORT(ARDUCAM_USART_PERIPHERAL_NO), &usart_config);

    // Set USART pin locations
#if defined(_SILICON_LABS_32B_SERIES_2)
    GPIO->USARTROUTE[ARDUCAM_USART_PERIPHERAL_NO].ROUTEEN = GPIO_USART_ROUTEEN_TXPEN
                                           | GPIO_USART_ROUTEEN_RXPEN
                                           | GPIO_USART_ROUTEEN_CLKPEN;

    GPIO->USARTROUTE[ARDUCAM_USART_PERIPHERAL_NO].TXROUTE = ((uint32_t)ARDUCAM_USART_TX_PORT
                                            << _GPIO_USART_TXROUTE_PORT_SHIFT)
                                           | ((uint32_t)ARDUCAM_USART_TX_PIN
                                              << _GPIO_USART_TXROUTE_PIN_SHIFT);

    GPIO->USARTROUTE[ARDUCAM_USART_PERIPHERAL_NO].RXROUTE = ((uint32_t)ARDUCAM_USART_RX_PORT
                                            << _GPIO_USART_RXROUTE_PORT_SHIFT)
                                           | ((uint32_t)ARDUCAM_USART_RX_PIN
                                              << _GPIO_USART_RXROUTE_PIN_SHIFT);

    GPIO->USARTROUTE[ARDUCAM_USART_PERIPHERAL_NO].CLKROUTE = ((uint32_t)ARDUCAM_USART_CLK_PORT
                                             << _GPIO_USART_CLKROUTE_PORT_SHIFT)
                                            | ((uint32_t)ARDUCAM_USART_CLK_PIN
                                               << _GPIO_USART_CLKROUTE_PIN_SHIFT);

#else
    // Enable USART pins
    USART_PORT(ARDUCAM_USART_PERIPHERAL_NO)->ROUTEPEN = USART_ROUTEPEN_CLKPEN | USART_ROUTEPEN_TXPEN | USART_ROUTEPEN_RXPEN;
    USART_PORT(ARDUCAM_USART_PERIPHERAL_NO)->ROUTELOC0 = 
        ((ARDUCAM_USART_CLK_LOC << _USART_ROUTELOC0_CLKLOC_SHIFT) |
         (ARDUCAM_USART_TX_LOC  << _USART_ROUTELOC0_TXLOC_SHIFT)  |
         (ARDUCAM_USART_RX_LOC  << _USART_ROUTELOC0_RXLOC_SHIFT)); 

#endif 

    // Enable USART
    USART_Enable(USART_PORT(ARDUCAM_USART_PERIPHERAL_NO), usartEnable);

    // Allocate DMA channels for burst reads
    status = sl_dma_manager_allocate_channel(NULL, &arducam_context.dma_rx_channel);
    if (status != SL_STATUS_OK)
    {
        return SL_STATUS_NO_MORE_RESOURCE;
    }
    status = sl_dma_channel_init(&rx_handle, NULL, arducam_context.dma_rx_channel,
                                  sli_on_dma_rx_complete_irq_handler, NULL);
    if (status != SL_STATUS_OK)
    {
        sl_dma_manager_free_channel(NULL, arducam_context.dma_rx_channel);
        return status;
    }
    sl_dma_channel_set_peripheral_signal(&rx_handle,
                                          USART_RX_DMA_SIGNAL(ARDUCAM_USART_PERIPHERAL_NO));
    s_dma_rx_channel_in_use = true;

    status = sl_dma_manager_allocate_channel(NULL, &arducam_context.dma_tx_channel);
    if (status != SL_STATUS_OK)
    {
        sl_dma_channel_deinit(&rx_handle);
        sl_dma_manager_free_channel(NULL, arducam_context.dma_rx_channel);
        s_dma_rx_channel_in_use = false;
        return SL_STATUS_NO_MORE_RESOURCE;
    }
    status = sl_dma_channel_init(&tx_handle, NULL, arducam_context.dma_tx_channel,
                                  tx_cb, NULL);
    if (status != SL_STATUS_OK)
    {
        sl_dma_manager_free_channel(NULL, arducam_context.dma_tx_channel);
        sl_dma_channel_deinit(&rx_handle);
        sl_dma_manager_free_channel(NULL, arducam_context.dma_rx_channel);
        s_dma_rx_channel_in_use = false;
        return status;
    }
    sl_dma_channel_set_peripheral_signal(&tx_handle,
                                          USART_TX_DMA_SIGNAL(ARDUCAM_USART_PERIPHERAL_NO));
    s_dma_tx_channel_in_use = true;


    // ---------------------------------------------------
    // Initialize I2C interface
    // ---------------------------------------------------
    CMU_ClockEnable(I2C_CLOCK(ARDUCAM_I2C_PERIPHERAL_NO), true);

    /* Output value must be set to 1 to not drive lines low. Set
     SCL first, to ensure it is high before changing SDA. */
    GPIO_PinModeSet(ARDUCAM_I2C_SCL_PORT, ARDUCAM_I2C_SCL_PIN, gpioModeWiredAndPullUp, 1);
    GPIO_PinModeSet(ARDUCAM_I2C_SDA_PORT, ARDUCAM_I2C_SDA_PIN, gpioModeWiredAndPullUp, 1);
   
    /* In some situations, after a reset during an I2C transfer, the slave
        device may be left in an unknown state. Send 9 clock pulses to
        set slave in a defined state. */
    for (int i = 0; i < 9; i++) 
    {
        GPIO_PinOutClear(ARDUCAM_I2C_SCL_PORT, ARDUCAM_I2C_SCL_PIN);
        ARDUCAM_DELAY_US(100);
        GPIO_PinOutSet(ARDUCAM_I2C_SCL_PORT, ARDUCAM_I2C_SCL_PIN);
        ARDUCAM_DELAY_US(100);
    }


  /* Enable pins and set location */
#if defined(_SILICON_LABS_32B_SERIES_2)
    GPIO->I2CROUTE[ARDUCAM_I2C_PERIPHERAL_NO].ROUTEEN = GPIO_I2C_ROUTEEN_SDAPEN | GPIO_I2C_ROUTEEN_SCLPEN;
    GPIO->I2CROUTE[ARDUCAM_I2C_PERIPHERAL_NO].SCLROUTE = 
        (uint32_t)((ARDUCAM_I2C_SCL_PIN << _GPIO_I2C_SCLROUTE_PIN_SHIFT) | (ARDUCAM_I2C_SCL_PORT << _GPIO_I2C_SCLROUTE_PORT_SHIFT));
    GPIO->I2CROUTE[ARDUCAM_I2C_PERIPHERAL_NO].SDAROUTE = 
        (uint32_t)((ARDUCAM_I2C_SDA_PIN << _GPIO_I2C_SDAROUTE_PIN_SHIFT) | (ARDUCAM_I2C_SDA_PORT << _GPIO_I2C_SDAROUTE_PORT_SHIFT));

#else
    I2C_PORT(ARDUCAM_I2C_PERIPHERAL_NO)->ROUTEPEN  = I2C_ROUTEPEN_SDAPEN | I2C_ROUTEPEN_SCLPEN;
    I2C_PORT(ARDUCAM_I2C_PERIPHERAL_NO)->ROUTELOC0 = (uint32_t)((ARDUCAM_I2C_SDA_LOC << _I2C_ROUTELOC0_SDALOC_SHIFT)
                                     | (ARDUCAM_I2C_SCL_LOC << _I2C_ROUTELOC0_SCLLOC_SHIFT));
#endif

    I2C_Init_TypeDef i2c_config = I2C_INIT_DEFAULT;

    i2c_config.enable = true;
    i2c_config.master = true; /* master mode only */
    i2c_config.freq = I2C_FREQ_STANDARD_MAX;
    i2c_config.refFreq = 0;
    i2c_config.clhr = i2cClockHLRStandard;

    I2C_Init(I2C_PORT(ARDUCAM_I2C_PERIPHERAL_NO), &i2c_config);


    // ---------------------------------------------------
    // Initialize the OV2640 
    // ---------------------------------------------------
    status = slx_ml_ov2640_init(config);
    if(status != SL_STATUS_OK)
    {
        return status;
    }

    // ---------------------------------------------------
    // Configure the camera FIFO registers
    // ---------------------------------------------------
#define SPI_TEST_VALUE 0xBA
    uint8_t reg_value;

    reg_value = 0;
    slx_ml_arducam_driver_spi_read_reg(ARDUCHIP_REG_REV, &reg_value);


    if(reg_value != 0x73)
    {
        // Older Arducam versions do not require a dummy byte between the burst read command and RX data
        arducam_context.add_dummy_byte_to_burst_read = true;
    }

    reg_value = 0;
    slx_ml_arducam_driver_spi_write_reg(ARDUCHIP_REG_TEST1, SPI_TEST_VALUE);
    slx_ml_arducam_driver_spi_read_reg(ARDUCHIP_REG_TEST1, &reg_value);

    if(reg_value != SPI_TEST_VALUE)
    {
        return SL_STATUS_BUS_ERROR;
    }

    slx_ml_arducam_driver_spi_write_reg(ARDUCHIP_REG_FIFO, FIFO_CLEAR_MASK|FIFO_RDPTR_RST_MASK|FIFO_WRPTR_RST_MASK);
    slx_ml_arducam_driver_spi_set_bit(ARDUCHIP_REG_TIM, MODE_MASK);
    slx_ml_arducam_driver_spi_write_reg(ARDUCHIP_REG_FRAMES, 1);
    slx_ml_arducam_driver_spi_read_reg(ARDUCHIP_REG_FRAMES, &reg_value);

    if(reg_value != 1)
    {
        return SL_STATUS_BUS_ERROR;
    }

    return SL_STATUS_OK;
}

/***************************************************************************//**
 *  Driver function to de-initialize arducam camera.
 ******************************************************************************/
sl_status_t slx_ml_arducam_driver_deinit()
{
    I2C_Reset(I2C_PORT(ARDUCAM_I2C_PERIPHERAL_NO));
    USART_Reset(USART_PORT(ARDUCAM_USART_PERIPHERAL_NO));

    CMU_ClockEnable(I2C_CLOCK(ARDUCAM_I2C_PERIPHERAL_NO), false);
    CMU_ClockEnable(USART_CLOCK(ARDUCAM_USART_PERIPHERAL_NO), false);

    if (s_dma_rx_channel_in_use)
    {
        sl_dma_channel_deinit(&rx_handle);
        sl_dma_manager_free_channel(NULL, arducam_context.dma_rx_channel);
        arducam_context.dma_rx_channel = 0;
        s_dma_rx_channel_in_use = false;
    }

    if (s_dma_tx_channel_in_use)
    {
        sl_dma_channel_deinit(&tx_handle);
        sl_dma_manager_free_channel(NULL, arducam_context.dma_tx_channel);
        arducam_context.dma_tx_channel = 0;
        s_dma_tx_channel_in_use = false;
    }

    return SL_STATUS_OK;
}

/***************************************************************************//**
 *  Camera driver function used to write single i2c register.
 ******************************************************************************/
sl_status_t slx_ml_arducam_driver_i2c_write_reg(uint8_t addr, uint8_t data)
{
    I2C_TransferSeq_TypeDef seq;
    //I2C_TransferReturn_TypeDef ret;
    uint8_t write_buffer[2] = {addr, data};

    seq.addr = OV2640_I2C_ADDRESS << 1;
    seq.flags = I2C_FLAG_WRITE;

    seq.buf[0].data = write_buffer;
    seq.buf[0].len  = 2;

    return sli_do_i2c_transfer(&seq);
}

/***************************************************************************//**
 *  Camera driver function used to read single i2c register.
 ******************************************************************************/
sl_status_t slx_ml_arducam_driver_i2c_read_reg(uint8_t addr, uint8_t *val)
{
    I2C_TransferSeq_TypeDef seq;
    //I2C_TransferReturn_TypeDef ret;

    seq.addr = OV2640_I2C_ADDRESS << 1;
    seq.flags = I2C_FLAG_WRITE_READ;

    seq.buf[0].data = &addr;
    seq.buf[0].len  = 1;

    seq.buf[1].data = val;
    seq.buf[1].len  = 1;

    return sli_do_i2c_transfer(&seq);
}

/***************************************************************************//**
 *  Camera driver function used to write i2c registers.
 ******************************************************************************/
sl_status_t slx_ml_arducam_driver_i2c_write_regs(
    const reg_addr_value_t *regs, 
    const reg_addr_value_t *action_list,
    uint8_t action_list_len
)
{
    for(const reg_addr_value_t *addr_value = regs;; ++addr_value)
    {
        if(addr_value->address == REG_ADDR_ACTION)
        {
            if(addr_value->value == REG_ACTION_TERMINATE)
            {
                return SL_STATUS_OK;
            }
            else if(action_list == NULL)
            {
                return SL_STATUS_NULL_POINTER;
            }
            else if(addr_value->value >= action_list_len)
            {
                return SL_STATUS_HAS_OVERFLOWED;
            }
            else
            {
                const reg_addr_value_t *action = &action_list[addr_value->value];
                SL_VERIFY(slx_ml_arducam_driver_i2c_write_reg(action->address, action->value));
            }
        }
        else
        {
            SL_VERIFY(slx_ml_arducam_driver_i2c_write_reg(addr_value->address, addr_value->value));
        }
    }

    return SL_STATUS_OK;
}

/***************************************************************************//**
 *  Camera driver function used to write single spi register.
 ******************************************************************************/
sl_status_t slx_ml_arducam_driver_spi_write_reg(uint8_t addr, uint8_t data)
{
    DISABLE_SPI_INTERRUPT();

    if(arducam_context.is_spi_active)
    {
        ENABLE_SPI_INTERRUPT();
        return SL_STATUS_INVALID_STATE;
    }

    arducam_context.is_spi_active = true;

    GPIO_PinOutClear(ARDUCAM_USART_CS_PORT,  ARDUCAM_USART_CS_PIN);

    //ARDUCAM_DELAY_US(5);

    USART_SpiTransfer(USART_PORT(ARDUCAM_USART_PERIPHERAL_NO), addr | ARDUCHIP_RW_FLAG);
    USART_SpiTransfer(USART_PORT(ARDUCAM_USART_PERIPHERAL_NO), data);

    //ARDUCAM_DELAY_US(5);

    GPIO_PinOutSet(ARDUCAM_USART_CS_PORT,  ARDUCAM_USART_CS_PIN);

    arducam_context.is_spi_active = false;

    ENABLE_SPI_INTERRUPT();

    return SL_STATUS_OK;
}

/***************************************************************************//**
 *  Camera driver function used to read single spi register.
 ******************************************************************************/
sl_status_t slx_ml_arducam_driver_spi_read_reg(uint8_t addr, uint8_t *data_ptr)
{
    DISABLE_SPI_INTERRUPT();

    if(arducam_context.is_spi_active)
    {
        ENABLE_SPI_INTERRUPT();
        return SL_STATUS_INVALID_STATE;
    }

    arducam_context.is_spi_active = true;

    GPIO_PinOutClear(ARDUCAM_USART_CS_PORT,  ARDUCAM_USART_CS_PIN);

    //ARDUCAM_DELAY_US(5);

    USART_SpiTransfer(USART_PORT(ARDUCAM_USART_PERIPHERAL_NO), addr);
    *data_ptr = USART_SpiTransfer(USART_PORT(ARDUCAM_USART_PERIPHERAL_NO), 0xFF);

    //ARDUCAM_DELAY_US(5);

    GPIO_PinOutSet(ARDUCAM_USART_CS_PORT,  ARDUCAM_USART_CS_PIN);

    arducam_context.is_spi_active = false;

    ENABLE_SPI_INTERRUPT();

    return SL_STATUS_OK;
}

/***************************************************************************//**
 *  Clear spesific bits on spi register.
 ******************************************************************************/
sl_status_t slx_ml_arducam_driver_spi_clear_bit(uint8_t addr, uint8_t bits)
{
    uint8_t reg_value;

    SL_VERIFY(slx_ml_arducam_driver_spi_read_reg(addr, &reg_value));

    reg_value &= ~bits;

    SL_VERIFY(slx_ml_arducam_driver_spi_write_reg(addr, reg_value));

    return SL_STATUS_OK;
}

/***************************************************************************//**
 *  Set spesific bits on spi register.
 ******************************************************************************/
sl_status_t slx_ml_arducam_driver_spi_set_bit(uint8_t addr, uint8_t bits)
{
    uint8_t reg_value;

    SL_VERIFY(slx_ml_arducam_driver_spi_read_reg(addr, &reg_value));

    reg_value |= bits;

    SL_VERIFY(slx_ml_arducam_driver_spi_write_reg(addr, reg_value));

    return SL_STATUS_OK;
}

/***************************************************************************//**
 *  Camera driver function used to read spi buffer in burst mode.
 ******************************************************************************/
sl_status_t slx_ml_arducam_driver_spi_burst_read(uint8_t *buffer, uint32_t length)
{
    DISABLE_SPI_INTERRUPT();

    if(arducam_context.is_spi_active)
    {
        ENABLE_SPI_INTERRUPT();
        return SL_STATUS_INVALID_STATE;
    }

    arducam_context.is_spi_active = true;

    GPIO_PinOutClear(ARDUCAM_USART_CS_PORT, ARDUCAM_USART_CS_PIN);

    USART_SpiTransfer(USART_PORT(ARDUCAM_USART_PERIPHERAL_NO), BURST_FIFO_READ);

    if(arducam_context.add_dummy_byte_to_burst_read)
    {
        USART_SpiTransfer(USART_PORT(ARDUCAM_USART_PERIPHERAL_NO), 0x00);
    }

    arducam_context.dma_length_remaining = length;
    arducam_context.dma_rx_ptr = buffer;
    is_first_dma_call = true;

    /* Must re-enable LDMA IRQ for the RX channel before DMA runs; polled SPI helpers do this, burst read must too. */
    ENABLE_SPI_INTERRUPT();

    sli_on_dma_rx_complete_irq_handler(&rx_handle, NULL, false, false);


    return SL_STATUS_OK;
}

/***************************************************************************//**
 *  Provides camera buffer fifo size.
 ******************************************************************************/
sl_status_t slx_ml_arducam_driver_get_fifo_size(uint32_t *size_ptr)
{
    uint32_t size;
    sl_status_t status;
    uint8_t size_part;

    *size_ptr = 0;

    status = slx_ml_arducam_driver_spi_read_reg(FIFO_SIZE3, &size_part);
    if(status != SL_STATUS_OK)
    {
        return status;
    }

    size = size_part;
    size <<= 8;

    status = slx_ml_arducam_driver_spi_read_reg(FIFO_SIZE2, &size_part);
    if(status != SL_STATUS_OK)
    {
        return status;
    }

    size |= (uint32_t)size_part;
    size <<= 8;

    status = slx_ml_arducam_driver_spi_read_reg(FIFO_SIZE1, &size_part);
    if(status != SL_STATUS_OK)
    {
        return status;
    }

    size |= (uint32_t)size_part;

    if(size > MAX_IMAGE_SIZE)
    {
        return SL_STATUS_BUS_ERROR;
    }

    *size_ptr = size;

    return SL_STATUS_OK;
}

 /***************************************************************************//**
 *  Local Functions
 ******************************************************************************/

/***************************************************************************//**
 *  Driver function to transfer data to I2C peripheral.
 ******************************************************************************/
sl_status_t sli_do_i2c_transfer(I2C_TransferSeq_TypeDef *seq)
{
    I2C_TransferReturn_TypeDef ret;
    uint32_t timeout = 300000;

    /* Do a polled transfer */
    ret = I2C_TransferInit(I2C_PORT(ARDUCAM_I2C_PERIPHERAL_NO), seq);
    while (ret == i2cTransferInProgress && timeout--) 
    {
        ret = I2C_Transfer(I2C_PORT(ARDUCAM_I2C_PERIPHERAL_NO));
    }

  return (ret == i2cTransferDone) ? SL_STATUS_OK : SL_STATUS_BUS_ERROR;
}

/***************************************************************************//**
 *   Callback function for signalling completion for DMA.
 ******************************************************************************/
void sli_on_dma_rx_complete_irq_handler(sl_dma_channel_handle_t *handle,
                                         void *user_data,
                                         bool error,
                                         bool aborted)
{
#define MAX_DMA_LENGTH ((_LDMA_CH_CTRL_XFERCNT_MASK >> _LDMA_CH_CTRL_XFERCNT_SHIFT)+1)
    static uint8_t dummy_tx_buffer = 0;
    sl_status_t status;

    (void)user_data;

    if (error || aborted) {
        GPIO_PinOutSet(ARDUCAM_USART_CS_PORT, ARDUCAM_USART_CS_PIN);
        arducam_context.is_spi_active = false;
        dma_error = true;
        return;
    }

    if (!is_first_dma_call)
    {
        /* Advance by the chunk that just completed; last chunk may be < MAX_DMA_LENGTH. */
        uint32_t completed = MIN(MAX_DMA_LENGTH, arducam_context.dma_length_remaining);
        arducam_context.dma_rx_ptr += completed;
        arducam_context.dma_length_remaining -= completed;
    }
    is_first_dma_call = false;

    if(arducam_context.dma_length_remaining == 0)
    {
        GPIO_PinOutSet(ARDUCAM_USART_CS_PORT, ARDUCAM_USART_CS_PIN);
        arducam_context.state = CAMERA_STATE_READ_COMPLETE;
        arducam_context.is_spi_active = false;
        slx_ml_arducam_poll();
        return;
    }

    const uint32_t chunk_length = MIN(MAX_DMA_LENGTH, arducam_context.dma_length_remaining);

    status = sl_dma_channel_submit_transfer_p2m(&rx_handle,
                                                 (void *)&USART_PORT(ARDUCAM_USART_PERIPHERAL_NO)->RXDATA,
                                                 arducam_context.dma_rx_ptr,
                                                 chunk_length,
                                                 SL_DMA_CTRL_SIZE_BYTE,
                                                 NULL);
    if (status != SL_STATUS_OK)
    {
        GPIO_PinOutSet(ARDUCAM_USART_CS_PORT, ARDUCAM_USART_CS_PIN);
        arducam_context.is_spi_active = false;
        dma_error = true;
        return;
    }

    status = sl_dma_channel_submit_transfer_m2p(&tx_handle,
                                                 (void *)&dummy_tx_buffer,
                                                 (void *)&USART_PORT(ARDUCAM_USART_PERIPHERAL_NO)->TXDATA,
                                                 chunk_length,
                                                 SL_DMA_CTRL_SIZE_BYTE,
                                                 NULL);
    if (status != SL_STATUS_OK)
    {
        GPIO_PinOutSet(ARDUCAM_USART_CS_PORT, ARDUCAM_USART_CS_PIN);
        arducam_context.is_spi_active = false;
        dma_error = true;
        return;
    }
}
