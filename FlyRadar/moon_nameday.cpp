#include "moon_nameday.h"
#include <math.h>

// ============================================================================
// Faza księżyca — algorytm Meeus "Astronomical Algorithms" rozdział 49
// Dokładność: < 1 dzień dla dat ±100 lat od J2000
// ============================================================================

// Numer dnia juliańskiego z daty kalendarzowej
static double toJD(int year, int month, int day) {
    if (month <= 2) { year--; month += 12; }
    int A = year / 100;
    int B = 2 - A + A / 4;
    return (int)(365.25 * (year + 4716))
         + (int)(30.6001 * (month + 1))
         + day + B - 1524.5;
}

float moonPhase(int year, int month, int day) {
    double JD = toJD(year, month, day);
    // Referencyjna pełnia: 2451550.1 JD = 6 sty 2000 18:14 UTC
    double daysSinceNew = JD - 2451550.1;
    double synodic = 29.53058867;
    double phase = fmod(daysSinceNew / synodic, 1.0);
    if (phase < 0) phase += 1.0;
    return (float)phase;
}

const char *moonPhaseName(float phase) {
    // Tylko ASCII — fonty na wyświetlaczu nie mają polskich liter
    if (phase < 0.03 || phase > 0.97) return "Now";
    if (phase < 0.22)                 return "Serpek roslnie";
    if (phase < 0.28)                 return "I kwadra";
    if (phase < 0.47)                 return "Garb roslnie";
    if (phase < 0.53)                 return "Pelnia";
    if (phase < 0.72)                 return "Garb maleje";
    if (phase < 0.78)                 return "III kwadra";
    return "Serpek maleje";
}

// ============================================================================
// daysToFullMoon — ile pełnych dni kalendarzowych do następnej pełni
//
// Metoda: wyznacz numer synodyczny k dla następnej pełni po dzisiejszym JD,
// oblicz dokładny JD pełni, zwróć ceil(JD_pelni - JD_dzis).
//
// Wzór Meeus (rozdział 49):
//   k = floor(synodic_age + 0.5) + 0.5  (0.5 = pełnia)
//   JDE = 2451550.09766 + 29.530588861 * k + ...
// ============================================================================
int daysToFullMoon(int year, int month, int day) {
    double JD_now = toJD(year, month, day);

    // Przybliżony numer synodyczny (licząc od J2000 nów)
    double daysSinceRef = JD_now - 2451550.1;
    double k_approx = daysSinceRef / 29.53058867;

    // Następna pełnia: k musi kończyć się na .5
    // Obecna faza w synodach: fractional part of k_approx
    // Zaokrąglamy k do .5 w przyszłości
    double k_full = floor(k_approx) + 0.5;
    if (k_full <= k_approx) k_full += 1.0;  // musi być w przyszłości

    // Dokładny JD pełni (wzór Meeus uproszczony — błąd < 2 minuty)
    double T = k_full / 1236.85;  // Julian centuries
    double JDE = 2451550.09766
               + 29.530588861 * k_full
               + 0.00015437 * T * T
               - 0.000000150 * T * T * T
               + 0.00000000073 * T * T * T * T;

    // Korekty słoneczno-księżycowe (najważniejsze terminy)
    double M  = 2.5534 + 29.10535670 * k_full;   // anomalia Słońca
    double Mc = 201.5643 + 385.81693528 * k_full; // anomalia Księżyca
    M  = fmod(M,  360.0) * M_PI / 180.0;
    Mc = fmod(Mc, 360.0) * M_PI / 180.0;
    JDE += -0.40614 * sin(Mc)
           + 0.17302 * sin(M)
           + 0.01614 * sin(2*Mc)
           + 0.01043 * sin(2*Mc)
           - 0.01183 * sin(Mc + M);

    // Ile nocy od dziś do pełni (jak Google: "9 nocy" = 9 wieczorów)
    // int(diff - 0.5) daje liczbę nocy: pełnia 29 lip wieczorem, dziś 20 lip rano = 9
    double diff = JDE - JD_now;
    int days = (int)(diff - 0.5);
    if (days < 0) days = 0;
    return days;
}
