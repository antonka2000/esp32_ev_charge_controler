#include <WiFi.h>
#include <ModbusTCP.h>
#include "ConfigWeb.h"

// ============================================================
// MODBUS
// ============================================================
ModbusTCP mb;

IPAddress sungrowIP;
IPAddress sungrowIP2;
IPAddress kebaIP;

// Sungrow
const uint16_t REG_EXPORT = 13009;
const uint16_t REG_PV     = 5016;
uint16_t sgRegs[2];
uint16_t sgPv1[2];
uint16_t sgPv2[2];
int32_t pvTotal;
int32_t  exportPower = 0;
bool     sgPending = false;
bool     sgNewData = false;

// Keba
const uint16_t REG_STATE = 1000;
const uint16_t REG_CABLE = 1004;
const uint16_t REG_POWER = 1020;
const uint16_t REG_SET_CURRENT = 5004;
int32_t pv1 = 0, pv2 = 0;
enum { SG_EXPORT = 0, SG_PV1, SG_PV2 };
uint8_t sgStep = SG_EXPORT;
uint32_t holdsec;


uint16_t kebaRegs[2];
uint32_t kebaState = 0;
uint32_t kebaCable = 0;
uint32_t kebaPower_mW = 0;
bool     kebaPending = false;
bool     kebaNewData = false;
uint8_t  kebaStep = 0;          // 0=State, 1=Cable, 2=Power

// Regelung
uint16_t currentSetpoint_mA = 6000;
unsigned long lastCurrentChange = 0;

bool writePending = false;

bool writeCallback(Modbus::ResultCode event, uint16_t tid, void* data) {
  writePending = false;
  if (event != Modbus::EX_SUCCESS) {
    Serial.printf("Schreiben FEHLER: 0x%02X\n", (uint8_t)event);
  } else {
    Serial.println("Schreiben OK");
  }
  return true;
}

void writeKebaCurrent(uint16_t mA) {
  if (mA > 0 && mA < 6000) mA = 6000;
  if (mA > 63000)           mA = 63000;

  if (writePending || kebaPending || sgPending) {
    Serial.println("Schreiben übersprungen – andere Anfrage läuft");
    return;
  }
  if (!ensureConnected(kebaIP, cfg.kebaPort)) {
    Serial.println("Keba nicht verbunden");
    return;
  }

  // Falls Station suspended/disabled ist → zuerst freigeben
  if (kebaState == 1 || kebaState == 5) {
    Serial.println("Station enable (5014 = 1)");
    mb.writeHreg(kebaIP, 5014, 1, nullptr, cfg.kebaUnitId);
    delay(300);
  }

  Serial.printf("Schreibe Register 5004 = %u mA\n", mA);
  uint16_t tid = mb.writeHreg(kebaIP, 5004, mA, writeCallback, cfg.kebaUnitId);
  if (tid) writePending = true;
  else     Serial.println("writeHreg fehlgeschlagen");
}


// ============================================================
// CALLBACKS
// ============================================================
bool sgCallback(Modbus::ResultCode event, uint16_t tid, void* data) {
  sgPending = false;

  if (event != Modbus::EX_SUCCESS) {
    Serial.printf("Sungrow Fehler: 0x%02X (step %d)\n", (uint8_t)event, sgStep);
    sgStep = (sgStep + 1) % 3;   // trotzdem weiter
    return true;
  }
 
  switch (sgStep) {
    case SG_EXPORT:
      exportPower = (int32_t)(((uint32_t)sgRegs[1] << 16) | sgRegs[0]);
      if (exportPower != (int32_t)0x7FFFFFFF) sgNewData = true;
      break;

    case SG_PV1:
      pv1 = (int32_t)(((uint32_t)sgPv1[1] << 16) | sgPv1[0]);
      break;

    case SG_PV2:
      pv2 = (int32_t)(((uint32_t)sgPv2[1] << 16) | sgPv2[0]);
      break;
  }

  pvTotal = pv1 + pv2;
  Serial.printf("Step %d | Export=%d | PV1=%d PV2=%d Summe=%d\n",
                sgStep, exportPower, pv1, pv2, pvTotal);

  // nächsten Schritt
  sgStep = (sgStep + 1) % 3;
  return true;
}

bool kebaCallback(Modbus::ResultCode event, uint16_t tid, void* data) {
  kebaPending = false;
  if (event != Modbus::EX_SUCCESS) {
    Serial.printf("Keba Fehler: 0x%02X (Step %d)\n", (uint8_t)event, kebaStep);
    return true;
  }
  uint32_t value = ((uint32_t)kebaRegs[0] << 16) | kebaRegs[1];

  switch (kebaStep) {
    case 0: kebaState    = value; break;
    case 1: kebaCable    = value; break;
    case 2: kebaPower_mW = value; kebaNewData = true; break;
  }
  return true;
}

// ============================================================
// HILFSFUNKTIONEN
// ============================================================
bool ensureConnected(IPAddress ip, uint16_t port) {
  if (mb.isConnected(ip)) return true;
  return mb.connect(ip, port);
}

void requestSungrow() {
  if (sgPending) return;

  bool ok = false;

  switch (sgStep) {

    case SG_EXPORT:  // Master: Export (Port 5021)
      if (mb.isConnected(sungrowIP2)) {
        mb.disconnect(sungrowIP2);
        delay(100);
      }
      if (!ensureConnected(sungrowIP, cfg.sungrowPort)) return;
      ok = mb.readIreg(sungrowIP, REG_EXPORT, sgRegs, 2, sgCallback, 1);
      break;

    case SG_PV1:     // Master: PV (Port 5021)
      if (mb.isConnected(sungrowIP2)) {
        mb.disconnect(sungrowIP2);
        delay(100);
      }
      if (!ensureConnected(sungrowIP, cfg.sungrowPort)) return;
      ok = mb.readIreg(sungrowIP, REG_PV, sgPv1, 2, sgCallback, 1);
      break;

    case SG_PV2:     // zweiter WR: PV (Port 5022)
      if (strlen(cfg.sungrowIP2) < 7) {
        sgStep = SG_EXPORT;
        return;
      }
      if (mb.isConnected(sungrowIP)) {
        mb.disconnect(sungrowIP);
        delay(100);
      }
      Serial.printf("Connect WR2 %s:%u\n", cfg.sungrowIP2, cfg.sungrowPort2);
      if (!mb.connect(sungrowIP2, cfg.sungrowPort2)) {
        Serial.println("WR2 connect fehlgeschlagen");
        sgStep = SG_EXPORT;
        return;
      }
      ok = mb.readIreg(sungrowIP2, REG_PV, sgPv2, 2, sgCallback, 1);
      break;
  }

  // WICHTIG: außerhalb des switch
  if (ok) sgPending = true;
}

void requestKeba() {
  if (kebaPending) return;
  if (!ensureConnected(kebaIP, cfg.kebaPort)) return;

  uint16_t reg = (kebaStep == 0) ? REG_STATE :
                 (kebaStep == 1) ? REG_CABLE : REG_POWER;

  if (mb.readHreg(kebaIP, reg, kebaRegs, 2, kebaCallback, cfg.kebaUnitId)) {
    kebaPending = true;
  }
}

uint16_t powerToCurrent_mA(int32_t powerW) {
  if (powerW <= 0) return 0;
  float amps = (float)powerW / 690.0f;          // 3 × 230 V
  uint16_t mA = (uint16_t)(amps * 1000.0f);
  mA = (mA / 100) * 100;                        // 100-mA-Schritte

  if (mA > 0 && mA < cfg.minCurrent_mA) mA = cfg.minCurrent_mA;
  if (mA > cfg.maxCurrent_mA) mA = cfg.maxCurrent_mA;
  return mA;
}

void writeAltKebaCurrent(uint16_t mA) {
  if (!ensureConnected(kebaIP, cfg.kebaPort)) return;
  mb.writeHreg(kebaIP, 5014, 1, writeCallback, cfg.kebaUnitId);  // Enable
  mb.writeHreg(kebaIP, REG_SET_CURRENT, mA, nullptr, cfg.kebaUnitId);
  Serial.printf("→ Sollstrom gesetzt: %d mA\n", mA);
}

void regulateCharging() {
  if (!cfg.regulationEnable) return;
  if (kebaCable < 5) return;                              // kein Auto verbunden
  if (millis() - lastCurrentChange < holdsec) return;

  // 1. Aktuelle Soll-Ladeleistung aus dem gesetzten Strom berechnen (3-phasig)
  //    P = I * 3 * 230
  int32_t currentChargeW = 0;
  if (currentSetpoint_mA >= cfg.minCurrent_mA) {
    currentChargeW = (int32_t)((kebaPower_mW / 1000.0f) * 690.0f);
  }

  // 2. Verfügbare Leistung = bisherige Ladeleistung + aktueller Export
  //    (Export positiv = Überschuss, negativ = Netzbezug)
  int32_t availableW = currentChargeW + pvTotal - cfg.houseBaseW;

  // 3. Ziel: möglichst wenig Export (unter targetExportW bleiben)
  //    Wir wollen availableW so einstellen, dass der Export danach ≈ 0…targetExportW ist
  int32_t targetChargeW = availableW - cfg.targetExportW;

  if (targetChargeW < 0) targetChargeW = 0;

  // 4. Hysterese – nur ändern, wenn die Abweichung groß genug ist
  int32_t diffW = targetChargeW - currentChargeW;
  if (abs(diffW) < cfg.hysteresisW) return;

  // 5. In Strom umrechnen
  uint16_t newCurrent = powerToCurrent_mA(targetChargeW);

  // Mindeständerung 0,5 A, sonst ignorieren
  if (abs((int)newCurrent - (int)currentSetpoint_mA) < 500) return;

  Serial.printf("Regelung: Export=%d W | bisher %d W (%d mA) → neu %d W (%d mA)\n",
                exportPower, currentChargeW, currentSetpoint_mA,
                targetChargeW, newCurrent);

  writeKebaCurrent(newCurrent);
  currentSetpoint_mA = newCurrent;
  lastCurrentChange  = millis();
}

// ============================================================
// AUSGABE
// ============================================================
void printSungrow() {
  Serial.println("------ SUNGROW ------");
  Serial.printf("Export Power: %d W\n", exportPower);
  if (exportPower > 0)      Serial.println("→ Einspeisung");
  else if (exportPower < 0) Serial.println("→ Netzbezug");
  else                      Serial.println("→ 0 W");
  Serial.printf("pvTotal: %d W\n", pvTotal);
}

void printKeba() {
  Serial.println("------ KEBA ------");
  Serial.printf("State: %u", kebaState);
  if (kebaState == 2) Serial.print(" (ready)");
  if (kebaState == 3) Serial.print(" (charging)");
  Serial.println();
  Serial.printf("Cable: %u → %s\n", kebaCable, (kebaCable >= 5) ? "verbunden" : "nicht verbunden");
  Serial.printf("Power: %d W\n", (int)(kebaPower_mW / 1000));
  Serial.println("---------------------------");
}

// ============================================================
// SETUP
// ============================================================
void setup() {
  Serial.begin(115200);
  delay(800);
  Serial.println("\nESP32 – Sungrow + Keba + WebConfig");

  // WLAN oder AP starten
  startWiFi();          // lädt auch die Config

  // IPs aus Config übernehmen (auch im AP-Modus unproblematisch)
  sungrowIP.fromString(cfg.sungrowIP);
  sungrowIP2.fromString(cfg.sungrowIP2);
  kebaIP.fromString(cfg.kebaIP);

  mb.client();
  startConfigWeb();

  Serial.println("Bereit.");

  holdsec = cfg.holdTime*1000;
}

// ============================================================
// LOOP
// ============================================================
void loop() {
  handleConfigWeb();
  mb.task();

  if (WiFi.status() != WL_CONNECTED) {
    sgPending = kebaPending = false;
    delay(1000);
    return;
  }

  if (sgNewData) {
    sgNewData = false;
    printSungrow();

  }

  if (kebaNewData) {
    kebaNewData = false;
    printKeba();

    regulateCharging();
  }

  // Polling abwechselnd
  static unsigned long lastPoll = 0;
  static uint8_t phase = 0;   // 0=Sungrow, 1=State, 2=Cable, 3=Power

  

  if (!sgPending && !kebaPending && (millis() - lastPoll >= cfg.pollIntervalMs)) {
    lastPoll = millis();

    if (phase == 0) {
      requestSungrow();

    } else {
      kebaStep = phase - 1;
      requestKeba();
    }
    phase = (phase + 1) % 4;
  }

  delay(5);
}