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
#pragma once

#include "tflite_micro_logger.hpp"
#include "tflite_micro_mvpv1_accelerator.hpp"


namespace npu_toolkit
{


extern "C" {
void mvpv1_dump_registers(const void *program_context);

void mvpv1_dump_instance_program_registers(int program_index);
void mvpv1_dump_program_registers(const void *program_context, int program_index);


void mvpv1_dump_alu_registers(const MVP_ALU_TypeDef* alu_regs);
void mvpv1_dump_alu_register(unsigned index, const MVP_ALU_TypeDef* alu_reg);

void mvpv1_dump_array_registers(const MVP_ARRAY_TypeDef* array_regs);
void mvpv1_dump_array_register(unsigned index, const MVP_ARRAY_TypeDef* array_reg);

void mvpv1_dump_loop_registers(const MVP_LOOP_TypeDef* loop_regs);
void mvpv1_dump_loop_register(unsigned index, const MVP_LOOP_TypeDef* loop_regs);

void mvpv1_dump_instr_registers(const MVP_INSTR_TypeDef* instr_regs);
void mvpv1_dump_instr_register(unsigned index, const MVP_INSTR_TypeDef* instr_regs);
void mvpv1_set_dump_writer(void (*writer)(const char*,va_list,void*), void *arg);

} // extern "C"




#ifdef MVPV1_ACCELERATOR_DUMP_REGISTERS_ENABLED
  #ifndef MVPV1_DEBUG_PRINTF
    #define MVPV1_DEBUG_PRINTF(msg, ...) NPU_TOOLKIT_INFO(msg, ## __VA_ARGS__)

    #define MVPV1_DEBUG_DUMP_PROGRAM(...) \
      auto& accelerator = npu_toolkit::TfliteMicroMvpv1Accelerator::instance(); \
      MVPV1_DEBUG_PRINTF("------------------------------------"); \
      MVPV1_DEBUG_PRINTF("%s: Program %d", npu_toolkit::TfliteMicroModelHelper::current_layer_name(), accelerator.program_counter);\
      dump_mvpv1_program(__VA_ARGS__)

    static inline void dump_mvpv1_program(const void *prog = nullptr)
    {
      auto vwriter = [](const char* msg, va_list args, void *arg)
      {
        VMicroPrintf(msg, args);
      };
      mvpv1_set_dump_writer(vwriter, nullptr);
      mvpv1_dump_registers(prog);
    }
  #endif // MVPV1_DEBUG_PRINTF

#else // MVPV1_ACCELERATOR_DUMP_REGISTERS_ENABLED

  #define MVPV1_DEBUG_PRINTF(...)
  #define MVPV1_DEBUG_DUMP_PROGRAM(...)
#endif // MVPV1_ACCELERATOR_DUMP_REGISTERS_ENABLED


} // namespace npu_toolkit