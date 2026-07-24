#include "wifi_manager.h"
#include "config.h"
#include "config_secrets.h"
#include <WiFiManager.h>
#include <WiFi.h>
#include <Arduino.h>

// ============================================================================
// WiFiManager — tzapu/WiFiManager
// WAŻNE: wifiBegin() musi być wywoływane z DEDYKOWANEGO TASKU z ≥32KB stosu
//        lub z setup() na ESP32-S3 (główny task ma ~8KB — za mało na WebServer)
//        Rozwiązanie: wifiBegin() w setup() ale tylko inicjalizuje,
//        a process() w network tasku który ma 10KB+
// ============================================================================

namespace {

WiFiManager s_wm;
bool        s_portalActive = false;
bool        s_initialized  = false;
char        s_ipBuf[20]    = {};
char        s_ssidBuf[48]  = {};

void onPortalStart(WiFiManager *) {
    s_portalActive = true;
    Serial.println("[WiFi] Portal AP: FlyRadar-Setup — 192.168.4.1");
}
void onSaved() {
    s_portalActive = false;
    Serial.println("[WiFi] New WiFi credentials saved");
}

} // namespace

void wifiBegin() {
    if (s_initialized) return;
    s_initialized = true;

    s_wm.setConfigPortalBlocking(false);  // nieblokujący portal
    s_wm.setConnectTimeout(10);           // 10s na połączenie przy starcie
    s_wm.setConfigPortalTimeout(0);       // portal działa bez timeoutu
    s_wm.setAPCallback(onPortalStart);
    s_wm.setSaveConfigCallback(onSaved);
    s_wm.setTitle("FlyRadar");
    s_wm.setDarkMode(true);
    s_wm.setDebugOutput(false);

    // Próba połączenia z zapisanymi kredencjałami
    // Fallback 1: config_secrets.h (jeśli nigdy nie konfigurowano)
    bool connected = s_wm.autoConnect("FlyRadar-Setup");

    if (!connected) {
        // Fallback 2: twarde kredencjale jeśli autoConnect nie ma nic zapisanego
        Serial.println("[WiFi] Probuje config_secrets...");
        WiFi.begin(WIFI_SSID, WIFI_PASSWORD);
        unsigned long t = millis();
        while (WiFi.status() != WL_CONNECTED && millis() - t < 8000)
            delay(200);
        if (WiFi.status() == WL_CONNECTED)
            Serial.printf("[WiFi] Connected to config_secrets: %s\n", WIFI_SSID);
    }

    if (WiFi.status() == WL_CONNECTED) {
        s_portalActive = false;
        Serial.printf("[WiFi] OK — %s  IP: %s\n",
                      WiFi.SSID().c_str(), WiFi.localIP().toString().c_str());
        s_wm.startWebPortal();  // portal dostępny pod IP urządzenia
    }
}

void wifiProcess() {
    // Musi być wywoływane regularnie — obsługuje HTTP serwer portalu
    if (s_initialized) s_wm.process();
}

bool wifiEnsureConnected() {
    if (WiFi.status() == WL_CONNECTED) return true;
    WiFi.reconnect();
    delay(100);
    return WiFi.status() == WL_CONNECTED;
}

bool wifiPortalActive() {
    return s_portalActive || (s_initialized && s_wm.getConfigPortalActive());
}

const char *wifiLocalIP() {
    if (WiFi.status() != WL_CONNECTED) return "";
    strncpy(s_ipBuf, WiFi.localIP().toString().c_str(), sizeof(s_ipBuf) - 1);
    return s_ipBuf;
}

const char *wifiSSID() {
    if (WiFi.status() != WL_CONNECTED) return "";
    strncpy(s_ssidBuf, WiFi.SSID().c_str(), sizeof(s_ssidBuf) - 1);
    return s_ssidBuf;
}

const char *wifiStatusText() {
    if (wifiPortalActive())          return "Hotspot active";
    switch (WiFi.status()) {
        case WL_CONNECTED:           return "Connected";
        case WL_CONNECT_FAILED:      return "Connection failed";
        case WL_DISCONNECTED:        return "Disconnected";
        case WL_IDLE_STATUS:         return "Connecting...";
        case WL_NO_SSID_AVAIL:       return "Network not found";
        default:                     return "Unknown";
    }
}

void wifiResetCredentials() {
    Serial.println("[WiFi] Clearing credentials...");
    s_wm.resetSettings();
    WiFi.disconnect(false, true);
}
