#include "weather_service.h"
#include "config.h"
#include "http_client_helper.h"
#include <ArduinoJson.h>
#include <stdio.h>
#include <string.h>

static const char *codeToText(int code) {
    switch (code) {
        case 0:  return "Slonecznie";
        case 1:  return "Przewaznie slonecznie";
        case 2:  return "Czesciowe zachmurzenie";
        case 3:  return "Pochmurno";
        case 45:
        case 48: return "Mgla";
        case 51:
        case 53:
        case 55: return "Mzawka";
        case 56:
        case 57: return "Szadz";
        case 61:
        case 63:
        case 65: return "Deszcz";
        case 66:
        case 67: return "Lodowy deszcz";
        case 71:
        case 73:
        case 75: return "Snieg";
        case 77: return "Ziarna sniegu";
        case 80:
        case 81:
        case 82: return "Przelotny deszcz";
        case 85:
        case 86: return "Przelotny snieg";
        case 95: return "Burza";
        default: return "?";
    }
}



bool weatherFetch(AppState &state) {
    Serial.println("[WEATHER] start");
    if (state.weatherFetching) { Serial.println("[WEATHER] skip — already fetching"); return false; }
    state.weatherFetching = true;
    state.lastWeatherFetchStartMs = millis();

    char url[512];
    snprintf(url, sizeof(url),
        "https://api.open-meteo.com/v1/forecast"
        "?latitude=%.4f&longitude=%.4f"
        "&current=temperature_2m,relative_humidity_2m,weather_code,wind_speed_10m"
        "&daily=weather_code,temperature_2m_max,temperature_2m_min,precipitation_sum,sunrise,sunset"
        "&forecast_days=3&timezone=auto&wind_speed_unit=ms",
        HOME_LAT, HOME_LON);

    String body;
    if (!httpGetJson(url, "FlyRadar/0.1", 10000, body)) {
        Serial.println("[WEATHER] httpGetJson FAILED");
        state.weatherAvailable = false;
        state.weatherFetching = false;
        return false;
    }

    DynamicJsonDocument doc(10 * 1024);
    DeserializationError err = deserializeJson(doc, body);
    if (err) {
        Serial.printf("[WEATHER] JSON parse FAILED: %s (body_len=%d)\n", err.c_str(), body.length());
        state.weatherAvailable = false;
        state.weatherFetching = false;
        return false;
    }
    if (!doc["current"].is<JsonObject>() || !doc["daily"].is<JsonObject>()) {
        Serial.println("[WEATHER] JSON missing fields");
        state.weatherAvailable = false;
        state.weatherFetching = false;
        return false;
    }

    JsonObject cur = doc["current"];
    state.weather.tempC       = cur["temperature_2m"]       | NAN;
    state.weather.humidity    = cur["relative_humidity_2m"] | -1;
    state.weather.windMs      = cur["wind_speed_10m"]       | NAN;
    state.weather.weatherCode = cur["weather_code"]         | -1;

    const char *txt = codeToText(state.weather.weatherCode);
    strncpy(state.weather.description, txt, sizeof(state.weather.description) - 1);
    snprintf(state.weather.icon, sizeof(state.weather.icon), "%d", state.weather.weatherCode);

    // Prognoza 3 dni
    JsonObject daily = doc["daily"];
    JsonArray wcodeArr  = daily["weather_code"].as<JsonArray>();
    JsonArray tmaxArr   = daily["temperature_2m_max"].as<JsonArray>();
    JsonArray tminArr   = daily["temperature_2m_min"].as<JsonArray>();
    JsonArray precipArr = daily["precipitation_sum"].as<JsonArray>();
    JsonArray timeArr   = daily["time"].as<JsonArray>();

    for (int i = 0; i < 3 && i < (int)timeArr.size(); ++i) {
        ForecastDay &fd = state.weather.forecast[i];
        fd.weatherCode = wcodeArr[i] | -1;
        fd.tempMin     = tminArr[i]  | NAN;
        fd.tempMax     = tmaxArr[i]  | NAN;
        fd.precip      = precipArr[i]| 0.0f;

        const char *ts = timeArr[i].as<const char *>();
        if (ts && strlen(ts) >= 10) {
            // ts = "2024-07-10" → copy last 5 chars "07-10"
            strncpy(fd.date, ts + 5, 5);
            fd.date[5] = '\0';
        }
    }

    JsonArray sunriseArr= daily["sunrise"].as<JsonArray>();
    JsonArray sunsetArr = daily["sunset"].as<JsonArray>();

    // Wschód/zachód słońca (dzisiaj = indeks 0), format "2026-07-10T04:42" → "04:42"
    if (sunriseArr.size() > 0) {
        const char *sr = sunriseArr[0].as<const char *>();
        if (sr && strlen(sr) >= 16)
            strncpy(state.weather.sunrise, sr + 11, 5);
    }
    if (sunsetArr.size() > 0) {
        const char *ss = sunsetArr[0].as<const char *>();
        if (ss && strlen(ss) >= 16)
            strncpy(state.weather.sunset, ss + 11, 5);
    }

    state.weatherAvailable = true;
    state.lastWeatherFetchMs = millis();
    state.weatherFetching = false;
    Serial.printf("[WEATHER] OK — temp=%.0f code=%d\n", state.weather.tempC, state.weather.weatherCode);
    return true;
}