#pragma once
#include <Arduino.h>

/**
 * @class LEDControl
 * @brief Controls the state and behavior of an LED, including on/off, blinking, and one-shot flash patterns.
 *
 * This class provides an interface to manage an LED connected to a specified pin.
 * It supports regular LED states (on, off, slow blink, fast blink) and allows for
 * one-shot flash patterns for indication purposes. Timing for blinking and flashing
 * is handled internally.
 *
 * Usage:
 * - Instantiate with the desired pin number. Inverted logic can be set if the LED is active LOW.
 * - Use setState() to set the regular LED state.
 * - Call indicate() to trigger a one-shot flash pattern.
 * - Call update() periodically (e.g., in the main loop) to handle timing and state changes.
 */
class LEDControl {
public:
    enum LedState : uint8_t {
        LED_OFF,
        LED_ON,
        LED_FLASH_SLOW,
        LED_FLASH_FAST
    };

    LEDControl(uint8_t pin, bool inverted = false);

    void update();

    void setState(LedState newState);  // regular led state
    void indicate(uint8_t flashNum);   // one-shot flash pattern

private:
    uint8_t pin;
    bool inverted;  // if true, LED is active LOW (on when pin is LOW)
    bool ledState = false;

    LedState currentState = LED_OFF;
    LedState savedState = LED_OFF;

    // regular blink timing
    uint32_t lastToggle = 0;
    static constexpr uint16_t BLINK_SLOW = 500;
    static constexpr uint16_t BLINK_FAST = 150;

    // one-shot flash control
    bool flashActive = false;
    uint8_t flashCount = 0;
    uint8_t flashesDone = 0;
    uint32_t flashTimer = 0;
    static constexpr uint16_t FLASH_INTERVAL = 100;

    void set(bool state) {
        ledState = state;
        digitalWrite(pin, (state ^ inverted));
        digitalWrite(LED_BUILTIN, state);
    }

    void toggle() {
        ledState = !ledState;
        set(ledState);
    }
};
