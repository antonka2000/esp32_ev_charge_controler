#include <WiFi.h>
#include <ModbusTCP.h>

#include "ConfigWeb.h"
#include "LED.h"
#include "ChargeMode.h"
#include "inverter.h"
#include "charger.h"

// ============================================================
// MODBUS
// ============================================================

ModbusTCP mbWR1;
ModbusTCP mbWR2;
ModbusTCP mbKEBA;

Inverter inverter(mbWR1, mbWR2);
Charger charger(mbKEBA);

IPAddress sungrowIP;
IPAddress sungrowIP2;
IPAddress kebaIP;


uint16_t currentSetpoint_mA = 6000;
unsigned long lastCurrentChange = 0;
uint32_t holdsec = 0;
uint8_t pollDevice = 0;
unsigned long lastPoll = 0;


constexpr uint32_t DEBOUNCE_MS   = 40;
constexpr uint32_t LONG_PRESS_MS = 800;   // ab hier = Long Press

volatile bool     buttonDown      = false;
volatile uint32_t pressStartTime  = 0;
volatile bool     longPressFired  = false;
volatile ButtonEvent pendingEvent = BUTTON_NONE;

static uint32_t lastLedState = 999;
static uint32_t lastLedCable = 999;
static bool lastLedStopped = false;
static ChargeMode lastLedMode = (ChargeMode)255;


volatile bool buttonFlag = false;
bool chargerStopped = false;

// mit interrupt
volatile bool buttonIRQ = false;
void IRAM_ATTR buttonISR() {
  buttonIRQ = true;   // nur Flag – nichts anderes
}

void IRAM_ATTR buttonISR1() {
  buttonIRQ = true;   // nur Flag – nichts anderes
}


ButtonEvent buttonEvent()
{
    static bool wasDown = false;
    static bool candidateState = false;

    static uint32_t pressStart = 0;
    static uint32_t debounceStart = 0;
    static uint32_t lastShortPress = 0;

    static bool longFired = false;

    uint32_t now = millis();

    // --------------------------------------------------------
    // Aktuellen Zustand beider Taster ermitteln
    // --------------------------------------------------------

    bool isPressed =
        (digitalRead(BUTTON_MODE)  == LOW) ||
        (digitalRead(BUTTON_MODE1) == LOW);

    // --------------------------------------------------------
    // Neue IRQ-Flanke
    // Nur merken, noch kein Event erzeugen
    // --------------------------------------------------------

    if (buttonIRQ)
    {
        buttonIRQ = false;

        if (isPressed != candidateState)
        {
            candidateState = isPressed;
            debounceStart = now;
        }
    }

    // --------------------------------------------------------
    // Zustand muss z.B. 120 ms stabil sein
    // --------------------------------------------------------

    if (candidateState != wasDown)
    {
        if (now - debounceStart < DEBOUNCE_MS)
            return BUTTON_NONE;

        // ----------------------------------------------------
        // Zustand jetzt wirklich übernehmen
        // ----------------------------------------------------

        wasDown = candidateState;

        // ----------------------------------------------------
        // Gedrückt
        // ----------------------------------------------------

        if (wasDown)
        {
            pressStart = now;
            longFired = false;

            return BUTTON_NONE;
        }

        // ----------------------------------------------------
        // Losgelassen
        // ----------------------------------------------------

        else
        {
            if (!longFired)
            {
                // zusätzlicher Schutz gegen Doppeltrigger
                if (now - lastShortPress >= 300)
                {
                    lastShortPress = now;
                    return BUTTON_SHORT_PRESS;
                }
            }
        }
    }

    // --------------------------------------------------------
    // Longpress unabhängig von IRQ prüfen
    // --------------------------------------------------------

    if (wasDown &&
        !longFired &&
        now - pressStart >= LONG_PRESS_MS)
    {
        longFired = true;

        return BUTTON_LONG_PRESS;
    }

    return BUTTON_NONE;
}


// ============================================================
// MODE-LED
// ============================================================

void updateModeLED()
{
    // --------------------------------------------------------
    // LED-Blinkart aus Fahrzeugstatus bestimmen
    //
    // kein Fahrzeug      -> dauerhaft an
    // angeschlossen      -> langsam blinken
    // lädt               -> schnell blinken
    // --------------------------------------------------------

    LedMode ledMode = LED_SOLID;

    if (chargerStopped)
    {
        ledMode = LED_SLOW_BLINK;
    }
    else if (charger.getCable() >= 5)
    {
        if (charger.getState() == 3)
            ledMode = LED_FAST_BLINK;
        else
            ledMode = LED_SLOW_BLINK;
    }


    // --------------------------------------------------------
    // Alle LEDs zuerst aus
    // --------------------------------------------------------

    ledSetMode(0, LED_OFF);
    ledSetMode(1, LED_OFF);
    ledSetMode(2, LED_OFF);
    ledSetMode(3, LED_OFF);


    // --------------------------------------------------------
    // Aktuellen ChargeMode anzeigen
    // --------------------------------------------------------

    switch (getChargeMode())
    {
        // ====================================================
        // PV
        //
        // LED 0 = PV / Fahrzeugstatus
        // LED 1 = 8 kW
        // LED 2 = 4 kW
        // LED 3 = 2 kW
        // ====================================================

        case PV_SURPLUS:
        {
            // PV-LED blinkt je nach Fahrzeugstatus
            ledSetMode(0, ledMode);

            // ------------------------------------------------
            // Aktuelle KEBA-Ladeleistung
            // 2-kW-Schritte
            //
            // 0..14 kW darstellbar
            // ------------------------------------------------

            int32_t powerW =
                (int32_t)(charger.getPower_mW() / 1000);

            // Auf 2-kW-Schritte abrunden
            // uint8_t units =
            //     powerW / 2000;

            uint8_t units;
            if (powerW >= 10900)
            {
                // 11 kW wird wie 12 kW dargestellt
                units = 6;
            }
            else
            {
                // Auf 2-kW-Schritte abrunden
                units = powerW / 2000;
            }
            // 3 Bit -> maximal 7 = 14 kW
            if (units > 7)
                units = 7;

            // 8 kW
            if (units & 0b100)
                ledSetMode(1, LED_SOLID);

            // 4 kW
            if (units & 0b010)
                ledSetMode(2, LED_SOLID);

            // 2 kW
            if (units & 0b001)
                ledSetMode(3, LED_SOLID);

            break;
        }


        // ====================================================
        // MIN + PV
        // ====================================================

        case PV_MIN:

            ledSetMode(0, ledMode);
            ledSetMode(1, ledMode);

            break;


        // ====================================================
        // MIN
        // ====================================================

        case MIN_CHARGE:

            ledSetMode(1, ledMode);

            break;


        // ====================================================
        // MAX
        // ====================================================

        case MAX_CHARGE:

            ledSetMode(2, ledMode);

            break;


        // ====================================================
        // MIN PRICE
        // ====================================================

        case MIN_PRICE:

            ledSetMode(3, ledMode);

            break;
    }
}



void applyChargeMode()
{
    switch (getChargeMode())
    {
        case PV_SURPLUS:

            // PV-Regelung übernimmt
            break;


        case PV_MIN:

            // Start mit Mindeststrom,
            // danach PV-Regelung
            if (charger.writeCurrent(6000))
            {
                currentSetpoint_mA = 6000;
            }

            break;


        case MIN_CHARGE:

            if (charger.writeCurrent(7000))
            {
                currentSetpoint_mA = 7000;
            }

            break;


        case MAX_CHARGE:

            if (charger.writeCurrent(16000))
            {
                currentSetpoint_mA = 16000;
            }

            break;


        case MIN_PRICE:

            if (charger.writeCurrent(11111))
            {
                currentSetpoint_mA = 11111;
            }

            break;
    }
}


// ============================================================
// Vehicle-Charging -- LED-Blink
// ============================================================

void updateVehicleLED()
{
    LedMode ledMode = LED_SOLID;

    // Fahrzeug angeschlossen?
    if (charger.getCable() >= 5)
    {
        // KEBA State 3 = charging
        if (charger.getState() == 3)
            ledMode = LED_FAST_BLINK;
        else
            ledMode = LED_SLOW_BLINK;
    }

    // Alle Mode-LEDs zunächst aus
    ledSetMode(0, LED_OFF);
    ledSetMode(1, LED_OFF);
    ledSetMode(2, LED_OFF);
    ledSetMode(3, LED_OFF);

    // Aktuellen ChargeMode setzen
    switch (getChargeMode())
    {
        case PV_SURPLUS:
            ledSetMode(0, ledMode);
            break;

        case PV_MIN:
            ledSetMode(0, ledMode);
            ledSetMode(1, ledMode);
            break;

        case MIN_CHARGE:
            ledSetMode(1, ledMode);
            break;

        case MAX_CHARGE:
            ledSetMode(2, ledMode);
            break;

        case MIN_PRICE:
            ledSetMode(3, ledMode);
            break;
    }
}
// ============================================================
// LEISTUNG -> STROM
// ============================================================

uint16_t powerToCurrent_mA(int32_t powerW)
{
    if (powerW <= 0)
        return 0;

    // Aktuelle Regelung: 3 x 230 V = 690 W/A
    float amps = (float)powerW / 690.0f;
    uint16_t mA = (uint16_t)(amps * 1000.0f);

    // 100-mA-Schritte
    mA = (mA / 100) * 100;

    if (mA > 0 && mA < cfg.minCurrent_mA)
        mA = cfg.minCurrent_mA;

    if (mA > cfg.maxCurrent_mA)
        mA = cfg.maxCurrent_mA;

    // KEBA darf hier nicht unter den absoluten Mindeststrom fallen
    if (mA > 0 && mA < 6000)
        mA = 6000;

    if (mA > 63000)
        mA = 63000;

    return mA;
}

// ============================================================
// LADESTROM SETZEN
// ============================================================

void writeKebaCurrent(uint16_t mA)
{
    Serial.printf("Schreibe Sollstrom = %u mA\n", mA);
    if (mA > 0 && mA < 6000)
        mA = 6000;

    if (mA > 16000)
        mA = 16000;

    Serial.printf("Schreibe Sollstrom = %u mA\n", mA);

    if (!charger.writeCurrent(mA)) {
        Serial.println("KEBA writeCurrent fehlgeschlagen");
        return;
    }

    currentSetpoint_mA = mA;
    lastCurrentChange = millis();
}

const char* modeName(ChargeMode mode)
{
    switch (mode)
    {
        case PV_SURPLUS:   return "PV";
        case MIN_CHARGE:   return "MIN";
        case PV_MIN:       return "MIN+PV";
        case MAX_CHARGE:   return "MAX";
        case MIN_PRICE:    return "MIN PRICE";
        default:           return "UNKNOWN";
    }
}

// ============================================================
// REGELUNG
// ============================================================

int32_t regulateCharging()
{
    // -1 = keine Änderung
    //  0 = Laden ausschalten
    // >0 = neuer Ladestrom in mA

    if (!cfg.regulationEnable)
        return -1;

    if (charger.getCable() < 5)
        return -1;

    ChargeMode mode = getChargeMode();

    // --------------------------------------------------------
    // Aktuelle Ladeleistung
    // --------------------------------------------------------

    int32_t currentChargeW =
        (int32_t)(charger.getPower_mW() / 1000);

    // --------------------------------------------------------
    // Verfügbare Leistung
    // --------------------------------------------------------

    int32_t availableW =
     //   inverter.getPVTotal() -
        inverter.getExport() -
        inverter.getBatt() + 
        currentChargeW;

   Serial.printf(
        "available: Export=%d W | PV=%d W | Batterie=%d W | "
        "aktuell %d W -> availableW %d W\n",
        inverter.getExport(),
        inverter.getPVTotal(),
        inverter.getBatt(),
        currentChargeW,
        availableW
               );

    int32_t targetChargeW =
        availableW - cfg.targetExportW;

    if (targetChargeW < 0)
        targetChargeW = 0;

    // --------------------------------------------------------
    // Rohwert berechnen
    //
    // Hier darf NOCH NICHT auf 6000 mA begrenzt werden.
    // --------------------------------------------------------

    uint16_t newCurrent;

    if (targetChargeW <= 0) {
        newCurrent = 0;
    }
    else {
        float amps =
            (float)targetChargeW / 690.0f;

        newCurrent =
            (uint16_t)(amps * 1000.0f);

        // 100-mA-Schritte
        newCurrent =
            (newCurrent / 100) * 100;
    }

    // ========================================================
    // PV
    // ========================================================

    if (mode == PV_SURPLUS)
    {
        // Unter 6 A kann die KEBA nicht sinnvoll laden.
        // Deshalb nach Hold-Time komplett ausschalten.

        if (newCurrent < cfg.minCurrent_mA)
        {
            if (millis() - lastCurrentChange >= holdsec)
            {
                Serial.printf(
                    "PV: Sollstrom %u mA < %u mA -> Laden AUS\n",
                    newCurrent,
                    cfg.minCurrent_mA
                );

                return 0;
            }

            return -1;
        }
    }

    // ========================================================
    // MIN + PV
    // ========================================================

    if (mode == PV_MIN)
    {
        // Immer mindestens 6 A
        if (newCurrent < cfg.minCurrent_mA)
            newCurrent = cfg.minCurrent_mA;
    }

    // ========================================================
    // Maximalstrom
    // ========================================================

    if (newCurrent >= cfg.maxCurrent_mA)
        newCurrent = cfg.maxCurrent_mA;

    // --------------------------------------------------------
    // Bei PV / MIN+PV nur Änderungen oberhalb der Hysterese
    // --------------------------------------------------------

    int32_t diffW =
        targetChargeW - currentChargeW;

    if (abs(diffW) < cfg.hysteresisW)
        return -1;

    // --------------------------------------------------------
    // 0,5-A-Schritt
    // --------------------------------------------------------

    if (abs((int)newCurrent -
            (int)currentSetpoint_mA) < 500)
        return -1;

    Serial.printf(
        "Regelung: Export=%d W | PV=%d W | Batterie=%d W | "
        "aktuell %d W (%u mA) -> Ziel %d W (%u mA)\n",
        inverter.getExport(),
        inverter.getPVTotal(),
        inverter.getBatt(),
        currentChargeW,
        currentSetpoint_mA,
        targetChargeW,
        newCurrent
    );

    return newCurrent;
}
// ============================================================
// AUSGABE
// ============================================================

void printSungrow()
{
    Serial.println();
    Serial.println("========== SUNGROW ==========");
    Serial.printf("WR1 PV        : %d W\n", inverter.getPV1());
    Serial.printf("WR2 PV        : %d W\n", inverter.getPV2());
    Serial.printf("PV Gesamt     : %d W\n", inverter.getPVTotal());
    Serial.printf("Einspeisung   : %d W\n", inverter.getExport());
    Serial.println("==============================");
}

void printKeba()
{
    Serial.println();
    Serial.println("========== KEBA ==============");
    Serial.printf("State : %u", charger.getState());

    if (charger.getState() == 2)
        Serial.print(" (ready)");
    else if (charger.getState() == 3)
        Serial.print(" (charging)");

    Serial.println();

    Serial.printf(
        "Cable : %u -> %s\n",
        charger.getCable(),
        (charger.getCable() >= 5) ? "verbunden" : "nicht verbunden"
    );

    Serial.printf(
        "Power : %d W\n",
        (int)(charger.getPower_mW() / 1000)
    );

    Serial.printf(
        "Sollstrom : %u mA\n",
        currentSetpoint_mA
    );
    Serial.printf(
        "Chargemode : %s\n",
        modeName(getChargeMode())
    );
    Serial.println("==============================");
}

// ============================================================
// SETUP
// ============================================================

void setup()
{
    Serial.begin(115200);

    delay(200);   // kurz warten, bis Serial bereit ist

    Serial.println(BUTTON_MODE);

    // --------------------------------------------------------
    // LED
    // --------------------------------------------------------

    ledInit();

    // --------------------------------------------------------
    // Taster
    // --------------------------------------------------------

    Serial.print("vor button mode  ");

    pinMode(BUTTON_MODE, INPUT_PULLUP);
    pinMode(BUTTON_MODE1, INPUT_PULLUP);

    Serial.println("attachinterrupt ");

    //   Interrupt erst nach grundlegendem Init anhängen
    attachInterrupt(
        digitalPinToInterrupt(BUTTON_MODE),
        buttonISR,
        CHANGE
    );

    attachInterrupt(
        digitalPinToInterrupt(BUTTON_MODE1),
        buttonISR1,
        CHANGE
    );
    --------------------------------------------------------
    // ChargeMode
    // --------------------------------------------------------

    currentSetpoint_mA = cfg.minCurrent_mA;

    updateModeLED();
    applyChargeMode();

    // --------------------------------------------------------
    // Start
    // --------------------------------------------------------

    Serial.println(
        "\nESP32 - Sungrow + Keba + WebConfig"
    );

    Serial.println("vor startWiFi");

    // WLAN starten
    startWiFi();

    // --------------------------------------------------------
    // IP-Adressen aus Config
    // --------------------------------------------------------

    sungrowIP.fromString(cfg.sungrowIP);
    sungrowIP2.fromString(cfg.sungrowIP2);
    kebaIP.fromString(cfg.kebaIP);

    holdsec =
        cfg.holdTime * 1000UL;

    // --------------------------------------------------------
    // Inverter
    // Zwei getrennte ModbusTCP-Verbindungen
    // --------------------------------------------------------

       inverter.begin(
        cfg.sungrowIP,
        cfg.sungrowPort,
        cfg.sungrowIP2,
        cfg.sungrowPort2
    );
    // --------------------------------------------------------
    // KEBA
    // Eigene ModbusTCP-Verbindung
    // --------------------------------------------------------

    charger.begin(
        cfg.kebaIP,
        cfg.kebaPort,
        cfg.kebaUnitId
    );

    // --------------------------------------------------------
    // Modbus Clients starten
    // --------------------------------------------------------

    mbWR1.client();
    mbWR2.client();
    mbKEBA.client();

    // --------------------------------------------------------
    // Webinterface
    // --------------------------------------------------------

    startConfigWeb();

    Serial.println("Bereit.");
}
// ============================================================
// LOOP
// ============================================================

void loop()
{
    // 1. Modbus-Hintergrundprozesse am Laufen halten
    mbWR1.task();
    mbWR2.task();
    mbKEBA.task();

    // 2. WiFi-Verbindungsschutz (wichtig, damit Modbus bei Verbindungsverlust nicht hängt)
    if (WiFi.status() != WL_CONNECTED) {
        inverter.resetPending();
        charger.resetPending();
        delay(10);
        return;
    }

    // Webserver & LEDs updaten
    handleConfigWeb();
    ledUpdate();

    // 3. Physischen Button abfragen

    ButtonEvent event = buttonEvent();

    if (event == BUTTON_SHORT_PRESS)
    {
        Serial.println("++++  Button pressed  ++++");

        if (chargerStopped)
        {
            // STOP aufgehoben → laden mit aktuellem Mode
            Serial.println("++++  Restart charging  ++++");

            chargerStopped = false;
            applyChargeMode();
            updateModeLED();
        }
        else
        {
            // Normalbetrieb → Mode wechseln
            nextChargeMode();
            updateModeLED();
            applyChargeMode();
        }
    }

    if (event == BUTTON_LONG_PRESS)
    {
        Serial.println("++++  Long pressed  ++++");

        if (chargerStopped)
        {
            // STOP aufgehoben
            Serial.println("++++  Restart charging  ++++");

            chargerStopped = false;
            applyChargeMode();
            updateModeLED();
        }
        else
        {
            // STOP
            Serial.println("++++  Stop charging  ++++");

            if (charger.setEnabled(false))
            {
                chargerStopped = true;
                updateModeLED();
            }
        }
    }

    // 4. Striktes & abwechselndes Polling (alle 5 Sekunden abwechselnd WR und Charger)
    static unsigned long lastPollTime = 0;
    static uint8_t pollDevice = 0; // 0 = Inverter, 1 = Charger

    // Nur pollen, wenn KEINE Abfrage mehr offen ("pending") ist und 5 Sekunden um sind
    if (!inverter.isPending() && !charger.isPending() && (millis() - lastPollTime >= 5000)) { 
        lastPollTime = millis();

        if (pollDevice == 0) {
            Serial.println("Starte Modbus-Abfrage: INVERTER...");
            inverter.request();
            pollDevice = 1; // Nächstes Mal ist der Charger dran
        } else {
            delay(200);
            Serial.println("Starte Modbus-Abfrage: CHARGER (Keba)...");
            charger.request();
            pollDevice = 0; // Nächstes Mal ist der Inverter dran
        }
    }

    // 5. Wenn NEUE INVERTER-DATEN da sind
    if (inverter.hasNewData()) {
        printSungrow(); // Ruft deine Ausgabe auf
    }

    // 6. Wenn NEUE CHARGER-DATEN da sind (hier läuft deine PV-Überschussregelung)
    if (charger.hasNewData()) {
        printKeba();

        ChargeMode mode = getChargeMode();



    if (mode != lastLedMode ||
        charger.getState() != lastLedState ||
        charger.getCable() != lastLedCable ||
        chargerStopped != lastLedStopped)
    {
        lastLedMode = mode;
        lastLedState = charger.getState();
        lastLedCable = charger.getCable();
        lastLedStopped = chargerStopped;

        updateModeLED();
    }
        // Regelung nur triggern, wenn im PV-Modus und keine Abfragen blockiert sind
        if ((mode == PV_SURPLUS || mode == PV_MIN) && !inverter.isPending() && !charger.isPending()) {
            
            int32_t newCurrent = regulateCharging();
            if (newCurrent == 0) {
                if (charger.setEnabled(false)) {
                    currentSetpoint_mA = 0;
                    lastCurrentChange = millis();
                }
            } else if (newCurrent > 0) {
                if (charger.writeCurrent((uint16_t)newCurrent)) {
                    currentSetpoint_mA = (uint16_t)newCurrent;
                    lastCurrentChange = millis();
                }
            }
        }

        int32_t availableW = inverter.getExport() -
        inverter.getBatt();

        Serial.printf(
        "available: PV=%d W | EXPORT=%d W | Batterie=%d W | "
        " -> availableW %d W\n",
        inverter.getPVTotal(),
        inverter.getExport(),
        inverter.getBatt(),
        availableW );
    }

    delay(1); // Kleines Yield für den ESP32 Core
}
