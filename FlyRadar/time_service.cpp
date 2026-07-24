#include "time_service.h"
#include "config.h"
#include <WiFi.h>
#include <sys/time.h>
#include <time.h>

void timeServiceBegin() {
    // configTzTime ustawia jednocześnie NTP i strefę czasową (ESP-IDF API).
    // Bezpieczniejsze niż setenv+configTime bo działa na wszystkich wersjach ESP-IDF.
    // POSIX TZ: CET-1CEST-2,M3.5.0/2,M10.5.0/3
    //   CET-1        = UTC+1 zimą
    //   CEST-2       = UTC+2 latem (DST)
    //   M3.5.0/2     = DST zaczyna się ostatnią niedzielę marca o 2:00
    //   M10.5.0/3    = DST kończy się ostatnią niedzielą października o 3:00
    configTzTime(NTP_TZ_STRING, NTP_SERVER_1, NTP_SERVER_2);
}

void timeServiceLoop(AppState &state) {
    if (!state.wifiConnected) {
        state.timeSynced = false;
        return;
    }
    if (state.timeSynced && (millis() - state.lastNtpSyncMs) < NTP_REFRESH_MS) {
        return;
    }
    struct tm timeInfo;
    if (getLocalTime(&timeInfo, 0)) {
        state.timeSynced    = true;
        state.lastNtpSyncMs = millis();
    }
}
