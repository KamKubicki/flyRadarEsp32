#include "aircraft_types.h"
#include <string.h>

// ============================================================================
// Wbudowana tabela ICAO type code → pełna nazwa
// Top ~80 typów pokrywa >95% ruchu handlowego
// ============================================================================

static const struct { const char *code; const char *name; } s_types[] = {
    // ── Airbus Narrow-body ──────────────────────────────────────────────────
    {"A318", "Airbus A-318"},
    {"A319", "Airbus A-319"},
    {"A319", "Airbus A-319"},
    {"A320", "Airbus A-320"},
    {"A321", "Airbus A-321"},
    {"A19N", "Airbus A-319neo"},
    {"A20N", "Airbus A-320neo"},
    {"A21N", "Airbus A-321neo"},
    // ── Airbus Wide-body ────────────────────────────────────────────────────
    {"A306", "Airbus A-300-600"},
    {"A30B", "Airbus A-300B"},
    {"A310", "Airbus A-310"},
    {"A332", "Airbus A-330-200"},
    {"A333", "Airbus A-330-300"},
    {"A338", "Airbus A-330-800neo"},
    {"A339", "Airbus A-330-900neo"},
    {"A342", "Airbus A-340-200"},
    {"A343", "Airbus A-340-300"},
    {"A345", "Airbus A-340-500"},
    {"A346", "Airbus A-340-600"},
    {"A359", "Airbus A-350-900"},
    {"A35K", "Airbus A-350-1000"},
    {"A388", "Airbus A-380-800"},
    // ── Boeing Narrow-body ──────────────────────────────────────────────────
    {"B736", "Boeing 737-600"},
    {"B737", "Boeing 737-700"},
    {"B738", "Boeing 737-800"},
    {"B739", "Boeing 737-900"},
    {"B37M", "Boeing 737 MAX 7"},
    {"B38M", "Boeing 737 MAX 8"},
    {"B39M", "Boeing 737 MAX 9"},
    {"B3XM", "Boeing 737 MAX 10"},
    {"B752", "Boeing 757-200"},
    {"B753", "Boeing 757-300"},
    // ── Boeing Wide-body ────────────────────────────────────────────────────
    {"B742", "Boeing 747-200"},
    {"B743", "Boeing 747-300"},
    {"B744", "Boeing 747-400"},
    {"B748", "Boeing 747-8"},
    {"B762", "Boeing 767-200"},
    {"B763", "Boeing 767-300"},
    {"B764", "Boeing 767-400"},
    {"B772", "Boeing 777-200"},
    {"B77L", "Boeing 777-200LR"},
    {"B773", "Boeing 777-300"},
    {"B77W", "Boeing 777-300ER"},
    {"B779", "Boeing 777X-9"},
    {"B788", "Boeing 787-8 Dreamliner"},
    {"B789", "Boeing 787-9 Dreamliner"},
    {"B78X", "Boeing 787-10 Dreamliner"},
    // ── Embraer ─────────────────────────────────────────────────────────────
    {"E135", "Embraer ERJ-135"},
    {"E145", "Embraer ERJ-145"},
    {"E170", "Embraer E-170"},
    {"E175", "Embraer E-175"},
    {"E190", "Embraer E-190"},
    {"E195", "Embraer E-195"},
    {"E290", "Embraer E-190-E2"},
    {"E295", "Embraer E-195-E2"},
    // ── Bombardier/CRJ ──────────────────────────────────────────────────────
    {"CRJ2", "Bombardier CRJ-200"},
    {"CRJ7", "Bombardier CRJ-700"},
    {"CRJ9", "Bombardier CRJ-900"},
    {"CRJX", "Bombardier CRJ-1000"},
    {"DH8A", "DHC-8-100 Dash 8"},
    {"DH8D", "DHC-8-400 Dash 8"},
    // ── ATR ─────────────────────────────────────────────────────────────────
    {"AT43", "ATR 42-300"},
    {"AT45", "ATR 42-500"},
    {"AT72", "ATR 72-200"},
    {"AT73", "ATR 72-500"},
    {"AT76", "ATR 72-600"},
    // ── Turboprops / Regionalne ─────────────────────────────────────────────
    {"SF34", "Saab SF-340"},
    {"F100", "Fokker 100"},
    {"F70",  "Fokker 70"},
    {"F50",  "Fokker 50"},
    // ── Cargosy ─────────────────────────────────────────────────────────────
    {"MD11", "McDonnell Douglas MD-11"},
    {"DC10", "Douglas DC-10"},
    {"IL76", "Ilyushin Il-76"},
    {"AN12", "Antonov An-12"},
    {"AN26", "Antonov An-26"},
    {"AN72", "Antonov An-72"},
    // ── Business/VIP ────────────────────────────────────────────────────────
    {"C56X", "Cessna Citation Excel"},
    {"C680", "Cessna Citation Sovereign"},
    {"C68A", "Cessna Citation Latitude"},
    {"GL5T", "Bombardier Global 5000"},
    {"GLEX", "Bombardier Global Express"},
    {"GLF5", "Gulfstream G-V"},
    {"GLF6", "Gulfstream G600/650"},
    {"F2TH", "Dassault Falcon 2000"},
    {"F900", "Dassault Falcon 900"},
    {"F7X",  "Dassault Falcon 7X"},
    {"LJ35", "Learjet 35"},
    {"LJ60", "Learjet 60"},
    {"CL30", "Bombardier Challenger 300"},
    {"CL35", "Bombardier Challenger 350"},
    {"CL60", "Bombardier Challenger 600"},
};

static constexpr int N_TYPES = sizeof(s_types) / sizeof(s_types[0]);

const char *aircraftTypeName(const char *icaoType) {
    if (!icaoType || !icaoType[0]) return nullptr;
    for (int i = 0; i < N_TYPES; ++i)
        if (strcmp(s_types[i].code, icaoType) == 0)
            return s_types[i].name;
    return nullptr;  // nie znaleziono — wyświetl oryginalny kod
}
