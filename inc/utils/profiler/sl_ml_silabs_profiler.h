/***************************************************************************//**
 * @file sl_ml_silabs_profiler.h
 * @brief ml silabs profiler header.
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
#ifndef SL_ML_SILABS_PROFILER_H
#define SL_ML_SILABS_PROFILER_H

#include <stdbool.h>
#include "em_device.h"
#include "em_cmu.h"
#include "em_system.h"
#include "em_emu.h"
#include <stdio.h>
#include <cstdint>
#include <stdint.h>
#include <memory>

#include "sl_ml_profiler_config.h"
#include "sl_ml_profiler_interpreter.h"
#include "sl_ml_profiler_debug_channel.h"

#include "sl_sleeptimer.h"

#if defined(SL_BOARD_SI91X)
#include "ml_clock_helper.h"
#endif

#if defined(SL_COMPONENT_CATALOG_PRESENT)
    #include "sl_component_catalog.h"
#endif

#if defined(SL_CATALOG_MVP_PRESENT)
    #include "sl_mvp.h"
    #include "sl_nn_util.h"
#endif

namespace sl {
namespace ml {
	// Subclass tflite::MicroProfiler to get Invoke() hooks from model layers.
	class SilabsProfiler : public tflite::MicroProfilerInterface {
	public:
		/***************************************************************************//**
		*  @brief Default constructor.
		*
		*  Initializes internal counters. Does not create an interpreter or
		*  allocate any tensor arena.
		******************************************************************************/
		SilabsProfiler();

		/***************************************************************************//**
		*  @brief Destructor.
		******************************************************************************/
		~SilabsProfiler() override;
		/***************************************************************************//**
		*  @brief Called at start of an operation (profiler hook).
		******************************************************************************/
		uint32_t BeginEvent(const char* tag) override;
		/***************************************************************************//**
		*  @brief Called at end of an operation (profiler hook).
		******************************************************************************/
		void EndEvent(uint32_t event_handle) override;

		/***************************************************************************//**
		*  @brief pointer to the interpreter.
		******************************************************************************/
		SLMicroInterpreter* interpreter_profiler = nullptr;
		/***************************************************************************//**
		*  @brief pointer to the model.
		******************************************************************************/
		const tflite::Model* model_profiler = nullptr;

	private:

		//uint32_t cpu_freq;
		uint32_t cpu_freq_ = 0;
		uint32_t operation_index_;

		// Model specific values
		uint32_t number_of_inferences_;
		uint16_t number_operators_model_;
		uint16_t operator_num_in_model_;
		const char *op_name_;

		// Init event info
		//cpu_util_scope_t cpu_metrics;
		sl_ml_profiler_event_info_t event_info;
		sl_ml_profiler_end_session_info_t end_session_info;
		sl_ml_profiler_start_session_info_t start_session_info;
#if (SL_ML_ENABLE_PROFILER_DEBUG_MSG)
		sl_ml_profiler_counter_pkt_info_t counter_pkt_info;
#endif //SL_ML_ENABLE_PROFILER_DEBUG_MSG

		// counter metrics
		uint32_t busy_cycles=0;
  		uint32_t wall_ticks=0;
		uint32_t mvp_instructions=0;
		uint32_t mvp_stall_cycles=0;
		uint32_t mvp_programs=0;
		float_t inference_time_ms_ = 0;

		// ================ SESSION METRICS FUNCTIONS =====================
		bool reset_profiler_metrics();
		const char* core_name_from_cpuid();
		sl_ml_mcu_id_t get_mcu_id();
		uint32_t read_mvp_ipversion();
		sl_ml_accelerator_id_t get_accelerator_id();
		bool sector_is_blank(uint32_t sector_addr);
		uint32_t flash_used_kb();
		void update_start_session_info(sl_ml_profiler_start_session_info_t* info);
		void update_end_session_info();
		// ================ COUNTER FUNCTIONS =====================
		size_t data_usage_bytes();
		size_t bss_usage_bytes();
		size_t heap_usage_bytes();
		size_t stack_usage_bytes();
		float get_memory_utilization_kb(void);
		uint32_t get_cpu_freq();
		float estimate_active_current_A(std::uint32_t cpu_hz);
#ifdef SL_ML_ENABLE_ENERGY_PROFILING
		float energyJ_from_cycles(uint32_t delta_cycles);
#endif // SL_ML_ENABLE_ENERGY_PROFILING
		// ================ EVENT METRIC FUNCTIONS =====================
		void dwt_enable_cyccnt(void);
		uint32_t get_cpu_cycles();
		void event_counter_begin();
		void event_counter_end();
		// ================ MODEL OP METRIC FUNCTIONS =====================
		bool update_start_event_info();
		void calculate_conv2d(const tflite::Operator* op);
		void calculate_fully_connected(const tflite::Operator* op);
		void calculate_transpose_conv(const tflite::Operator* op);
		void calculate_depthwise_conv2d(const tflite::Operator* op);
		void calculate_max_pool2d(const tflite::Operator* op);
		void calculate_average_pool2d(const tflite::Operator* op);
		void calculate_softmax(const tflite::Operator* op);
		void calculate_add(const tflite::Operator* op);
		void calculate_quantize(const tflite::Operator* op);
		void calculate_dequantize(const tflite::Operator* op);
		void calculate_pad(const tflite::Operator* op);
		void calculate_reshape(const tflite::Operator* op);
		void calculate_mean(const tflite::Operator* op);
		void calculate_resize_nearest_neighbor(const tflite::Operator* op);
		void calculate_relu(const tflite::Operator* op);
		void calculate_multiply(const tflite::Operator* op);
		uint32_t add_activation_ops(tflite::ActivationFunctionType activation, int count);
		void FillLayerDimsFrom(const TfLiteEvalTensor* t, layer_dimension_t* dst);
		void NoteShapesAndCounters(const TfLiteEvalTensor* in,
                                         const TfLiteEvalTensor* out,
                                         uint64_t macs,
                                         uint64_t ops);
		// =============== OUTPUT FUNCTIONS =================================
		void emit_parent_tracks();
		void emit_processor_core_start_track_packets();
		void emit_processor_core_end_track_packets();
		void emit_counter_start_track_packets();
		void emit_counter_end_track_packets();
		void emit_start_session_info();
		void emit_end_session_info();
		void emit_counter_packet_info();
		void emit_start_event_info();
		void emit_end_event_info();
		/***************************************************************************//**
		*  @brief @brief prints a 64bit unsigned integer (uint64_t) with thousands separators
		******************************************************************************/
		void sli_ui32_print_ts(uint32_t n);
		/***************************************************************************//**
		*  @brief prints a 64bit unsigned integer (uint64_t) with text
		******************************************************************************/
		void sli_print_ui32(const char* lead_str, uint32_t n, const char* lag_str);
		/***************************************************************************//**
		*  @brief Convert a cycle count to human-friendly time and print it.
		*  Uses the profiler's cpu_freq to format seconds/ms/us.
		******************************************************************************/
		void sli_print_time(const char* str, uint32_t time);

		TF_LITE_REMOVE_VIRTUAL_DELETE
	};
}  // namespace ml
}  // namespace sl

extern "C" {

}

#endif  // SL_ML_SILABS_PROFILER_H
