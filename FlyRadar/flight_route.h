#pragma once

#include "app_types.h"

void flightRouteCacheClear();

bool flightRouteFetch(const char *callsign, RouteInfo &route);
