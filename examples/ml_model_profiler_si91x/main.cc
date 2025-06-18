#include <cstdio>

#include "em_cmu.h"
#include "energy_profiling_utils.hpp"
#include "ml_clock_helper.h"
#include "sl_ml_model_model.h"
#include "sl_status.h"
#include "sl_system_init.h"

static sl_status_t dump_recorded_data();
static sl_status_t load_model(void);

extern "C" int main(void) {
    sl_system_init();

    // Used to configure the platform's max clock rate
    ml_configure_clocks_to_max_rate();

    auto& logger = get_logger();

    printf("Starting Model Profiler\n");
    model_model_status = load_model();
    if (model_model_status != SL_STATUS_OK) {
        printf("Error while loading model\n");
        return -1;
    }

    auto profiler = model_model.root_profiler();
    profiling::print_metrics(profiler, &logger, TfliteMicroModel::profiling_metrics_callback, &model_model);

    // Initialize the input tensors to 0
    for (unsigned int i = 0; i < model_model.n_inputs(); ++i) {
        auto& input_tensor = *model_model.input(i);
        memset(input_tensor.data.raw, 0, input_tensor.bytes);
    }

    // If energy profiling is enabled
    // then initialize the feature now
    ENERGY_PROFILING_INIT();

    for (int i = 0; i < 2; ++i) {
        profiling::reset(profiler);
        model_model_status = slx_ml_model_model_run();
        if (model_model_status != SL_STATUS_OK) {
            printf("Error while running inference\n");
            break;
        }

        // If energy profiling is enabled,
        // then add a short delay after each inference
        ENERGY_PROFILING_INFERENCE_DELAY();
    }

    if (model_model_status == SL_STATUS_OK) {
        profiling::print_stats(profiler, &logger);
        logger.info("Successfully profiled model");
        dump_recorded_data();
    }

    return 0;
}

static sl_status_t load_model() {
    // Register the accelerator if the TFLM lib was built with one
    auto accelerator = register_tflite_micro_accelerator();
    printf("Using accelerator: %s, %s\n", accelerator->name(), accelerator->description());

    model_model.set_profiler_enabled(true);
#ifdef TFLITE_MICRO_RECORDER_ENABLED
    model_model.set_recording_flags(TfliteMicroModel::RecordingFlags::All);
#endif

    printf("Loading model\n");
    model_model_status = slx_ml_model_model_init();

    if (model_model_status != SL_STATUS_OK) {
        printf("Failed to initialize model: %ld\n", model_model_status);
        return SL_STATUS_FAIL;
    }

    dump_recorded_data();
    return SL_STATUS_OK;
}

static sl_status_t dump_recorded_data() {
#ifdef TFLITE_MICRO_RECORDER_ENABLED
    const uint8_t* buffer;
    uint32_t buffer_length;
    msgpack_object_t* root_obj;

    if (!model_model.get_recorded_data(&buffer, &buffer_length)) {
        printf("No recorded data\n");
        return SL_STATUS_FAIL;
    }

    if (msgpack_deserialize_with_buffer(&root_obj, buffer, buffer_length, MSGPACK_FLAGS_NONE) != 0) {
        printf("Failed to de-serialize recorded data\n");
        free((void*)buffer);
        return SL_STATUS_FAIL;
    }

    auto& logger = get_logger();
    if (msgpack_dump(
            root_obj,
            10,
            [](const char* s, void* arg) {
                auto& l = *reinterpret_cast<logging::Logger*>(arg);
                l.write_buffer(logging::Info, s);
            },
            &logger) != 0) {
        printf("Error while dumping recorded data\n");
    }

    msgpack_free_objects(root_obj);
    free((void*)buffer);
#endif  // TFLITE_MICRO_RECORDER_ENABLED
    return SL_STATUS_OK;
}
