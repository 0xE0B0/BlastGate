#pragma once
#include <Arduino.h>
#include <AccelStepper.h>
#include "LEDControl.h"
#include "DebugInterface.h"
#include "Debounce.h"
#include "ThresholdDetector.h"
#include "version.h"

class BlastGate {
public:
    BlastGate(uint8_t stepPin, uint8_t dirPin, uint8_t limitPin, uint8_t inputPin, uint8_t buttonPin, uint8_t ledPin);

    void begin();
    void update();

    void debounce() {
        thd.setValue(analogRead(inputPin));
        auto input = (((digitalRead(limitPin)  == HIGH) ? LIMIT_SWITCH : 0) |  // high active limit switch
                      ((digitalRead(buttonPin) == LOW)  ? CMD_BUTTON   : 0) |  // low active button
                      (thd.isOver() ? CMD_INPUT : 0));  // analog input via threshold detector
        deb.tick(input);
    }

private:

    // pin identifiers
    static constexpr uint8_t LIMIT_SWITCH = 1;
    static constexpr uint8_t CMD_INPUT = 2;
    static constexpr uint8_t CMD_BUTTON = 4;

    // command input monitors the power supply voltage
    // - a low (default) supply voltage commands the gate into closed position
    // - a high supply voltage commands the gate into open position
    // the input is read via an analog pin and compared to a threshold, values are in adc increments
    // Vin: 12V | 14V with voltage devider: R1 = 120k, R2 = 10k
    // -> 860 adc increments at 12V, 1002 adc increments at 14V (hysteresis 72 per volt)
    // Vin: 17V | 19V with voltage devider: R1 = 220k, R2 = 12k
    // -> 818 adc increments at 17V, 914 adc increments at 19V (hysteresis 48 per volt)

    static constexpr uint16_t CMD_INPUT_THRESHOLD = 860; 
    static constexpr uint16_t CMD_INPUT_HYSTERESIS = 50;

    // motion parameters
    static constexpr int32_t POSITION_MAX = 100000;
    static constexpr int32_t SPEED_MAX = 1000;
    static constexpr int32_t SPEED_MIN = 100;
    static constexpr int32_t SPEED_INC = 10;
    static constexpr uint32_t HOMING_TIMEOUT = 5000; // ms
    static constexpr uint32_t CALIBRATION_TIMEOUT = 30000; // ms

    // hardware
    AccelStepper stepper;
    LEDControl led;
    uint8_t limitPin;
    uint8_t inputPin;
    uint8_t buttonPin;
    Debounce<uint8_t, 10> deb = Debounce<uint8_t>(0, CMD_BUTTON);
    ThresholdDetector<uint16_t> thd = ThresholdDetector<uint16_t>(CMD_INPUT_THRESHOLD, 
                                                                 CMD_INPUT_HYSTERESIS);

    int32_t posOpen = 0;
    int32_t posClosed = 0;  // default closed position, e.g. zero position
    int32_t moveSpeed = SPEED_MIN;
    int32_t moveAcceleration = SPEED_MIN*2;
    bool speedDirUp = true;
    int8_t inputState = -1; // -1 = unknown, 0 = closed, 1 = open
    int16_t inputThreshold = CMD_INPUT_THRESHOLD;
    int16_t inputHysteresis = CMD_INPUT_HYSTERESIS;
    uint16_t adcOpen = 0;
    uint16_t adcClosed = 0;

    // states
    enum GateState : uint8_t {
        STATE_IDLE,
        STATE_MOVING,
        STATE_HOMING_START,
        STATE_HOMING_MOVING,
        STATE_HOMING_DONE,
        STATE_CALIBRATION_START,
        STATE_CALIBRATION_MOVE_TO_LIMIT,
        STATE_CALIBRATION_WAIT_TO_START,
        STATE_CALIBRATION_MOVE_TO_OPEN,
        STATE_CALIBRATION_DONE,
        STATE_SPEED_ADJUSTMENT,
        STATE_ERROR
    };
    GateState state = STATE_IDLE;

    // timing
    uint32_t startTime = 0;

    // eeprom layout
    static constexpr uint16_t EEPROM_ADDR_INPUT_TH = 0;
    static constexpr uint16_t EEPROM_ADDR_INPUT_HYST = 4;
    static constexpr uint16_t EEPROM_ADDR_SPEED   = 8;
    static constexpr uint16_t EEPROM_ADDR_POS_OPEN = 12;

    bool readSettings();
    void writeSettings();

    void handleOperation();

    void startHoming();
    void handleHoming();

    void startCalibration();
    void handleCalibration();

    void enterSpeedAdjustment();
    void handleSpeedAdjustment();

    bool setSpeed(int32_t speed);

    inline bool incSpeed(int32_t inc) {
        return setSpeed(moveSpeed + inc);
    }

    inline bool getLimitState() { return deb.getKeyState(LIMIT_SWITCH); }
    inline bool getInputState() { return deb.getKeyState(CMD_INPUT); }
    inline bool getButtonState() { return deb.getKeyState(CMD_BUTTON); }
    inline bool buttonPressedShort() { return deb.getKeyShort(CMD_BUTTON); }
    inline bool buttonPressedLong() { return deb.getKeyLong(CMD_BUTTON); }

    inline bool isNear(uint16_t a, uint16_t b, uint16_t threshold) {
    return (abs((int32_t)a - (int32_t)b) <= threshold);
    }
};
