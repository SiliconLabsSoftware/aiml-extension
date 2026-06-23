# Wi-Fi UDP LED Switch (FreeRTOS)

This application runs the SiWx917 as a Wi-Fi station (STA) under FreeRTOS using the
WiseConnect stack. After it joins your access point and receives an IPv4 address via
DHCP, it opens a UDP socket bound to a fixed local port and listens for datagrams.
When a payload contains the substring `off`, the blue status LED is turned off; when
it contains `on`, the LED is turned on. Matching is done with substring search (not
whole-word), and `off` is evaluated before `on` if both could apply.

The LED used on **BRD2605A** is the blue LED (**LED0_B**, ULP GPIO 15). It is wired
active-low: driving the pin low turns the LED on. For other boards you may need to
adjust the GPIO mapping in `app.c`.

Serial logs on VCOM show Wi-Fi bring-up, the assigned IP address, and a line each time
an `on` or `off` command is recognised.  

This application expects the aiml_wifi_soc_voice_control_light_siwg917_freertos to be flashed first since it expects the Soft Ap + UDP detections from the other board.

## Configuration

Set your access point SSID and passphrase in the project configuration (for example the
`DEFAULT_WIFI_CLIENT_PROFILE_SSID` and `DEFAULT_WIFI_CLIENT_CREDENTIAL` values in
Simplicity Studio) .slcp file. If that doesn't work or edit the generated `sl_net_default_values.h` after generate.

In `app.c`, the UDP listen port defaults to **5000** (`LISTENING_PORT`). The build is
set up for UDP receive (`THROUGHPUT_TYPE` → `UDP_RX`) with the asynchronous BSD socket
API so incoming packets are handled in a callback.

## Testing

1. *(For the Wi-Fi package version from 4.0.0 to 4.0.1, the NWP firmware version 2.15.5.0.0.12 works.)* . The current stable version is located at https://github.com/SiliconLabsSoftware/wiseconnect-prerelease/tree/wiseconnect-2026.6/connectivity_firmware/standard
   **First, flash the `aiml_wifi_soc_voice_control_light_siwg917_freertos` application on a different board. Then proceed with the steps below.**  
   `FLASH the NWP first and then flash the app.`

2. Build, flash, and run the application. Confirm on the serial console that the device associates and prints its IPv4 address.

3. From a host on the same LAN *(running the `aiml_wifi_soc_voice_control_light_siwg917_freertos` application)*, send UDP datagrams to that IP and port.

4. Observe the blue LED and the `LED command: ON` / `LED command: OFF` messages on VCOM.

Payloads are treated as text up to a small internal buffer; any text that includes
`off` or `on` as a substring triggers the corresponding action.

## Relationship to Other Examples

The project structure and networking flow are derived from the WiseConnect WLAN
throughput sample (UDP RX path), with the throughput loop replaced by continuous
listen and GPIO control for the LED. Unlike the
[Wi-Fi Voice Control Light](../aiml_wifi_soc_voice_control_light_siwg917_freertos/readme.md)
example, this application does not use TensorFlow Lite or the microphone—it only uses
UDP text commands.

## References

- [WiseConnect Wi-Fi Documentation](https://docs.silabs.com/wiseconnect/latest/)
- [Machine Learning (AI/ML) Documentation](https://docs.silabs.com/machine-learning/latest/aiml-developing-with)
