#include "inverter.h"

Inverter* Inverter::instance = nullptr;


// ============================================================
// KONSTRUKTOR
// ============================================================

Inverter::Inverter(
    ModbusTCP& modbusWR1,
    ModbusTCP& modbusWR2
)
    : mbWR1(modbusWR1),
      mbWR2(modbusWR2)
{
    instance = this;
}


// ============================================================
// SETUP
// ============================================================

void Inverter::begin(
    IPAddress wr1,
    uint16_t port1,
    IPAddress wr2,
    uint16_t port2
)
{
    wr1IP = wr1;
    wr1Port = port1;

    wr2IP = wr2;
    wr2Port = port2;

    sgStep = SG_PV1;

    sgPending = false;
    sgNewData = false;

    pv1 = 0;
    pv2 = 0;
    pvTotal = 0;
    exportPower = 0;
    load = 0;
    batt = 0;
}


void Inverter::resetPending()
{
    sgPending = false;
}


// ============================================================
// VERBINDUNG WR1
// ============================================================

bool Inverter::ensureConnectedWR1()
{
    if (mbWR1.isConnected(wr1IP))
        return true;

    return mbWR1.connect(wr1IP, wr1Port);
}


// ============================================================
// VERBINDUNG WR2
// ============================================================

bool Inverter::ensureConnectedWR2()
{
    if (mbWR2.isConnected(wr2IP))
        return true;

    return mbWR2.connect(wr2IP, wr2Port);
}


// ============================================================
// STATIC CALLBACK
// ============================================================

bool Inverter::callbackStatic(
    Modbus::ResultCode event,
    uint16_t transactionId,
    void* data
)
{
    if (instance == nullptr)
        return true;

    return instance->callback(
        event,
        transactionId,
        data
    );
}


// ============================================================
// SUNGROW CALLBACK
// ============================================================

bool Inverter::callback(
    Modbus::ResultCode event,
    uint16_t transactionId,
    void* data
)
{
    sgPending = false;

    // --------------------------------------------------------
    // Fehler
    // --------------------------------------------------------

    if (event != Modbus::EX_SUCCESS)
    {
        Serial.printf(
            "Sungrow Fehler: 0x%02X (Step %d)\n",
            (uint8_t)event,
            sgStep
        );

        sgStep =
            (Step)(((uint8_t)sgStep + 1) % 5);

        return true;
    }


    switch (sgStep)
    {
        // ====================================================
        // WR1 PV - int32sw
        // WORD0 = Low Word
        // WORD1 = High Word
        // ====================================================

        case SG_PV1:
        {
            uint32_t value =
                ((uint32_t)sgPv1[1] << 16) |
                sgPv1[0];

            pv1 = (int32_t)value;

            Serial.printf(
                "WR1 PV = %d W\n",
                pv1
            );

            break;
        }


        // ====================================================
        // WR2 PV - int32sw
        // ====================================================

        case SG_PV2:
        {
            uint32_t value =
                ((uint32_t)sgPv2[1] << 16) |
                sgPv2[0];

            pv2 = (int32_t)value;

            Serial.printf(
                "WR2 PV = %d W\n",
                pv2
            );

            break;
        }


        // ====================================================
        // EXPORT - int32sw
        // ====================================================

        case SG_EXPORT:
        {
            uint32_t value =
                ((uint32_t)sgExport[1] << 16) |
                sgExport[0];

            exportPower = (int32_t)value;

            Serial.printf(
                "Export = %d W\n",
                exportPower
            );

            break;
        }


        // ====================================================
        // LOAD - int32sw
        // Register 13007
        // ====================================================

        case SG_LOAD:
        {
            uint32_t value =
                ((uint32_t)sgLoad[1] << 16) |
                sgLoad[0];

            load = (int32_t)value;

            Serial.printf(
                "Load = %d W\n",
                load
            );

            break;
        }


        // ====================================================
        // BATTERIE - int16be
        // Register 5213
        // ====================================================

        case SG_BATT:
        {
            int16_t value16 =
                (int16_t)sgBatt[0];

            batt = (int32_t)value16;

            Serial.printf(
                "Batterie = %d W\n",
                batt
            );

            break;
        }
    }


    // --------------------------------------------------------
    // PV Gesamt
    // --------------------------------------------------------

    pvTotal = pv1 + pv2;


    // --------------------------------------------------------
    // Nach Batterie ist kompletter Datensatz fertig
    // --------------------------------------------------------

    if (sgStep == SG_BATT)
    {
        sgNewData = true;

        Serial.println();
        Serial.println(
            "========== SUNGROW =========="
        );

        Serial.printf(
            "WR1 PV        : %d W\n",
            pv1
        );

        Serial.printf(
            "WR2 PV        : %d W\n",
            pv2
        );

        Serial.printf(
            "PV Gesamt     : %d W\n",
            pvTotal
        );

        Serial.printf(
            "Einspeisung   : %d W\n",
            exportPower
        );

        Serial.printf(
            "Load          : %d W\n",
            load
        );

        Serial.printf(
            "Batterie      : %d W\n",
            batt
        );

        Serial.println(
            "=============================="
        );
    }


    // --------------------------------------------------------
    // Nächster Schritt
    // --------------------------------------------------------

    sgStep =
        (Step)(((uint8_t)sgStep + 1) % 5);

    return true;
}


// ============================================================
// SUNGROW ABFRAGE
// ============================================================

void Inverter::request()
{
    if (sgPending)
        return;

    bool ok = false;


    switch (sgStep)
    {
        // ====================================================
        // WR1 PV
        // ====================================================

        case SG_PV1:

            Serial.printf(
                "Connect WR1 %s:%u\n",
                wr1IP.toString().c_str(),
                wr1Port
            );

            if (!ensureConnectedWR1())
            {
                Serial.println(
                    "WR1 connect fehlgeschlagen"
                );

                return;
            }

            ok =
                mbWR1.readIreg(
                    wr1IP,
                    REG_PV,
                    sgPv1,
                    2,
                    callbackStatic,
                    1
                );

            break;


        // ====================================================
        // WR2 PV
        // ====================================================

        case SG_PV2:

            Serial.printf(
                "Connect WR2 %s:%u\n",
                wr2IP.toString().c_str(),
                wr2Port
            );

            if (!ensureConnectedWR2())
            {
                Serial.println(
                    "WR2 connect fehlgeschlagen"
                );

                return;
            }

            ok =
                mbWR2.readIreg(
                    wr2IP,
                    REG_PV,
                    sgPv2,
                    2,
                    callbackStatic,
                    1
                );

            break;


        // ====================================================
        // EXPORT
        // NUR WR1
        // ====================================================

        case SG_EXPORT:

            Serial.printf(
                "Connect WR1 %s:%u\n",
                wr1IP.toString().c_str(),
                wr1Port
            );

            if (!ensureConnectedWR1())
            {
                Serial.println(
                    "WR1 connect fehlgeschlagen"
                );

                return;
            }

            ok =
                mbWR1.readIreg(
                    wr1IP,
                    REG_EXPORT,
                    sgExport,
                    2,
                    callbackStatic,
                    1
                );

            break;


        // ====================================================
        // LOAD
        // NUR WR1
        // Register 13007 = int32sw
        // ====================================================

        case SG_LOAD:

            Serial.printf(
                "Connect WR1 %s:%u\n",
                wr1IP.toString().c_str(),
                wr1Port
            );

            if (!ensureConnectedWR1())
            {
                Serial.println(
                    "WR1 connect fehlgeschlagen"
                );

                return;
            }

            ok =
                mbWR1.readIreg(
                    wr1IP,
                    REG_LOAD,
                    sgLoad,
                    2,
                    callbackStatic,
                    1
                );

            break;


        // ====================================================
        // BATTERIE
        // NUR WR1
        // Register 5213 = int16be
        // ====================================================

        case SG_BATT:

            Serial.printf(
                "Connect WR1 %s:%u\n",
                wr1IP.toString().c_str(),
                wr1Port
            );

            if (!ensureConnectedWR1())
            {
                Serial.println(
                    "WR1 connect fehlgeschlagen"
                );

                return;
            }

            ok =
                mbWR1.readIreg(
                    wr1IP,
                    REG_BATT,
                    sgBatt,
                    1,
                    callbackStatic,
                    1
                );

            break;
    }


    if (ok)
        sgPending = true;
}


// ============================================================
// STATUS
// ============================================================

bool Inverter::isPending() const
{
    return sgPending;
}


bool Inverter::hasNewData()
{
    if (!sgNewData)
        return false;

    sgNewData = false;

    return true;
}


// ============================================================
// GETTER
// ============================================================

int32_t Inverter::getPV1() const
{
    return pv1;
}


int32_t Inverter::getPV2() const
{
    return pv2;
}


int32_t Inverter::getPVTotal() const
{
    return pvTotal;
}


int32_t Inverter::getExport() const
{
    return exportPower;
}


int32_t Inverter::getLoad() const
{
    return load;
}


int32_t Inverter::getBatt() const
{
    return batt;
}