#include "flight_selector.h"
#include "flight_filter.h"
#include "config.h"
#include <string.h>
#include <stdio.h>
#include <math.h>

namespace {

static const char *airlineByPrefix(const char *callsign) {
    if (!callsign || strlen(callsign) < 3) return nullptr;
    char pre[4] = {callsign[0], callsign[1], callsign[2], 0};
    for (int i = 0; i < 3; ++i)
        if (pre[i] >= 'a' && pre[i] <= 'z') pre[i] -= 32;

    static const struct { const char *pre; const char *name; } map[] = {
        {"LOT", "LOT Polish Airlines"}, {"RYR", "Ryanair"},
        {"EZY", "easyJet"},            {"WZZ", "Wizz Air"},
        {"DLH", "Lufthansa"},          {"UAE", "Emirates"},
        {"KLM", "KLM"},                {"AFR", "Air France"},
        {"BAW", "British Airways"},    {"THY", "Turkish Airlines"},
        {"SWR", "Swiss"},              {"AUA", "Austrian Airlines"},
        {"QTR", "Qatar Airways"},      {"FIN", "Finnair"},
        {"SAS", "Scandinavian Airlines"}, {"IBE", "Iberia"},
        {"CPA", "Cathay Pacific"},     {"ENT", "Enter Air"},
        {"EXS", "Jet2"},               {"TAP", "TAP Portugal"},
        {"RYA", "Ryanair"},            {"JGO", "Jet2"},
        {"TVP", "Travel Service"},     {"PLL", "LOT Polish Airlines"},
    };
    for (auto &m : map)
        if (strcmp(pre, m.pre) == 0) return m.name;
    return nullptr;
}

// ============================================================================
// Scoring — priorytety dla rodziców w Węgrzcach:
//
// 1. WIDOCZNOŚĆ Z OKNA (najważniejsze):
//    - Wschód (20-160°):       +50  ← okno na wschód, najlepszy widok
//    - Południe (140-220°):    +25  ← widok przez balkon/okno
//    - Zachód (>220° lub <20°): -20  ← trudny do zobaczenia
//
// 2. RODZAJ LOTU:
//    - Lądowanie:   +35  ← powolne, niskie, długo widoczne
//    - Start:       +25
//    - Przelot:     -20  ← szybko przelatuje, mały bonus
//
// 3. BLISKOŚĆ DOMU (widoczność): +20 proporcjonalnie
//
// 4. WYSOKOŚĆ (niżej = lepiej widoczny): +15 jeśli <3000ft
//
// 5. KARA ZA "prawie wylądował":
//    Jeśli lądowanie + dystans_od_EPKK < 2km → sam znika za budynkami → -40
//    (Lepiej pokazywać następny samolot który jeszcze będzie widoczny)
// ============================================================================

// Sektor widzialności — szerszy niż "east only"
static int visibilityScore(double bearingDeg) {
    if (isnan(bearingDeg)) return 0;

    // Normalizuj do 0-360
    while (bearingDeg < 0)   bearingDeg += 360.0;
    while (bearingDeg >= 360) bearingDeg -= 360.0;

    // Wschód (20-160°) — najlepszy widok z okna
    if (bearingDeg >= 20.0 && bearingDeg <= 160.0) return 50;

    // Południe/południe-wschód (160-200°) — dobry widok
    if (bearingDeg > 160.0 && bearingDeg <= 200.0) return 30;

    // Południe-zachód (200-240°) — umiarkowany widok
    if (bearingDeg > 200.0 && bearingDeg <= 240.0) return 10;

    // Zachód/północ — słaby widok
    return -20;
}

constexpr int SCORE_ARRIVAL       = 35;
constexpr int SCORE_DEPARTURE     = 25;
constexpr int SCORE_NEAR          = 20;   // max jeśli distanceKm=0
constexpr int SCORE_LOW_ALT       = 15;   // <3000ft
constexpr int SCORE_TRANSIT       = -20;
constexpr int SCORE_ALMOST_LANDED = -40;  // za blisko EPKK przy lądowaniu

} // namespace

void flightSelectorUpdate(AppState &state) {
    if (state.aircraftCount == 0) return;

    state.hasSelectedFlight      = false;
    state.hasBestEastCandidate   = false;

    const AppSettings &s = state.settings;
    FlightCandidate bestEast;
    FlightCandidate bestOverall;

    for (int i = 0; i < state.aircraftCount; ++i) {
        const AircraftState &a = state.aircraft[i];

        ClassificationResult r = flightFilterClassify(a);

        FlightCandidate cand;
        cand.aircraft      = a;
        cand.kind          = r.kind;
        cand.isEastVisible = r.isEastVisible;
        strncpy(cand.operation, r.operation, sizeof(cand.operation) - 1);

        const char *al = airlineByPrefix(a.callsign);
        if (al) strncpy(cand.airline, al, sizeof(cand.airline) - 1);

        if (!flightFilterIsCandidateRelevant(r, s.showArrivals, s.showDepartures, s.visibleSide)) {
            cand.isRelevant = false;
        } else {
            cand.isRelevant = true;

            int score = 0;

            // 1. Widoczność z okna (bearing od domu)
            score += visibilityScore(a.bearingDeg);

            // 2. Rodzaj lotu
            if      (cand.kind == FlightKind::Arrival)   score += SCORE_ARRIVAL;
            else if (cand.kind == FlightKind::Departure)  score += SCORE_DEPARTURE;
            else if (cand.kind == FlightKind::Transit)    score += SCORE_TRANSIT;

            // 3. Bliskość domu (proporcjonalnie: bliżej = więcej punktów)
            if (!isnan(a.distanceKm)) {
                double prox = SCORE_NEAR * (MAX_HOME_DISTANCE_KM - a.distanceKm)
                              / MAX_HOME_DISTANCE_KM;
                score += (int)fmax(0.0, prox);
            }

            // 4. Niska wysokość = lepiej widoczny
            if (!isnan(a.altitudeFt) && a.altitudeFt < 3000.0)
                score += SCORE_LOW_ALT;

            // 5. Kara za "prawie wylądował" — lądujący bardzo blisko EPKK
            // (dystans od lotniska < 2km) → wkrótce zniknie za budynkami/drzewami
            if (cand.kind == FlightKind::Arrival
                && !isnan(a.airportDistanceKm)
                && a.airportDistanceKm < 2.0) {
                score += SCORE_ALMOST_LANDED;
            }

            cand.score = score;

            // Aktualizuj najlepszego ogólnego
            if (!state.hasSelectedFlight || score > bestOverall.score) {
                bestOverall = cand;
                state.hasSelectedFlight = true;
            }
        }

        // Najlepszy wschodni (tylko relevant + widoczny od wschodu)
        if (cand.isEastVisible && cand.isRelevant &&
            (!state.hasBestEastCandidate || cand.score > bestEast.score)) {
            bestEast = cand;
            state.hasBestEastCandidate = true;
        }
    }

    // Zachowaj trasę jeśli ten sam samolot wybrany ponownie
    auto preserveRoute = [](RouteInfo &dst, const RouteInfo &old) {
        if (old.valid) dst = old;
    };

    if (state.hasSelectedFlight) {
        if (strcmp(bestOverall.aircraft.hex, state.selectedFlight.aircraft.hex) == 0)
            preserveRoute(bestOverall.route, state.selectedFlight.route);
        state.selectedFlight = bestOverall;
    }
    if (state.hasBestEastCandidate) {
        if (strcmp(bestEast.aircraft.hex, state.bestEastCandidate.aircraft.hex) == 0)
            preserveRoute(bestEast.route, state.bestEastCandidate.route);
        state.bestEastCandidate = bestEast;
    }
}
