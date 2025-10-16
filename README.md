# PanTiltZoom Firmware

## Project overview
The PanTiltZoom (PTZ) firmware drives a three-axis camera head using an ESP32
and stepper-motor drivers while accepting operator input from Bluetooth game
controllers via the Bluepad32 stack. The firmware splits responsibilities across
FreeRTOS tasks, translating controller snapshots into motion commands, handling
safety interlocks, and persisting presets in EEPROM for quick recall. 【F:PTZ_Head_Firmware.ino†L1-L122】【F:MotionController.cpp†L1-L137】

## Hardware reference
### Control electronics
- **Microcontroller:** ESP32 development board (two-core FreeRTOS scheduling and
  native Bluetooth are required for Bluepad32). 【F:PTZ_Head_Firmware.ino†L59-L122】
- **Motor drivers:** Three step/dir/enable stepper drivers (e.g. TMC, DRV, or
  A4988) connected to the ESP32. Each axis uses an individual driver that is
  configured by `MotionController::configureStepper`. 【F:PTZ_Head_Firmware.ino†L10-L19】【F:MotionController.cpp†L37-L65】
- **Motors:** NEMA-class steppers or similar that are compatible with the chosen
  drivers. The firmware controls velocity via FastAccelStepper in Hz, so ensure
  driver settings and supply voltage match the desired slew rates. 【F:MotionController.cpp†L8-L104】
- **Bluetooth gamepad:** Any controller supported by Bluepad32. Triggers provide
  zoom, left stick handles pan, and right stick controls tilt. 【F:InputMapper.cpp†L1-L84】

### Pin map (default)
| Axis | STEP | DIR | ENABLE |
|------|------|-----|--------|
| Pan  | GPIO 2 | GPIO 3 | GPIO 4 |
| Tilt | GPIO 5 | GPIO 6 | GPIO 7 |
| Zoom | GPIO 8 | GPIO 9 | GPIO 10 |

These assignments are declared at the top of `PTZ_Head_Firmware.ino`. Adjust the
constants to match your wiring before compiling if you need different pins. 【F:PTZ_Head_Firmware.ino†L10-L19】

### Power and wiring considerations
- Supply each stepper driver with the appropriate motor voltage and current
  limits. Only connect ESP32 GPIO to the driver logic (typically 3.3–5 V logic).
- Tie all logic grounds together (ESP32, drivers, and any encoder modules) to
  avoid communication glitches.
- If you add magnetic encoders (e.g. AS5600), wire them on the I²C bus and
  update `MotionController::refreshEncoderData()` accordingly. 【F:MotionController.cpp†L109-L112】

## Firmware architecture
### Task model
- **ControlTask (500 Hz):** Receives controller events from Bluepad32, maps them
  into `InputState`, forwards updates to the motion queue, and performs preset
  management plus safety checks. 【F:PTZ_Head_Firmware.ino†L123-L222】
- **MotionTask (1 kHz):** Consumes the latest `InputState`, applies velocities to
  each axis through FastAccelStepper, and keeps the safety heartbeat alive. 【F:PTZ_Head_Firmware.ino†L223-L258】
- **EncoderTask (200 Hz):** Placeholder for polling external encoders to improve
  positional feedback. 【F:PTZ_Head_Firmware.ino†L259-L271】

### Core modules
- **InputMapper:** Converts raw Bluepad32 snapshots into normalized pan/tilt/zoom
  velocities with deadzones, exponential curves, and button-based actions such as
  preset save/recall and diagnostics dumps. 【F:InputMapper.cpp†L1-L84】
- **MotionController:** Initializes the three steppers, applies acceleration
  limits, maintains the latest step counts, and executes preset moves. 【F:MotionController.cpp†L1-L139】
- **SafetyManager:** Tracks controller presence, timeouts, and heartbeats to
  force the rig idle if input is lost. 【F:SafetyManager.cpp†L1-L75】
- **PresetManager:** Stores axis positions in EEPROM with a lightweight CRC and
  recalls them on demand. Slot 0 is currently used for both saving and recall. 【F:PresetManager.cpp†L1-L74】
- **Logger:** Wraps the target `Stream` for timestamped log output. 【F:Logger.cpp†L1-L55】

## Software prerequisites
- **Arduino IDE 2.x** (or PlatformIO) with the ESP32 board support package
  installed so the sketch can target an ESP32 development board.
- **Libraries:**
  - [Bluepad32](https://github.com/bluepad32/bluepad32) for Bluetooth controller
    support. 【F:PTZ_Head_Firmware.ino†L2-L3】
  - [FastAccelStepper](https://github.com/ralphjy/FastAccelStepper) to drive the
    stepper motors. 【F:PTZ_Head_Firmware.ino†L3-L4】【F:MotionController.h†L3-L6】
  - EEPROM is provided by the ESP32 core, no extra install required. 【F:PresetManager.h†L4-L7】

## Uploading the firmware (Arduino IDE example)
1. Install the **ESP32** board package in Arduino IDE via *File → Preferences →
   Additional Boards Manager URLs* and select your ESP32 module (e.g.
   “ESP32 Dev Module”).
2. Install the **Bluepad32** and **FastAccelStepper** libraries through the
   Library Manager.
3. Open `PTZ_Head_Firmware.ino` from this repository. 【F:PTZ_Head_Firmware.ino†L1-L271】
4. Review and adjust the step/dir/enable pin constants to match your wiring. 【F:PTZ_Head_Firmware.ino†L10-L19】
5. Connect the ESP32 via USB, choose the correct **Port** and **Board** in the
   *Tools* menu, then click **Upload**. Compilation will pull in all `.cpp` and
   `.h` modules automatically.
6. Monitor the serial output at 115200 baud to confirm startup, controller
   pairing, and diagnostic messages. 【F:PTZ_Head_Firmware.ino†L40-L94】【F:Logger.cpp†L21-L41】

### PlatformIO notes
If you prefer PlatformIO, create a new ESP32 project, copy the repository files
into `src/`, and add Bluepad32 plus FastAccelStepper to `lib_deps`. Set the
upload speed and monitor baud to 115200.

## Operating the PTZ head
1. Power the stepper drivers and ESP32, then open the serial monitor to watch
   logs.
2. Pair a supported Bluetooth gamepad; the firmware reports connections through
   `onConnectedGamepad`. 【F:PTZ_Head_Firmware.ino†L95-L172】
3. Use the controller:
   - **Pan:** Left stick horizontal (`axisLX`).
   - **Tilt:** Right stick vertical (`axisRY`, inverted for natural camera feel).
   - **Zoom:** Right trigger zooms in (`R2`), left trigger zooms out (`L2`).
   - **Preset save:** Hold **A** for ≥2 s to store the current position in slot 0.
   - **Preset recall:** Tap **B** twice within 1.5 s to recall slot 0.
   - **Reset encoders:** Press **Start** once.
   - **Diagnostics:** Hold both stick buttons simultaneously.
     【F:InputMapper.cpp†L20-L84】
4. The SafetyManager forces the rig to idle if no input is received for roughly
   300 ms, or if the controller disconnects. 【F:SafetyManager.cpp†L21-L63】
5. Preset data survives reboots because it is written to EEPROM with a CRC. 【F:PresetManager.cpp†L1-L74】

## Extending the system
- Map additional preset slots by decoding more button combos in
  `InputMapper::map` and `PresetManager::saveCurrentPosition`. 【F:InputMapper.cpp†L20-L84】【F:PresetManager.cpp†L25-L49】
- Implement encoder feedback in `MotionController::refreshEncoderData()` for
  closed-loop positioning. 【F:MotionController.cpp†L105-L112】
- Integrate a configuration web UI as hinted by the TODO in `setup()`. 【F:PTZ_Head_Firmware.ino†L71-L96】

## Repository layout
```
PanTiltZoom/
├── PTZ_Head_Firmware.ino   # Main Arduino sketch (task scheduling & setup)
├── InputMapper.*           # Controller snapshot parsing and mapping
├── MotionController.*      # Stepper configuration and runtime control
├── PresetManager.*         # EEPROM persistence for presets
├── SafetyManager.*         # Watchdog logic for controller safety
└── Logger.*                # Serial logging helper
```

Use this document as a baseline reference when assembling hardware, wiring the
rig, and deploying updated firmware.
