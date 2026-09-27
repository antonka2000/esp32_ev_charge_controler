#pragma once

#include <Arduino.h>
#include <IPAddress.h>
#include <ModbusTCP.h>

class Inverter
{
public:

    Inverter(
        ModbusTCP& modbusWR1,
        ModbusTCP& modbusWR2
    );

    void begin(
        IPAddress wr1IP,
        uint16_t wr1Port,
        IPAddress wr2IP,
        uint16_t wr2Port
    );

    void request();
    void resetPending();

    bool isPending() const;
    bool hasNewData();

    int32_t getPV1() const;
    int32_t getPV2() const;
    int32_t getPVTotal() const;
    int32_t getExport() const;
    int32_t getLoad() const;
    int32_t getBatt() const;

private:

    ModbusTCP& mbWR1;
    ModbusTCP& mbWR2;

    IPAddress wr1IP;
    IPAddress wr2IP;

    uint16_t wr1Port;
    uint16_t wr2Port;

    static const uint16_t REG_PV     = 5016;
    static const uint16_t REG_EXPORT = 13009;
    static const uint16_t REG_LOAD   = 13007;
    static const uint16_t REG_BATT   = 5213;

    uint16_t sgPv1[2]    = {};
    uint16_t sgPv2[2]    = {};
    uint16_t sgExport[2] = {};
    uint16_t sgLoad[2]   = {};
    uint16_t sgBatt[2]   = {};

    int32_t pv1         = 0;
    int32_t pv2         = 0;
    int32_t pvTotal     = 0;
    int32_t exportPower = 0;
    int32_t load        = 0;
    int32_t batt        = 0;


    bool sgPending = false;
    bool sgNewData = false;

    enum Step
    {
        SG_PV1 = 0,
        SG_PV2,
        SG_EXPORT,
        SG_LOAD,
        SG_BATT
    };

    Step sgStep = SG_PV1;

    bool ensureConnectedWR1();
    bool ensureConnectedWR2();

    bool callback(
        Modbus::ResultCode event,
        uint16_t transactionId,
        void* data
    );

    static bool callbackStatic(
        Modbus::ResultCode event,
        uint16_t transactionId,
        void* data
    );

    static Inverter* instance;
};