#include "http_client_helper.h"
#include <HTTPClient.h>
#include <WiFi.h>

bool httpGetJson(const char *url, const char *userAgent, int timeoutMs, String &body) {
    Serial.printf("[HTTP] WiFi: %d, GET %s\n", WiFi.status() == WL_CONNECTED, url);
    if (WiFi.status() != WL_CONNECTED) return false;

    HTTPClient http;
    http.begin(url);
    http.setTimeout(timeoutMs > 0 ? timeoutMs : 5000);
    if (userAgent && userAgent[0]) http.setUserAgent(userAgent);

    const int code = http.GET();
    Serial.printf("[HTTP] code=%d\n", code);
    if (code != 200) {
        http.end();
        return false;
    }

    body = http.getString();

    http.end();
    Serial.printf("[HTTP] body_len=%d\n", body.length());
    return body.length() > 0;
}
