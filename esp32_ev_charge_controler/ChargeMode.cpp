#include "ChargeMode.h"
#include <Preferences.h>

Preferences preferences;

constexpr char PREF_NAMESPACE[] = "charger";
constexpr char PREF_MODE[] = "mode";

constexpr ChargeMode DEFAULT_MODE = MIN_CHARGE;

ChargeMode currentMode;


// --------------------------------------------------
// Initialization
// --------------------------------------------------

void chargeModeInit()
{
    preferences.begin(PREF_NAMESPACE, false);

    uint8_t storedMode =
        preferences.getUChar(PREF_MODE, DEFAULT_MODE);

    // Safety check: only accept valid modes
    if (storedMode <= MIN_PRICE) {
        currentMode = static_cast<ChargeMode>(storedMode);
    }
    else {
        currentMode = DEFAULT_MODE;
    }
}


// --------------------------------------------------
// Get current mode
// --------------------------------------------------

ChargeMode getChargeMode()
{
    return currentMode;
}


// --------------------------------------------------
// Set mode
// --------------------------------------------------

void setChargeMode(ChargeMode mode)
{
    if (mode > MIN_PRICE)
        return;

    currentMode = mode;

    preferences.putUChar(
        PREF_MODE,
        static_cast<uint8_t>(mode)
    );
}


// --------------------------------------------------
// Select next mode
// --------------------------------------------------

void nextChargeMode()
{
    uint8_t next =
        static_cast<uint8_t>(currentMode) + 1;

    if (next > MIN_PRICE) {
        next = PV_SURPLUS;
    }

    setChargeMode(static_cast<ChargeMode>(next));
}