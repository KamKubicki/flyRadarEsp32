// FlyRadar.ino — wersja 2.2

#include <lvgl.h>
#include <time.h>
#include <WiFi.h>
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>
#include "config.h"
#include "config_secrets.h"
#include "app_state.h"
#include "hal_display.h"
#include "hal_touch.h"
#include "hal_backlight.h"
#include "wifi_manager.h"
#include "time_service.h"
#include "adsb_service.h"
#include "weather_service.h"
#include "flight_selector.h"
#include "flight_route.h"
#include "ui_router.h"
#include "sd_manager.h"

AppState gState;

// ---------------------------------------------------------------------------
// Task sieciowy — core 0, wszystkie blokujące operacje HTTP
// ---------------------------------------------------------------------------
static void networkTask(void *) {
    vTaskDelay(pdMS_TO_TICKS(2000));  // poczekaj na WiFi

    while (true) {
        unsigned long now = millis();

        // Obsługa portalu WiFiManager — musi być w każdej iteracji
        wifiProcess();

        gState.wifiConnected = wifiEnsureConnected();
        if (!gState.wifiConnected) {
            vTaskDelay(pdMS_TO_TICKS(WIFI_RECHECK_MS));
            continue;
        }

        timeServiceLoop(gState);

        // ADS-B
        unsigned long adsbMs = (unsigned long)gState.settings.adsbRefreshSec * 1000UL;
        if (!gState.adsbFetching &&
            (gState.lastAdsBFetchMs == 0 || (now - gState.lastAdsBFetchMs) >= adsbMs)) {
            adsbFetchNearest(gState);
            flightSelectorUpdate(gState);
        }

        // Pogoda
        if (!gState.weatherFetching &&
            (gState.lastWeatherFetchMs == 0 ||
             (now - gState.lastWeatherFetchMs) >= WEATHER_REFRESH_MS)) {
            weatherFetch(gState);
        }

        // Trasa
        {
            const bool hasCand = gState.hasSelectedFlight || gState.hasBestEastCandidate;
            if (hasCand) {
                const char *cs = gState.hasBestEastCandidate
                                 ? gState.bestEastCandidate.aircraft.callsign
                                 : gState.selectedFlight.aircraft.callsign;
                if (cs && strlen(cs) > 0 && strcmp(cs, gState.lastRouteCallsign) != 0) {
                    RouteInfo ri;
                    if (flightRouteFetch(cs, ri)) {
                        if (gState.hasBestEastCandidate) gState.bestEastCandidate.route = ri;
                        if (gState.hasSelectedFlight)    gState.selectedFlight.route    = ri;
                    }
                    strncpy(gState.lastRouteCallsign, cs, sizeof(gState.lastRouteCallsign) - 1);
                }
            }
        }

        vTaskDelay(pdMS_TO_TICKS(500));
    }
}

// ---------------------------------------------------------------------------
// setup / loop — core 1, tylko LVGL
// ---------------------------------------------------------------------------
enum class UiMode : uint8_t { Auto, Manual };
static UiMode s_mode = UiMode::Auto;

void setup() {
    Serial.begin(115200);
    Serial.println("FlyRadar v2.2 starting...");

    psramInit();
    Serial.printf("PSRAM: total=%u free=%u\n", ESP.getPsramSize(), ESP.getFreePsram());

    appStateInit(gState);
    appSettingsLoad(gState.settings);

    halDisplayBegin();
    halBacklightBegin();   // inicjalizuje LEDC z domyślną jasnością
    halTouchBegin();

    // Zastosuj jasność z NVS (wczytaną przez appSettingsLoad) zamiast domyślnej
    halBacklightSetPercent(gState.settings.brightnessPercent);

    uiRouterBegin(gState);

    wifiBegin();
    timeServiceBegin();
    sdInit();

    // Network task — 16KB stosu (WiFiManager WebServer potrzebuje >8KB)
    xTaskCreatePinnedToCore(networkTask, "net", 16384, nullptr, 1, nullptr, 0);
    Serial.println("FlyRadar ready.");
}

void loop() {
    lv_timer_handler();
    delay(5);

    const unsigned long now           = millis();
    const unsigned long sinceInteract = now - gState.lastUiInteractionMs;
    const UiScreenId   cur            = uiRouterCurrent();

    if (cur == UiScreenId::Settings)  s_mode = UiMode::Manual;
    else if (sinceInteract < 5000)    s_mode = UiMode::Manual;

    if (s_mode == UiMode::Auto && cur != UiScreenId::Weather)
        uiRouterShow(UiScreenId::Weather);

    uiRouterRefresh(gState);
}
