#pragma once
#include "app_state.h"

void wifiBegin();           // inicjalizacja (wywoływana w setup())
void wifiProcess();         // obsługa portalu (wywoływana w loop() lub tasku)
bool wifiEnsureConnected();
bool wifiPortalActive();
const char *wifiLocalIP();
const char *wifiSSID();
const char *wifiStatusText();
void wifiResetCredentials();
