#pragma once

#include "app_state.h"

// Przegląda pełną tablicę aircraft w AppState, klasyfikuje każdy rekord
// i wypełnia state.selectedFlight + state.bestEastCandidate.
// Funkcja jest idempotentna i może być wywoływana co każdy ADS-B fetch.
void flightSelectorUpdate(AppState &state);
