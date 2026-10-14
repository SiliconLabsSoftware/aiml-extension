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

#include "compiled_model.hpp"
#include "compiled_model_context.hpp"
#include "compiled_model_paging.hpp"
#include "tflite_micro_mvpv1_accelerator.hpp"
#include "tflite_micro_mvpv1_context.hpp"
#include "mvpv1_kernel_debug.hpp"
#include "mvpv1_kernel_utils.hpp"




namespace npu_toolkit
{

/**
 * Execute the pre-compiled data for the current model layer on the accelerator.
 *
 * This is called by each accelerator kernel's "compiled" implementation.
 * It iterates through the pre-compiled data for the current ML model layer.

 * @param context TF-Lite context
 * @param program_callback Optional callback to be invoked for each accelerator program used by current layer
 *                         This callback is invoked BEFORE the accelerator registers are populated for the current program
*/
TfLiteStatus compiled_model_process_layer_with_accelerator(
    TfLiteContext *context,
    void (*program_callback)(void*, int),
    void* program_callback_arg
)
{
  volatile uint32_t* MVP_BASE_ADDR = (uint32_t*)&MVP->PROGRAMSTATE;
  auto& accelerator = TfliteMicroMvpv1Accelerator::instance();
  auto& mvpv1_context = *TfliteMicroMvpv1Context::get(context);
  auto& compiled_context = *mvpv1_context.compiled_context;
  bool errors_detected = false;

  // Prepare the accelerator for compiled execution (e.g. enable interrupts)
  accelerator.prepare_compiled_execution();

  // Signal that the next layer of the compiled model is beginning execution
  if(!compiled_context.begin_layer())
  {
    return kTfLiteError;
  }

  // Retrieve the number of programs to be executed for this layer
  const auto n_programs = compiled_context.n_programs();
  MVPV1_DEBUG_PRINTF("n_programs=%d", n_programs);

  // Iterate through each compiled MVP program used by this layer
  for(int program_index = 0; program_index < n_programs; ++program_index)
  {
    CompiledProgramInfo prog_info;

    // Retrieve the CompiledProgramInfo for the next program in this layer
    if(!compiled_context.get_next_program(&prog_info))
    {
      errors_detected = true;
      break;
    }
    assert(prog_info.n_register_groups == 2);

    // Invoke the program callback if necessary
    if(program_callback != nullptr)
    {
      program_callback(program_callback_arg, program_index);
    }

    auto register_offsets = prog_info.register_offsets;
    auto register_values = prog_info.register_values;

    // The first register groups are the ARRAY[]->ADDRCFG registers.
    // We must convert from a relative offset to an absolute address
    auto n_addrcfg_registers = *register_offsets++;
    MVPV1_DEBUG_PRINTF("n_addrcfg_registers=%d", n_addrcfg_registers);
    while(n_addrcfg_registers--)
    {
      const auto register_offset = *register_offsets++;

      // If register_offset = 0xFF, then this is a special case for the fused_relu programs.
      // In this case, ARRAY[1] and ARRAY[0] have the same address.
      if(register_offset == 0xFF)
      {
        MVPV1_DEBUG_PRINTF("Fused ReLU");
        MVP->ARRAY[1].ADDRCFG = MVP->ARRAY[0].ADDRCFG;
      }
      // If the array_id == 0xFE, then this is a special that must be handled by
      // the the compiled kernel-specific implementation.
      // So call the callback now, which will update mvpv1_array_base_addresses.
      else if(register_offset == 0xFE)
      {
        MVPV1_DEBUG_PRINTF("Invoke callback");
        assert(mvpv1_context.update_array_base_addresses_callback != nullptr);
        mvpv1_context.update_array_base_addresses_callback(mvpv1_context);
      }
      else
      {
        const auto memory_region = CompressedProgramConfig::get_memory_region_id_from_register_offset(register_offset);
        const auto array_id = CompressedProgramConfig::get_array_id_from_register_offset(register_offset);
        const auto base_addr = (memory_region == CompressedProgramConfig::DEFAULT_MEMORY_REGION_ID) ?
          mvpv1_context.array_base_addresses[array_id] :
          compiled_context._paging->get_base_address(memory_region);

        const uint32_t rel_addr = *register_values++;

        MVPV1_DEBUG_PRINTF("MVP->ARRAY[%d].ADDRCFG = %p (memory_region=%d)", array_id, base_addr + rel_addr, (int)memory_region);
        MVP->ARRAY[array_id].ADDRCFG = base_addr + rel_addr;
      }
    }

    // Populate the "delta" registers for this MVP program
    auto n_mvp_registers = *register_offsets++;
    MVPV1_DEBUG_PRINTF("n_mvp_registers=%d", n_mvp_registers);
    while(n_mvp_registers--)
    {
      const uint8_t reg_addr_offset = *register_offsets++;
      MVP_BASE_ADDR[reg_addr_offset] = *register_values++;
    }

    MVPV1_DEBUG_DUMP_PROGRAM();

    // Wait for any output tensor buffers to become available
    // (This does nothing if paging is not used by this layer)
    if(!compiled_context.wait())
    {
      errors_detected = true;
      break;
    }

    // Start the MVP program execution
    // and wait for it to complete
    accelerator.execute_program();

    // Release any cache buffers as necessary
    // (This does nothing if paging is not used by this layer)
    compiled_context.release();

    // Break out of the loop if any MVP errors occurred
    const uint32_t error_flags = (MVP->IF & MVP_FAULT_MASK);
    errors_detected = error_flags != 0;
    if(errors_detected)
    {
      NPU_TOOLKIT_ERROR("MVP error: 0x%08X\n", error_flags);
      break;
    }
  } // for(int program_index = 0; program_index < n_programs; ++program_index)


  accelerator.finalize_compiled_execution();

  return errors_detected ? kTfLiteError : kTfLiteOk;
}



} // namespace npu_toolkit