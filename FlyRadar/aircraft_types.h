#pragma once
// Lookup table: ICAO type code → "Manufacturer Model"
// Źródło: vradarserver/standing-data (CC0), top 80 typów handlowych
// Użycie: aircraftTypeName("A21N") → "Airbus A-321neo"

const char *aircraftTypeName(const char *icaoType);
