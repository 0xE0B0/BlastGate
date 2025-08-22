#include "LEDControl.h"

LEDControl::LEDControl(uint8_t pin, bool inverted) : pin(pin), inverted(inverted) {
    pinMode(pin, OUTPUT);
    pinMode(LED_BUILTIN, OUTPUT); // also control built-in LED for feedback
    set(LOW);
}

void LEDControl::setState(LedState newState) {
    // if flashes are running, just remember new state for later
    if (flashActive) {
        savedState = newState;
        return;
    }
    currentState = newState;
    lastToggle = millis();
    if (newState == LED_ON) {
        set(true);
    } else if (newState == LED_OFF) {
        set(false);
    }
}

void LEDControl::indicate(uint8_t flashNum) {
    flashActive = true;
    flashCount = flashNum;
    flashesDone = 0;
    flashTimer = millis();
    set(false);
    // save current mode so we can restore it afterwards
    savedState = currentState;
}

void LEDControl::update() {
    uint32_t now = millis();

    if (flashActive) {
        // handle one-shot flash pattern
        if (now - flashTimer >= FLASH_INTERVAL) {
            flashTimer = now;
            toggle();
            if (!ledState) { // only count when OFF cycle ends
                flashesDone++;
                if (flashesDone >= flashCount) {
                    flashActive = false;
                    setState(savedState); // restore regular state
                }
            }
        }
    } else {
        // handle regular LED states
        auto blinkInterval = BLINK_FAST;
        switch (currentState) {
            case LED_OFF:
            case LED_ON:
                break;
            case LED_FLASH_SLOW:
                blinkInterval = BLINK_SLOW;
                // fall through
            case LED_FLASH_FAST:
                if (now - lastToggle >= blinkInterval) {
                    lastToggle = now;
                    toggle();
                }
                break;
        }
    }
}
