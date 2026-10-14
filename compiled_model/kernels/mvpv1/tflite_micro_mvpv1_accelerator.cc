/*******************************************************************************
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

#include "tflite_micro_model_config.h"
#include "em_device.h"
#include "sl_core.h"

#include "tflite_micro_mvpv1_accelerator.hpp"
#include "tflite_micro_mvpv1_context.hpp"
#include "tflite_micro_model_helper.hpp"
#include "compiled_model_context.hpp"
#include "compiled_model.hpp"
#include "mvpv1_kernel_utils.hpp"

#if __has_include("sl_si91x_clock_manager.h")
#include "sl_si91x_clock_manager.h"

static inline void set_mvp_enabled(bool enabled) {
  #define MVP_CLK_ENABLE_BIT 25
  if(enabled) {
    M4CLK->CLK_ENABLE_SET_REG1 |= BIT(MVP_CLK_ENABLE_BIT);
    M4CLK->DYN_CLK_GATE_DISABLE_REG |= BIT(MVP_CLK_ENABLE_BIT);
  } else {
      M4CLK->CLK_ENABLE_CLEAR_REG1 = BIT(MVP_CLK_ENABLE_BIT);
      M4CLK->DYN_CLK_GATE_DISABLE_REG &= ~(BIT(MVP_CLK_ENABLE_BIT));
  }
}

#elif __has_include("sl_clock_manager.h")
#include "sl_clock_manager.h"

static inline void set_mvp_enabled(bool enabled) {
  if(enabled) sl_clock_manager_enable_bus_clock(SL_BUS_CLOCK_MVP);
  else sl_clock_manager_disable_bus_clock(SL_BUS_CLOCK_MVP);
}

#else
#error Unsupported platform
#endif


namespace npu_toolkit
{


const char* TfliteMicroMvpv1Accelerator::name() const
{
  return "mvpv1";
}

const char* TfliteMicroMvpv1Accelerator::description() const
{
  return "Matrix Vector Processor v1";
}

bool TfliteMicroMvpv1Accelerator::init(const void* flatbuffer)
{
  // Ensure this application's source code is compatible
  // with the ML compiler used to compile the given .tflite flatbuffer
  if((flatbuffer != nullptr) && !compiled_model_validate_compiler_version(flatbuffer, name(), true))
  {
    return false;
  }

  if(_ref_counter == INT32_MAX)
  {
    NPU_TOOLKIT_ERROR("Reference counter has reached its maximum value");
    return false;
  }

  if(_ref_counter == 0)
  {
    set_mvp_enabled(true);
    MVP->EN_SET = MVP_EN_EN;
    NVIC_EnableIRQ(MVP_IRQn);
    MVP->SWRST_SET = MVP_SWRST_SWRST;
    while(MVP->SWRST & MVP_SWRST_RESETTING)
    {
    }
  }

  ++_ref_counter;

  return true;
}

void TfliteMicroMvpv1Accelerator::deinit(TfLiteContext *context)
{
  auto compiled_context = (context != nullptr) ? CompiledModelContext::get<TfliteMicroMvpv1Context>(context) : nullptr;
  if(compiled_context != nullptr)
  {
    compiled_context->deinit();
  }

  if(_ref_counter <= 0)
  {
    NPU_TOOLKIT_ERROR("Reference counter is already zero");
    return;
  }

  --_ref_counter;

  if(_ref_counter == 0)
  {
    MVP->EN_CLR = MVP_EN_EN;
    set_mvp_enabled(false);
  }
}

TfliteMicroModelContext* TfliteMicroMvpv1Accelerator::create_context(
  TfLiteContext *context
)
{
  return TfliteMicroMvpv1Context::create(context);
}


bool TfliteMicroMvpv1Accelerator::is_model_compiled(TfLiteContext *context)
{
  return CompiledModelContext::get<TfliteMicroMvpv1Context>(context) != nullptr;
}

bool TfliteMicroMvpv1Accelerator::is_current_layer_compiled(TfLiteContext *context)
{
  auto compiled_context = CompiledModelContext::get<TfliteMicroMvpv1Context>(context);
  if(compiled_context == nullptr)
  {
    return false;
  }

  const auto accelerator_id = compiled_context->get_layer_accelerator(
      TfliteMicroModelHelper::current_layer_index(context)
  );

  return accelerator_id != CompiledAcceleratorId::NoAccelerator;
}


void TfliteMicroMvpv1Accelerator::prepare_compiled_execution()
{
  // Enable the MVP hardware
  MVP->EN_SET = MVP_EN_EN;

  // Wait for the MVP to be idle before starting execution
  wait_for_idle();

  // Enable the MVP PROGDONE interrupt
  MVP->IF_CLR = MVP_IEN_PROGDONE|MVP_FAULT_MASK;
  MVP->IEN_SET = MVP_IEN_PROGDONE|MVP_FAULT_MASK;
}


#define ENABLE_CPU_CYCLE_COUNTER() DWT->CTRL |= DWT_CTRL_CYCCNTENA_Msk
#define DISABLE_CPU_CYCLE_COUNTER() DWT->CTRL &= ~DWT_CTRL_CYCCNTENA_Msk

void TfliteMicroMvpv1Accelerator::wait_for_idle()
{
  CORE_DECLARE_IRQ_STATE;
  while (!(MVP->STATUS & MVP_STATUS_IDLE))
  {
    CORE_ENTER_CRITICAL();
    if (!(MVP->STATUS & MVP_STATUS_IDLE))
    {
      DISABLE_CPU_CYCLE_COUNTER();
      //EMU_EnterEM1();
      __WFI();
      ENABLE_CPU_CYCLE_COUNTER();
    }
    CORE_EXIT_CRITICAL();
  }
}

void TfliteMicroMvpv1Accelerator::execute_program()
{
  auto& accelerator_context = *TfliteMicroMvpv1Context::get();
  ++accelerator_context.program_counter;

  // Start the MVP program execution
  // NOTE: MVP_CMD_INIT is optionally included in the compiled "delta" registers
  MVP->CMD_SET = MVP_CMD_START;

  wait_for_idle();
}

void TfliteMicroMvpv1Accelerator::finalize_compiled_execution()
{
  // Disable the MVP PROGDONE interrupt
  MVP->IEN_CLR = MVP_IEN_PROGDONE|MVP_FAULT_MASK;
}



__WEAK TfliteMicroAccelerator* get_tflite_micro_accelerator()
{
  static TfliteMicroMvpv1Accelerator accelerator;
  return &accelerator;
}

__WEAK TfliteMicroAccelerator* register_tflite_micro_accelerator()
{
    return register_tflite_micro_accelerator(get_tflite_micro_accelerator());
}


} // namespace npu_toolkit