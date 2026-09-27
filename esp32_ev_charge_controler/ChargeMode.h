#ifndef CHARGE_MODE_H
#define CHARGE_MODE_H

#include <Arduino.h>

// --------------------------------------------------
// Charging modes
// --------------------------------------------------

enum ChargeMode {
    PV_SURPLUS,
    PV_MIN,
    MIN_CHARGE,
    MAX_CHARGE,
    MIN_PRICE
};


// --------------------------------------------------
// Functions
// --------------------------------------------------

void chargeModeInit();

ChargeMode getChargeMode();

void setChargeMode(ChargeMode mode);

void nextChargeMode();

#endif