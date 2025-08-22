#include "BlastGate.h"
#include <EEPROM.h>

static inline Print& beginl(Print &stream) {
    static constexpr const char name[] = "BG";
    return beginl<name>(stream);
}

BlastGate::BlastGate(uint8_t stepPin, uint8_t dirPin, uint8_t limitPin,
                     uint8_t inputPin, uint8_t buttonPin, uint8_t ledPin)
    : stepper(AccelStepper::DRIVER, stepPin, dirPin),
      led(ledPin),
      limitPin(limitPin), inputPin(inputPin), buttonPin(buttonPin) {}

void BlastGate::begin() {
    Serial << magenta << F("BlastGate v");
    Serial << magenta << BG_VERSION_MAJOR << "." << BG_VERSION_MINOR << "." << BG_VERSION_SUB;
    Serial << magenta << " (" <<  __TIMESTAMP__  << ")" << DI::endl;

    pinMode(inputPin, INPUT);
    analogReference(INTERNAL); // use internal 1.1V reference for analog input

    pinMode(limitPin, INPUT_PULLUP);
    pinMode(buttonPin, INPUT_PULLUP);

    delay(200);  // button debounce must have detect valid state before proceeding
    bool startCalib = !readSettings();
    if (getButtonState()) {
        Serial << beginl << yellow << "Button pressed, starting calibration..." << DI::endl;
        startCalib = true;
    }
    if (startCalib) {
        Serial << beginl << yellow << "Release button to start..." << DI::endl;
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
    int32_t speed = 0;
    EEPROM.get(EEPROM_ADDR_SPEED, speed);
    if (speed < SPEED_MIN || speed > SPEED_MAX) {
        Serial << beginl << blue << "EEPROM invalid, using default speed" << DI::endl;
        speed = SPEED_MIN;
    }
    setSpeed(speed);
    int32_t pos = 0;
    EEPROM.get(EEPROM_ADDR_OPENPOS, pos);
    if (pos <= 0 || pos > POSITION_MAX) {
        Serial << beginl << red << "EEPROM invalid, force openPos calibration" << DI::endl;
        return false;
    }
    openPos = pos;
    Serial << beginl << "Read EEPROM: speed=" << moveSpeed << ", openPos=" << openPos << DI::endl;
    return true;
}

void BlastGate::writeSettings() {
    EEPROM.put(EEPROM_ADDR_SPEED, moveSpeed);
    EEPROM.put(EEPROM_ADDR_OPENPOS, openPos);
    Serial << beginl << "Write EEPROM: speed=" << moveSpeed << ", openPos=" << openPos << DI::endl;
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
        case STATE_CALIBRATION_WAIT_RELEASE:
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
    Serial << beginl << yellow << "Starting homing..." << DI::endl;
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
            Serial << beginl << blue << "Moving to limit switch..." << DI::endl;
            break;
        case STATE_HOMING_MOVING:
            if (getLimitState()) {
                stepper.stop();
                stepper.setCurrentPosition(closedPos);
                state = STATE_HOMING_DONE;
            } else if (millis() - startTime > HOMING_TIMEOUT) {
                Serial << beginl << red << "Homing timeout" << DI::endl;
                state = STATE_ERROR;
                led.setState(LEDControl::LED_FLASH_FAST);
            }
            break;
        case STATE_HOMING_DONE:
            Serial << beginl << green << "Homing done" << DI::endl;
            led.setState(LEDControl::LED_OFF);
            state = STATE_MOVING;
            break;

        default:
            break;
    }
}

// ---------------- Calibration ----------------
void BlastGate::startCalibration() {
    Serial << beginl << yellow << "Starting calibration..." << DI::endl;
    state = STATE_CALIBRATION_START;
    startTime = millis();
}

void BlastGate::handleCalibration() {

    auto checkTimeout = [this]() {
        if (millis() - startTime > CALIBRATION_TIMEOUT) {
            Serial << beginl << red << "Calibration timeout" << DI::endl;
            stepper.stop();
            state = STATE_ERROR;
            led.setState(LEDControl::LED_FLASH_FAST);
        }
    };
    switch (state) {
        case STATE_CALIBRATION_START:
            stepper.setMaxSpeed(moveSpeed / 2);
            stepper.setAcceleration(moveAcceleration);
            stepper.moveTo(-POSITION_MAX);
            state = STATE_CALIBRATION_MOVE_TO_LIMIT;
            led.setState(LEDControl::LED_FLASH_SLOW);
            Serial << beginl << blue << "Moving to limit switch..." << DI::endl;
            break;
        case STATE_CALIBRATION_MOVE_TO_LIMIT:
            if (getLimitState()) {
                stepper.stop();
                stepper.setCurrentPosition(closedPos);
                Serial << beginl << blue << "Limit reached, press button to start" << DI::endl;
                state = STATE_CALIBRATION_WAIT_TO_START;
            } else {
                checkTimeout();
            }
            break;
        case STATE_CALIBRATION_WAIT_TO_START:
            if (buttonPressedShort()) {
                Serial << beginl << blue << "Press button when required open position is reached" << DI::endl;
                stepper.moveTo(POSITION_MAX);
                state = STATE_CALIBRATION_WAIT_RELEASE;
            } else {
                checkTimeout();
            }
            break;
        case STATE_CALIBRATION_WAIT_RELEASE:
            if (buttonPressedShort()) {
                stepper.stop();
                openPos = stepper.currentPosition();
                writeSettings();
                Serial << beginl << green << "Calibration done, openPos=" << openPos << DI::endl;
                state = STATE_CALIBRATION_DONE;
            } else {
                checkTimeout();
                if (millis() % 100 == 0)
                    Serial << clearLine << beginl << "openPos=" << stepper.currentPosition() << DI::endl;
            }
            break;
        case STATE_CALIBRATION_DONE:
            led.setState(LEDControl::LED_OFF);
            state = STATE_MOVING;
            break;
        default:
            break;
    }
}

// ---------------- Normal Operation ----------------
void BlastGate::handleOperation() {
    int8_t input = getInputState() ? 1 : 0;
    if (input != inputState) {
        inputState = input;
        if (input) {
            Serial << beginl << blue << "Input active, moving to open position..." << DI::endl;
            stepper.moveTo(openPos);
            led.setState(LEDControl::LED_ON);
        } else {
            Serial << beginl << blue << "Input inactive, moving to closed position..." << DI::endl;
            stepper.moveTo(closedPos);
            led.setState(LEDControl::LED_OFF);
        }
        state = STATE_MOVING;
        led.setState(LEDControl::LED_FLASH_SLOW);
    }

    if ((state == STATE_MOVING) && (stepper.distanceToGo() == 0)) {
        bool closed = (stepper.targetPosition() == closedPos);
        if (closed) {
            // sanity check: limit switch must be active in closed position
            if (!getLimitState()) {
                Serial << beginl << red << "Error: Limit switch not active in closed position" << DI::endl;
                state = STATE_ERROR;
                led.setState(LEDControl::LED_FLASH_FAST);
                return;
            }
        }
        Serial << beginl << yellow << "Gate is now " << (closed ? "closed" : "open") << DI::endl;
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
    Serial << beginl << "Speed set to " << moveSpeed << ", acceleration set to " << moveAcceleration << DI::endl;
    return clamped;
}

void BlastGate::enterSpeedAdjustment() {
    Serial << beginl << yellow << "Entering speed adjustment..." << DI::endl;
    Serial << beginl << "Press button to modify speed, long press to exit" << DI::endl;
    stepper.setMaxSpeed(moveSpeed);
    stepper.setAcceleration(moveAcceleration);
    stepper.moveTo(openPos);
    led.setState(LEDControl::LED_FLASH_SLOW);
    state = STATE_SPEED_ADJUSTMENT;
    startTime = millis();
}

void BlastGate::handleSpeedAdjustment() {
    // bounce between open/closed
    if (stepper.distanceToGo() == 0) {
        if (stepper.currentPosition() == openPos)
            stepper.moveTo(closedPos);
        else
            stepper.moveTo(openPos);
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
        Serial << beginl << green << "Exit speed adjustment" << DI::endl;
        state = STATE_IDLE;
        inputState = -1; // trigger move to input defined position
    }
}

