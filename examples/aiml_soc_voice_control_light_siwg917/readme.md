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

The application uses `keyword_spotting_on_off_v3.tflite` as the default model.
On development boards featuring an MVP hardware accelerator, inference runs
faster, allowing larger models to be used for improved keyword-detection
accuracy.

Details about the model architectures and scripts for generating the models can
be found in the [Silicon Labs machine learning applications](https://github.com/SiliconLabs/machine_learning_applications/tree/main/) repository, under
`application/voice/keyword_spotting/model`.

Silicon Labs AI/ML projects use quantized `.tflite` models with TensorFlow Lite
for Microcontrollers. Integer quantization is recommended for deployment on
Silicon Labs devices. See [Developing a Machine Learning Model](https://docs.silabs.com/machine-learning/latest/aiml-developers-guide/developing-a-model)
for model development and conversion guidance.

The [Audio Feature Generator](https://docs.silabs.com/machine-learning/latest/aiml-developers-guide/feature-generators)
converts microphone samples into the input features expected by the model. Its
frontend parameters must match those used during model training. The compiler
configuration for the model enables model-parameter header generation so that
model-specific settings can be generated with the project and used by the audio
frontend.

## References

The example is based on TensorFlow's example called **[micro speech](https://github.com/tensorflow/tflite-micro/tree/main/tensorflow/lite/micro/examples/micro_speech)**
and audio is processed using the **[audio feature generator](https://docs.silabs.com/machine-learning/latest/aiml-reference-guide/ml-audio-feature-generation)**.

- [Machine Learning (AI/ML) Documentation](https://docs.silabs.com/machine-learning/latest/aiml-developing-with)
- [TensorFlow Lite Micro](https://www.tensorflow.org/lite/microcontrollers)
