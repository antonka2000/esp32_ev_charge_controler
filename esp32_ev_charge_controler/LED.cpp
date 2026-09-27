#include "LED.h"

// --------------------------------------------------
// Internal LED state
// --------------------------------------------------

volatile bool buttonPressed = false;
volatile uint32_t lastInterruptTime = 0;
const uint32_t DEBOUNCE_MS = 40;     // Entprellzeit



struct LedState {
    uint8_t pin;
    LedMode mode;
    bool state;
    unsigned long lastChange;
};

extern bool isPaused; 

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
  const uint8_t pins[] = { LED_PV, LED_MIN, LED_MAX, LED_PRICE };

  for (uint8_t i = 0; i < 4; i++) {
    Serial.printf("pinMode %u\n", pins[i]);
    pinMode(pins[i], OUTPUT);
    digitalWrite(pins[i], LOW);

    leds[i].pin = pins[i];
    leds[i].state = false;
    leds[i].mode = LED_OFF;
    leds[i].lastChange = millis();
  }
}


// --------------------------------------------------
// Set LED mode
// --------------------------------------------------
void ledSetMode(uint8_t led, LedMode mode)
{
    if (led >= 4)
        return;

    // Nichts tun, wenn der Modus bereits identisch ist.
    // Dadurch wird beim erneuten Aufruf der Blink-Timer nicht
    // zurückgesetzt.
    if (leds[led].mode == mode)
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

// void ledSetMode(uint8_t led, LedMode mode)
// {
//     if (led >= 4)
//         return;

//     leds[led].mode = mode;

//     if (mode == LED_OFF) {
//         leds[led].state = false;
//         digitalWrite(leds[led].pin, LOW);
//     }
//     else if (mode == LED_SOLID) {
//         leds[led].state = true;
//         digitalWrite(leds[led].pin, HIGH);
//     }

//     leds[led].lastChange = millis();
// }


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


void handlePauseBlinking() {
  // Nur blinken, wenn der Pausenmodus auch wirklich aktiv ist
  if (!isPaused) return;

  // Zeitbasis: Alle 500ms den Zustand wechseln (1000ms Gesamtperiode)
  static unsigned long lastBlinkTime = 0;
  static bool ledState = false;

  if (millis() - lastBlinkTime >= 500) {
    lastBlinkTime = millis();
    ledState = !ledState; // Zustand umkehren

    // Alle LEDs sicherheitshalber aus, bis auf die blinkende
    digitalWrite(LED_MIN, LOW);
    digitalWrite(LED_MAX, LOW);
    digitalWrite(LED_PRICE, LOW);

    // Die PV-LED im Wechsel an- und ausschalten
    digitalWrite(LED_PV, ledState ? HIGH : LOW);
  }
}

