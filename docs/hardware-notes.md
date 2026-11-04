# Hardware notes

Status: functional Wokwi simulation; physical assembly, voltage checks and timing calibration are not yet recorded. These notes describe the checked-in project, not completed physical measurements.

## Connections in the project

| Component | ESP32-S3 connection |
|---|---|
| TFT CS / D-C / RESET | GPIO10 / GPIO9 / GPIO8 |
| TFT MOSI / SCK | GPIO11 / GPIO12 |
| TFT MISO | Not used |
| TFT VCC / backlight | 3V3 in the simulation; verify the actual module supply and backlight requirements before wiring |
| Five LEDs | GPIO4, GPIO5, GPIO6, GPIO7, GPIO15; each with its own 220 ohm series resistor |
| Reaction button | GPIO16 to GND, internal pull-up |
| Mode switch | GPIO17 common, one side to GND, internal pull-up |
| Ground | Common GND |

GPIO17 LOW selects Debug; HIGH selects Normal. The mode is locked at round start.

## Physical-board checks still to do

- Record the exact board and display markings and revision. N16R8 is the planned vendor board configuration, not evidence that its pinout equals every DevKitC board.
- Compare the module's regulator and J1 solder-jumper arrangement with the matching schematic before choosing 3.3 V or 5 V supply. Do not apply a vendor package filename as a wiring instruction.
- Verify logic levels, backlight current, resistor values and shared ground.
- Record actual flash/PSRAM board settings and a successful hardware upload.
- Measure input response and display latency independently before making timing-accuracy claims.

## Timing settings in the current source

`sketch.ino` currently sets `TIME_MULTIPLIER = 1.0f` and `FIXED_COMPENSATION_MS = -80.0f`. Because the formula subtracts this offset, the current setting adds 80 ms to the scaled raw result before clamping at zero. It is an experimental setting, not a measured hardware correction. No firmware settings were changed during documentation cleanup.

## Documents

Use [hardware references](references.md) for upstream documents. Sensor modules such as BME280, INA219, INMP441 and MPU6050 are not part of the current F1 circuit.
