# Profiler firmware for efr32

This application uses TensorFlow Lite for Microcontrollers together with the ml_profiler component 
to profile machine learning models on EFR Series 2 boards. It enables dynamic loading and switching
of ML models at runtime without recompiling the profiler logic, making it easier to evaluate and 
benchmark different models directly on the device..

## Behavior

The application runs inference using the currently loaded ML model and sends profiling results,
 performance metrics, and debug logs through the debug channel. In the application configuration file called ml_profiler_config.h, the user can configure runtime parameters.

At a regular interval, the application will perform inference using the active model and collect profiling data such as execution time,
 memory usage, and performance counters etc. These results are then transmitted via the debug channel for analysis.

The application supports dynamic model switching at runtime. A new model can be loaded into memory without recompiling the firmware,
 and the profiler logic will automatically adapt to the new model. This allows developers to quickly test and compare multiple models on the same hardware platform.

 ## Configuration

Before building the firmware, update the following parameters in your `.slcp` file or component configuration:

- **Arena Size**  
  `SL_TFLITE_MICRO_ARENA_SIZE` controls the TensorFlow Lite Micro tensor arena size.  
  - Current default: `-1` (auto-select from model metadata where available).  
  - You may override to a fixed size if required by your models.

- **Model Base Address**  
  Update `SL_ML_PROFILER_DEBUG_MODEL_BASE_ADDR`.  
  - Default value based on generated firmware file size from the app approximatly 650KB, we reserve 650KB from start address of flash and remaining portion can be utilized to model size.
  - Can be updated in the `.slcp` file.
  
  **Example Calculation for BRD2601B**
    - Total flash available on xG24 SoC: 1,536 KB (`0x180000`).
    - Reserved for profiler firmware: 650 KB  
    - 650 KB = 650 × 1024 = 665,600 bytes = `0x00A2800`
    - Flash base address: `0x08000000`  
    - Model region start (end of reserved firmware region, exclusive):
      `0x08000000 + 0x00A2800 = 0x080A2800`
    - Default model flatbuffer base address: `0x080A2800` (adjust if firmware size changes).
    
- Use **SL_ML_ENABLE_PROFILER_DEBUG_MSG**
  - 1-> for writing profiler statistics to debug channel.
  - 0-> To print profiler statistics on VCOM terminal.

## Model Conversion
- Change the model file extention from .tflite to .bin
- Convert to .s37 format using Simplicity Commander
 - --address specifies the Flash memory location where the model will be stored.
 - Adjust this address according to your board’s memory map and model size.
 - value of SL_ML_PROFILER_DEBUG_MODEL_BASE_ADDR should be same as address used here.
    
    `commander convert <model_name>.bin --address 0x08140000 --outfile <model_name>.s37`
## Example workflow
- **Build profiler firmware**
    
    `slc generate -p ml_profiler_firmware.slcp -d target/brd2601b --with brd2601b`
    
    `sled slc build target/brd2601b`
- **Flash firmware to board**
    
    `commander device erase`
    
    `commander flash ml_profiler_firmware.s37`
- **Flash model firmware to board**
    
    `commander flash <model_name>.s37`



