# Voice Control Light + Sine Blink (SiWG917)

This application is the same **Voice Control Light** flow as `aiml_soc_voice_control_light_siwg917`
(continuous microphone capture, keyword spotting for **on** / **off**, VCOM logs), plus **sine
blink** math from **`aiml_soc_blink_siwg917`**: same `TIME_DELAY_MS`-style frame (**10 ms**),
steps per **0 → 2π** cycle, tensor I/O, and brightness → **off** portion of the frame. The sine
effect uses **only** the **indicator** LED (`DETECTION_ON`): a small software PWM in
`voice_control_light.cc` (no separate RGB driver files in this app).

## Behavior

- **Keyword spotting** runs on every super-loop iteration, unchanged from the base voice app.
- **Indicator LED** (`DETECTION_ON`): **on** / **off** voice commands still control steady on/off;
  after **on**, the **same** LED runs the sine-style PWM for **three** full cycles, then stays
  **on** until **off**.
- **off**: stops any sine sequence and turns the indicator off.

The number of sine cycles is `SINE_WAVE_LOOPS` (default **3**) in `voice_control_light.cc`.

## Models

- `keyword_spotting_on_off_v3.tflite` — keyword pipeline (unchanged).
- `blink.tflite` — same as `aiml_soc_blink_siwg917`.

## References

See [Voice Control Light SiWG917](../aiml_soc_voice_control_light_siwg917/readme.md) and
[Blink SiWG917](../aiml_soc_blink_siwg917/readme.md).
