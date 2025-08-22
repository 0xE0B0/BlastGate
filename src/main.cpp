#include <Arduino.h>
#include "BlastGate.h"

// pin mapping
constexpr uint8_t STEP_PIN          = 2;
constexpr uint8_t DIR_PIN           = 3;
constexpr uint8_t LIMIT_SWITCH_PIN  = 6;
constexpr uint8_t CMD_BUTTON_PIN    = 7;
constexpr uint8_t LED_PIN           = LED_BUILTIN;
constexpr uint8_t CMD_INPUT_PIN     = A0;

static_assert(CMD_INPUT_PIN >= A0 && CMD_INPUT_PIN <= A5, "CMD_INPUT_PIN must be an analog pin (A0-A5) on Uno/Nano.");

BlastGate gate(STEP_PIN, DIR_PIN, LIMIT_SWITCH_PIN, CMD_INPUT_PIN, CMD_BUTTON_PIN, LED_PIN);

// timer1 interrupt handler for 10ms intervals
#if defined(TIMER1_COMPA_vect)
ISR(TIMER1_COMPA_vect) {
#else
ISR(TIM1_COMPA_vect) {
#endif
    gate.debounce();
}

// initialize timer1 to generate CTC compare-A interrupts every 10 ms
// assumes 16 MHz CPU and prescaler 64: ticks = 16e6 / 64 = 250000 Hz
// required ticks per 10ms = 250000 * 0.01 = 2500 -> OCR1A = 2499
void initTimer1_10ms() {
    cli();
    TCCR1A = 0;
    TCCR1B = 0;
    TCNT1 = 0;
    // CTC mode: WGM12 = 1 (CTC with OCR1A as TOP)
    // compare value for 10 ms (for 16 MHz CPU, prescaler 64)
#if defined(WGM12)
    TCCR1B |= (1 << WGM12);
#endif
#if defined(OCR1A)
    OCR1A = 2499;
#endif
#if defined(CS11) && defined(CS10)
    TCCR1B |= (1 << CS11) | (1 << CS10);
#endif
#if defined(TIMSK1)
    TIMSK1 |= (1 << OCIE1A);
#elif defined(TIMSK)
    TIMSK |= (1 << OCIE1A);
#endif
    sei();
}

void setup() {
    initTimer1_10ms();
    Serial.begin(debugBaudRate);
    gate.begin();
}

void loop() {
    gate.update();
}
