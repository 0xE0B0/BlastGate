## Cust Collector Blast Gate

BlastGate is a Arduino/PlatformIO project that controls a stepper-driven gate intended for an airflow shutter of a dust collection system. A homing routine moves the gate to the closed position at start up using a physical limit switch. The open position and move speed can be teached in with a calibration and speed adjustment routine.

## Key Features
- Stepper control (step + dir) with configurable speed and acceleration (AccelStepper).
- Homing and calibration using a mechanical limit switch.
- Analog input command with hysteresis (threshold detector) to open/close the gate.
- Push button with short and long press detection for open-position calibration and speed adjustment.
- LED status feedback and persistent settings stored in EEPROM.

## Intended Hardware
- Target MCU: Arduino Uno (or any other AVR Arduino board). The project is configured for AVR/Arduino environments and `platformio.ini` can be set to `board = uno` or another AVR board.
- A stepper motor and a step/dir driver (direct step/dir wiring).
- Mechanical limit switch.
- Analog command input used to measure the system power supply voltage (see "Voltage Divider" below).
- A push button connected to a digital pin (configured active-LOW).
- A status LED on a digital pin.

### Pins
Pins are defined in `main.cpp`:
- STEP_PIN         = 2: step signal for stepper driver
- DIR_PIN          = 3: direction signal for stepper driver
- ENA_PIN          = 4: enable signal for stepper driver
- LIMIT_SWITCH_PIN = 6: limit switch (HIGH active)
- CMD_BUTTON_PIN   = 7: push button (LOW active)
- LED_PIN          = 8: status LED
- CMD_INPUT_PIN    = A0: analog command input (ADC)

### Voltage Divider & ADC reference
- The analog input measures the supply voltage (e.g.: 12 V => gate closed, 14 V => gate open).
  The MCU ADC is configured to use the internal 1.1 V reference, so the divider must scale the measured voltage to be within 0..1.1 V.
- Formula: Vout = Vin * (R2 / (R1 + R2)). ADC reading = Vout / Vref * 1023 (here Vref = 1.1 V).
- Example divider: R1 = 120 kΩ, R2 = 10 kΩ:
    - 12 V -> Vout = 0,9230 V -> ADC = 859
    - 13 V -> Vout = 1.0000 V -> ADC = 930
    - 14 V -> Vout = 1,0769 V -> ADC = 1002
- Example divider: R1 = 220 kΩ, R2 = 12 kΩ:
    - 17 V -> Vout = 0,8793 V -> ADC = 818
    - 18 V -> Vout = 0,9310 V -> ADC = 866
    - 19 V -> Vout = 0,9827 V -> ADC = 914

- The default tThreshold and hysteresis may be configured to `CMD_INPUT_THRESHOLD` and `CMD_INPUT_HYSTERESIS`.

## Behavior
1. Power on: device performs homing or restores persisted settings. If the limit switch is not activated after a timeout
  the gate enters an error state indicated by a fast flashing LED. The error state can only be exited by a power cycle.
2. When the analog input crosses the configured threshold (with hysteresis), the gate opens or closes automatically. Low (default) commands gate closed, high commands gate open. The limit switch defines the mechanical end position and is used for homing and calibration. The status LED shows the current state (gate closed: LED off, gate opened: LED on). While moving to a new position the status LED flashes slowly.
As a sanity check, the limit switch is expected to be activated in closed position. If it is not, an error is indicated by fast flashing of the status LED. The error state can only be cleared by a power cycle.

## Button Control

### Calibration

If the button is pressed at start-up (or if no valid EEPROM settings can be loaded) the calibration routine is started. The intended flow is:

- The gate moves toward the closed position until the limit switch is activated. At this point apply the "closed-position" supply voltage level (the voltage you want the system to treat as "closed") and press the button once - this records the closed-position ADC level.
- A single button press then starts movement toward the open position. When the required open position is reached, press the button again to stop and store the open position to EEPROM.
- After stopping at the open position, apply the "open-position" supply voltage level (the voltage you want the system to treat as "open") and press the button a third time; this records the open-position ADC level and triggers the threshold/hysteresis calculation.

The firmware compares the recorded ADC levels for closed and open positions. If they are sufficiently separated the input threshold and hysteresis are updated and saved to EEPROM. If the two ADC readings are too close (within the safety margin) the threshold/hysteresis update is omitted to avoid ambiguous input detection.

If no button press is detected for the configured calibration timeout the gate enters an error state indicated by a fast flashing LED. The error state can only be cleared by a power cycle.

### Position Commanding
Button short press toggles the open and closed position and therefore allows manual operation. The input level must change in order to return to the input level defined position.
 
### Speed Adjustment
Button long press enters the speed adjustment mode. In this mode the gate moves constantly between the closed and the open position with the current speed. Short presses modifies the speed. A long press exits the adjustment mode and stores the current speed to EEPROM to be restored at startup.

## Build & Upload (VS Code + PlatformIO on Windows PowerShell)
1. Install VS Code and the PlatformIO IDE extension.
2. Open this repository folder (`BlastGate.code-workspace` or the project root) in VS Code.
3. Wait for PlatformIO to initialize and install toolchains.
4. Build the project from the workspace terminal:

```powershell
cd 'D:\PlayGround\BlastGate\BlastGate'
platformio run
```

5. Upload to a connected board (or use the PlatformIO GUI upload button):

```powershell
platformio run --target upload
```

6. (Optional) Open the serial monitor:

```powershell
platformio device monitor
```
