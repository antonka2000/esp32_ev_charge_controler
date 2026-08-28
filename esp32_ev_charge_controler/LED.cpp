#include "LED.h"

// --------------------------------------------------
// Internal LED state
// --------------------------------------------------

struct LedState {
    uint8_t pin;
    LedMode mode;
    bool state;
    unsigned long lastChange;
};

LedState leds[] = {
    { LED_PV,    LED_OFF, false, 0 },
    { LED_MIN,   LED_OFF, false, 0 },
    { LED_MAX,   LED_OFF, false, 0 },
    { LED_PRICE, LED_OFF, false, 0 }
};


// --------------------------------------------------
// Timing
// --------------------------------------------------

constexpr unsigned long SLOW_BLINK_TIME = 1000;
constexpr unsigned long FAST_BLINK_TIME = 200;

constexpr unsigned long BUTTON_DEBOUNCE_TIME = 50;
constexpr unsigned long BUTTON_LONG_PRESS_TIME = 1500;


// --------------------------------------------------
// Button state
// --------------------------------------------------

bool buttonLastReading = HIGH;
bool buttonStableState = HIGH;

unsigned long buttonLastChange = 0;
unsigned long buttonPressedAt = 0;

bool longPressReported = false;


// --------------------------------------------------
// Initialization
// --------------------------------------------------

void ledInit()
{
    for (auto &led : leds) {
        pinMode(led.pin, OUTPUT);
        digitalWrite(led.pin, LOW);

        led.state = false;
        led.mode = LED_OFF;
        led.lastChange = millis();
    }

    pinMode(BUTTON_MODE, INPUT_PULLUP);

    buttonLastReading = HIGH;
    buttonStableState = HIGH;
    buttonLastChange = millis();

    buttonPressedAt = 0;
    longPressReported = false;
}


// --------------------------------------------------
// Set LED mode
// --------------------------------------------------

void ledSetMode(uint8_t led, LedMode mode)
{
    if (led >= 4)
        return;

    leds[led].mode = mode;

    if (mode == LED_OFF) {
        leds[led].state = false;
        digitalWrite(leds[led].pin, LOW);
    }
    else if (mode == LED_SOLID) {
        leds[led].state = true;
        digitalWrite(leds[led].pin, HIGH);
    }

    leds[led].lastChange = millis();
}


// --------------------------------------------------
// Update blinking LEDs
// --------------------------------------------------

void ledUpdate()
{
    unsigned long now = millis();

    for (auto &led : leds) {

        if (led.mode != LED_SLOW_BLINK &&
            led.mode != LED_FAST_BLINK) {
            continue;
        }

        unsigned long interval =
            (led.mode == LED_SLOW_BLINK)
            ? SLOW_BLINK_TIME
            : FAST_BLINK_TIME;

        if (now - led.lastChange >= interval) {

            led.lastChange = now;
            led.state = !led.state;

            digitalWrite(led.pin, led.state ? HIGH : LOW);
        }
    }
}


// --------------------------------------------------
// Button event handling
// --------------------------------------------------

ButtonEvent buttonEvent()
{
    bool reading = digitalRead(BUTTON_MODE);
    unsigned long now = millis();

    // ----------------------------------------------
    // Physical state changed
    // ----------------------------------------------

    if (reading != buttonLastReading) {
        buttonLastChange = now;
        buttonLastReading = reading;
    }


    // ----------------------------------------------
    // Wait until state is stable
    // ----------------------------------------------

    if ((now - buttonLastChange) < BUTTON_DEBOUNCE_TIME) {
        return BUTTON_NONE;
    }


    // ----------------------------------------------
    // Stable state changed
    // ----------------------------------------------

    if (reading != buttonStableState) {

        buttonStableState = reading;

        // ------------------------------------------
        // Button pressed
        // ------------------------------------------

        if (buttonStableState == LOW) {
            buttonPressedAt = now;
            longPressReported = false;
        }

        // ------------------------------------------
        // Button released
        // ------------------------------------------

        else {

            if (!longPressReported) {
                return BUTTON_SHORT_PRESS;
            }
        }
    }


    // ----------------------------------------------
    // Long press
    // ----------------------------------------------

    if (buttonStableState == LOW &&
        !longPressReported &&
        (now - buttonPressedAt >= BUTTON_LONG_PRESS_TIME)) {

        longPressReported = true;

        return BUTTON_LONG_PRESS;
    }


    return BUTTON_NONE;
}
