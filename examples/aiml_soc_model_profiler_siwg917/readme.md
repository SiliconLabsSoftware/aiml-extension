# TensorFlow Model Profiler

This application is designed to profile a TensorFlow Lite Micro model on Silicon
Labs hardware. The model used by the application is provided by a TensorFlow
Lite flatbuffer at `tflite_models/tflite/soc_model_profiler.tflite` (avoid the basename `model`; it collides with generated C++ symbol names). The
profiler will measure the number of CPU clock cycles and elapsed time in each
layer of the model when performing an inference. It will also produce a summary
when inference is done. The input layer of the model is filled with all zeroes
before performing a single inference. Profiling results are transmitted over
VCOM.