#include "charger.h"

Charger* Charger::instance = nullptr;

// ============================================================
// Konstruktor
// ============================================================

Charger::Charger(ModbusTCP& modbus)
    : mbKEBA(modbus)
{
    instance = this;
}

// ============================================================
// Initialisierung
// ============================================================

void Charger::begin(
    IPAddress ip,
    uint16_t port,
    uint8_t unitId
)
{
    kebaIP = ip;
    kebaPort = port;
    kebaUnitId = unitId;

    Serial.printf(
        "KEBA: %s:%u Unit-ID %u\n",
        kebaIP.toString().c_str(),
        kebaPort,
        kebaUnitId
    );
}

// ============================================================
// Status
// ============================================================

bool Charger::isPending() const
{
    return pending || writePending;
}

void Charger::resetPending()
{
    pending = false;
    writePending = false;
    writeStep = WRITE_IDLE;
}

bool Charger::hasNewData()
{
    if (newData)
    {
        newData = false;
        return true;
    }

    return false;
}

// ============================================================
// Getter
// ============================================================

uint32_t Charger::getState() const
{
    return state;
}

uint32_t Charger::getCable() const
{
    return cable;
}

uint32_t Charger::getPower_mW() const
{
    return power_mW;
}

// ============================================================
// KEBA Callback - Lesen
// ============================================================
bool Charger::readCallbackStatic(Modbus::ResultCode event, uint16_t transactionId, void* data)
{
    if (instance) {
        return instance->readCallback(event, transactionId, data);
    }
    return false;
}

bool Charger::readCallback(
    Modbus::ResultCode event,
    uint16_t transactionId,
    void* data
)
{
    if (!instance)
        return false;

    instance->pending = false;

 
 if (event != Modbus::EX_SUCCESS)
{
    Serial.printf(
        "KEBA Modbus Fehler: 0x%02X\n",
        (uint8_t)event
    );

    instance->pending = false;

  if (event == Modbus::EX_TIMEOUT)
{
    Serial.println("KEBA Timeout -> Verbindung und Transaktionen zurücksetzen");

    instance->mbKEBA.disconnect(instance->kebaIP);
    instance->mbKEBA.dropTransactions();

    instance->reconnectDelayUntil = millis() + 1500;
}

    instance->step =
        (instance->step + 1) % 3;

    return false;

}
    uint32_t value =
        ((uint32_t)instance->regs[0] << 16) |
         instance->regs[1];

    switch (instance->step)
    {
        case 0:
            instance->state = value;
            break;

        case 1:
            instance->cable = value;
            break;

        case 2:
            instance->power_mW = value;
            instance->newData = true;
            break;
    }

    instance->step =
        (instance->step + 1) % 3;

    return true;
}

// ============================================================
// KEBA Callback - Schreiben
// ============================================================

bool Charger::writeCallback(
    Modbus::ResultCode event,
    uint16_t transactionId,
    void* data
)
{
    if (!instance)
        return false;

    // --------------------------------------------------------
    // Fehler beim Schreiben
    // --------------------------------------------------------

    if (event != Modbus::EX_SUCCESS)
    {
        Serial.printf(
            "KEBA Schreibfehler: 0x%02X\n",
            (uint8_t)event
        );

        instance->writePending = false;
        instance->writeStep = WRITE_IDLE;
        return false;
    }

    // --------------------------------------------------------
    // 5014 = 1 erfolgreich -> danach 5004 schreiben
    // --------------------------------------------------------

    if (instance->writeStep == WRITE_ENABLE)
    {
        Serial.printf(
            "KEBA 5014 = 1 OK -> 5004 = %u mA\n",
            instance->requestedCurrent_mA
        );

        instance->writeStep = WRITE_CURRENT;

        if (!instance->mbKEBA.writeHreg(
                instance->kebaIP,
                5004,
                instance->requestedCurrent_mA,
                writeCallback,
                instance->kebaUnitId))
        {
            Serial.println("KEBA write 5004 fehlgeschlagen");
            instance->writePending = false;
            instance->writeStep = WRITE_IDLE;
            return false;
        }

        return true;
    }

    // --------------------------------------------------------
    // 5004 erfolgreich
    // --------------------------------------------------------

    if (instance->writeStep == WRITE_CURRENT)
    {
        Serial.printf(
            "KEBA 5004 = %u mA OK\n",
            instance->requestedCurrent_mA
        );

        instance->writePending = false;
        instance->writeStep = WRITE_IDLE;
        return true;
    }

    // --------------------------------------------------------
    // 5014 = 0 erstes Schreiben erfolgreich
    // -> zweites Mal schreiben
    // --------------------------------------------------------

    if (instance->writeStep == WRITE_DISABLE_FIRST)
    {
        Serial.println("KEBA 5014 = 0 OK -> zweites Mal 5014 = 0");

        instance->writeStep = WRITE_DISABLE_SECOND;

        if (!instance->mbKEBA.writeHreg(
                instance->kebaIP,
                5014,
                0,
                writeCallback,
                instance->kebaUnitId))
        {
            Serial.println("KEBA write 5014 (2) fehlgeschlagen");
            instance->writePending = false;
            instance->writeStep = WRITE_IDLE;
            return false;
        }

        return true;
    }

    // --------------------------------------------------------
    // 5014 = 0 zweites Schreiben erfolgreich
    // --------------------------------------------------------

    if (instance->writeStep == WRITE_DISABLE_SECOND)
    {
        Serial.println("KEBA 5014 = 0 zweites Mal OK");

        instance->writePending = false;
        instance->writeStep = WRITE_IDLE;
        return true;
    }

    // --------------------------------------------------------
    // Unbekannter Zustand
    // --------------------------------------------------------

    instance->writePending = false;
    instance->writeStep = WRITE_IDLE;
    return true;
}

//===========================================================
void Charger::request()
{
    if (pending || writePending)
        return;

    if (millis() < reconnectDelayUntil)  // längere Wartezeit nach einem Poll Fehler
       return;
    if (!mbKEBA.isConnected(kebaIP))
    {
        Serial.printf(
            "Connect KEBA %s:%u\n",
            kebaIP.toString().c_str(),
            kebaPort
        );

        if (!mbKEBA.connect(kebaIP, kebaPort))
        {
            Serial.println("KEBA connect fehlgeschlagen");
            return;
        }

        delay(300);
    }

    uint16_t reg = 0;

    switch (step)
    {
        case 0: reg = 1000; break;
        case 1: reg = 1004; break;
        case 2: reg = 1020; break;
    }

    Serial.printf(
        "KEBA starte readHreg Reg %u (step %u)\n",
        reg,
        step
    );

    pending = true;

    if (!mbKEBA.readHreg(
            kebaIP,
            reg,
            regs,
            2,
            readCallbackStatic,
            kebaUnitId))
    {
        pending = false;

        Serial.println(
            "KEBA readHreg konnte nicht gestartet werden"
        );

        mbKEBA.disconnect(kebaIP);
        mbKEBA.dropTransactions();
    }
}

bool Charger::setEnabled(bool enabled)
{
    if (pending || writePending)
        return false;

    if (!mbKEBA.isConnected(kebaIP))
    {
        Serial.printf(
            "Connect KEBA %s:%u\n",
            kebaIP.toString().c_str(),
            kebaPort
        );

        if (!mbKEBA.connect(kebaIP, kebaPort))
        {
            Serial.println("KEBA connect fehlgeschlagen");
            return false;
        }
    }

    writePending = true;

    if (enabled)
    {
        writeStep = WRITE_ENABLE;

        if (!mbKEBA.writeHreg(
                kebaIP,
                5014,
                1,
                writeCallback,
                kebaUnitId))
        {
            writePending = false;
            writeStep = WRITE_IDLE;

            Serial.println("KEBA write 5014 = 1 fehlgeschlagen");
            return false;
        }

        Serial.println("KEBA 5014 = 1 gestartet");
        return true;
    }

    writeStep = WRITE_DISABLE_FIRST;

    if (!mbKEBA.writeHreg(
            kebaIP,
            5014,
            0,
            writeCallback,
            kebaUnitId))
    {
        writePending = false;
        writeStep = WRITE_IDLE;

        Serial.println("KEBA write 5014 = 0 fehlgeschlagen");
        return false;
    }

    Serial.println("KEBA 5014 = 0 gestartet");
    return true;
}

// ============================================================
// Ladestrom schreiben
//
// Register 5004
// 0 = stoppen
// Minimum 6000 mA
// Maximum 63000 mA
//
// Bei Strom > 0 wird zuerst 5014 = 1 geschrieben.
// Erst nach dessen Bestätigung folgt 5004.
// ============================================================

bool Charger::writeCurrent(uint16_t current_mA)
{
    if (pending || writePending)
        return false;

    if (!mbKEBA.isConnected(kebaIP))
    {
        Serial.printf(
            "Connect KEBA %s:%u\n",
            kebaIP.toString().c_str(),
            kebaPort
        );

        if (!mbKEBA.connect(kebaIP, kebaPort))
        {
            Serial.println(
                "KEBA connect fehlgeschlagen"
            );

            return false;
        }
    }

    // --------------------------------------------------------
    // Strom begrenzen
    // --------------------------------------------------------

    if (current_mA > 0 && current_mA < 6000)
        current_mA = 6000;

    if (current_mA > 16000)
        current_mA = 16000;

    // 0 = direkt ausschalten
    if (current_mA == 0)
        return setEnabled(false);

    requestedCurrent_mA = current_mA;
    writeStep = WRITE_ENABLE;
    writePending = true;

    if (!mbKEBA.writeHreg(
            kebaIP,
            5014,
            1,
            writeCallback,
            kebaUnitId))
    {
        writePending = false;
        writeStep = WRITE_IDLE;

        Serial.println(
            "KEBA write 5014 = 1 fehlgeschlagen"
        );

        return false;
    }

    Serial.printf(
        "Schreibe KEBA Sollstrom = %u mA\n",
        current_mA
    );

    return true;
}

void Charger::sendModeToDisplay(const char* text)
{
    if (kebaIP == IPAddress(0, 0, 0, 0))
        return;

    // udp.beginPacket(kebaIP, displayPort);
    // udp.print(text);
    // udp.endPacket();

    Serial.printf(
        "KEBA Display UDP: %s\n",
        text
    );
}