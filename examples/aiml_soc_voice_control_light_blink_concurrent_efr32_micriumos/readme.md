# Voice Control Light + Sine Blink Concurrent (MicriumOS)

This application uses microphone audio to detect the spoken commands **"on"** and **"off"** in a MicriumOS kernel task. When **"on"** is detected, a sine blink model drives LED0 with PWM for three full cycles, then leaves the LED on at full duty. When **"off"** is detected, the application stops the LED output and turns LED0 off.

The concurrent load variant initializes both the keyword spotting model and the sine blink model at startup. This keeps both models available in RAM so the LED animation can run without swapping model memory.

Audio is sampled from the microphone and converted into features for keyword inference at the configured interval.

## Models

- **keyword_spotting_on_off_v2** or **keyword_spotting_on_off**: detects spoken **"on"** and **"off"** commands.
- **blink.tflite**: maps a phase value from **0 … 2π** to an LED PWM duty cycle.
