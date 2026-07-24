#include "ui_home.h"
#include "ui_theme.h"
#include "ui_router.h"
#include "geo_utils.h"
#include "config.h"
#include "sd_manager.h"
#include "aircraft_types.h"
#include "compass_widget.h"
#include <lvgl.h>
#include <string.h>
#include <stdio.h>
#include <math.h>

namespace {

lv_obj_t *s_scr        = nullptr;
lv_obj_t *s_callsign   = nullptr;
lv_obj_t *s_statusBadge = nullptr;
lv_obj_t *s_statusText = nullptr;
lv_obj_t *s_route      = nullptr;
lv_obj_t *s_meta       = nullptr;
lv_obj_t *s_cardVal[4] = {};
lv_obj_t *s_cardUnit[4] = {};
lv_obj_t *s_vario      = nullptr;   // wznoszenie/opadanie pod kartami
lv_obj_t *s_detail     = nullptr;

// Logo linii lotniczej
lv_obj_t   *s_logoCanvas  = nullptr;
lv_color_t *s_logoBuf     = nullptr;
char        s_lastLogoPrefix[4] = {};

// Kompas kierunku samolotu (prawy górny róg)
lv_obj_t   *s_compassCanvas = nullptr;

static const char *bearingToDir(double deg) {
    if (isnan(deg)) return "--";
    const char *dirs[] = {"N", "NE", "E", "SE", "S", "SW", "W", "NW"};
    int idx = (int)round(deg / 45.0) % 8;
    const char *pl[] = {"polnocy", "polnocnego wschodu",
                    "wschodu", "poludniowego wschodu",
                    "poludnia", "poludniowego zachodu",
                    "zachodu", "polnocnego zachodu"};
    return pl[idx];
}

static void makeCard(lv_obj_t *parent, lv_obj_t *&val, lv_obj_t *&unit,
                     const char *unitText, const char *label,
                     int x, int y, int w, int h) {
    lv_obj_t *card = lv_obj_create(parent);
    lv_obj_set_pos(card, x, y);
    lv_obj_set_size(card, w, h);
    lv_obj_set_style_bg_color(card, uiTheme::CARD, 0);
    lv_obj_set_style_border_width(card, 0, 0);
    lv_obj_set_style_radius(card, 8, 0);
    lv_obj_set_style_pad_all(card, 0, 0);
    lv_obj_set_style_shadow_width(card, 0, 0);

    lv_obj_t *lbl = lv_label_create(card);
    lv_obj_set_pos(lbl, 0, 6);
    lv_obj_set_size(lbl, w, 16);
    lv_obj_set_style_text_align(lbl, LV_TEXT_ALIGN_CENTER, 0);
    lv_obj_set_style_text_color(lbl, uiTheme::MUTED, 0);
    lv_obj_set_style_text_font(lbl, &lv_font_montserrat_14, 0);
    lv_label_set_text(lbl, label);

    val = lv_label_create(card);
    lv_obj_set_pos(val, 0, 22);
    lv_obj_set_size(val, w, 40);
    lv_obj_set_style_text_align(val, LV_TEXT_ALIGN_CENTER, 0);
    lv_obj_set_style_text_color(val, uiTheme::TEXT, 0);
    lv_obj_set_style_text_font(val, &lv_font_montserrat_30, 0);
    lv_label_set_text(val, "--");

    unit = lv_label_create(card);
    lv_obj_set_pos(unit, 0, 66);
    lv_obj_set_size(unit, w, 16);
    lv_obj_set_style_text_align(unit, LV_TEXT_ALIGN_CENTER, 0);
    lv_obj_set_style_text_color(unit, uiTheme::MUTED, 0);
    lv_obj_set_style_text_font(unit, &lv_font_montserrat_14, 0);
    lv_label_set_text(unit, unitText);
}

} // namespace

void uiHomeBegin(AppState &state) {
    (void)state;
    if (s_scr) return;
    s_scr = lv_obj_create(NULL);
    lv_obj_set_style_bg_color(s_scr, uiTheme::BG, 0);

    // ===== Kompas kierunku samolotu (prawy górny róg, 80×80) =====
    // Pokazuje w którą stronę patrzeć żeby zobaczyć samolot
    s_compassCanvas = compassCreate(s_scr, 700, 44);

    // ===== Logo linii lotniczej (lewo, 120×50) =====
    // Bufor w PSRAM — alokacja raz, reużywany przy każdej zmianie linii
    s_logoBuf = (lv_color_t *)heap_caps_malloc(
                    LOGO_W * LOGO_H * sizeof(lv_color_t), MALLOC_CAP_SPIRAM);
    if (!s_logoBuf)
        s_logoBuf = (lv_color_t *)malloc(LOGO_W * LOGO_H * sizeof(lv_color_t));

    s_logoCanvas = lv_canvas_create(s_scr);
    lv_obj_set_pos(s_logoCanvas, 40, 50);
    lv_obj_set_size(s_logoCanvas, LOGO_W, LOGO_H);
    if (s_logoBuf)
        lv_canvas_set_buffer(s_logoCanvas, s_logoBuf, LOGO_W, LOGO_H, LV_IMG_CF_TRUE_COLOR);
    lv_canvas_fill_bg(s_logoCanvas, uiTheme::BG, LV_OPA_COVER);
    lv_obj_add_flag(s_logoCanvas, LV_OBJ_FLAG_HIDDEN);  // ukryty dopóki nie ma logo

    // ===== Callsign — środek ekranu =====
    s_callsign = lv_label_create(s_scr);
    lv_obj_set_pos(s_callsign, 0, 48);
    lv_obj_set_size(s_callsign, 800, 46);
    lv_obj_set_style_text_align(s_callsign, LV_TEXT_ALIGN_CENTER, 0);
    lv_obj_set_style_text_color(s_callsign, uiTheme::TEXT, 0);
    lv_obj_set_style_text_font(s_callsign, &lv_font_montserrat_30, 0);
    lv_label_set_text(s_callsign, "---");

    // Status badge (pill)
    s_statusBadge = lv_obj_create(s_scr);
    lv_obj_set_pos(s_statusBadge, 200, 98);
    lv_obj_set_size(s_statusBadge, 400, 30);
    lv_obj_set_style_border_width(s_statusBadge, 0, 0);
    lv_obj_set_style_radius(s_statusBadge, 15, 0);
    lv_obj_set_style_pad_all(s_statusBadge, 0, 0);
    lv_obj_set_style_shadow_width(s_statusBadge, 0, 0);
    lv_obj_add_flag(s_statusBadge, LV_OBJ_FLAG_HIDDEN);

    s_statusText = lv_label_create(s_statusBadge);
    lv_obj_center(s_statusText);
    lv_obj_set_style_text_color(s_statusText, lv_color_hex(0x000000), 0);
    lv_obj_set_style_text_font(s_statusText, &lv_font_montserrat_18, 0);
    lv_label_set_text(s_statusText, "");

    // Trasa (np. "KRK \u2192 STR")
    s_route = lv_label_create(s_scr);
    lv_obj_set_pos(s_route, 0, 142);
    lv_obj_set_size(s_route, 800, 22);
    lv_obj_set_style_text_align(s_route, LV_TEXT_ALIGN_CENTER, 0);
    lv_obj_set_style_text_color(s_route, uiTheme::ACCENT, 0);
    lv_obj_set_style_text_font(s_route, &lv_font_montserrat_20, 0);
    lv_label_set_text(s_route, "");

    // ===== Poziom 2: meta =====
    s_meta = lv_label_create(s_scr);
    lv_obj_set_pos(s_meta, 0, 180);
    lv_obj_set_size(s_meta, 800, 20);
    lv_obj_set_style_text_align(s_meta, LV_TEXT_ALIGN_CENTER, 0);
    lv_obj_set_style_text_color(s_meta, uiTheme::MUTED, 0);
    lv_obj_set_style_text_font(s_meta, &lv_font_montserrat_16, 0);
    lv_label_set_text(s_meta, "");

    // ===== Poziom 3: 4 karty =====
    const int cw = 172, ch = 92, gap = 14;
    const int x0 = (800 - 4 * cw - 3 * gap) / 2;
    makeCard(s_scr, s_cardVal[0], s_cardUnit[0], "ft",     "WYSOKOSC",  x0,                218, cw, ch);
    makeCard(s_scr, s_cardVal[1], s_cardUnit[1], "kt",     "PREDKOSC",  x0 + cw + gap,     218, cw, ch);
    makeCard(s_scr, s_cardVal[2], s_cardUnit[2], "km",     "DYSTANS",   x0 + 2*(cw+gap),   218, cw, ch);
    makeCard(s_scr, s_cardVal[3], s_cardUnit[3], "\u00B0", "KURS",      x0 + 3*(cw+gap),   218, cw, ch);

    // ===== Wznoszenie/opadanie — duży wskaźnik pod kartami =====
    s_vario = lv_label_create(s_scr);
    lv_obj_set_pos(s_vario, 0, 318);
    lv_obj_set_size(s_vario, 800, 30);
    lv_obj_set_style_text_align(s_vario, LV_TEXT_ALIGN_CENTER, 0);
    lv_obj_set_style_text_font(s_vario, &lv_font_montserrat_20, 0);
    lv_obj_set_style_text_color(s_vario, uiTheme::MUTED, 0);
    lv_label_set_text(s_vario, "");

    // ===== Poziom 4: detail bar =====
    lv_obj_t *detailBg = lv_obj_create(s_scr);
    lv_obj_set_pos(detailBg, 0, 430);
    lv_obj_set_size(detailBg, 800, 50);
    lv_obj_set_style_bg_color(detailBg, uiTheme::BAR, 0);
    lv_obj_set_style_border_width(detailBg, 0, 0);
    lv_obj_set_style_radius(detailBg, 0, 0);
    lv_obj_set_style_pad_all(detailBg, 0, 0);
    lv_obj_set_style_shadow_width(detailBg, 0, 0);

    s_detail = lv_label_create(detailBg);
    lv_obj_center(s_detail);
    lv_obj_set_style_text_color(s_detail, lv_color_hex(0x6B7A8B), 0);
    lv_obj_set_style_text_font(s_detail, &lv_font_montserrat_16, 0);
    lv_label_set_text(s_detail, "");
}

void uiHomeShow() {
    lv_scr_load(s_scr);
}

lv_obj_t *uiHomeScr() { return s_scr; }

void uiHomeRefresh(const AppState &state) {
    if (lv_scr_act() != s_scr) return;

    // Aktualizuj tylko gdy dane ADS-B się zmieniły (zmiana wersji co ~12s)
    static int s_lastVersion = -1;
    if (state.adsbVersion == s_lastVersion) return;
    s_lastVersion = state.adsbVersion;

    const bool hasEast = state.hasBestEastCandidate;
    const bool hasSel  = state.hasSelectedFlight;
    const bool hasCand = hasEast || hasSel;
    const FlightCandidate &cand = hasEast
                                  ? state.bestEastCandidate
                                  : state.selectedFlight;

    if (!hasCand) {
        lv_label_set_text(s_callsign, "---");
        lv_obj_add_flag(s_statusBadge, LV_OBJ_FLAG_HIDDEN);
        lv_obj_add_flag(s_logoCanvas,  LV_OBJ_FLAG_HIDDEN);
        lv_label_set_text(s_route,   "");
        lv_label_set_text(s_meta,    "");
        if (s_vario) lv_label_set_text(s_vario, "");
        for (int i = 0; i < 4; ++i) lv_label_set_text(s_cardVal[i], "--");
        lv_label_set_text(s_detail, "");
        s_lastLogoPrefix[0] = '\0';
        compassClear(s_compassCanvas);
        return;
    }

    const AircraftState &a = cand.aircraft;
    char buf[80];

    // ===== Kompas =====
    compassUpdate(s_compassCanvas, a.bearingDeg);

    // ===== Logo linii lotniczej =====
    // Wyciągnij 3-literowy prefix ICAO z callsign (np. "WZZ2705" → "WZZ")
    if (s_logoCanvas && s_logoBuf && sdAvailable()) {
        char prefix[4] = {};
        if (strlen(a.callsign) >= 3) {
            prefix[0] = a.callsign[0];
            prefix[1] = a.callsign[1];
            prefix[2] = a.callsign[2];
        }
        // Załaduj tylko jeśli prefix się zmienił
        if (prefix[0] && strcmp(prefix, s_lastLogoPrefix) != 0) {
            strncpy(s_lastLogoPrefix, prefix, 3);
            lv_canvas_fill_bg(s_logoCanvas, uiTheme::BG, LV_OPA_COVER);
            bool ok = sdLoadAirlineLogo(prefix, s_logoBuf);
            if (ok) {
                // Odśwież canvas — dane są już w buforze, wystarczy invalidate
                lv_obj_invalidate(s_logoCanvas);
                lv_obj_clear_flag(s_logoCanvas, LV_OBJ_FLAG_HIDDEN);
            } else {
                lv_obj_add_flag(s_logoCanvas, LV_OBJ_FLAG_HIDDEN);
            }
        }
    } else {
        lv_obj_add_flag(s_logoCanvas, LV_OBJ_FLAG_HIDDEN);
    }

    // ===== Callsign =====
    lv_label_set_text(s_callsign, a.callsign);

    // ===== Status badge z kierunkiem =====
    // np. "LADUJE od wschodu", "STARTUJE na zachod", "PRZELOT"
    lv_color_t badgeColor;
    char statusBuf[32];
    bool isEast = cand.isEastVisible;

    switch (cand.kind) {
        case FlightKind::Arrival:
            if (isEast)
                snprintf(statusBuf, sizeof(statusBuf), "LADUJE od wschodu");
            else
                snprintf(statusBuf, sizeof(statusBuf), "LADUJE od zachodu");
            badgeColor = uiTheme::EAST_OK;
            break;
        case FlightKind::Departure:
            if (isEast)
                snprintf(statusBuf, sizeof(statusBuf), "STARTUJE na wschod");
            else
                snprintf(statusBuf, sizeof(statusBuf), "STARTUJE na zachod");
            badgeColor = uiTheme::ACCENT;
            break;
        case FlightKind::Transit:
            snprintf(statusBuf, sizeof(statusBuf), "PRZELOT");
            badgeColor = uiTheme::MUTED;
            break;
        default:
            snprintf(statusBuf, sizeof(statusBuf), "W POBLIZU");
            badgeColor = uiTheme::WEST_OFF;
            break;
    }
    lv_label_set_text(s_statusText, statusBuf);
    lv_obj_set_style_bg_color(s_statusBadge, badgeColor, 0);
    lv_obj_clear_flag(s_statusBadge, LV_OBJ_FLAG_HIDDEN);

    // ===== Trasa =====
    if (cand.route.valid && strlen(cand.route.origin) > 0 && strlen(cand.route.dest) > 0) {
        snprintf(buf, sizeof(buf), "%s -> %s", cand.route.origin, cand.route.dest);
    } else {
        buf[0] = '\0';
    }
    lv_label_set_text(s_route, buf);

    // ===== Meta: airline • typ samolotu (pełna nazwa) • kierunek =====
    const char *dir      = bearingToDir(a.bearingDeg);
    const char *typeFull = aircraftTypeName(a.type);
    const char *typeStr  = (typeFull && typeFull[0]) ? typeFull
                         : (strlen(a.type) > 0 ? a.type : nullptr);

    if (strlen(cand.airline) > 0 && typeStr)
        snprintf(buf, sizeof(buf), "%s  |  %s  |  od %s",
                 cand.airline, typeStr, dir);
    else if (strlen(cand.airline) > 0)
        snprintf(buf, sizeof(buf), "%s  |  od %s",
                 cand.airline, dir);
    else if (typeStr)
        snprintf(buf, sizeof(buf), "%s  |  od %s",
                 typeStr, dir);
    else
        snprintf(buf, sizeof(buf), "Widoczny od %s", dir);
    lv_label_set_text(s_meta, buf);

    // ===== 4 karty =====
    if (!isnan(a.altitudeFt))
        snprintf(buf, sizeof(buf), "%.0f", a.altitudeFt);
    else
        snprintf(buf, sizeof(buf), "--");
    lv_label_set_text(s_cardVal[0], buf);

    if (!isnan(a.speedKt))
        snprintf(buf, sizeof(buf), "%.0f", a.speedKt);
    else
        snprintf(buf, sizeof(buf), "--");
    lv_label_set_text(s_cardVal[1], buf);

    if (!isnan(a.distanceKm))
        snprintf(buf, sizeof(buf), "%.1f", a.distanceKm);
    else
        snprintf(buf, sizeof(buf), "--");
    lv_label_set_text(s_cardVal[2], buf);

    if (!isnan(a.trackDeg))
        snprintf(buf, sizeof(buf), "%.0f", a.trackDeg);
    else
        snprintf(buf, sizeof(buf), "--");
    lv_label_set_text(s_cardVal[3], buf);

    // ===== Wznoszenie/opadanie — widoczny wskaźnik z kolorem =====
    if (s_vario) {
        if (!isnan(a.verticalRateFpm)) {
            if (a.verticalRateFpm > 200) {
                snprintf(buf, sizeof(buf), "^ Wznosi sie %.0f ft/min", a.verticalRateFpm);
                lv_obj_set_style_text_color(s_vario, uiTheme::EAST_OK, 0);  // zielony
            } else if (a.verticalRateFpm < -200) {
                snprintf(buf, sizeof(buf), "v Opada %.0f ft/min", fabs(a.verticalRateFpm));
                lv_obj_set_style_text_color(s_vario, uiTheme::WEST_OFF, 0); // czerwony
            } else {
                snprintf(buf, sizeof(buf), "= Lot poziomy");
                lv_obj_set_style_text_color(s_vario, uiTheme::MUTED, 0);
            }
        } else {
            buf[0] = '\0';
        }
        lv_label_set_text(s_vario, buf);
    }

    // ===== Detail bar =====
    const char *regStr = strlen(a.reg) > 0 ? a.reg : "--";

    char varioBuf[28];
    if (!isnan(a.verticalRateFpm)) {
        if (a.verticalRateFpm < -50)
            snprintf(varioBuf, sizeof(varioBuf), "Opada %.0f ft/min",
                     fabs(a.verticalRateFpm));
        else if (a.verticalRateFpm > 50)
            snprintf(varioBuf, sizeof(varioBuf), "Wznosi sie %.0f ft/min",
                     a.verticalRateFpm);
        else
            snprintf(varioBuf, sizeof(varioBuf), "Poziomo");
    } else {
        snprintf(varioBuf, sizeof(varioBuf), "--");
    }

    if (!isnan(a.airportDistanceKm))
        snprintf(buf, sizeof(buf), "Rej. %s  \u2022  %s  \u2022  EPKK %.1f km",
                 regStr, varioBuf, a.airportDistanceKm);
    else
        snprintf(buf, sizeof(buf), "Rej. %s  \u2022  %s  \u2022  EPKK --",
                 regStr, varioBuf);
    lv_label_set_text(s_detail, buf);
}
