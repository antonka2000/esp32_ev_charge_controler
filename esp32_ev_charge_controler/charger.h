#pragma once

#include <Arduino.h>
#include <IPAddress.h>
#include <ModbusTCP.h>
#include <WiFiUdp.h>



class Charger
{
public:


    explicit Charger(ModbusTCP& modbus);

    void begin(
        IPAddress ip,
        uint16_t port,
        uint8_t unitId
    );

    void sendModeToDisplay(const char* text);

    void request();

    bool setEnabled(bool enabled);
    bool isPending() const;
    bool hasNewData();

    void resetPending();

    bool writeCurrent(uint16_t current_mA);

    uint32_t getState() const;
    uint32_t getCable() const;
    uint32_t getPower_mW() const;

private:

    ModbusTCP& mbKEBA;
 //   WiFiUDP udp;

    

    IPAddress kebaIP;
    uint16_t kebaPort;
    uint8_t kebaUnitId;
    uint16_t displayPort = 7090;

    uint16_t regs[2] = {0};

    uint32_t state = 0;
    uint32_t cable = 0;
    uint32_t power_mW = 0;

    bool pending = false;
    bool newData = false;
    bool writePending = false;

    float reconnectDelayUntil;
    
    uint8_t step = 0;

    enum WriteStep : uint8_t {
        WRITE_IDLE = 0,
        WRITE_ENABLE,
        WRITE_CURRENT,
        WRITE_DISABLE_FIRST,
        WRITE_DISABLE_SECOND
    };

    WriteStep writeStep = WRITE_IDLE;
    uint16_t requestedCurrent_mA = 0;

    static Charger* instance;

    static bool readCallback(
        Modbus::ResultCode event,
        uint16_t transactionId,
        void* data
    );

// NEU: Statischer Callback für die Library
static bool readCallbackStatic(Modbus::ResultCode event, uint16_t transactionId, void* data);

    static bool writeCallback(
        Modbus::ResultCode event,
        uint16_t transactionId,
        void* data
    );
};