# ESP32 F1 Reaction Game

**An F1-inspired reaction time tester built with ESP32-S3, an ILI9341 TFT display, and physical LEDs.**

A beginner-friendly embedded systems project that simulates a Formula 1 starting light sequence and measures the player's reaction time using a physical push button.

The project features a graphical interface, false-start detection, selectable operating modes, and configurable timing compensation.

**Current status:** Functional Wokwi simulation. Physical hardware testing and timing calibration are planned.

## Project Preview

### Circuit Diagram

![Circuit Diagram](images/circuit.png)

### Gameplay

![Gameplay](images/gameplay.png)

**[▶ Run the project in Wokwi](https://wokwi.com/projects/479746846531493889)**

## Features

- Five physical red LEDs controlled by ESP32-S3 GPIO pins
- 320 × 240 TFT graphical interface
- F1-style starting lights arranged in two rows and five columns
- Randomized lights-out sequence
- Reaction time measurement using GPIO interrupts
- False-start and timeout detection
- Manual start and reset using a single push button
- Physical switch for selecting Debug or Normal mode
- Configurable timing multiplier and fixed latency compensation
- Serial output for inspecting measured results

## Hardware Requirements

| Component | Quantity | Description |
|---|---:|---|
| ESP32-S3 development board | 1 | ESP32-S3 DevKitC-style board |
| ILI9341 TFT display | 1 | 2.8-inch, 320 × 240, SPI |
| Red LEDs | 5 | 5 mm LEDs |
| 220Ω resistors | 5 | Current limiting for LEDs |
| Momentary push button | 1 | Start, reaction, and reset |
| Slide switch | 1 | Debug / Normal selection |
| Breadboard | As needed | Solderless prototyping |
| Jumper wires | As needed | Electrical connections |
| USB cable | 1 | Programming and power |

The planned physical development board is an ESP32-S3 N16R8 variant with 16 MB Flash and 8 MB PSRAM.

## Pin Connections

### TFT Display — SPI Interface

| TFT Pin | ESP32-S3 |
|---|---|
| VCC | 3V3 |
| GND | GND |
| CS | GPIO10 |
| RST | GPIO8 |
| D/C | GPIO9 |
| MOSI | GPIO11 |
| SCK | GPIO12 |
| LED / Backlight | 3V3 in Wokwi |
| MISO | Not used |

**Note:** Backlight power and input voltage requirements must be checked against the specific physical TFT module before wiring it.

### Physical LEDs

| LED | GPIO | Resistor |
|---|---|---|
| LED 1 | GPIO4 | 220Ω |
| LED 2 | GPIO5 | 220Ω |
| LED 3 | GPIO6 | 220Ω |
| LED 4 | GPIO7 | 220Ω |
| LED 5 | GPIO15 | 220Ω |

Each LED is connected in series with its own current-limiting resistor. All LED cathodes connect to GND.

### User Controls

| Component | GPIO | Connection |
|---|---|---|
| Reaction button | GPIO16 | Button to GND |
| Mode switch | GPIO17 | Switch common to GPIO17, one side to GND |

Both inputs use the ESP32's internal pull-up resistors.

- **GPIO17 LOW:** Debug mode
- **GPIO17 HIGH:** Normal mode

## How to Play

1. Select **Debug** or **Normal** using the slide switch.
2. Press the reaction button to start a new round.
3. The five red starting lights illuminate sequentially.
4. After all five lights illuminate, the system waits for a random interval.
5. When the lights go out, press the button as quickly as possible.
6. The TFT displays your reaction time.
7. Press the button again to reset the game.

Pressing the button before lights-out results in a **FALSE START**.

If no valid reaction is detected within 10 seconds, the game displays **TIMEOUT**.

The game does not start or reset automatically.

## Operating Modes

### Debug Mode

Designed for testing, observation, and timing calibration.

- Shows a continuously updated timer after lights-out
- Displays the raw measured time (`RAW`)
- Displays a compensated time preview (`CAL`)
- Stops updating the displayed values after the reaction button is pressed
- Keeps the result visible until manual reset

### Normal Mode

Designed for regular gameplay.

- Hides the continuously updated timer
- Displays the starting light sequence
- Calculates and displays the compensated reaction time after the button press
- Waits for manual reset before another round

The selected mode is locked when a round begins. Switching modes during a round does not affect that round.

## Reaction Time Measurement

The ESP32-S3 uses `micros()` to obtain timestamps in microseconds.

When the five physical LEDs are switched off, the program records a starting timestamp. A GPIO interrupt records the reaction button's falling-edge timestamp.

The raw reaction time is calculated as:

\[
T_{\mathrm{raw}}=\frac{t_{\mathrm{press}}-t_{\mathrm{start}}}{1000}
\]

Here, the timestamps are measured in microseconds, while the result is expressed in milliseconds.

### Timing Compensation

Two optional compensation parameters are supported:

- **Time Multiplier:** Scales the measured interval
- **Fixed Compensation:** Subtracts a fixed time offset

The displayed compensated value is:

\[
T_{\mathrm{final}}=\max(0,T_{\mathrm{raw}}\times M-D)
\]

Where:

- \(T_{\mathrm{raw}}\) — raw reaction time in milliseconds
- \(M\) — time multiplier
- \(D\) — fixed compensation in milliseconds

Both parameters can be changed near the beginning of `sketch.ino`:

```cpp
const float TIME_MULTIPLIER = 1.0f;
const float FIXED_COMPENSATION_MS = -80.0f;
```

The current source setting adds 80 ms because the formula subtracts a negative offset. To disable compensation, use `M = 1.0` and `D = 0.0`. The current offset is experimental and has not been validated on physical hardware.

**Accuracy note:** Wokwi simulation speed may differ from real-world time. The multiplier is intended for experimentation with simulated timing, not as proof of measurement accuracy. Real hardware should normally start with a multiplier of `1.0` and an offset of `0.0 ms`, followed by independent calibration.

## Software Architecture

The program uses a state machine with five states:

| State | Description |
|---|---|
| `ST_IDLE` | Waiting for the start button |
| `ST_LIGHTING` | Illuminating the starting lights |
| `ST_WAIT` | Random delay before lights-out |
| `ST_REACTION` | Measuring reaction time |
| `ST_RESULT` | Displaying the result until reset |

The main loop handles state transitions, while the button interrupt records the button press timestamp.

This design separates game logic, timing calculations, input handling, and TFT drawing into individual functions.

## Running the Simulation

The project can be tested online without physical hardware.

1. Open the [Wokwi simulation](https://wokwi.com/projects/479746846531493889).
2. Start the simulation.
3. Select the desired mode using the slide switch.
4. Press the green reaction button to begin.
5. Watch the LEDs and TFT display.
6. Press the button after lights-out to measure your reaction time.

The simulation uses these Arduino libraries:

- [Adafruit GFX Library](https://github.com/adafruit/Adafruit-GFX-Library)
- [Adafruit ILI9341](https://github.com/adafruit/Adafruit_ILI9341)
- SPI (included with the Arduino ESP32 environment)

Library dependencies are listed in `libraries.txt`.

## Using Physical Hardware

For physical deployment:

1. Install Arduino IDE and the Espressif ESP32 board package.
2. Install Adafruit GFX and Adafruit ILI9341 through the Library Manager.
3. Select the appropriate ESP32-S3 board and its Flash/PSRAM configuration.
4. Assemble the circuit following the pin connection tables.
5. Verify the module's voltage and backlight requirements.
6. Upload `sketch.ino`.
7. Test LED outputs, button behavior, and display operation.
8. Calibrate the timing system before treating results as accurate real-world measurements.

Physical hardware performance has not yet been verified.

## Project Structure

```text
ESP32-F1-Reaction-Game/
├── sketch.ino
├── diagram.json
├── libraries.txt
├── wokwi-project.txt
├── README.md
├── LICENSE
├── images/
│   ├── circuit.png
│   └── gameplay.png
└── docs/
    ├── hardware-notes.md
    └── references.md
```

Project-specific wiring notes are in [`docs/hardware-notes.md`](docs/hardware-notes.md). Upstream hardware documentation is linked from [`docs/references.md`](docs/references.md); vendor PDF and software bundles are not redistributed in the current tree.

## Technical References

- [Espressif — ESP32-S3 Technical Documents](https://www.espressif.com/en/support/documents/technical-documents)
- [Adafruit GFX Library](https://github.com/adafruit/Adafruit-GFX-Library)
- [Adafruit ILI9341 Library](https://github.com/adafruit/Adafruit_ILI9341)
- [Wokwi Documentation](https://docs.wokwi.com/)

Third-party datasheets and documentation remain the property of their respective copyright holders.

## Future Improvements

- Testing and calibration on a physical ESP32-S3
- Improved TFT rendering performance
- High-score tracking and persistent storage
- Reaction time statistics over multiple rounds
- Improved button debounce and timing validation
- Optional sound effects and additional game modes
- A compact enclosure for the finished device

## License

The original project code is released under the [MIT License](LICENSE).

Third-party libraries, documentation, and hardware datasheets are subject to their respective licenses and copyright notices.
