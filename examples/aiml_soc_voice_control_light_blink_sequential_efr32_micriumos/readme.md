# Voice Control Light + Sine Blink Sequential (MicriumOS)

This application uses microphone audio to detect the spoken commands **"on"** and **"off"** in a MicriumOS kernel task. When **"on"** is detected, a sine blink model drives LED0 with PWM for three full cycles, then leaves the LED on at full duty. When **"off"** is detected, the application stops the LED output and turns LED0 off.

The sequential load variant keeps only one model initialized in RAM at a time. It runs keyword spotting during normal operation, switches to the sine blink model for the LED animation, then returns to keyword spotting after the animation completes.

Audio is sampled from the microphone and converted into features for keyword inference at the configured interval.

## Models

- **keyword_spotting_on_off_v2** or **keyword_spotting_on_off**: detects spoken **"on"** and **"off"** commands.
- **blink.tflite**: maps a phase value from **0 … 2π** to an LED PWM duty cycle.
