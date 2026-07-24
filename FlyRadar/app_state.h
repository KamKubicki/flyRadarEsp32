#pragma once

#include "app_types.h"
#include "config.h"

// ============================================================================
// Central application state. Services only update it, UI only reads.
// All flags are "data-driven" — UI reaguje na zmiany w stanach, nie na
// hardware events.
// ============================================================================

struct AppSettings {
    int             radarRadiusKm      = 37;
    bool            radarShowAll       = true;
    bool            showArrivals       = true;
    bool            showDepartures     = true;
    VisibilitySide  visibleSide        = VisibilitySide::Any;
    int             weatherIdleMinutes = 15;
    int             brightnessPercent  = BACKLIGHT_DEFAULT_PCT;
    int             adsbRefreshSec     = 12;   // interwał odświeżania ADS-B (5-60s)
};

struct AppState {
    bool            wifiConnected      = false;
    bool            timeSynced         = false;
    bool            weatherAvailable   = false;
    bool            adsbAvailable      = false;

    AppSettings     settings;

    WeatherCurrent  weather;

    AircraftState   aircraft[MAX_AIRCRAFT];
    int             aircraftCount      = 0;

    FlightCandidate selectedFlight;
    bool            hasSelectedFlight  = false;

    FlightCandidate bestEastCandidate;
    bool            hasBestEastCandidate = false;

    unsigned long   lastAdsBFetchMs     = 0;
    unsigned long   lastAdsBFetchStartMs = 0;
    unsigned long   lastWeatherFetchMs  = 0;
    unsigned long   lastWeatherFetchStartMs = 0;
    unsigned long   lastNtpSyncMs       = 0;
    unsigned long   lastWifiCheckMs     = 0;
    unsigned long   lastUiRefreshMs     = 0;
    unsigned long   lastUiInteractionMs = 0;

    int             adsbVersion         = 0;
    bool            adsbFetching        = false;
    bool            weatherFetching     = false;

    char            lastRouteCallsign[16] = {0};
};

void appStateInit(AppState &state);
void appStateMarkInteraction(AppState &state);
bool appStateShouldShowWeather(const AppState &state);
void appSettingsSave(const AppSettings &s);
void appSettingsLoad(AppSettings &s);
