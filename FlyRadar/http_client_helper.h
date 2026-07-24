#pragma once

#include <Arduino.h>

// Prosty wrapper HTTP GET. Zwraca true przy 200, wypełnia `body` (PSRAM-free).
// Zwraca false jeśli timeout / błąd / puste ciało.
bool httpGetJson(const char *url, const char *userAgent, int timeoutMs, String &body);
