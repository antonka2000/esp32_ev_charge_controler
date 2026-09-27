#ifndef LED_H
#define LED_H
//#define ESP32C3

#include <Arduino.h>

// --------------------------------------------------
// Pin assignment
// --------------------------------------------------



#ifdef ESP32C3
    constexpr uint8_t LED_PV       = 0;
    constexpr uint8_t LED_MIN      = 1;
    constexpr uint8_t LED_MAX      = 3;
    constexpr uint8_t LED_PRICE    = 10;

    constexpr uint8_t BUTTON_MODE  = 9;
#else
    constexpr uint8_t LED_PV       = 14;  // esp32 DEV Board
    constexpr uint8_t LED_MIN      = 27;
    constexpr uint8_t LED_MAX      = 25;
    constexpr uint8_t LED_PRICE    = 33;

    constexpr uint8_t BUTTON_MODE  = 0;
    constexpr uint8_t BUTTON_MODE1  = 26;  
#endif
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

// void buttonISR();
// void buttonISR1();

//ButtonEvent buttonEvent();

void handlePauseBlinking(); 
// extern volatile ButtonEvent pendingEvent;


#endif