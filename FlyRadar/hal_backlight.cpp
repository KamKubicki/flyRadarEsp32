#include "hal_backlight.h"
#include "config.h"
#include <Arduino.h>
#include <math.h>

namespace {
int s_percent = BACKLIGHT_DEFAULT_PCT;
constexpr uint8_t BACKLIGHT_PWM_CH = 0;
}

static int clampPct(int pct) {
    if (pct < 0) return 0;
    if (pct > 100) return 100;
    return pct;
}

void halBacklightSetPercent(int pct) {
    s_percent = clampPct(pct);
#ifdef TFT_BL
    const int maxDuty = (1 << BACKLIGHT_PWM_RES_BITS) - 1;  // 255

    // Ten panel LED wymaga minimalnego progu duty żeby cokolwiek było widać.
    // Mapujemy suwak 0-100% na zakres duty MIN_DUTY..255 liniowo.
    // MIN_DUTY=80 (~31%) = minimalny widoczny poziom dla JC8048W550
    // Suwak 0% = przyciemniony ale widoczny, 100% = pełna jasność
    constexpr int MIN_DUTY = 80;
    int duty = MIN_DUTY + (int)((maxDuty - MIN_DUTY) * s_percent / 100);
    if (duty < 0)        duty = 0;
    if (duty > maxDuty)  duty = maxDuty;
    ledcWrite(BACKLIGHT_PWM_CH, duty);
#endif
}

void halBacklightBegin() {
#ifdef TFT_BL
    ledcSetup(BACKLIGHT_PWM_CH, BACKLIGHT_PWM_FREQ_HZ, BACKLIGHT_PWM_RES_BITS);
    ledcAttachPin(TFT_BL, BACKLIGHT_PWM_CH);
    halBacklightSetPercent(s_percent);
#endif
}

void halBacklightSleep() {
#ifdef TFT_BL
    ledcWrite(BACKLIGHT_PWM_CH, 0);
#endif
}

void halBacklightWake() {
#ifdef TFT_BL
    halBacklightSetPercent(s_percent);
#endif
}