#include "BlastGate.h"
#include <EEPROM.h>

static inline Print& beginl(Print &stream) {
    static constexpr const char name[] = "BG";
    return beginl<name>(stream);
}

BlastGate::BlastGate(uint8_t stepPin, uint8_t dirPin, uint8_t enaPin, uint8_t limitPin,
                     uint8_t inputPin, uint8_t buttonPin, uint8_t ledPin)
    : stepper(AccelStepper::DRIVER, stepPin, dirPin, enaPin),
      led(ledPin),
      limitPin(limitPin), inputPin(inputPin), buttonPin(buttonPin) {}

void BlastGate::begin() {
    Serial << magenta << F("BlastGate v");
    Serial << magenta << BG_VERSION_MAJOR << F(".") << BG_VERSION_MINOR << F(".") << BG_VERSION_SUB;
    Serial << magenta << F(" (") <<  __TIMESTAMP__  << F(")") << DI::endl;

    pinMode(inputPin, INPUT);
    analogReference(INTERNAL); // use internal 1.1V reference for analog input

    pinMode(limitPin, INPUT_PULLUP);
    pinMode(buttonPin, INPUT_PULLUP);

    delay(200);  // button debounce must have detect valid state before proceeding
    bool startCalib = !readSettings();
    if (getButtonState()) {
        Serial << beginl << yellow << F("Button pressed, starting calibration...") << DI::endl;
        startCalib = true;
    }
    if (startCalib) {
        Serial << beginl << yellow << F("Release button to start...") << DI::endl;
        while (getButtonState());  // wait for button release
        buttonPressedShort();  // clear button state
        buttonPressedLong();
        startCalibration();
    } else {
        startHoming();
    }
}

// ---------------- EEPROM ----------------
bool BlastGate::readSettings() {
    int32_t inputTh = 0;
    EEPROM.get(EEPROM_ADDR_INPUT_TH, inputTh);
    if (inputTh < -1020 || inputTh > 1020) {
    Serial << beginl << blue << F("EEPROM invalid, using default input threshold") << DI::endl;
        inputThreshold = CMD_INPUT_THRESHOLD;
        inputInverted = false;
    } else {
        // inversion is coded in the sign
        inputThreshold = abs(inputTh);
        inputInverted = (inputTh < 0);
    }
    int32_t inputHyst = 0;
    EEPROM.get(EEPROM_ADDR_INPUT_HYST, inputHyst);
    if (inputHyst < 0 || inputHyst > 300) {
        Serial << beginl << blue << F("EEPROM invalid, using default input hysteresis") << DI::endl;
        inputHysteresis = CMD_INPUT_HYSTERESIS;
    } else {
        inputHysteresis = inputHyst;
    }
    thd.setLimit(inputThreshold);
    thd.setHysteresis(inputHysteresis);
    int32_t speed = 0;
    EEPROM.get(EEPROM_ADDR_SPEED, speed);
    if (speed < SPEED_MIN || speed > SPEED_MAX) {
        Serial << beginl << blue << F("EEPROM invalid, using default speed") << DI::endl;
        speed = SPEED_MIN;
    }
    setSpeed(speed);
    int32_t pos = 0;
    EEPROM.get(EEPROM_ADDR_POS_OPEN, pos);
    if (pos <= 0 || pos > POSITION_MAX) {
        Serial << beginl << red << F("EEPROM invalid, force calibration") << DI::endl;
        return false;
    }
    posOpen = pos;
    Serial << beginl << F("Read EEPROM: inputTh=") << inputThreshold
           << F(", inputHyst=") << inputHysteresis
           << F(", speed=") << moveSpeed
           << F(", posOpen=") << posOpen
           << DI::endl;
    return true;
}

void BlastGate::writeSettings() {
    EEPROM.put(EEPROM_ADDR_INPUT_TH, (inputInverted ? -inputThreshold : inputThreshold));
    EEPROM.put(EEPROM_ADDR_INPUT_HYST, inputHysteresis);
    EEPROM.put(EEPROM_ADDR_SPEED, moveSpeed);
    EEPROM.put(EEPROM_ADDR_POS_OPEN, posOpen);
    Serial << beginl << F("Write EEPROM: inputTh=") << inputThreshold
              << F(", inputHyst=") << inputHysteresis
              << F(", speed=") << moveSpeed
              << F(", posOpen=") << posOpen
              << DI::endl;
}

// ---------------- State Machine ----------------
void BlastGate::update() {
    stepper.run();
    led.update();

    switch (state) {
        case STATE_IDLE:
        case STATE_MOVING:
            handleOperation();
            break;
        case STATE_SPEED_ADJUSTMENT:
            handleSpeedAdjustment();
            break;
        case STATE_HOMING_START:
        case STATE_HOMING_MOVING:
        case STATE_HOMING_DONE:
            handleHoming();
            break;
        case STATE_CALIBRATION_START:
        case STATE_CALIBRATION_MOVE_TO_LIMIT:
        case STATE_CALIBRATION_WAIT_TO_START:
        case STATE_CALIBRATION_MOVE_TO_OPEN:
        case STATE_CALIBRATION_DONE:
            handleCalibration();
            break;
        case STATE_ERROR:
            // locked until restart
            break;
    }
}

// ---------------- Homing ----------------
void BlastGate::startHoming() {
    Serial << beginl << yellow << F("Starting homing...") << DI::endl;
    state = STATE_HOMING_START;
    startTime = millis();
}

void BlastGate::handleHoming() {
    switch (state) {
        case STATE_HOMING_START:
            stepper.setMaxSpeed(moveSpeed / 2);  // reduced speed for homing
            stepper.setAcceleration(moveAcceleration);
            stepper.moveTo(-POSITION_MAX); // move towards limit switch
            state = STATE_HOMING_MOVING;
            led.setState(LEDControl::LED_FLASH_SLOW);
            Serial << beginl << blue << F("Moving to limit switch...") << DI::endl;
            break;
        case STATE_HOMING_MOVING:
            if (getLimitState()) {
                stepper.stop();
                stepper.setCurrentPosition(posClosed);
                state = STATE_HOMING_DONE;
            } else if (millis() - startTime > HOMING_TIMEOUT) {
                Serial << beginl << red << F("Homing timeout") << DI::endl;
                state = STATE_ERROR;
                led.setState(LEDControl::LED_FLASH_FAST);
            }
            break;
        case STATE_HOMING_DONE:
            Serial << beginl << green << F("Homing done") << DI::endl;
            led.setState(LEDControl::LED_OFF);
            state = STATE_MOVING;
            break;

        default:
            break;
    }
}

// ---------------- Calibration ----------------
void BlastGate::startCalibration() {
    Serial << beginl << yellow << F("Starting calibration...") << DI::endl;
    state = STATE_CALIBRATION_START;
    startTime = millis();
}

void BlastGate::handleCalibration() {

    auto checkTimeout = [this]() {
        if (millis() - startTime > CALIBRATION_TIMEOUT) {
            Serial << beginl << red << F("Calibration timeout") << DI::endl;
            stepper.stop();
            state = STATE_ERROR;
            led.setState(LEDControl::LED_FLASH_FAST);
        }
    };

    auto readInput = [this](bool open) {
        if (millis() % 100 == 0) {
            cli();  // thd is updated with adc value in interrupt context
            auto val = thd.getValue();
            sei();
            if (open) adcOpen = val;
            else adcClosed = val;
            Serial << clearLine << beginl << F("inputVal(") << (open ? F("open") : F("closed")) << F(")=") << val << DI::endl;
        }
    };

    switch (state) {
        case STATE_CALIBRATION_START:
            stepper.setMaxSpeed(moveSpeed / 2);
            stepper.setAcceleration(moveAcceleration);
            stepper.moveTo(-POSITION_MAX);
            state = STATE_CALIBRATION_MOVE_TO_LIMIT;
            led.setState(LEDControl::LED_FLASH_SLOW);
                Serial << beginl << blue << F("Moving to limit switch...") << DI::endl;
            break;
        case STATE_CALIBRATION_MOVE_TO_LIMIT:
            if (getLimitState()) {
                stepper.stop();
                stepper.setCurrentPosition(posClosed);
                Serial << beginl << blue << F("Limit reached, apply 'closed-position' supply voltage and press button to start") << DI::endl << DI::endl;
                state = STATE_CALIBRATION_WAIT_TO_START;
            } else {
                checkTimeout();
            }
            break;
        case STATE_CALIBRATION_WAIT_TO_START:
            if (buttonPressedShort()) {
                Serial << beginl << blue << F("Press button when required open position is reached") << DI::endl << DI::endl;
                stepper.moveTo(POSITION_MAX);
                state = STATE_CALIBRATION_MOVE_TO_OPEN;
            } else {
                checkTimeout();
                readInput(false); 
            }
            break;
        case STATE_CALIBRATION_MOVE_TO_OPEN:
            if (buttonPressedShort()) {
                stepper.stop();
                posOpen = stepper.currentPosition();
                Serial << beginl << blue << F("Apply 'open-position' supply voltage and press button to finish calibration") << DI::endl << DI::endl;
                state = STATE_CALIBRATION_DONE;
            } else {
                checkTimeout();
                if (millis() % 100 == 0)
                    Serial << clearLine << beginl << F("posOpen=") << stepper.currentPosition() << DI::endl;
            }
            break;
        case STATE_CALIBRATION_DONE:
            if (buttonPressedShort()) {
                if (isNear(adcClosed, adcOpen, 50)) {
                    Serial << beginl << cyan << F("ADC values for open and closed positions are too close,") << DI::endl;
                    Serial << beginl << cyan << F("omit threshold and hysteresis update") << DI::endl;
                } else {
                    inputThreshold = (adcClosed + adcOpen) / 2;
                    inputInverted = (adcClosed > adcOpen);
                    inputHysteresis = abs(adcClosed - adcOpen) / 2;
                    Serial << beginl << cyan << F("update input threshold: ") << inputThreshold << F(", inversion: ") <<
                        (inputInverted ? F("enabled") : F("disabled")) << F(", hysteresis: ") << inputHysteresis << DI::endl;
                }
                writeSettings();
                Serial << beginl << green << F("Calibration done") << DI::endl;
                led.setState(LEDControl::LED_OFF);
                state = STATE_MOVING;
            } else {
                checkTimeout();
                readInput(true);
            }
            break;
        default:
            break;
    }
}

// ---------------- Normal Operation ----------------
void BlastGate::handleOperation() {
    int8_t input = (getInputState() ^ inputInverted) ? 1 : 0;
    if (input != inputState) {
        inputState = input;
        if (input) {
            Serial << beginl << blue << F("Input active, moving to open position...") << DI::endl;
            stepper.moveTo(posOpen);
            led.setState(LEDControl::LED_ON);
        } else {
            Serial << beginl << blue << F("Input inactive, moving to closed position...") << DI::endl;
            stepper.moveTo(posClosed);
            led.setState(LEDControl::LED_OFF);
        }
        state = STATE_MOVING;
        led.setState(LEDControl::LED_FLASH_SLOW);
    }

    // short button press -> toggle position
    if (buttonPressedShort()) {
        if (stepper.targetPosition() == posClosed) {
            Serial << beginl << blue << F("Command button pressed, moving to open position...") << DI::endl;
            stepper.moveTo(posOpen);
            led.setState(LEDControl::LED_ON);
        } else {
            Serial << beginl << blue << F("Command button pressed, moving to closed position...") << DI::endl;
            stepper.moveTo(posClosed);
            led.setState(LEDControl::LED_OFF);
        }
        state = STATE_MOVING;
        led.setState(LEDControl::LED_FLASH_SLOW);
    }

    if ((state == STATE_MOVING) && (stepper.distanceToGo() == 0)) {
        bool closed = (stepper.targetPosition() == posClosed);
        if (closed) {
            // sanity check: limit switch must be active in closed position
            if (!getLimitState()) {
                Serial << beginl << red << F("Error: Limit switch not active in closed position") << DI::endl;
                state = STATE_ERROR;
                led.setState(LEDControl::LED_FLASH_FAST);
                return;
            }
        }
        Serial << beginl << yellow << F("Gate is now ") << (closed ? F("closed") : F("open")) << DI::endl;
        led.setState((closed) ? LEDControl::LED_OFF : LEDControl::LED_ON);
        state = STATE_IDLE;
    }

    // long press -> enter speed adjust
    if (buttonPressedLong())
        enterSpeedAdjustment();
}

// ---------------- Speed Adjustment ----------------

// return true if speed was clamped, false otherwise
bool BlastGate::setSpeed(int32_t speed) {
    bool clamped = false;
    if (speed < SPEED_MIN) {
        speed = SPEED_MIN;
        clamped = true;
    } else if (speed > SPEED_MAX) {
        speed = SPEED_MAX;
        clamped = true;
    }
    moveSpeed = speed;
    moveAcceleration = speed * 2;
    stepper.setMaxSpeed(moveSpeed);
    stepper.setAcceleration(moveAcceleration);
    Serial << beginl << F("Speed set to ") << moveSpeed << F(", acceleration set to ") << moveAcceleration << DI::endl;
    return clamped;
}

void BlastGate::enterSpeedAdjustment() {
    Serial << beginl << yellow << F("Entering speed adjustment...") << DI::endl;
    Serial << beginl << F("Press button to modify speed, long press to exit") << DI::endl;
    stepper.setMaxSpeed(moveSpeed);
    stepper.setAcceleration(moveAcceleration);
    stepper.moveTo(posOpen);
    led.setState(LEDControl::LED_FLASH_SLOW);
    state = STATE_SPEED_ADJUSTMENT;
    startTime = millis();
}

void BlastGate::handleSpeedAdjustment() {
    // bounce between open/closed
    if (stepper.distanceToGo() == 0) {
        if (stepper.currentPosition() == posOpen)
            stepper.moveTo(posClosed);
        else
            stepper.moveTo(posOpen);
    }

    // short press -> change speed
    if (buttonPressedShort()) {
        if (speedDirUp) {
            if (incSpeed(SPEED_INC)) {
                speedDirUp = false;
                led.indicate(3); // turnaround
            } else {
                led.indicate(1);
            }
        } else {
            if (incSpeed(-SPEED_INC)) {
                speedDirUp = true;
                led.indicate(3); // turnaround
            } else {
                led.indicate(1);
            }
        }
        startTime = millis();  // reset timeout
    }

    // long press or timeout -> exit
    bool timeout = (millis() - startTime > CALIBRATION_TIMEOUT);
    if (buttonPressedLong() || timeout) {
        writeSettings();
        led.indicate(5);
    Serial << beginl << green << F("Exit speed adjustment") << DI::endl;
        state = STATE_IDLE;
        inputState = -1; // trigger move to input defined position
    }
}

