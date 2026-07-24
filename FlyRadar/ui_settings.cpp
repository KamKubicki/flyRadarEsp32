#include "ui_settings.h"
#include "ui_theme.h"
#include "hal_backlight.h"
#include "app_state.h"
#include "wifi_manager.h"
#include <lvgl.h>
#include <stdio.h>

namespace {

lv_obj_t *s_scr           = nullptr;
lv_obj_t *s_radSlider     = nullptr;
lv_obj_t *s_radLabel      = nullptr;
lv_obj_t *s_depSwitch     = nullptr;
lv_obj_t *s_arrSwitch     = nullptr;
lv_obj_t *s_eastSwitch    = nullptr;
lv_obj_t *s_brightSlider  = nullptr;
lv_obj_t *s_brightLabel   = nullptr;
lv_obj_t *s_idleSlider    = nullptr;
lv_obj_t *s_idleLabel     = nullptr;
lv_obj_t *s_adsbSlider    = nullptr;
lv_obj_t *s_adsbLabel     = nullptr;
lv_obj_t *s_wifiStatusLabel = nullptr;
lv_obj_t *s_wifiIpLabel     = nullptr;
lv_obj_t *s_wifiResetBtn    = nullptr;

AppState *s_state = nullptr;

static void onRadSlider(lv_event_t *) {
    if (!s_state) return;
    s_state->settings.radarRadiusKm = lv_slider_get_value(s_radSlider);
    char buf[32]; snprintf(buf, sizeof(buf), "%d km", s_state->settings.radarRadiusKm);
    lv_label_set_text(s_radLabel, buf);
    appSettingsSave(s_state->settings);
}
static void onDepSwitch(lv_event_t *) {
    if (!s_state) return;
    s_state->settings.showDepartures = lv_obj_has_state(s_depSwitch, LV_STATE_CHECKED);
    appSettingsSave(s_state->settings);
}
static void onArrSwitch(lv_event_t *) {
    if (!s_state) return;
    s_state->settings.showArrivals = lv_obj_has_state(s_arrSwitch, LV_STATE_CHECKED);
    appSettingsSave(s_state->settings);
}
static void onEastSwitch(lv_event_t *) {
    if (!s_state) return;
    s_state->settings.visibleSide = lv_obj_has_state(s_eastSwitch, LV_STATE_CHECKED)
                                    ? VisibilitySide::EastOnly : VisibilitySide::Any;
    appSettingsSave(s_state->settings);
}
static void onIdleSlider(lv_event_t *) {
    if (!s_state) return;
    s_state->settings.weatherIdleMinutes = lv_slider_get_value(s_idleSlider);
    char buf[32]; snprintf(buf, sizeof(buf), "%d min", s_state->settings.weatherIdleMinutes);
    lv_label_set_text(s_idleLabel, buf);
    appSettingsSave(s_state->settings);
}
static void onBrightSlider(lv_event_t *) {
    if (!s_state) return;
    s_state->settings.brightnessPercent = lv_slider_get_value(s_brightSlider);
    halBacklightSetPercent(s_state->settings.brightnessPercent);
    char buf[32]; snprintf(buf, sizeof(buf), "%d%%", s_state->settings.brightnessPercent);
    lv_label_set_text(s_brightLabel, buf);
    appSettingsSave(s_state->settings);
}
static void onAdsbSlider(lv_event_t *) {
    if (!s_state) return;
    s_state->settings.adsbRefreshSec = lv_slider_get_value(s_adsbSlider);
    char buf[32]; snprintf(buf, sizeof(buf), "%d s", s_state->settings.adsbRefreshSec);
    lv_label_set_text(s_adsbLabel, buf);
    appSettingsSave(s_state->settings);
}
static void onWifiReset(lv_event_t *) {
    wifiResetCredentials();
    if (s_wifiStatusLabel)
        lv_label_set_text(s_wifiStatusLabel, "Restart za 2s...");
    delay(2000);
    ESP.restart();
}

// ── pomocnicze ───────────────────────────────────────────────────────────────

static lv_obj_t *sectionHeader(lv_obj_t *parent, const char *text, int y) {
    lv_obj_t *lbl = lv_label_create(parent);
    lv_obj_set_pos(lbl, 30, y);
    lv_obj_set_style_text_color(lbl, uiTheme::MUTED, 0);
    lv_obj_set_style_text_font(lbl, &lv_font_montserrat_16, 0);
    lv_label_set_text(lbl, text);
    return lbl;
}

static void buildSlider(lv_obj_t *parent, const char *label, int y,
                        int minV, int maxV, int initVal,
                        lv_event_cb_t cb, lv_obj_t *&slider, lv_obj_t *&valLabel,
                        const char *fmt) {
    lv_obj_t *lbl = lv_label_create(parent);
    lv_obj_set_pos(lbl, 30, y);
    lv_obj_set_style_text_color(lbl, uiTheme::TEXT, 0);
    lv_label_set_text(lbl, label);

    slider = lv_slider_create(parent);
    lv_obj_set_size(slider, 300, 20);
    lv_obj_set_pos(slider, 280, y + 1);
    lv_slider_set_range(slider, minV, maxV);
    lv_slider_set_value(slider, initVal, LV_ANIM_OFF);
    lv_obj_add_event_cb(slider, cb, LV_EVENT_VALUE_CHANGED, NULL);

    valLabel = lv_label_create(parent);
    lv_obj_align_to(valLabel, slider, LV_ALIGN_OUT_RIGHT_MID, 10, 0);
    lv_obj_set_style_text_color(valLabel, uiTheme::ACCENT, 0);
    char buf[32];
    snprintf(buf, sizeof(buf), fmt, initVal);
    lv_label_set_text(valLabel, buf);
}

static void buildSwitch(lv_obj_t *parent, const char *label, int y,
                        bool initVal, lv_event_cb_t cb, lv_obj_t *&sw) {
    lv_obj_t *lbl = lv_label_create(parent);
    lv_obj_set_pos(lbl, 30, y);
    lv_obj_set_style_text_color(lbl, uiTheme::TEXT, 0);
    lv_label_set_text(lbl, label);

    sw = lv_switch_create(parent);
    lv_obj_set_pos(sw, 280, y - 4);
    if (initVal) lv_obj_add_state(sw, LV_STATE_CHECKED);
    lv_obj_add_event_cb(sw, cb, LV_EVENT_VALUE_CHANGED, NULL);
}

} // namespace

// ============================================================================
// uiSettingsBegin
// ============================================================================
void uiSettingsBegin(AppState &state) {
    s_state = &state;
    if (s_scr) return;

    // Ekran z scrollowaniem — ważne: ustaw height > 480 żeby LVGL włączył scroll
    s_scr = lv_obj_create(NULL);
    lv_obj_set_style_bg_color(s_scr, uiTheme::BG, 0);
    // Domyślnie s_scr ma h=480. Scroll włączy się automatycznie gdy dzieci
    // przekroczą tę wysokość — ale musimy ustawić scroll_dir
    lv_obj_set_scroll_dir(s_scr, LV_DIR_VER);
    lv_obj_set_scrollbar_mode(s_scr, LV_SCROLLBAR_MODE_AUTO);

    int y = 50;

    // ── RADAR ─────────────────────────────────────────────────────────────────
    sectionHeader(s_scr, "RADAR", y); y += 30;
    buildSlider(s_scr, "Zasieg radaru", y, 5, 50, state.settings.radarRadiusKm,
                onRadSlider, s_radSlider, s_radLabel, "%d km"); y += 40;
    buildSwitch(s_scr, "Pokaz starty", y,
                state.settings.showDepartures, onDepSwitch, s_depSwitch); y += 38;
    buildSwitch(s_scr, "Pokaz ladowania", y,
                state.settings.showArrivals, onArrSwitch, s_arrSwitch); y += 38;
    buildSwitch(s_scr, "Tylko od wschodu", y,
                state.settings.visibleSide == VisibilitySide::EastOnly,
                onEastSwitch, s_eastSwitch); y += 50;

    // ── SIEC ──────────────────────────────────────────────────────────────────
    sectionHeader(s_scr, "SIEC", y); y += 30;
    buildSlider(s_scr, "Odswiezanie ADS-B", y, 5, 60, state.settings.adsbRefreshSec,
                onAdsbSlider, s_adsbSlider, s_adsbLabel, "%d s"); y += 50;

    // ── WIFI ──────────────────────────────────────────────────────────────────
    sectionHeader(s_scr, "WIFI", y); y += 30;

    // Status
    {
        lv_obj_t *lbl = lv_label_create(s_scr);
        lv_obj_set_pos(lbl, 30, y);
        lv_obj_set_style_text_color(lbl, uiTheme::TEXT, 0);
        lv_label_set_text(lbl, "Status:");
    }
    s_wifiStatusLabel = lv_label_create(s_scr);
    lv_obj_set_pos(s_wifiStatusLabel, 200, y);
    lv_obj_set_size(s_wifiStatusLabel, 540, 22);
    lv_obj_set_style_text_color(s_wifiStatusLabel, uiTheme::ACCENT, 0);
    lv_label_set_text(s_wifiStatusLabel, "...");
    y += 30;

    // IP
    {
        lv_obj_t *lbl = lv_label_create(s_scr);
        lv_obj_set_pos(lbl, 30, y);
        lv_obj_set_style_text_color(lbl, uiTheme::TEXT, 0);
        lv_label_set_text(lbl, "Adres IP:");
    }
    s_wifiIpLabel = lv_label_create(s_scr);
    lv_obj_set_pos(s_wifiIpLabel, 200, y);
    lv_obj_set_size(s_wifiIpLabel, 540, 22);
    lv_obj_set_style_text_color(s_wifiIpLabel, uiTheme::MUTED, 0);
    lv_label_set_text(s_wifiIpLabel, "--");
    y += 36;

    // Opis — wystarczająco duży żeby zmieścił 3 linie (fix ucięcia tekstu)
    {
        lv_obj_t *hint = lv_label_create(s_scr);
        lv_obj_set_pos(hint, 30, y);
        lv_obj_set_size(hint, 700, 60);   // 60px = 3 linie × 20px
        lv_obj_set_style_text_color(hint, uiTheme::MUTED, 0);
        lv_obj_set_style_text_font(hint, &lv_font_montserrat_14, 0);
        lv_label_set_long_mode(hint, LV_LABEL_LONG_WRAP);
        lv_label_set_text(hint,
            "Aby zmienic siec WiFi nacisnij przycisk nizej.\n"
            "ESP zrestartuje sie i otworzy hotspot FlyRadar-Setup.\n"
            "Polacz sie z nim telefonem pod adresem 192.168.4.1");
        y += 68;
    }

    // Przycisk
    s_wifiResetBtn = lv_btn_create(s_scr);
    lv_obj_set_pos(s_wifiResetBtn, 30, y);
    lv_obj_set_size(s_wifiResetBtn, 280, 44);
    lv_obj_set_style_bg_color(s_wifiResetBtn, uiTheme::WEST_OFF, 0);
    lv_obj_set_style_radius(s_wifiResetBtn, 8, 0);
    lv_obj_add_event_cb(s_wifiResetBtn, onWifiReset, LV_EVENT_CLICKED, nullptr);
    {
        lv_obj_t *lbl = lv_label_create(s_wifiResetBtn);
        lv_label_set_text(lbl, "Zmien WiFi / Reset");
        lv_obj_center(lbl);
        lv_obj_set_style_text_color(lbl, lv_color_white(), 0);
    }
    y += 60;

    // ── EKRAN ─────────────────────────────────────────────────────────────────
    sectionHeader(s_scr, "EKRAN", y); y += 30;
    buildSlider(s_scr, "Jasnosc", y, 0, 100, state.settings.brightnessPercent,
                onBrightSlider, s_brightSlider, s_brightLabel, "%d%%"); y += 40;
    buildSlider(s_scr, "Czas bezczynnosci", y, 1, 60, state.settings.weatherIdleMinutes,
                onIdleSlider, s_idleSlider, s_idleLabel, "%d min"); y += 60;

    // Wymuś rozmiar ekranu żeby scroll działał do końca ostatniego elementu
    // LVGL potrzebuje jawnie ustawionej wysokości kontenera lub padding na dole
    lv_obj_set_style_pad_bottom(s_scr, 60, 0);
}

void uiSettingsShow() {
    if (!s_scr) return;
    lv_scr_load(s_scr);
    // Przewiń na górę przy każdym otwarciu
    lv_obj_scroll_to_y(s_scr, 0, LV_ANIM_OFF);
    if (s_state) {
        lv_slider_set_value(s_radSlider,    s_state->settings.radarRadiusKm,      LV_ANIM_OFF);
        lv_slider_set_value(s_idleSlider,   s_state->settings.weatherIdleMinutes, LV_ANIM_OFF);
        lv_slider_set_value(s_brightSlider, s_state->settings.brightnessPercent,  LV_ANIM_OFF);
        lv_slider_set_value(s_adsbSlider,   s_state->settings.adsbRefreshSec,     LV_ANIM_OFF);
    }
}

lv_obj_t *uiSettingsScr() { return s_scr; }

void uiSettingsRefresh(const AppState &state) {
    if (!s_wifiStatusLabel || !s_wifiIpLabel) return;
    char buf[64];
    if (wifiPortalActive()) {
        snprintf(buf, sizeof(buf), "Portal AP: FlyRadar-Setup");
        lv_obj_set_style_text_color(s_wifiStatusLabel, uiTheme::WEST_OFF, 0);
    } else if (state.wifiConnected) {
        const char *ssid = wifiSSID();
        snprintf(buf, sizeof(buf), "Polaczone: %s", ssid[0] ? ssid : "?");
        lv_obj_set_style_text_color(s_wifiStatusLabel, uiTheme::EAST_OK, 0);
    } else {
        snprintf(buf, sizeof(buf), "%s", wifiStatusText());
        lv_obj_set_style_text_color(s_wifiStatusLabel, uiTheme::MUTED, 0);
    }
    lv_label_set_text(s_wifiStatusLabel, buf);

    const char *ip = wifiLocalIP();
    lv_label_set_text(s_wifiIpLabel, ip[0] ? ip : "--");

    if (s_wifiResetBtn)
        lv_obj_set_style_bg_color(s_wifiResetBtn,
            wifiPortalActive() ? uiTheme::MUTED : uiTheme::WEST_OFF, 0);
}
