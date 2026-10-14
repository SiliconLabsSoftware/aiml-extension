# Audio Classifier

This application uses TensorFlow Lite for Microcontrollers to run audio
classification machine learning models to classify words from audio data
recorded from a microphone. The detection is visualized using the LED's on the
board and the classification results are written to the VCOM serialport.

## Behavior

The application is using two LEDs to show detection and activity and it is
printing detection results and debug log output on the VCOM serial port. In the
application configuration file called audio_classifier_config.h the user can
select which LED to use for activity and which LED to use for detection. By
default the detection LED is green/led1 and the activity LED is red/led0.

At a regular interval the application will perform an inference and the result
will be processed to find the average score for each class in the current
window. If the top result score is higher than a detection threshold then a
detection is triggered and the detection LED (green) will light up for about 750
ms.

Once the detection LED turns off the application goes back to responding to the
input data. If the change in model output is greater than a configurable
sensitivity threshold, then the activity LED (red) will blink for about 500 ms.

The activity LED indicates that audio has been detected on the input and the
model output is changing, but no clear classification was made.

In audio classification it is common to have some results that map to silence or
unknown. These results are something that we usually want to ignore. This is
being filtered out in the audio classifier application based on the label text.
By default any labels that start with an underscore are ignored when processing
results. This behavior can be disabled in the application configuration file.

## Model

The application uses `keyword_spotting_on_off_v3.tflite` as the default model.
On development boards featuring an MVP hardware accelerator, inference runs
faster, allowing larger models to be used for improved keyword-detection
accuracy.

Details about the model architectures and scripts for generating the models can
be found in the [Silicon Labs machine learning applications](https://github.com/SiliconLabs/machine_learning_applications/tree/main/) repository, under
`application/voice/keyword_spotting/model`.

The application is designed to work with an audio classification model created
using the Silicon Labs Machine Learning Toolkit
([MLTK](https://siliconlabs.github.io/mltk)). Use the MLTK to train a new audio
classifier model and replace the model inside this example with the new audio
classification model. To replace the audio classification model with a new model
created using the MLTK simply replace the .tflite file in the config/tflite folder
of this project with your new. tflite file. After a new .tflite file is added
to the project Simplicity Studio will automatically use the [flatbuffer converter tool](https://docs.silabs.com/machine-learning/latest/aiml-developers-guide/flatbuffer-conversion)
to convert the .tflite file into a c file which is added to the project.

In order for the audio classification to work correctly we need to use the same
audio feature generator configuration parameters for inference as is used when
training the model. When using the MLTK to train an audio classification model
the model parameters will be embedded in the metadata section of the .tflite
file. When generating a project this metadata is extracted by tools in the Simplicity
SDK and placed in the file called sl_tflite_micro_model_parameters.h inside the
autogen folder of the example.

This example will automatically use the parameters from the .tflite file in
order to configure the audio feature generator to correctly extract features
from input audio data which can be passed to TensorFlow when doing inference.
See the MLTK documentation for more information about the model parameters:
<https://siliconlabs.github.io/mltk/docs/model_parameters.html>

# Switching between I2S0 and ULP I2S on brd2605a / brd2605b / brd4338a

The microphone driver (`sl_ml_mic_i2s_si91x.c`) supports two I2S peripherals on the SiWG917. Only **one** is active per build:

| Mode | Peripheral | Typical use case |
|------|-----------|-----------------|
| **I2S0** (full-power) | `SL_I2S0_PERIPHERAL` | Onboard digital microphone (brd2605a / brd2605b) or external mic on HP I2S GPIOs |
| **ULP I2S** (ultra-low-power) | `SL_ULP_I2S_PERIPHERAL` | External INMP441 (or similar) mic on ULP GPIOs for low-power audio |

### How to switch

Change a **single value** in the `.slcp` file under `configuration:` and rebuild:

```yaml
configuration:
  - name: SL_MIC_USE_ULP_I2S
    value: 0   # 0 = I2S0,  1 = ULP I2S
```

- **`SL_MIC_USE_ULP_I2S = 0`** — selects I2S0. The build includes `sl_si91x_i2s_init_i2s0_config.h`, which defines `SL_I2S0_PERIPHERAL` and uses the `SL_I2S0_*` pin defines.
- **`SL_MIC_USE_ULP_I2S = 1`** — selects ULP I2S. The build includes `sl_si91x_i2s_init_ulp_i2s_config.h`, which defines `SL_ULP_I2S_PERIPHERAL` and uses the `SL_ULP_I2S_*` pin defines.

No other file needs to change. The conditional include is handled in `sl_i2s_instances.h`.

### Pin configuration

Both sets of pin defines are always present in the `.slcp` `configuration:` section. Only the set matching the active instance is used at build time.

**I2S0 pins (brd2605a / brd2605b / brd4338a):**

Port IDs `RTE_I2S0_SCLK_PORT_ID`, `RTE_I2S0_WSCLK_PORT_ID`, `RTE_I2S0_DIN0_PORT_ID` are set to `2` for the standard brd2605a / brd2605b / brd4338a schematic. No manual pin configuration is needed for I2S0.

**ULP I2S pins (brd2605a / brd2605b / brd4338a):**

1. In the `.slcp` file, set the I2S instance to **`ulp_i2s`**:
   ```yaml
   component:
     - id: i2s_instance
       instance: [ulp_i2s]
   ```
2. Ensure the `.slcp` `configuration:` section has `SL_MIC_USE_ULP_I2S` set to `1` and the ULP I2S pin values:
   - `SL_ULP_I2S_SCLK_PIN = 7`, `SL_ULP_I2S_WSCLK_PIN = 10`, `SL_ULP_I2S_DIN0_PIN = 6`

| Signal | Define | Value | ULP GPIO |
|--------|--------|-------|----------|
| SCLK | `SL_ULP_I2S_SCLK_PIN` | 7 | ULP_GPIO_7 |
| WSCLK | `SL_ULP_I2S_WSCLK_PIN` | 10 | ULP_GPIO_10 |
| DIN0 | `SL_ULP_I2S_DIN0_PIN` | 6 | ULP_GPIO_6 |

### Required: pin_config.h for ULP I2S

**This step must be repeated every time the `target/` folder is regenerated.**

The `.slcp` pin values (`SL_ULP_I2S_*`) only reach the SL API layer. The CMSIS I2S driver (`rsi_i2s.c`) reads pin assignments from `RTE_Device_917.h`, which in turn reads from `pin_config.h`. Without the entries below, the CMSIS driver falls back to wrong default pins and the I2S DMA receive will never complete.

After generating or regenerating the project, open `target/brd2605a/config/pin_config.h` (or `target/brd2605b/config/pin_config.h` for BRD2605B) and add the following inside the `// $[ULP_I2S]` section:

```c
// $[ULP_I2S]
#define ULP_I2S_SCLK_PIN  7
#define ULP_I2S_SCLK_LOC  0

#define ULP_I2S_WSCLK_PIN 10
#define ULP_I2S_WSCLK_LOC 3

#define ULP_I2S_DOUT0_PIN 1
#define ULP_I2S_DOUT0_LOC 4

#define ULP_I2S_DIN0_PIN  6
#define ULP_I2S_DIN0_LOC  8
// [ULP_I2S]$
```

These defines bridge the pin assignment to the CMSIS driver via `RTE_Device_917.h`:

| Define | Value | Effect in RTE_Device_917.h |
|--------|-------|---------------------------|
| `ULP_I2S_SCLK_PIN=7, LOC=0` | ULP_GPIO_7 | `RTE_I2S1_SCLK_PIN = 71` (7 + GPIO_MAX_PIN) |
| `ULP_I2S_WSCLK_PIN=10, LOC=3` | ULP_GPIO_10 | `RTE_I2S1_WSCLK_PIN = 74` (10 + GPIO_MAX_PIN) |
| `ULP_I2S_DIN0_PIN=6, LOC=8` | ULP_GPIO_6 | `RTE_I2S1_DIN0_PIN = 70` (6 + GPIO_MAX_PIN) |

If this section is left empty, the CMSIS driver defaults to SCLK=ULP_GPIO_8, WSCLK=ULP_GPIO_2, DIN0=ULP_GPIO_6 which will not match the microphone wiring.

## References

The example is based on TensorFlow's example called **[micro speech](https://github.com/tensorflow/tflite-micro/tree/main/tensorflow/lite/micro/examples/micro_speech)**
and audio is processed using the **[audio feature generator](https://docs.silabs.com/machine-learning/latest/aiml-reference-guide/ml-audio-feature-generation)**.

- [Machine Learning (AI/ML) Documentation](https://docs.silabs.com/machine-learning/latest/aiml-developing-with)
- [TensorFlow Lite Micro](https://www.tensorflow.org/lite/microcontrollers)
