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

The application uses one of two different available models
(`keyword_spotting_on_off.tflite` or `keyword_spotting_on_off_v2.tflite`)
as the default model, depending on whether the application is generated for a
development board featuring an MVP hardware accelerator or not. When an MVP
hardware accelerator is featured on the board, inference will run at a faster
speed such that a larger model can be chosen, yielding more accurate keyword
detections.

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
configuration for each model enables model-parameter header generation so that
model-specific settings can be generated with the project and used by the audio
frontend.

## References

The example is based on TensorFlow's example called **[micro speech](https://github.com/tensorflow/tflite-micro/tree/main/tensorflow/lite/micro/examples/micro_speech)** and audio is processed using the **[audio feature generator](https://docs.silabs.com/machine-learning/latest/aiml-reference-guide/ml-audio-feature-generation)**.

- [Machine Learning (AI/ML) Documentation](https://docs.silabs.com/machine-learning/latest/aiml-developing-with)
- [TensorFlow Lite Micro](https://www.tensorflow.org/lite/microcontrollers)
