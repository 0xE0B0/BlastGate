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
- stepPin   - step signal for stepper driver
- dirPin    - direction signal for stepper driver
- limitPin  - limit switch (HIGH active)
- inputPin  - analog command input (ADC)
- buttonPin - push button (LOW active)
- ledPin    - status LED

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

- Threshold and hysteresis must be configured  to `CMD_INPUT_THRESHOLD` and `CMD_INPUT_HYSTERESIS`.

## Behavior
1. Power on: device performs homing or restores persisted settings. If the limit switch is not activated after a timeout
  the gate enters an error state indicated by a fast flashing LED. The error state can only be exited by a power cycle.
2. When the analog input crosses the configured threshold (with hysteresis), the gate opens or closes automatically. Low (default) commands gate closed, high commands gate open. The limit switch defines the mechanical end position and is used for homing and calibration. The status LED shows the current state (gate closed: LED off, gate opened: LED on). While moving to a new position the status LED flashes slowly.

## Button Control

### Calibration
  If the button is pressed at start-up (or if no valid EEPROM settings can be loaded) the open-position
  calibration routine is started: The gate moves towards the closed position until the limit switch is activated.
  A single button press starts movement towards the open position. As soon as the required open position has been reached,
  the button must be pressed again to stop and store the open position to EEPROM. If no button press is detected for a specific duration, a timout triggers and the gate enters an error state indicated by a fast flashing LED. The error state can only be exited by a power cycle.

### Speed Adjustment
  Button long press enters the speed adjustment mode. In this mode the gate moves constantly between the closed and the open
  position with the current speed. Short presses modifies the speed. A long press exits the adjustment mode and stores the
  current speed to EEPROM to be used at startup.

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
