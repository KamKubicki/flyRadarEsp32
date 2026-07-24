#pragma once

#include "app_types.h"

// Wynik klasyfikacji pojedynczego samolotu.
// Jeśli "relevant == false", samolot nie powinien trafić na ekran główny,
// ale nadal może być widoczny na radarze (po settings.radarShowAll).
struct ClassificationResult {
    bool          accepted     = false;        // przeszedł filtry fizyczne (dystans/alt)
    FlightKind    kind         = FlightKind::Unknown;
    bool          isEastVisible = false;
    char          operation[32] = {0};
};

ClassificationResult flightFilterClassify(const AircraftState &a);

bool flightFilterIsCandidateRelevant(const ClassificationResult &r,
                                    bool showArrivals,
                                    bool showDepartures,
                                    VisibilitySide side);
