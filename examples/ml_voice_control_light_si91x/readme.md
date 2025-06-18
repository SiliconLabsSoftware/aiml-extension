# Voice Control Light

This application uses TensorFlow Lite for Microcontrollers to detect the spoken
words "on" and "off" from audio data recorded on the microphone. The detected
keywords are used to control an LED on the board.

Audio is sampled continuously from the microphone at a rate of 8kHz. The
frequency components are then extracted by calculating the FFT on short segments
of audio as they become available. The result is an array of filterbanks,
representing the frequency components of the past second of audio. The processed
audio data is passed into a neural network at an interval of 200 ms. If the
model detects that either of the keywords "on" or "off" were spoken, LED0 will
toggle accordingly and the keyword detection result is printed on VCOM.

The application is based on TensorFlow's example application, **[micro speech](https://github.com/tensorflow/tensorflow/tree/v2.3.1/tensorflow/lite/micro/examples/micro_speech)**.

## Model

The application uses `keyword_spotting_on_off_v3.tflite`
as the default model, depending on whether the application is generated for a
development board featuring an MVP hardware accelerator or not. When an MVP
hardware accelerator is featured on the board, inference will run at a faster
speed such that a larger model can be chosen, yielding more accurate keyword
detections.

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

## I2S Pin Configuration (IMPORTANT)

The I2S pin configuration is required for correct operation of all ML audio applications. Please refer to the shared documentation for instructions: [I2S Pin Configuration Guide](https://docs.silabs.com/machine-learning/latest/aiml-getting-started/i2s-pin-configuration).

## References

The example is based on TensorFlow's example called **[micro speech](https://github.com/tensorflow/tflite-micro/tree/main/tensorflow/lite/micro/examples/micro_speech)**
and audio is processed using the **[audio feature generator](https://docs.silabs.com/machine-learning/latest/aiml-reference-guide/ml-audio-feature-generation)**.

- [Machine Learning (AI/ML) Documentation](https://docs.silabs.com/machine-learning/latest/aiml-developing-with)
- [MLTK Documentation](https://siliconlabs.github.io/mltk)
- [TensorFlow Lite Micro](https://www.tensorflow.org/lite/microcontrollers)
