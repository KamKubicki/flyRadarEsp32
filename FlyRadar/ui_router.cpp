#include "ui_router.h"
#include "ui_home.h"
#include "ui_radar.h"
#include "ui_weather.h"
#include "ui_settings.h"
#include "ui_theme.h"
#include "app_state.h"
#include <Arduino.h>
#include <lvgl.h>
#include <time.h>
#include <stdio.h>

namespace {

UiScreenId s_current = UiScreenId::Weather;
AppState  *s_state   = nullptr;

// Osobne kontenery: status bar (gora) i nav bar (dol) — przenoszone osobno
// (bez wspolnego full-screen parenta, ktory blokowal dotyk w Settings)
lv_obj_t *s_statusBar = nullptr;
lv_obj_t *s_navBar    = nullptr;
lv_obj_t *s_clockLabel = nullptr;
lv_obj_t *s_dateLabel  = nullptr;
lv_obj_t *s_wifiIcon   = nullptr;
lv_obj_t *s_ntpIcon    = nullptr;

lv_obj_t *s_navBtns[4] = {};
lv_obj_t *s_navLabels[4] = {};

char s_lastClockStr[6] = "";  // "HH:MM\0"
char s_lastDateStr[6]  = "";  // "DD.MM\0"
bool s_lastWifi = false;
bool s_lastNtp  = false;

void setNavActive(UiScreenId id) {
    int activeIdx = (int)id;
    for (int i = 0; i < 4; ++i) {
        if (!s_navBtns[i]) continue;
        if (i == activeIdx) {
            lv_obj_set_style_bg_color(s_navBtns[i], uiTheme::ACCENT, 0);
            lv_obj_set_style_text_color(s_navLabels[i], uiTheme::TEXT, 0);
        } else {
            lv_obj_set_style_bg_color(s_navBtns[i], lv_palette_darken(LV_PALETTE_GREY, 3), 0);
            lv_obj_set_style_text_color(s_navLabels[i], uiTheme::MUTED, 0);
        }
    }
}

void showInternal(UiScreenId id) {
    s_current = id;
    setNavActive(id);
    // Paski są na lv_layer_top() — nie ruszają się przy zmianie ekranów
    switch (id) {
        case UiScreenId::Home:     uiHomeShow();     break;
        case UiScreenId::Radar:    uiRadarShow();    break;
        case UiScreenId::Weather:  uiWeatherShow();  break;
        case UiScreenId::Settings: uiSettingsShow(); break;
    }
}

void navBtnCb(lv_event_t *e) {
    const int id = (intptr_t)lv_event_get_user_data(e);
    if (s_state) appStateMarkInteraction(*s_state);
    switch (id) {
        case 0: uiRouterShow(UiScreenId::Home);      break;
        case 1: uiRouterShow(UiScreenId::Radar);     break;
        case 2: uiRouterShow(UiScreenId::Weather);   break;
        case 3: uiRouterShow(UiScreenId::Settings);  break;
    }
}

} // namespace

void uiNavBuild() {
    if (s_statusBar) return;

    // ── lv_layer_top() — globalny overlay zawsze ponad ekranami ──────────────
    // KLUCZOWE: musimy ustawić overlay jako przezroczysty, inaczej zakryje ekran
    lv_obj_t *overlay = lv_layer_top();
    lv_obj_set_style_bg_opa(overlay, LV_OPA_TRANSP, 0);  // ← fix "czarny ekran"
    lv_obj_clear_flag(overlay, LV_OBJ_FLAG_CLICKABLE);   // dotyk przechodzi niżej

    // ---- Status bar (góra, 40px) ----
    s_statusBar = lv_obj_create(overlay);
    lv_obj_set_pos(s_statusBar, 0, 0);
    lv_obj_set_size(s_statusBar, 800, 40);
    lv_obj_set_style_bg_color(s_statusBar, uiTheme::BAR, 0);
    lv_obj_set_style_border_width(s_statusBar, 0, 0);
    lv_obj_set_style_pad_all(s_statusBar, 0, 0);
    lv_obj_set_style_radius(s_statusBar, 0, 0);
    lv_obj_set_style_shadow_width(s_statusBar, 0, 0);

    s_clockLabel = lv_label_create(s_statusBar);
    lv_obj_set_pos(s_clockLabel, 12, 4);
    lv_obj_set_style_text_color(s_clockLabel, uiTheme::TEXT, 0);
    lv_label_set_text(s_clockLabel, "--:--");

    s_dateLabel = lv_label_create(s_statusBar);
    lv_obj_set_pos(s_dateLabel, 12, 24);
    lv_obj_set_style_text_color(s_dateLabel, uiTheme::MUTED, 0);
    lv_label_set_text(s_dateLabel, "");

    s_wifiIcon = lv_label_create(s_statusBar);
    lv_obj_set_pos(s_wifiIcon, 720, 6);
    lv_obj_set_style_text_color(s_wifiIcon, uiTheme::MUTED, 0);
    lv_label_set_text(s_wifiIcon, "");

    s_ntpIcon = lv_label_create(s_statusBar);
    lv_obj_set_pos(s_ntpIcon, 760, 6);
    lv_obj_set_style_text_color(s_ntpIcon, uiTheme::MUTED, 0);
    lv_label_set_text(s_ntpIcon, "");

    // ---- Nav bar (dół, 50px) ----
    s_navBar = lv_obj_create(overlay);
    lv_obj_set_pos(s_navBar, 0, 430);
    lv_obj_set_size(s_navBar, 800, 50);
    lv_obj_set_style_bg_color(s_navBar, uiTheme::BAR, 0);
    lv_obj_set_style_border_width(s_navBar, 0, 0);
    lv_obj_set_style_pad_all(s_navBar, 0, 0);
    lv_obj_set_style_radius(s_navBar, 0, 0);
    lv_obj_set_style_shadow_width(s_navBar, 0, 0);

    static const char *labels[] = { "Lot", "Radar", "Pogoda", "Ustaw" };
    const int w = 800 / 4;
    for (int i = 0; i < 4; ++i) {
        lv_obj_t *btn = lv_btn_create(s_navBar);
        lv_obj_set_size(btn, w - 4, 44);
        lv_obj_set_pos(btn, i * w + 2, 3);
        lv_obj_set_style_border_width(btn, 0, 0);
        lv_obj_set_style_radius(btn, 4, 0);

        lv_obj_t *lbl = lv_label_create(btn);
        lv_label_set_text(lbl, labels[i]);
        lv_obj_center(lbl);

        s_navBtns[i] = btn;
        s_navLabels[i] = lbl;

        lv_obj_add_event_cb(btn, navBtnCb, LV_EVENT_CLICKED, (void *)(intptr_t)i);
    }

    setNavActive(UiScreenId::Weather);
}

void uiBarUpdate(const AppState &state) {
    if (!s_clockLabel) return;

    struct tm ti;
    if (state.timeSynced && getLocalTime(&ti, 0)) {
        char buf[12];
        snprintf(buf, sizeof(buf), "%02d:%02d", ti.tm_hour, ti.tm_min);
        if (strcmp(buf, s_lastClockStr) != 0) {
            strncpy(s_lastClockStr, buf, sizeof(s_lastClockStr) - 1);
            lv_label_set_text(s_clockLabel, buf);
        }

        snprintf(buf, sizeof(buf), "%02d.%02d", ti.tm_mday, ti.tm_mon + 1);
        if (strcmp(buf, s_lastDateStr) != 0) {
            strncpy(s_lastDateStr, buf, sizeof(s_lastDateStr) - 1);
            lv_label_set_text(s_dateLabel, buf);
        }
    } else {
        if (s_lastClockStr[0] != '\0') {
            s_lastClockStr[0] = '\0';
            lv_label_set_text(s_clockLabel, "--:--");
        }
        if (s_lastDateStr[0] != '\0') {
            s_lastDateStr[0] = '\0';
            lv_label_set_text(s_dateLabel, "");
        }
    }

    if (s_wifiIcon && state.wifiConnected != s_lastWifi) {
        s_lastWifi = state.wifiConnected;
        lv_label_set_text(s_wifiIcon, state.wifiConnected ? "WiFi" : "");
        lv_obj_set_style_text_color(s_wifiIcon,
            state.wifiConnected ? uiTheme::EAST_OK : uiTheme::WEST_OFF, 0);
    }
    if (s_ntpIcon && state.timeSynced != s_lastNtp) {
        s_lastNtp = state.timeSynced;
        lv_label_set_text(s_ntpIcon, state.timeSynced ? "NTP" : "");
        lv_obj_set_style_text_color(s_ntpIcon,
            state.timeSynced ? uiTheme::EAST_OK : uiTheme::WEST_OFF, 0);
    }

    // Powiadomienie o locie w pasku statusu
    (void)state; // będzie uzyte w przyszlosci
}

void uiRouterBegin(AppState &state) {
    s_state = &state;
    uiThemeInit();

    uiWeatherBegin(state);
    uiHomeBegin(state);
    uiRadarBegin(state);
    uiSettingsBegin(state);

    // Załaduj startowy ekran
    lv_scr_load(uiWeatherScr());
    s_current = UiScreenId::Weather;

    // Paski na lv_layer_top() — niezależne od ekranów, nigdy nie reparentowane
    uiNavBuild();
    setNavActive(UiScreenId::Weather);
}

void uiRouterShow(UiScreenId id) {
    if (id == s_current) return;   // już na tym ekranie — nic nie rób
    showInternal(id);
}

void uiRouterShowBest(const AppState &state) {
    const auto target = (state.hasSelectedFlight || state.hasBestEastCandidate)
                        ? UiScreenId::Home
                        : UiScreenId::Weather;
    uiRouterShow(target);
}

void uiRouterRefresh(const AppState &state) {
    uiBarUpdate(state);
    switch (s_current) {
        case UiScreenId::Home:     uiHomeRefresh(state);     break;
        case UiScreenId::Radar:    uiRadarRefresh(state);    break;
        case UiScreenId::Weather:  uiWeatherRefresh(state);  break;
        case UiScreenId::Settings: uiSettingsRefresh(state); break;
    }
}

UiScreenId uiRouterCurrent() { return s_current; }