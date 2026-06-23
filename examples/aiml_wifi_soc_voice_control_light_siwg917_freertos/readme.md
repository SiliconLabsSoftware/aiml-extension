# Wi-Fi Voice Control Light

This application uses TensorFlow Lite for Microcontrollers to detect the spoken
words "on" and "off" from audio data recorded on the microphone. The detected
keywords are used to control an LED on the board. In addition, the detection
results are sent as UDP messages over Wi-Fi to a remote listener.

The device boots as a Wi-Fi Access Point (AP) on the 2.4 GHz band using the
WiseConnect stack under FreeRTOS. A laptop or phone joins the AP and receives an
IP address from the on-board DHCPv4 server. Once a client is connected, the
application starts the ML inference loop. When a keyword is recognised, LED0
toggles accordingly (the word "on" lights the LED, "off" turns it off), the
result is printed on VCOM, and a UDP datagram is sent to the configured server
IP and port. A convenience Python script `udp_listener.py` is listed below in the script section which enables to
receive and display these messages on the remote machine.

Audio is sampled continuously from the microphone at a rate of 8 kHz. The
frequency components are then extracted by calculating the FFT on short segments
of audio as they become available. The result is an array of filterbanks,
representing the frequency components of the past second of audio. The processed
audio data is passed into a neural network at an interval of 200 ms. If the
model detects that either of the keywords "on" or "off" were spoken, LED0 will
toggle accordingly and the keyword detection result is printed on VCOM and sent
over UDP.

The application is based on TensorFlow's example application, **[micro speech](https://github.com/tensorflow/tensorflow/tree/v2.3.1/tensorflow/lite/micro/examples/micro_speech)**.

## Differences from the Baremetal Voice Control Light Example

This project re-uses the same keyword-spotting pipeline as the
[baremetal Voice Control Light](../aiml_soc_voice_control_light_siwg917_baremetal/readme.md)
example — the same `.tflite` model, audio feature generator, and
`RecognizeCommands` post-processor — but adds Wi-Fi AP networking and FreeRTOS.

This application uses `si91x_mem_config_3` instead of the
`si91x_memory_default_config` used by the baremetal example and the other AI/ML
demos. The `si91x_mem_config_3` component provides a memory split that reserves
a larger share of SRAM for the wireless (NWP) subsystem while still leaving
enough room for the ML arena and FreeRTOS heap. This is necessary because the
Wi-Fi AP stack — association handling, DHCP server, and socket buffers — demands
more NWP memory than a non-wireless build.

The application also sets the boot option to `LOAD_NWP_FW` in the Wi-Fi device
configuration, which instructs the bootloader to flash and boot a new NWP
(Network Wireless Processor) firmware image that ships with the WiseConnect SDK.
The baremetal voice-control example does not load any NWP firmware because it
has no wireless functionality. The updated NWP firmware is required for Wi-Fi AP
mode, the built-in DHCPv4 server, and BSD socket support used by the UDP
notification path (for the wifi package version from 4.0.0 to 4.0.1 the nwp firmware version 2.15.5.0.0.12 works) `FLASH the NWP first and then flash the app.`
The current NWP firmware is available at https://github.com/SiliconLabsSoftware/wiseconnect-prerelease/tree/wiseconnect-2026.6/connectivity_firmware/standard 

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

## Script

Given Below is the udp_listener.py script which will listen to packets on the Station device. To use this save the below code as udp_listener.py and then with ```python udp_listener.py```. This will listen to all packets coming onto port 5000 and Ip address 192.168.10.10 . For Mobile Phone as a Station We can Use any app that listening on UDP server where the port can be configured as 5000.
```
import socket

s = socket.socket(socket.AF_INET, socket.SOCK_DGRAM)
s.bind(('0.0.0.0', 5000))
print('Listening on UDP port 5000...')

while True:
    data, addr = s.recvfrom(1024)
    print(f"From {addr}: {data.decode(errors='replace')}")
```

## References

The example is based on TensorFlow's example called **[micro speech](https://github.com/tensorflow/tflite-micro/tree/main/tensorflow/lite/micro/examples/micro_speech)**
and audio is processed using the **[audio feature generator](https://docs.silabs.com/machine-learning/latest/aiml-reference-guide/ml-audio-feature-generation)**.

- [Machine Learning (AI/ML) Documentation](https://docs.silabs.com/machine-learning/latest/aiml-developing-with)
- [TensorFlow Lite Micro](https://www.tensorflow.org/lite/microcontrollers)
- [WiseConnect Wi-Fi Documentation](https://docs.silabs.com/wiseconnect/latest/)
