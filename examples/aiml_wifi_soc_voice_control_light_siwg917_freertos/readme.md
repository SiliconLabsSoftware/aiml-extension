# Wi-Fi Voice Control Light

This application uses TensorFlow Lite for Microcontrollers to detect the spoken
words "on" and "off" from audio data recorded on the microphone. The detected
keywords are used to control an LED on the board. In addition, the detection
results are sent as UDP messages over Wi-Fi to a remote listener.

The device boots as a Wi-Fi Access Point (AP) on the 2.4 GHz band using the
WiseConnect stack under FreeRTOS. A station (STA) client joins the AP and receives
an IP address from the on-board DHCPv4 server. Once a client is connected, the
application starts the ML inference loop. When a keyword is recognised, LED0
toggles accordingly (the word "on" lights the LED, "off" turns it off), the
result is printed on VCOM, and a UDP datagram is sent to the configured server
IP and port.

Audio is sampled continuously from the microphone at a rate of 8 kHz. The
frequency components are then extracted by calculating the FFT on short segments
of audio as they become available. The result is an array of filterbanks,
representing the frequency components of the past second of audio. The processed
audio data is passed into a neural network at an interval of 200 ms. If the
model detects that either of the keywords "on" or "off" were spoken, LED0 will
toggle accordingly and the keyword detection result is printed on VCOM and sent
over UDP.

The application is based on TensorFlow's example application, **[micro speech](https://github.com/tensorflow/tensorflow/tree/v2.3.1/tensorflow/lite/micro/examples/micro_speech)**.

## Primary Use Case: DUT-to-DUT Testing

**This application is designed and validated primarily for device-under-test (DUT)
to DUT testing** — two SiWx917 boards on the same Soft AP link, with no host PC or
phone in the data path.

| Role | Board | Application |
|------|-------|-------------|
| AP (keyword source) | Board 1 | This example (`aiml_wifi_soc_voice_control_light_siwg917_freertos`) |
| STA (UDP listener) | Board 2 | [Wi-Fi UDP LED Switch](../aiml_wifi_soc_led_switch_siwg917_freertos/readme.md) (`aiml_wifi_soc_led_switch_siwg917_freertos`) |

The voice-control board acts as the Soft AP and sends keyword notifications over
UDP. The LED-switch board joins that AP as a station, binds a UDP socket on port
**5000**, and toggles its LED when it receives payloads containing `on` or `off`.

### DUT-to-DUT setup

1. **Flash NWP firmware first, then the application** on both boards.  
   *(For the Wi-Fi package version from 4.0.0 to 4.1.0, NWP firmware version
   2.15.5.0.0.12 works. ***Download the containing .rps file and flash it on both STA and AP***)* The current NWP firmware is available at
   https://github.com/SiliconLabsSoftware/wiseconnect-prerelease/tree/wiseconnect-2026.6/connectivity_firmware/standard

2. **Match the Wi-Fi credentials** on both projects:
   - AP board (this example): `DEFAULT_WIFI_AP_PROFILE_SSID` / `DEFAULT_WIFI_AP_CREDENTIAL`
     (defaults: `AIML_WIFI_SSID` / `MY_AP_PASSPHRASE` in the `.slcp` file).
   - STA board (LED switch): `DEFAULT_WIFI_CLIENT_PROFILE_SSID` /
     `DEFAULT_WIFI_CLIENT_CREDENTIAL` must use the same SSID and passphrase.

3. **Power the AP board first** (this voice-control example). Confirm on VCOM that
   the AP is up and prints its IPv4 address.

4. **Power the STA board** (LED switch). Confirm on VCOM that it associates to
   `AIML_WIFI_SSID` and receives the DHCP address **192.168.10.11** (the default
   UDP target configured in `app.c` as `SERVER_IP`).

5. **Speak "on" or "off"** near the AP board microphone. The local LED toggles,
   VCOM prints the detection, a UDP datagram is sent to `192.168.10.11:5000`, and
   the LED on the STA board follows the command.

If the STA board receives a different DHCP address, either assign it
**192.168.10.11** statically on the STA side or change `SERVER_IP` in `app.c` on
the AP board to match the address the STA actually receives, then rebuild and
reflash the AP board.

## Alpha / Experimental: DUT to Laptop or Mobile

An **alpha, experimental** path is also provided: a single DUT (this application
as AP) paired with a **laptop or mobile phone** as the UDP listener instead of a
second board. This path is useful for quick bring-up and debugging, but it is
**not** the primary supported workflow. Behaviour and addressing may change in
future releases.

On the laptop path, reception relies on the bundled Python script `udp_listener.py`,
which uses only the Python standard-library `socket` module (no third-party
packages).

### Recommended order

Follow this sequence — the order matters:

1. **Power on the Soft AP DUT board** (this voice-control example). Wait until VCOM
   shows the AP is up.
2. **Start `udp_listener.py` on the laptop** (or open your UDP listener app on the
   phone) *before* or as you prepare to join the AP.
3. **Connect the laptop or mobile to the DUT Soft AP** (`AIML_WIFI_SSID` /
   `MY_AP_PASSPHRASE`).

The DUT only starts the ML inference loop after a station associates. Once your
laptop or phone is on the Soft AP, the on-board DHCP server assigns the client IP
address (typically **192.168.10.11** for the first connected station). You do not
need to set a manual static IP on the laptop for the normal case.

### What you configure vs. what is automatic

| | Laptop (`udp_listener.py`) | Mobile (**Any App that has UDP listener capability and funcitonality to configure UDP listening port no.) |
|--|---------------------------|---------------------------|
| Wi-Fi join | Connect to `AIML_WIFI_SSID` | Connect to `AIML_WIFI_SSID` |
| Client IP | Assigned by DUT DHCP when you join the Soft AP | Assigned by DUT DHCP when you join the Soft AP |
| UDP listener | **Automatic** — the script binds to port **5000** | **You configure** — set the app to receive on UDP port **5000** |

The DUT sends keyword notifications to **192.168.10.11:5000** (`SERVER_IP` /
`SERVER_PORT` in `app.c`). After you connect to the Soft AP, addressing is handled
by the DUT DHCP server; on the laptop, `udp_listener.py` takes care of binding
the UDP receiver on port **5000**. On a phone, choose a UDP server/listener app and
set it to listen on port **5000** — that is the main manual step on mobile.

Ensure no other station on the AP is already using **192.168.10.11** (for example
a second DUT running the LED-switch example). Only one listener should be
connected for this test. If your phone or laptop receives a different DHCP address,
note it and update `SERVER_IP` in `app.c` on the DUT, then rebuild and reflash.

### Running `udp_listener.py` (socket-only steps)

The script creates a UDP socket, binds to all local interfaces on port **5000**,
and prints each datagram received from the DUT.

**Script reference** (`udp_listener.py`):

```python
import socket

s = socket.socket(socket.AF_INET, socket.SOCK_DGRAM)
s.bind(('0.0.0.0', 5000))
print('Listening on UDP port 5000...')

while True:
    data, addr = s.recvfrom(1024)
    print(f"From {addr}: {data.decode(errors='replace')}")
```

**Requirements**

- Python 3.6 or later
- No `pip install` — only the built-in `socket` module is used

**Steps**

1. **Power on the Soft AP DUT board** and confirm on VCOM that the AP is running.

2. Open a terminal on the laptop and change to the directory that contains the
   script (shipped with this example):

   ```bash
   cd path/to/aiml_wifi_soc_voice_control_light_siwg917_freertos
   ```

3. Start the listener:

   ```bash
   python3 udp_listener.py
   ```

   On Windows you may use `python udp_listener.py` if `python3` is not on your PATH.

4. Confirm the script prints:

   ```text
   Listening on UDP port 5000...
   ```

   The script binds the UDP receiver on port **5000** automatically. The process
   blocks here: `socket.recvfrom()` waits for incoming UDP packets. Leave this
   terminal open while testing.

5. **Connect the laptop to the DUT Soft AP** (`AIML_WIFI_SSID` / `MY_AP_PASSPHRASE`).
   The DUT DHCP server assigns your laptop an IP address (typically
   **192.168.10.11**). No manual static IP configuration is required on the
   laptop in the normal case.

6. Wait until the DUT has a connected station (the ML loop starts only after
   association). Speak **"on"** or **"off"** near the microphone.

7. Each keyword detection should appear in the terminal, for example:

   ```text
   Listening on UDP port 5000...
   From ('192.168.10.10', 52431): Detected class=0 label=on score=255 @157308ms

   From ('192.168.10.10', 52431): Detected class=0 label=on score=249 @160294ms

   From ('192.168.10.10', 52431): Detected class=1 label=off score=243 @160744ms

   From ('192.168.10.10', 52431): Detected class=1 label=off score=254 @163736ms
   ```

   The source address is the DUT Soft AP (typically **192.168.10.10**); the port
   varies per datagram. Each line is the full detection string sent over UDP by
   the firmware.

8. Stop the listener with `Ctrl+C`. This closes the UDP socket and exits.

### Mobile listener (experimental)

Phones cannot run `udp_listener.py` directly. Use a UDP server/listener app instead:

1. **Power on the Soft AP DUT board.**
2. **Open your UDP listener app** and set it to receive on port **5000** (this is
   the main configuration step on mobile — the script handles this automatically
   on the laptop).
3. **Connect the phone to the DUT Soft AP** (`AIML_WIFI_SSID` / `MY_AP_PASSPHRASE`).
   The DUT DHCP server assigns the phone an IP address (typically **192.168.10.11**
   for the first connected station).
4. Speak **"on"** or **"off"** near the DUT microphone and confirm the app shows
   the received payload (the same `Detected class=… label=on|off score=…` strings
   as in the laptop example above).


**Troubleshooting (alpha path)**

- No output after speaking: confirm you followed the order (DUT powered on → script
  started → laptop joined Soft AP), verify the laptop received **192.168.10.11**
  from DHCP (or update `SERVER_IP` on the DUT to match), and check the DUT VCOM log
  for UDP send success.
- `Address already in use` / bind error: another process is already bound to port
  5000; stop that process or choose a different port in both `udp_listener.py` and
  `SERVER_PORT` in `app.c` (requires rebuild).
- Firewall: allow inbound UDP on port **5000** for the Python process or
  temporarily disable the firewall for this Soft AP interface.

## Differences from the Baremetal Voice Control Light Example

This project re-uses the same keyword-spotting pipeline as the
[baremetal Voice Control Light](../aiml_soc_voice_control_light_siwg917/readme.md)
example — the same `.tflite` model, audio feature generator, and
`RecognizeCommands` post-processor — but adds Wi-Fi AP networking and FreeRTOS.
For the full ML pipeline, model, and audio-processing description, see that
[readme](../aiml_soc_voice_control_light_siwg917/readme.md).

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
notification path.

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
- [WiseConnect Wi-Fi Documentation](https://docs.silabs.com/wiseconnect/latest/)
