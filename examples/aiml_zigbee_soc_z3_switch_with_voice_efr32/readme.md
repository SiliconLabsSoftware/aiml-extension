# Zigbee 3.0 Switch with Voice

Zigbee 3.0 provides a foundation of commissioning and network management mechanisms
to be used in all Zigbee applications. The sample scenario presented here
demonstrates the flexibility that the Zigbee 3.0 specification provides to
applications. It also acts as an excellent starting point for users wishing to
build their own Zigbee 3.0 applications.

This guide covers a **device-to-device** setup with a switch board
and a light board on a distributed Zigbee network. The light forms the network;
the switch joins, binds, and controls the light with buttons or voice keywords
**"on"** and **"off"**. See the [Two-Board Setup Guide](#two-board-setup-guide)
below.

The light provides a very simplistic user interface. On power up, the light will
perform network steering automatically. If the light does not find a suitable
network to join, it forms its own distributed network. The light will not open
its network for joining as soon as the network comes up, so this must be done
manually. Once a distributed network is formed and opened, the switch may join
its network by performing network steering. Once the light is on a network, it
will set its COMMISSIONING_STATUS_LED. When the light starts identifying as a
find-and-bind target, it will blink its COMMISSIONING_STATUS_LED. The light will
start identifying as a find-and-bind target automatically when it brings its
network up, but users can reopen the find-and-bind target window by pushing either
button on the light at any time when it is on a network.

The switch provides a simple application interface to the user. When the switch
is not on a network, it can initiate network steering to join a network using
button 0. After the switch has successfully joined a network, it will perform the
finding and binding procedure for an initiator. After this takes place, the switch
should have at least two bindings to the light in its binding table: one for the
On/Off cluster and one for the Level Control cluster. While the switch is
performing its network commissioning, it will blink its COMMISSIONING_STATUS_LED.
When the switch is active on the network, it will set its COMMISSIONING_STATUS_LED.
Once the switch has finished finding and binding, users can use buttons 0 and 1
to send On/Off Toggle and Level Control Move to Level commands to the light,
respectively.

Voice recognition has been added to the Switch with Voice scenario application.
The **ML compiler** component (`ml_compiler`) compiles the bundled
`keyword_spotting_on_off_v2.tflite` into target-specific sources under the
project autogen folder (same flow as the SoC Audio Classifier MicriumOS example).
At runtime the app loads recognition settings (`average_window_duration_ms`,
`detection_threshold`, `suppression_ms`, `minimum_count`, `latency_ms`, `classes`,
and so on) from the model flatbuffer metadata, configures the audio feature
generator from that metadata, and runs inference through the generated
`slx_ml_keyword_spotting_on_off_v2_model_init` /
`slx_ml_keyword_spotting_on_off_v2_model_run` API. Once the switch has finished
finding and binding, the user can turn the bound light on or off by speaking the
trained keywords (typically **"on"** / **"off"**) into the microphone.

Application hooks: `keyword_detection_init()` runs from
`sl_zigbee_af_main_init_cb`, and `keyword_detection_process_action()` runs each
main loop iteration from `sl_zigbee_af_main_tick_cb`, driven by a periodic
sleeptimer set from the model `latency_ms` parameter.

---

## Two-Board Setup Guide

This is the **device-to-device** workflow.

Throughout this guide, **switch** and **light** refer to the two applications
below:

| Role | Example project | Board | CLI prompt |
|------|-----------------|-------|------------|
| Switch | `AI/ML - SoC Zigbee 3.0 Switch with Voice EFR32` | BRD2601B / BRD2608A / BRD2608B | `zigbee_z3_switch_with_voice>` |
| Light | `Zigbee - SoC Z3 Light` | Any Zigbee 3.0-capable EFR32 kit | `zigbee_z3_light>` |
| Bootloader | `Bootloader - SoC SPI Flash Storage (single image)` | Both boards | — |

The switch joins the network, binds to the light, and sends On/Off and Level
Control commands. The light forms a distributed network and acts as the bound
light target.

---

## Software Requirements

| Tool | Purpose |
|------|---------|
| **Simplicity Studio 6/5** | Create, build, and flash all example projects |
| **Simplicity Commander** | Flash `.s37` images and open the serial console (included with Simplicity Studio) |

Install the **Silicon Labs AI/ML** extension through the Simplicity Studio
**SDK Manager** before creating the example projects below.

Add the **ZAP** tool directory to your system **PATH** so Zigbee project
generation can find it. The default SLT location is
`~/.silabs/slt/installs/archive/zap` on Linux and macOS, and
`%USERPROFILE%\.silabs\slt\installs\archive\zap` on Windows.

**Windows** — add to **PATH** (System Properties → Environment Variables), for
example:

```text
C:\Users\<username>\.silabs\slt\installs\archive\zap
```

**Linux** — add to `~/.bashrc` or `~/.profile`:

```bash
export PATH="$HOME/.silabs/slt/installs/archive/zap:$PATH"
```

**macOS** — add to `~/.zshrc` or `~/.bash_profile`:

```bash
export PATH="$HOME/.silabs/slt/installs/archive/zap:$PATH"
```

Restart Simplicity Studio (or open a new terminal) after updating **PATH**.

---

## 1. Build the Applications

Build the switch, light, and bootloader example projects from the table above in
**Simplicity Studio**.

---

## 2. Flash Bootloader and Application

On **both** boards, erase, flash the bootloader, then flash the application:

```text
commander device erase
commander flash <path-to-bootloader>.s37
commander flash <path-to-application>.s37
```

---

## 3. Connect the Boards

### Prerequisites

1. Switch board flashed with bootloader and switch application.
2. Light board flashed with bootloader and light application.
3. Both boards connected via USB.
4. Boards labeled and placed close together (~1 m).

### 3.1 Open serial consoles

Each board exposes a **Virtual COM (VCOM)** port over USB. Open one console per
board to run Zigbee CLI commands and view debug output using **Simplicity
Commander**.

List connected J-Link adapters:

```text
commander adapter list
```

Open the serial console on a single board:

```text
commander vcom connect
```

When multiple boards are connected, specify the J-Link serial number:

```text
commander vcom connect --serialno <J-Link-serial-number>
```

You should see:

```text
Opening serial port 'COM3'...
Connection established!
```

Type CLI commands at the prompt.

If Enter does not submit commands, try a different line ending:

```text
commander vcom connect --lineending cr
```

Open a **second terminal** for the other board and repeat with its serial number.

At boot, the switch should print keyword detection settings from the model
metadata. Fatal errors such as `Failed to load keyword model` indicate a build or
board-target mismatch.

### 3.2 Join the Zigbee network

The light forms a distributed network. The switch joins that network, then
find-and-bind links the switch to the light.

**Step 1 — Form and open the network on the light**

On the **light** console:

```text
network leave
plugin network-creator start 0
plugin network-creator-security open-network
```

**Note — join timeout:** The light only accepts new devices for a **limited time**
after `open-network`. The light console reports the remaining join window, for
example `SL_STATUS_ZIGBEE_NETWORK_OPENED: 254 sec` or a similar message. Press
**BTN0** on the switch (or run `plugin network-steering start 0`) **before this
timer expires**. If the window closes, run `plugin network-creator-security
open-network` again on the light and retry Step 2.

**Step 2 — Join the switch**

On the **switch**, press **BTN0**, or run on the switch console:

```text
plugin network-steering start 0
```

Wait a few seconds for find-and-bind to complete automatically, then try voice
control: speak **"on"** or **"off"** toward the **bottom** of the switch board.
If the light responds and the switch console shows `Heard on` / `Heard off` and
`Send to bindings: 0x00`, setup is complete and the app is working as expected.

**If voice or button control does not work**, continue from Step 3 below.

**Step 3 — Verify network membership**

On **both** consoles:

```text
info
```

Confirm both devices report the same **PAN ID**. The switch LED should be solid,
then continue with [Find and bind](#33-find-and-bind) below.

### 3.3 Find and bind

Use this section when control did not work after Step 2. After a successful join,
the switch automatically starts find-and-bind initiator after 3 seconds. The light
acts as a find-and-bind target when its network is up.

On the **switch** console, expect:

```text
Find and bind initiator start: 0x00
Find and bind initiator complete: 0x00
```

If find-and-bind does not start automatically, run manually.

**Switch:**

```text
plugin find-and-bind initiator 1
```

**Light:**

```text
plugin find-and-bind target 1
```

Verify bindings on the switch:

```text
binding print
```

You should see at least two bindings to the light: **On/Off** and **Level Control**.

### 3.4 Test control

**Buttons**

| Action | Expected result |
|--------|-----------------|
| Press **BTN0** on switch | Light toggles on/off |
| Press **BTN1** on switch | Light brightness changes |
| Switch console | `Send to bindings: 0x00` |

**Voice**

1. Speak clearly toward the **bottom** of the switch board.
2. Say **"on"** — light should turn on.
3. Say **"off"** — light should turn off.

On the switch console:

```text
Heard on (<score>) @<time>ms
Send to bindings: 0x00
```

---

## Verification Checklist

| # | Test | Pass criteria |
|---|------|---------------|
| 1 | ML boot | Keyword settings printed; no fatal init errors |
| 2 | Network join | Same PAN ID on light and switch; switch LED solid |
| 3 | Find and bind | `complete: 0x00`; bindings listed in `binding print` |
| 4 | BTN0 | Light toggles |
| 5 | BTN1 | Light level changes |
| 6 | Voice "on" | `Heard on` logged; light turns on |
| 7 | Voice "off" | `Heard off` logged; light turns off |

---

## Useful CLI Commands

**Switch and light**

```text
info                              # Network state, PAN ID, node ID
network leave                     # Leave current network
plugin network-steering start 0   # Start network steering (switch)
plugin find-and-bind initiator 1  # Start find-and-bind (switch)
binding print                     # Show binding table (switch)
```

**Light only**

```text
plugin network-creator start 0
plugin network-creator-security open-network
plugin find-and-bind target 1
```

---

## Troubleshooting

Use this section if setup or control does not work after following
[Connect the Boards](#3-connect-the-boards).

### Serial console

Open one **Simplicity Commander** VCOM session per board. Run `commander` commands
only in the host terminal — not at the `zigbee_z3_light>` or
`zigbee_z3_switch_with_voice>` prompt (`No command found` means a Commander
command was typed in the Zigbee CLI).

If Enter does not submit a Zigbee CLI command, exit VCOM and reconnect with:

```text
commander vcom connect --serialno <J-Link-serial-number> --lineending cr
```

| Setting | Value |
|---------|-------|
| Baud rate | 115200 |
| Data bits | 8 |
| Parity | None |
| Stop bits | 1 |

### Switch LED (LED0)

| State | Meaning |
|-------|---------|
| Blinking | Commissioning in progress |
| Solid on | Joined to the network |
| Off | Not on a network |

### Common issues

| Symptom | Likely cause | What to try |
|---------|--------------|-------------|
| `Failed to load keyword model` or `Failed to init audio feature generator` | Wrong board target or incomplete build | Rebuild `AI/ML - SoC Zigbee 3.0 Switch with Voice EFR32` in Simplicity Studio for BRD2601B, BRD2608A, or BRD2608B |
| No keyword settings at switch boot | ML init did not run | Confirm the switch board has a microphone and MVP; rebuild for the correct target |
| Switch will not join | Light network not open, boards too far apart, or join window expired | On the light console: `plugin network-creator-security open-network`; move boards within ~1 m; join the switch with **BTN0** before the open-network timer expires (see [join timeout note](#32-join-the-zigbee-network)) |
| Different PAN ID on `info` | Switch and light are not on the same network | Repeat [Join the Zigbee network](#32-join-the-zigbee-network) from Step 1 |
| `Heard on` / `Heard off` logged but light unchanged | Find-and-bind not complete or no bindings | On the switch: wait for `Find and bind initiator complete: 0x00`, then run `binding print`; if needed run find-and-bind manually ([§3.3](#33-find-and-bind)) |
| No `Heard on/off` messages | Noise, distance, or mic placement | Use a quiet room; speak 15–30 cm from the **bottom** of the switch board; wait 1–2 s between commands |
| Buttons on switch have no effect | Same as missing bindings | Complete find-and-bind; confirm bindings with `binding print` |
| Device does not boot after flash | Bootloader missing or erased | On each board: `commander device erase`, flash bootloader `.s37`, then application `.s37` ([§2](#2-flash-bootloader-and-application)) |
| `commander vcom connect` opens wrong board | Multiple kits connected | Run `commander adapter list` and pass `--serialno` for each board |

---

## Additional Resources

- [Simplicity Studio User's Guide](https://docs.silabs.com/simplicity-studio-5-users-guide/latest)
- [Simplicity Commander VCOM commands](https://docs.silabs.com/simplicity-commander/latest/simplicity-commander-commands/vcom-commands)
- [Gecko Bootloader User's Guide](https://docs.silabs.com/mcu-bootloader/latest/)
- [Zigbee finding and binding](https://docs.silabs.com/zigbee/latest/zigbee-concepts-app-layer/)
- [Silicon Labs ML documentation](https://docs.silabs.com/machine-learning/latest)

---

## Additional Features

The following capabilities are inherited from the base Zigbee Z3 sample. Gateway
and Touchlink options below are available in the application but **not tested**
end-to-end with voice control in this release.

These applications can take on three possible roles.

1. `zigbee_z3_gateway` — the gateway can form a centralized network, and the
   light and the switch can join the centralized network by performing network
   steering.
2. **Device-to-device (this application):** `Zigbee - SoC Z3 Light`, acting as a
   router, forms a distributed network, and
   `AI/ML - SoC Zigbee 3.0 Switch with Voice EFR32`, acting as an end device,
   joins that network. This is the workflow in the
   [Two-Board Setup Guide](#two-board-setup-guide).
3. `Zigbee - SoC Z3 Light`, acting as a Touchlink target, can Touchlink with the
   switch, acting as a Touchlink initiator.

Note: Touchlinking is optional in Zigbee 3.0. In this application the Touchlink
functionality is available on endpoint 2. This functionality can be removed by
deleting endpoint 2 in the Zigbee Cluster Configurator. When the switch is not on
a network, it can also initiate Touchlink commissioning using button 1.

The gateway provides a CLI command application interface to the user. The CLI
command `plugin network-creator start 1` can be issued to form a centralized
network. The gateway application can then be triggered to allow other devices
onto the network with the CLI command
`plugin network-creator-security open-network`. Devices can then join the network
using the ZigBeeAlliance09 link key, or by manually entering the install
code-derived link key into the gateway using the CLI command
`plugin network-creator-security set-joining-link-key`. The CLI command
`plugin network-creator-security close-network` will close the network and no
longer allow devices onto the gateway's network.

This application also demonstrates functionality of a Green Power proxy
application.

The current debug printing settings in these applications are only for the
purpose of aiding users in understanding and debugging this sample scenario.
Debug printing should be turned off in order to save code size on a final product.
