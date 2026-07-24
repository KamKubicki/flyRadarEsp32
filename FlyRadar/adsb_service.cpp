#include "adsb_service.h"
#include "config.h"
#include "http_client_helper.h"
#include "geo_utils.h"
#include <ArduinoJson.h>
#include <string.h>
#include <stdio.h>

namespace {

void copyField(const char *src, char *dst, size_t dstSize) {
    if (!src) {
        dst[0] = '\0';
        return;
    }
    size_t n = strnlen(src, dstSize - 1);
    memcpy(dst, src, n);
    dst[n] = '\0';
}

double optDouble(JsonVariant v) {
    if (v.isNull()) return NAN;
    if (v.is<float>() || v.is<double>() || v.is<int>() || v.is<long>()) {
        return v.as<double>();
    }
    return NAN;
}

bool optBool(JsonVariant v) {
    if (v.isNull()) return false;
    if (v.is<bool>()) return v.as<bool>();
    if (v.is<int>())  return v.as<int>() != 0;
    return false;
}

} // namespace

bool adsbFetchNearest(AppState &state) {
    Serial.println("[ADSB] start");
    if (state.adsbFetching) { Serial.println("[ADSB] skip"); return false; }
    state.adsbFetching = true;
    state.lastAdsBFetchStartMs = millis();

    int radiusNm = (int)(state.settings.radarRadiusKm / 1.852 + 0.5);
    if (radiusNm < 3)  radiusNm = 3;
    if (radiusNm > 60) radiusNm = 60;

    // Fallback chain — próbuj kolejno, użyj pierwszego który odpowie
    // Kolejność: airplanes.live → adsb.lol → adsb.fi (jak w esp32flight)
    struct Provider { const char *urlFmt; const char *name; };
    static const Provider providers[] = {
        { "https://api.airplanes.live/v2/point/%.5f/%.5f/%d", "airplanes.live" },
        { "https://api.adsb.lol/v2/point/%.5f/%.5f/%d",      "adsb.lol"       },
        { "https://opendata.adsb.fi/api/v3/lat/%.5f/lon/%.5f/dist/%d", "adsb.fi" },
    };

    String body;
    bool fetched = false;

    for (auto &p : providers) {
        char url[256];
        // adsb.fi ma inny format URL (lat/lon/dist zamiast point/lat/lon/dist)
        if (strstr(p.urlFmt, "adsb.fi")) {
            snprintf(url, sizeof(url), p.urlFmt, HOME_LAT, HOME_LON, radiusNm);
        } else {
            snprintf(url, sizeof(url), p.urlFmt, HOME_LAT, HOME_LON, radiusNm);
        }

        Serial.printf("[ADSB] trying: %s\n", p.name);
        if (httpGetJson(url, ADSB_FI_USER_AGENT, ADSB_FI_TIMEOUT_MS, body)) {
            fetched = true;
            Serial.printf("[ADSB] OK od: %s\n", p.name);
            break;
        }
        Serial.printf("[ADSB] %s FAIL — next\n", p.name);
    }

    if (!fetched) {
        Serial.println("[ADSB] All sources unavailable");
        state.adsbAvailable = false;
        state.adsbFetching = false;
        return false;
    }

    DynamicJsonDocument doc(HTTP_RESPONSE_BUF_SIZE * 8);
    if (deserializeJson(doc, body)) {
        Serial.println("[ADSB] JSON parse FAILED");
        state.adsbAvailable = false;
        state.adsbFetching = false;
        return false;
    }

    // Obsługa różnych formatów — "ac" (Exchange/airplanes.live/adsb.lol) lub "aircraft" (dump1090)
    JsonArray ac = doc["ac"].as<JsonArray>();
    if (ac.isNull()) ac = doc["aircraft"].as<JsonArray>();

    int idx = 0;
    for (JsonVariant v : ac) {
        if (idx >= MAX_AIRCRAFT) break;

        AircraftState a;
        JsonObject obj = v.as<JsonObject>();

        copyField(obj["hex"]    .as<const char *>(), a.hex,      sizeof(a.hex));
        copyField(obj["flight"] .as<const char *>(), a.callsign, sizeof(a.callsign));
        copyField(obj["r"]      .as<const char *>(), a.reg,      sizeof(a.reg));
        copyField(obj["t"]      .as<const char *>(), a.type,     sizeof(a.type));
        copyField(obj["desc"]   .as<const char *>(), a.desc,     sizeof(a.desc));

        a.lat       = optDouble(obj["lat"]);
        a.lon       = optDouble(obj["lon"]);
        a.speedKt    = optDouble(obj["gs"]);
        a.trackDeg   = optDouble(obj["track"]);
        a.verticalRateFpm = optDouble(obj["baro_rate"]);
        a.onGround   = optBool(obj["ground"]);

        // alt_baro == "ground" → traktujemy jak brak wysokości (None w Pythonie).
        JsonVariant altV = obj["alt_baro"];
        const char *altStr = altV.as<const char *>();
        if (altStr && strcasecmp(altStr, "ground") == 0) {
            a.altitudeFt = NAN;
            a.onGround = true;
        } else {
            a.altitudeFt = optDouble(altV);
        }

        a.hasPosition = !isnan(a.lat) && !isnan(a.lon);
        if (a.hasPosition) {
            a.distanceKm = geo::distanceKm(HOME_LAT, HOME_LON, a.lat, a.lon);
            a.airportDistanceKm = geo::distanceKm(AIRPORT_LAT, AIRPORT_LON, a.lat, a.lon);
            a.bearingDeg = geo::bearingDeg(HOME_LAT, HOME_LON, a.lat, a.lon);
        } else {
            a.distanceKm = NAN;
            a.airportDistanceKm = NAN;
            a.bearingDeg = NAN;
        }
        a.seenMs = millis();

        state.aircraft[idx++] = a;
    }
    state.aircraftCount = idx;
    state.adsbVersion++;
    state.adsbAvailable = true;
    state.lastAdsBFetchMs = millis();
    state.adsbFetching = false;
    Serial.printf("[ADSB] OK — %d aircraft\n", state.aircraftCount);
    return true;
}
