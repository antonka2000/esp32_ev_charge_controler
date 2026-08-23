#pragma once
#include <WiFi.h>
#include <WebServer.h>
#include <Preferences.h>
#include <DNSServer.h>

DNSServer dnsServer;
const byte DNS_PORT = 53;
bool isAPMode = false;
 enum PhaseMode {
  PHASE_SINGLE = 0,
  PHASE_THREE  = 1,
  PHASE_AUTO   = 2
  };


struct AppConfig {
  char     ssid[32]         = "dahoam";
  char     password[64]     = "Gria5Di1";

  char     sungrowIP[16]    = "192.168.7.222";
  uint16_t sungrowPort      = 5021;
  char     sungrowIP2[16]   = "192.168.7.222";  // zweiter WR (leer = nur einer)
  uint16_t sungrowPort2     = 5022;

  int16_t  houseBaseW      = 700;   // angenommener Hausverbrauch (einstellbar)

  char     kebaIP[16]       = "192.168.7.178";
  uint16_t kebaPort         = 502;
  uint8_t  kebaUnitId       = 255;

  uint16_t pollIntervalMs   = 2000;
  int16_t  hysteresisW      = 150;
  int16_t  targetExportW    = 200;
  uint16_t minCurrent_mA    = 6000;
  uint16_t maxCurrent_mA    = 16000;
  uint32_t holdTime       = 30;
  bool     regulationEnable = true;
 
  PhaseMode phaseMode = PHASE_THREE;

  bool batteryConnected = true;
};

AppConfig cfg;
Preferences prefs;
WebServer server(80);

void loadConfig() {
  prefs.begin("cfg", true);

  strlcpy(cfg.ssid,       prefs.getString("ssid",   cfg.ssid).c_str(),       sizeof(cfg.ssid));
  strlcpy(cfg.password,   prefs.getString("pw",     cfg.password).c_str(),   sizeof(cfg.password));
  strlcpy(cfg.sungrowIP,  prefs.getString("sgIP",   cfg.sungrowIP).c_str(),  sizeof(cfg.sungrowIP));
  strlcpy(cfg.sungrowIP2, prefs.getString("sgIP2",  cfg.sungrowIP2).c_str(), sizeof(cfg.sungrowIP2));
  strlcpy(cfg.kebaIP,     prefs.getString("kebaIP", cfg.kebaIP).c_str(),     sizeof(cfg.kebaIP));

  cfg.sungrowPort      = prefs.getUShort("sgPort",   cfg.sungrowPort);
  cfg.sungrowPort2     = prefs.getUShort("sgPort2",  cfg.sungrowPort2);
  cfg.kebaPort         = prefs.getUShort("kebaPort", cfg.kebaPort);
  cfg.kebaUnitId       = prefs.getUChar ("kebaId",   cfg.kebaUnitId);
  cfg.pollIntervalMs   = prefs.getUShort("poll",     cfg.pollIntervalMs);
  cfg.houseBaseW       = prefs.getShort ("houseBase",cfg.houseBaseW);
  cfg.hysteresisW      = prefs.getShort ("hyst",     cfg.hysteresisW);
  cfg.targetExportW    = prefs.getShort ("target",   cfg.targetExportW);
  cfg.minCurrent_mA    = prefs.getUShort("minCur",   cfg.minCurrent_mA);
  cfg.maxCurrent_mA    = prefs.getUShort("maxCur",   cfg.maxCurrent_mA);
  cfg.holdTime         = prefs.getULong ("hold",     cfg.holdTime);
  cfg.phaseMode = (PhaseMode)prefs.getULong(
    "PhaseMode",
    (uint32_t)cfg.phaseMode
  );
  cfg.batteryConnected = prefs.getBool  ("BattCon",  cfg.batteryConnected);
  cfg.regulationEnable = prefs.getBool  ("regEn",    cfg.regulationEnable);
  prefs.end();
}

void saveConfig() {
  prefs.begin("cfg", false);
  prefs.putString("ssid",    cfg.ssid);
  prefs.putString("pw",      cfg.password);
  prefs.putString("sgIP",    cfg.sungrowIP);
  prefs.putUShort("sgPort",  cfg.sungrowPort);
  prefs.putString("sgIP2",   cfg.sungrowIP2);
  prefs.putUShort("sgPort2", cfg.sungrowPort2);
  prefs.putShort ("houseBase", cfg.houseBaseW);
  prefs.putString("kebaIP",  cfg.kebaIP);
  prefs.putUShort("kebaPort",cfg.kebaPort);
  prefs.putUChar ("kebaId",  cfg.kebaUnitId);
  prefs.putUShort("poll",    cfg.pollIntervalMs);
  prefs.putShort ("hyst",    cfg.hysteresisW);
  prefs.putShort ("target",  cfg.targetExportW);
  prefs.putUShort("minCur",  cfg.minCurrent_mA);
  prefs.putUShort("maxCur",  cfg.maxCurrent_mA);
  prefs.putULong ("hold",    cfg.holdTime);
  prefs.putULong(
    "PhaseMode",
    (uint32_t)cfg.phaseMode
  );
  prefs.putBool  ("BattCon", cfg.batteryConnected);
  prefs.putBool  ("regEn",   cfg.regulationEnable);
  prefs.end();
}

void handleRoot() {
  String h = R"(<!DOCTYPE html><html><head><meta name="viewport" content="width=device-width,initial-scale=1">
<title>Config</title>
<style>
body{font-family:system-ui,sans-serif;max-width:420px;margin:16px auto;padding:0 12px;background:#f5f5f5}
.card{background:#fff;padding:16px;border-radius:10px;box-shadow:0 1px 4px rgba(0,0,0,.1)}
h2{margin:0 0 12px;font-size:1.2rem}
label{display:block;margin:10px 0 3px;font-size:.9rem;color:#444}
input[type=text],input[type=number],input[type=password]{width:100%;padding:8px;box-sizing:border-box;border:1px solid #ccc;border-radius:6px}
.radioGroup{margin-top:14px;padding:10px;background:#f8f8f8;border-radius:8px}
.radioGroup label{margin:8px 0;color:#222}
.radioGroup input{margin-right:8px}
button{width:100%;margin-top:18px;padding:12px;background:#007aff;color:#fff;border:none;border-radius:8px;font-size:1rem}
</style></head><body><div class="card"><h2>PV-Regler</h2><form method="POST" action="/save">)";

  auto f = [](const char* n, const char* l, const String& v, const char* t="text"){
    return "<label>"+String(l)+"</label><input type='"+String(t)+"' name='"+String(n)+"' value='"+v+"'>";
  };

  h += f("ssid",     "WLAN SSID",           cfg.ssid);
  h += f("pw",       "WLAN Passwort",       cfg.password, "password");
  h += f("sgIP",     "Sungrow IP",          cfg.sungrowIP);
  h += f("sgPort",   "Sungrow Port",        String(cfg.sungrowPort), "number");
  h += f("sgIP2",    "Sungrow IP2",         cfg.sungrowIP);
  h += f("sgPort2",  "Sungrow Port2",       String(cfg.sungrowPort), "number");
  h += f("kebaIP",   "Keba IP",             cfg.kebaIP);
  h += f("kebaPort", "Keba Port",           String(cfg.kebaPort), "number");
  h += f("kebaId",   "Keba Unit-ID",        String(cfg.kebaUnitId), "number");
  h += f("poll",     "Abfrageintervall ms", String(cfg.pollIntervalMs), "number");
  h += f("houseBase","Hausverbrauch W",     String(cfg.houseBaseW), "number");
  h += f("target",   "Ziel-Export max. W",  String(cfg.targetExportW), "number");
  h += f("hyst",     "Hysterese W",          String(cfg.hysteresisW), "number");
  h += f("minCur",   "Min. Strom mA",       String(cfg.minCurrent_mA), "number");
  h += f("maxCur",   "Max. Strom mA",       String(cfg.maxCurrent_mA), "number");
  h += f("hold",     "Haltezeit sec",       String(cfg.holdTime), "number");


  // ==========================================================
  // PHASEN
  // ==========================================================

  h += "<div class='radioGroup'>";
  h += "<label><b>Ladephasen</b></label>";

  h += "<label>";
  h += "<input type='radio' name='phaseMode' value='0'";
  if (cfg.phaseMode == PHASE_SINGLE) h += " checked";
  h += "> Einphasig";
  h += "</label>";

  h += "<label>";
  h += "<input type='radio' name='phaseMode' value='1'";
  if (cfg.phaseMode == PHASE_THREE) h += " checked";
  h += "> Dreiphasig";
  h += "</label>";

  h += "<label>";
  h += "<input type='radio' name='phaseMode' value='2'";
  if (cfg.phaseMode == PHASE_AUTO) h += " checked";
  h += "> Automatisch";
  h += "</label>";

  h += "</div>";


  // ==========================================================
  // BATTERIE
  // ==========================================================

  h += "<div class='radioGroup'>";
  h += "<label><b>Hausbatterie</b></label>";

  h += "<label>";
  h += "<input type='radio' name='battery' value='1'";
  if (cfg.batteryConnected) h += " checked";
  h += "> Batterie vorhanden";
  h += "</label>";

  h += "<label>";
  h += "<input type='radio' name='battery' value='0'";
  if (!cfg.batteryConnected) h += " checked";
  h += "> Keine Batterie";
  h += "</label>";

  h += "</div>";


  // ==========================================================
  // REGELUNG
  // ==========================================================

  h += "<label style='margin-top:14px'><input type='checkbox' name='regEn' value='1'";
  if (cfg.regulationEnable) h += " checked";
  h += "> Regelung aktiv</label>";


  h += "<button>Speichern & Neustart</button></form></div></body></html>";

  server.send(200, "text/html", h);
}
void handleSave() {
  if (server.hasArg("ssid"))     strlcpy(cfg.ssid,      server.arg("ssid").c_str(),     sizeof(cfg.ssid));
  if (server.hasArg("pw"))       strlcpy(cfg.password,  server.arg("pw").c_str(),       sizeof(cfg.password));
  if (server.hasArg("sgIP"))     strlcpy(cfg.sungrowIP, server.arg("sgIP").c_str(),     sizeof(cfg.sungrowIP));
  if (server.hasArg("kebaIP"))   strlcpy(cfg.kebaIP,    server.arg("kebaIP").c_str(),   sizeof(cfg.kebaIP));
  if (server.hasArg("sgPort"))   cfg.sungrowPort    = server.arg("sgPort").toInt();
  if (server.hasArg("kebaPort")) cfg.kebaPort       = server.arg("kebaPort").toInt();
  if (server.hasArg("kebaId"))   cfg.kebaUnitId     = server.arg("kebaId").toInt();
  if (server.hasArg("poll"))     cfg.pollIntervalMs = server.arg("poll").toInt();
  if (server.hasArg("target"))   cfg.targetExportW  = server.arg("target").toInt();
  if (server.hasArg("hyst"))     cfg.hysteresisW    = server.arg("hyst").toInt();
  if (server.hasArg("minCur"))   cfg.minCurrent_mA  = server.arg("minCur").toInt();
  if (server.hasArg("maxCur"))   cfg.maxCurrent_mA  = server.arg("maxCur").toInt();
  if (server.hasArg("hold"))     cfg.holdTime     = server.arg("hold").toInt();
  if (server.hasArg("phaseMode"))
    {
        cfg.phaseMode =
            (PhaseMode)server.arg("phaseMode").toInt();
    }

  cfg.batteryConnected =
      server.hasArg("battery") &&
      server.arg("battery") == "1";
  cfg.regulationEnable = server.hasArg("regEn");

  saveConfig();
  server.send(200, "text/html", "<h3 style='text-align:center;margin-top:40px'>Gespeichert - Neustart...</h3>");
  delay(400);
  ESP.restart();
}

void startConfigWeb() {
  server.on("/", HTTP_GET, handleRoot);
  server.on("/save", HTTP_POST, handleSave);

  // Catch-all für Captive Portal (leitet alles auf die Config-Seite um)
  server.onNotFound([]() {
    server.sendHeader("Location", String("http://") + WiFi.softAPIP().toString(), true);
    server.send(302, "text/plain", "");
  });

  server.begin();
  Serial.println("Webserver gestartet");
}

void handleConfigWeb() {
  if (isAPMode) {
    dnsServer.processNextRequest();   // für Captive Portal
  }
  server.handleClient();
}

// ============================================================
// WLAN verbinden oder als AP starten
// ============================================================
bool startWiFi() {
  loadConfig();

  // Prüfen ob eine SSID hinterlegt ist
  bool hasSSID = (strlen(cfg.ssid) > 0 && strcmp(cfg.ssid, "dahoam") != 0) || 
                 (strlen(cfg.ssid) > 2);   // grobe Prüfung

  if (hasSSID) {
    Serial.printf("Versuche WLAN: %s\n", cfg.ssid);
    WiFi.mode(WIFI_STA);
    WiFi.begin(cfg.ssid, cfg.password);

    unsigned long start = millis();
    while (WiFi.status() != WL_CONNECTED && millis() - start < 15000) {
      delay(400);
      Serial.print(".");
    }
    Serial.println();

    if (WiFi.status() == WL_CONNECTED) {
      Serial.print("WLAN OK – IP: ");
      Serial.println(WiFi.localIP());
      isAPMode = false;
      return true;
    }
    Serial.println("WLAN Verbindung fehlgeschlagen");
  } else {
    Serial.println("Keine gültige SSID hinterlegt");
  }

  // ---------- Fallback: Access Point ----------
  Serial.println("Starte Access Point...");
  WiFi.mode(WIFI_AP);
  WiFi.softAP("ESP-PV-Config", "12345678");   // SSID + Passwort des APs

  delay(500);
  IPAddress apIP = WiFi.softAPIP();           // normalerweise 192.168.4.1
  Serial.print("AP gestartet → http://");
  Serial.println(apIP);

  // DNS-Umleitung (damit viele Handys automatisch die Config-Seite öffnen)
  dnsServer.start(DNS_PORT, "*", apIP);

  isAPMode = true;
  return false;
}