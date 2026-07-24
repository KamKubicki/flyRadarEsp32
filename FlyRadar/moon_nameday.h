#pragma once
#include <stdint.h>

// Faza księżyca (0.0 = nów, 0.5 = pełnia)
float moonPhase(int year, int month, int day);

// Nazwa fazy po polsku
const char *moonPhaseName(float phase);

// Ile dni do następnej pełni (0 = dziś pełnia, -1 = nie znaleziono w 30 dniach)
int daysToFullMoon(int year, int month, int day);
