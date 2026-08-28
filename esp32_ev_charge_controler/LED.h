#ifndef LED_H
#define LED_H

#include <Arduino.h>

// --------------------------------------------------
// Pin assignment
// --------------------------------------------------

constexpr uint8_t LED_PV       = 0;
constexpr uint8_t LED_MIN      = 1;
constexpr uint8_t LED_MAX      = 3;
constexpr uint8_t LED_PRICE    = 10;

constexpr uint8_t BUTTON_MODE  = 20;


// --------------------------------------------------
// LED operating modes
// --------------------------------------------------

enum LedMode {
    LED_OFF,
    LED_SOLID,
    LED_SLOW_BLINK,
    LED_FAST_BLINK
};


// --------------------------------------------------
// Button events
// --------------------------------------------------

enum ButtonEvent {
    BUTTON_NONE,
    BUTTON_SHORT_PRESS,
    BUTTON_LONG_PRESS
};


// --------------------------------------------------
// Functions
// --------------------------------------------------

void ledInit();

void ledSetMode(uint8_t led, LedMode mode);

void ledUpdate();

ButtonEvent buttonEvent();

#endif