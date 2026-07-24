#include "flight_route.h"
#include "http_client_helper.h"
#include <ArduinoJson.h>
#include <string.h>
#include <stdio.h>

namespace {

struct CacheEntry {
    char callsign[16] = {0};
    RouteInfo route;
    unsigned long fetchedMs = 0;
};

constexpr int CACHE_SIZE = 5;
constexpr unsigned long CACHE_TTL_MS = 600000; // 10 minut

CacheEntry s_cache[CACHE_SIZE];

// Zwraca indeks 0 — zawsze wstawia/przesuwa na czoło (LRU)
int cacheSlot(const char *callsign) {
    for (int i = 0; i < CACHE_SIZE; ++i) {
        if (strcmp(s_cache[i].callsign, callsign) == 0) {
            CacheEntry tmp = s_cache[i];
            for (int j = i; j > 0; --j) s_cache[j] = s_cache[j - 1];
            s_cache[0] = tmp;
            return 0;
        }
    }
    for (int i = CACHE_SIZE - 1; i > 0; --i) s_cache[i] = s_cache[i - 1];
    s_cache[0] = CacheEntry();
    return 0;
}

// Kopiuj pole JSON do bufora — próbuje priorytetowo IATA, fallback ICAO
static void copyAirportCode(JsonObject &ap, char *buf, size_t bufSize) {
    const char *iata = ap["iata_code"].as<const char *>();
    const char *icao = ap["icao_code"].as<const char *>();
    const char *src  = (iata && iata[0]) ? iata : icao;
    if (src) strncpy(buf, src, bufSize - 1);
}

static void copyAirportName(JsonObject &ap, char *buf, size_t bufSize) {
    const char *name = ap["name"].as<const char *>();
    if (name) strncpy(buf, name, bufSize - 1);
}

} // namespace

void flightRouteCacheClear() {
    for (auto &e : s_cache) e = CacheEntry();
}

bool flightRouteFetch(const char *callsign, RouteInfo &route) {
    if (!callsign || strlen(callsign) < 3) {
        route = RouteInfo();
        return false;
    }

    // Sprawdź cache
    int slot = cacheSlot(callsign);
    if (strcmp(s_cache[slot].callsign, callsign) == 0 &&
        s_cache[slot].fetchedMs != 0 &&
        (millis() - s_cache[slot].fetchedMs) < CACHE_TTL_MS) {
        route = s_cache[slot].route;
        return route.valid;
    }

    auto saveCache = [&](const RouteInfo &ri) {
        strncpy(s_cache[slot].callsign, callsign, sizeof(s_cache[slot].callsign) - 1);
        s_cache[slot].route     = ri;
        s_cache[slot].fetchedMs = millis();
    };

    // ── Fallback chain: adsbdb → hexdb.io ───────────────────────────────────
    // Źródło 1: api.adsbdb.com (crowd-sourced, dobre pokrycie)
    {
        char url[128];
        snprintf(url, sizeof(url), "https://api.adsbdb.com/v0/callsign/%s", callsign);
        Serial.printf("[ROUTE] adsbdb: %s\n", callsign);

        String body;
        if (httpGetJson(url, "FlyRadar/0.1", 5000, body)) {
            DynamicJsonDocument doc(4096);
            if (!deserializeJson(doc, body)) {
                JsonObject fr = doc["response"]["flightroute"];
                if (!fr.isNull()) {
                    RouteInfo ri;
                    JsonObject origin = fr["origin"];
                    JsonObject dest   = fr["destination"];
                    if (!origin.isNull()) {
                        const char *iata = origin["iata_code"].as<const char *>();
                        const char *icao = origin["icao_code"].as<const char *>();
                        const char *name = origin["name"].as<const char *>();
                        if (iata && iata[0]) strncpy(ri.origin, iata, sizeof(ri.origin)-1);
                        else if (icao)       strncpy(ri.origin, icao, sizeof(ri.origin)-1);
                        if (name) strncpy(ri.originName, name, sizeof(ri.originName)-1);
                    }
                    if (!dest.isNull()) {
                        const char *iata = dest["iata_code"].as<const char *>();
                        const char *icao = dest["icao_code"].as<const char *>();
                        const char *name = dest["name"].as<const char *>();
                        if (iata && iata[0]) strncpy(ri.dest, iata, sizeof(ri.dest)-1);
                        else if (icao)       strncpy(ri.dest, icao, sizeof(ri.dest)-1);
                        if (name) strncpy(ri.destName, name, sizeof(ri.destName)-1);
                    }
                    ri.valid = (ri.origin[0] != '\0' && ri.dest[0] != '\0');
                    if (ri.valid) {
                        Serial.printf("[ROUTE] adsbdb OK: %s→%s\n", ri.origin, ri.dest);
                        route = ri;
                        saveCache(ri);
                        return true;
                    }
                }
            }
        }
        Serial.println("[ROUTE] adsbdb failed — trying hexdb");
    }

    // Źródło 2: hexdb.io (callsign lookup — prostszy format)
    {
        char url[128];
        snprintf(url, sizeof(url), "https://hexdb.io/api/v1/route/%s", callsign);
        Serial.printf("[ROUTE] hexdb: %s\n", callsign);

        String body;
        if (httpGetJson(url, "FlyRadar/0.1", 5000, body)) {
            DynamicJsonDocument doc(1024);
            if (!deserializeJson(doc, body)) {
                // hexdb zwraca: {"route": "WAW-LHR", "callsign": "LOT283"}
                const char *routeStr = doc["route"].as<const char *>();
                if (routeStr && strlen(routeStr) >= 3) {
                    // Format: "ORIG-DEST" np. "WAW-LHR"
                    RouteInfo ri;
                    char buf[32]; strncpy(buf, routeStr, sizeof(buf)-1);
                    char *sep = strchr(buf, '-');
                    if (sep) {
                        *sep = '\0';
                        strncpy(ri.origin, buf,    sizeof(ri.origin)-1);
                        strncpy(ri.dest,   sep+1,  sizeof(ri.dest)-1);
                        ri.valid = true;
                        Serial.printf("[ROUTE] hexdb OK: %s→%s\n", ri.origin, ri.dest);
                        route = ri;
                        saveCache(ri);
                        return true;
                    }
                }
            }
        }
        Serial.println("[ROUTE] hexdb failed — no route found");
    }

    // Nothing found
    RouteInfo empty;
    route = empty;
    saveCache(empty);
    return false;
}
