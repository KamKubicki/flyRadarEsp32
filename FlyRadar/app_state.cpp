#include "app_state.h"
#include "config.h"
#include <Preferences.h>

void appStateInit(AppState &state) {
    state.aircraftCount = 0;
    state.hasSelectedFlight = false;
    state.hasBestEastCandidate = false;
    state.wifiConnected = false;
    state.timeSynced = false;
    state.weatherAvailable = false;
    state.adsbAvailable = false;
    state.lastAdsBFetchMs = 0;
    state.lastAdsBFetchStartMs = 0;
    state.lastWeatherFetchMs = 0;
    state.lastWeatherFetchStartMs = 0;
    state.lastNtpSyncMs = 0;
    state.lastWifiCheckMs = 0;
    state.lastUiRefreshMs = 0;
    state.lastUiInteractionMs = millis();
    state.adsbVersion = 0;
    state.adsbFetching = false;
    state.weatherFetching = false;
}

void appStateMarkInteraction(AppState &state) {
    state.lastUiInteractionMs = millis();
}

bool appStateShouldShowWeather(const AppState &state) {
    if (!state.hasBestEastCandidate) {
        return true;
    }
    const unsigned long idleMs = millis() - state.lastUiInteractionMs;
    const unsigned long limitMs = (unsigned long)state.settings.weatherIdleMinutes * 60UL * 1000UL;
    return idleMs > limitMs;
}

// ---------------------------------------------------------------------------
// NVS persistence (Arduino Preferences — wrapper na ESP32 NVS)
// ---------------------------------------------------------------------------

void appSettingsSave(const AppSettings &s) {
    Preferences p;
    if (!p.begin(NVS_NAMESPACE, false)) return;
    p.putInt (NVS_KEY_RADAR_KM,     s.radarRadiusKm);
    p.putBool(NVS_KEY_SHOW_DEPS,    s.showDepartures);
    p.putBool(NVS_KEY_SHOW_ARR,     s.showArrivals);
    p.putInt (NVS_KEY_EAST_ONLY,    (int)s.visibleSide);
    p.putInt (NVS_KEY_IDLE_MIN,     s.weatherIdleMinutes);
    p.putInt (NVS_KEY_BRIGHTNESS,   s.brightnessPercent);
    p.putInt (NVS_KEY_ADSB_INTERVAL, s.adsbRefreshSec);
    p.end();
}

void appSettingsLoad(AppSettings &s) {
    Preferences p;
    if (!p.begin(NVS_NAMESPACE, true)) return;   // true = read-only
    s.radarRadiusKm       = p.getInt (NVS_KEY_RADAR_KM,      s.radarRadiusKm);
    s.showDepartures      = p.getBool(NVS_KEY_SHOW_DEPS,     s.showDepartures);
    s.showArrivals        = p.getBool(NVS_KEY_SHOW_ARR,      s.showArrivals);
    s.visibleSide         = (VisibilitySide)p.getInt(NVS_KEY_EAST_ONLY, (int)s.visibleSide);
    s.weatherIdleMinutes  = p.getInt (NVS_KEY_IDLE_MIN,      s.weatherIdleMinutes);
    s.brightnessPercent   = p.getInt (NVS_KEY_BRIGHTNESS,    s.brightnessPercent);
    s.adsbRefreshSec      = p.getInt (NVS_KEY_ADSB_INTERVAL, s.adsbRefreshSec);
    p.end();
}
