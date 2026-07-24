#pragma once

#include <Arduino.h>

struct GeoPoint {
    double lat = 0.0;
    double lon = 0.0;
};

struct AircraftState {
    char        hex[16]      = {0};
    char        callsign[16] = {0};
    char        reg[16]      = {0};
    char        type[16]     = {0};
    char        desc[40]     = {0};

    double      lat          = NAN;
    double      lon          = NAN;
    double      altitudeFt   = NAN;
    double      speedKt      = NAN;
    double      trackDeg     = NAN;
    double      verticalRateFpm = NAN;

    bool        onGround     = false;
    bool        hasPosition  = false;
    unsigned long seenMs     = 0;

    double      distanceKm       = NAN;
    double      airportDistanceKm = NAN;
    double      bearingDeg       = NAN;
};

struct ForecastDay {
    char   date[6] = {0};   // "Pon", "Wt", …
    float  tempMin = NAN;
    float  tempMax = NAN;
    float  precip  = NAN;
    int    weatherCode = -1;
};

struct WeatherCurrent {
    float   tempC          = NAN;
    int     humidity       = -1;
    float   windMs         = NAN;
    int     weatherCode    = -1;
    char    description[24] = {0};
    char    icon[4]        = {0};
    char    sunrise[6]     = {0};   // "HH:MM"
    char    sunset[6]      = {0};   // "HH:MM"
    ForecastDay forecast[3];
};

enum class FlightKind : uint8_t {
    Unknown,
    Arrival,
    Departure,
    Transit,
};

enum class VisibilitySide : uint8_t {
    Any,
    EastOnly,
};

struct RouteInfo {
    char origin[8]      = {0};
    char dest[8]        = {0};
    char originName[48] = {0};
    char destName[48]   = {0};
    bool valid          = false;
};

struct FlightCandidate {
    AircraftState aircraft;
    FlightKind    kind        = FlightKind::Unknown;
    bool          isRelevant  = false;
    bool          isEastVisible = false;
    int           score       = 0;
    char          operation[32] = {0};
    char          airline[32]   = {0};
    RouteInfo     route;
};