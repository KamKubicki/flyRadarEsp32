#include "ui_weather.h"
#include "ui_theme.h"
#include "ui_router.h"
#include "moon_nameday.h"
#include "sd_manager.h"
#include <Arduino.h>#include <lvgl.h>
#include <stdio.h>
#include <time.h>
#include <math.h>

// ============================================================================
// BALANCED LAYOUT  800 × 480  (status 40px + nav 50px = 390px usable)
//
//  Y=  0..39   → status bar (router)
//  Y= 40..219  → BLOK GŁÓWNY (180px)
//    left  0..199   → zegar (duży) + data
//    mid 200..479   → ikona 90×90 + temperatura 48px + opis 20px + słońce 16px
//  Y=220..299  → 4 KAFELKI danych  (80px)
//  Y=300..369  → PROGNOZA 3 dni   (70px)
//  Y=370..409  → PASEK DOLNY rotacyjny (40px): księżyc / imieniny / lot
//  Y=410..429  → nav bar (router)
// ============================================================================

namespace {

// ── widgety ─────────────────────────────────────────────────────────────────
lv_obj_t *s_scr        = nullptr;
lv_obj_t *s_clock      = nullptr;
lv_obj_t *s_date       = nullptr;
lv_obj_t *s_iconCanvas = nullptr;   // ikona pogody dziś 90×90
lv_obj_t *s_temp       = nullptr;
lv_obj_t *s_desc       = nullptr;
lv_obj_t *s_sunRow     = nullptr;

lv_obj_t *s_cardVal[4] = {};        // wiatr / wilgotność / opad / min-max

// Prognoza 3 dni — każda karta: ikonka + dzień + temp
static constexpr int FCAST_ICON_W = 50;
static constexpr int FCAST_ICON_H = 50;
lv_obj_t   *s_fcastDay[3]        = {};
lv_obj_t   *s_fcastTemp[3]       = {};
lv_obj_t   *s_fcastPrecip[3]     = {};   // procent opadu
lv_obj_t   *s_fcastIconCanvas[3] = {};
lv_color_t *s_fcastIconBuf[3]    = {};
int         s_fcastLastCode[3]   = {-1, -1, -1};

lv_obj_t *s_ticker      = nullptr;  // rotacyjny pasek dolny (księżyc/imieniny/lot)
lv_obj_t *s_moonCanvas  = nullptr;  // ikonka fazy księżyca 40×40 w pasku
lv_color_t *s_moonBuf   = nullptr;  // bufor PSRAM dla moon canvas

// ── PSRAM bufor ikony ────────────────────────────────────────────────────────
lv_color_t *s_iconBuf = nullptr;

// ── cache do change-guard ────────────────────────────────────────────────────
int   s_lastWCode    = -1;
float s_lastTempC    = -999.f;
int   s_lastHumidity = -1;
float s_lastWindMs   = -999.f;
float s_lastPrecip   = -999.f;
char  s_lastClock[6] = "";
char  s_lastDate[52] = "";

// ── rotacja paska dolnego ────────────────────────────────────────────────────
unsigned long s_tickerMs      = 0;
int           s_tickerSlot    = 0;    // 0=księżyc 1=imieniny 2=rodzinne 3=popularne 4=lot
int           s_lastMoonPhase = -1;
char          s_tickerNameday[32]  = "";
char          s_tickerFlight[48]   = "";
char          s_tickerFamily[64]   = "";   // swieto rodzinne
char          s_tickerHoliday[64]  = "";   // popularne swieto
// ============================================================================
// Paleta ikon
// ============================================================================
static const lv_color_t C_SUN   = lv_color_hex(0xFFD93D);
static const lv_color_t C_CLOUD = lv_color_hex(0x9AA4B2);
static const lv_color_t C_RAIN  = lv_color_hex(0x4DA6FF);
static const lv_color_t C_SNOW  = lv_color_hex(0xD6E4F0);
static const lv_color_t C_BOLT  = lv_color_hex(0xFFE066);
static const lv_color_t C_FOG   = lv_color_hex(0x7A8494);


// ============================================================================
// SYSTEM RYSOWANIA IKON — parametryczny (działa dla 90×90 i 40×40)
// Wszystkie pozycje/rozmiary wyrażone jako ułamek `s` (size) canvasu
// ============================================================================

// Klipowane wypełnione koło (bounds check dynamiczny)
static void icircle(lv_obj_t *cv, int cx, int cy, int r, lv_color_t col, int S) {
    for (int dy = -r; dy <= r; ++dy)
        for (int dx = -r; dx <= r; ++dx)
            if (dx*dx + dy*dy <= r*r) {
                int px = cx+dx, py = cy+dy;
                if (px >= 0 && px < S && py >= 0 && py < S)
                    lv_canvas_set_px_color(cv, px, py, col);
            }
}

// Rysuje słońce (kółko + promienie)  s=rozmiar canvasu
static void iSun(lv_obj_t *cv, int cx, int cy, int S, float scale) {
    int r  = (int)(S * 0.18f * scale);   // promień tarczy
    int r1 = (int)(S * 0.22f * scale);   // wewnętrzny promień promieni
    int r2 = (int)(S * 0.32f * scale);   // zewnętrzny promień promieni
    icircle(cv, cx, cy, r, C_SUN, S);
    lv_draw_line_dsc_t ld; lv_draw_line_dsc_init(&ld);
    ld.width = (S >= 60) ? 2 : 1;
    ld.color = C_SUN;
    for (int a = 0; a < 360; a += 45) {
        double rad = a * 3.14159265 / 180.0;
        lv_point_t pts[] = {
            {(lv_coord_t)(cx + (int)(r1*cos(rad))), (lv_coord_t)(cy + (int)(r1*sin(rad)))},
            {(lv_coord_t)(cx + (int)(r2*cos(rad))), (lv_coord_t)(cy + (int)(r2*sin(rad)))}
        };
        lv_canvas_draw_line(cv, pts, 2, &ld);
    }
}

// Rysuje chmurę z opcjonalnym deszczem/śniegiem/burzą
static void iCloud(lv_obj_t *cv, int cx, int cy, int S, float scale,
                   bool rain, bool storm, bool snow) {
    int r1 = (int)(S * 0.14f * scale);
    int r2 = (int)(S * 0.18f * scale);
    int r3 = (int)(S * 0.15f * scale);

    // 3 koła tworzące chmurę
    icircle(cv, cx - (int)(r1*0.8f), cy + (int)(r1*0.2f), r1, C_CLOUD, S);
    icircle(cv, cx + (int)(r2*0.6f), cy,                   r2, C_CLOUD, S);
    icircle(cv, cx,                  cy + (int)(r3*0.3f),   r3, C_CLOUD, S);

    // Dolny prostokąt chmury
    int xL = cx - (int)(S*0.26f*scale), xR = cx + (int)(S*0.30f*scale);
    int yT = cy + (int)(S*0.06f*scale), yB = cy + (int)(S*0.16f*scale);
    if (xL < 0) xL = 0; if (xR >= S) xR = S-1;
    for (int y = yT; y <= yB && y < S; ++y)
        for (int x = xL; x <= xR; ++x)
            if (x >= 0) lv_canvas_set_px_color(cv, x, y, C_CLOUD);

    int precip_y0 = cy + (int)(S * 0.20f * scale);
    int precip_dy = (int)(S * 0.12f * scale);
    if (precip_dy < 4) precip_dy = 4;

    lv_draw_line_dsc_t ld; lv_draw_line_dsc_init(&ld);
    ld.width = (S >= 60) ? 2 : 1;

    if (rain) {
        ld.color = C_RAIN;
        int gap = (int)(S * 0.12f * scale);
        for (int i = -1; i <= 1; ++i) {
            lv_point_t pts[] = {
                {(lv_coord_t)(cx + i*gap), (lv_coord_t)(precip_y0)},
                {(lv_coord_t)(cx + i*gap), (lv_coord_t)(precip_y0 + precip_dy)}
            };
            lv_canvas_draw_line(cv, pts, 2, &ld);
        }
    }
    if (snow) {
        int gap = (int)(S * 0.12f * scale);
        int rs  = (int)(S * 0.04f * scale); if (rs < 2) rs = 2;
        for (int i = -1; i <= 1; ++i)
            icircle(cv, cx + i*gap, precip_y0 + precip_dy/2, rs, C_SNOW, S);
    }
    if (storm) {
        ld.color = C_BOLT;
        int bx = cx, by = precip_y0;
        int bw = (int)(S * 0.08f * scale);
        int bh = (int)(S * 0.20f * scale);
        lv_point_t pts[] = {
            {(lv_coord_t)bx,       (lv_coord_t)(by)},
            {(lv_coord_t)(bx-bw),  (lv_coord_t)(by + bh/2)},
            {(lv_coord_t)(bx+bw/2),(lv_coord_t)(by + bh/2)},
            {(lv_coord_t)(bx-bw),  (lv_coord_t)(by + bh)}
        };
        lv_canvas_draw_line(cv, pts, 4, &ld);
    }
}

// Rysuje mgłę (3 poziome kreski)
static void iFog(lv_obj_t *cv, int cx, int cy, int S, float scale) {
    lv_draw_line_dsc_t ld; lv_draw_line_dsc_init(&ld);
    ld.color = C_FOG;
    ld.width = (S >= 60) ? 3 : 2;
    int w  = (int)(S * 0.38f * scale);
    int gap = (int)(S * 0.12f * scale);
    for (int i = -1; i <= 1; ++i) {
        int y = cy + i * gap;
        lv_point_t pts[] = {{(lv_coord_t)(cx-w), (lv_coord_t)y},
                            {(lv_coord_t)(cx+w), (lv_coord_t)y}};
        lv_canvas_draw_line(cv, pts, 2, &ld);
    }
}

// ─── Publiczne funkcje ────────────────────────────────────────────────────────

// Rysuje ikonę pogody na canvas o rozmiarze S×S (S=90 lub S=40)
static void drawWeatherIconScaled(int code, lv_obj_t *cv, int S,
                                   lv_color_t bg = uiTheme::BG) {
    lv_canvas_fill_bg(cv, bg, LV_OPA_COVER);
    int cx = S / 2, cy = S / 2;
    float sc = S / 90.0f;   // skala względem "wzorcowego" 90px

    if (code == 0) {
        iSun(cv, cx, cy, S, sc);
    } else if (code == 1 || code == 2) {
        // Słońce z chmurką — słońce nieco przesunięte w lewo/górę
        iSun(cv, cx - (int)(S*0.1f), cy - (int)(S*0.1f), S, sc * 0.7f);
        iCloud(cv, cx + (int)(S*0.05f), cy + (int)(S*0.05f), S, sc * 0.85f,
               false, false, false);
    } else if (code == 3) {
        iCloud(cv, cx, cy, S, sc, false, false, false);
    } else if (code == 45 || code == 48) {
        iFog(cv, cx, cy, S, sc);
    } else if (code >= 51 && code <= 57) {
        iCloud(cv, cx, cy - (int)(S*0.05f), S, sc, true, false, false);
    } else if ((code >= 61 && code <= 67) || (code >= 80 && code <= 82)) {
        iCloud(cv, cx, cy - (int)(S*0.05f), S, sc, true, false, false);
    } else if ((code >= 71 && code <= 77) || (code >= 85 && code <= 86)) {
        iCloud(cv, cx, cy - (int)(S*0.05f), S, sc, false, false, true);
    } else if (code >= 95) {
        iCloud(cv, cx, cy - (int)(S*0.05f), S, sc, false, true, false);
    } else {
        iCloud(cv, cx, cy, S, sc, false, false, false);
    }
}

// Wrappers dla kompatybilności z istniejącym kodem
static void drawWeatherIcon(int code, lv_obj_t *cv) {
    drawWeatherIconScaled(code, cv, 90, uiTheme::BG);
}

static void drawWeatherIconSmall(int code, lv_obj_t *cv) {
    drawWeatherIconScaled(code, cv, FCAST_ICON_W, uiTheme::CARD);
}


// ── helper: kafelek danych ───────────────────────────────────────────────────
static void makeCard(lv_obj_t *parent, lv_obj_t *&val,
                     const char *title, int x, int y, int w, int h) {
    lv_obj_t *card = lv_obj_create(parent);
    lv_obj_set_pos(card, x, y);
    lv_obj_set_size(card, w, h);
    lv_obj_set_style_bg_color(card, uiTheme::CARD, 0);
    lv_obj_set_style_border_width(card, 0, 0);
    lv_obj_set_style_radius(card, 8, 0);
    lv_obj_set_style_pad_all(card, 0, 0);

    lv_obj_t *lbl = lv_label_create(card);
    lv_obj_set_pos(lbl, 0, 5);
    lv_obj_set_size(lbl, w, 14);
    lv_obj_set_style_text_align(lbl, LV_TEXT_ALIGN_CENTER, 0);
    lv_obj_set_style_text_color(lbl, uiTheme::MUTED, 0);
    lv_obj_set_style_text_font(lbl, &lv_font_montserrat_12, 0);
    lv_label_set_text(lbl, title);

    val = lv_label_create(card);
    lv_obj_set_pos(val, 0, 22);
    lv_obj_set_size(val, w, 36);
    lv_obj_set_style_text_align(val, LV_TEXT_ALIGN_CENTER, 0);
    lv_obj_set_style_text_color(val, uiTheme::TEXT, 0);
    lv_obj_set_style_text_font(val, &lv_font_montserrat_20, 0);
    lv_label_set_text(val, "--");
}

} // namespace

// ============================================================================
// uiWeatherBegin
// ============================================================================
void uiWeatherBegin(AppState &state) {
    (void)state;
    if (s_scr) return;
    s_scr = lv_obj_create(NULL);
    lv_obj_set_style_bg_color(s_scr, uiTheme::BG, 0);

    // ── LEWA KOLUMNA: zegar + data  (x 0..219, y 40..219) ──────────────────
    s_clock = lv_label_create(s_scr);
    lv_obj_set_pos(s_clock, 24, 52);
    lv_obj_set_style_text_color(s_clock, uiTheme::TEXT, 0);
    lv_obj_set_style_text_font(s_clock, &lv_font_montserrat_30, 0);
    lv_label_set_text(s_clock, "--:--");

    s_date = lv_label_create(s_scr);
    lv_obj_set_pos(s_date, 24, 152);
    lv_obj_set_style_text_color(s_date, uiTheme::MUTED, 0);
    lv_obj_set_style_text_font(s_date, &lv_font_montserrat_18, 0);
    lv_label_set_text(s_date, "");

    // ── ŚRODKOWA: ikona 90×90 (x 220..309, y 52..141) ──────────────────────
    s_iconBuf = (lv_color_t *)heap_caps_malloc(90*90*sizeof(lv_color_t), MALLOC_CAP_SPIRAM);
    if (!s_iconBuf) s_iconBuf = (lv_color_t *)malloc(90*90*sizeof(lv_color_t));
    s_iconCanvas = lv_canvas_create(s_scr);
    lv_obj_set_pos(s_iconCanvas, 220, 52);
    lv_obj_set_size(s_iconCanvas, 90, 90);
    if (s_iconBuf) lv_canvas_set_buffer(s_iconCanvas, s_iconBuf, 90, 90, LV_IMG_CF_TRUE_COLOR);
    lv_canvas_fill_bg(s_iconCanvas, uiTheme::BG, LV_OPA_COVER);

    // ── PRAWA: temperatura + opis + słońce (x 320..799, y 52..219) ─────────
    s_temp = lv_label_create(s_scr);
    lv_obj_set_pos(s_temp, 322, 52);
    lv_obj_set_style_text_color(s_temp, uiTheme::TEXT, 0);
    lv_obj_set_style_text_font(s_temp, &lv_font_montserrat_30, 0);
    lv_label_set_text(s_temp, "--\u00B0C");

    s_desc = lv_label_create(s_scr);
    lv_obj_set_pos(s_desc, 322, 148);
    lv_obj_set_style_text_color(s_desc, uiTheme::MUTED, 0);
    lv_obj_set_style_text_font(s_desc, &lv_font_montserrat_20, 0);
    lv_label_set_text(s_desc, "");

    s_sunRow = lv_label_create(s_scr);
    lv_obj_set_pos(s_sunRow, 322, 178);
    lv_obj_set_style_text_color(s_sunRow, lv_color_hex(0xFFD93D), 0);
    lv_obj_set_style_text_font(s_sunRow, &lv_font_montserrat_16, 0);
    lv_label_set_text(s_sunRow, "");

    // ── separator poziomy ────────────────────────────────────────────────────
    lv_obj_t *sep1 = lv_obj_create(s_scr);
    lv_obj_set_pos(sep1, 0, 218);
    lv_obj_set_size(sep1, 800, 1);
    lv_obj_set_style_bg_color(sep1, uiTheme::CARD, 0);
    lv_obj_set_style_border_width(sep1, 0, 0);
    lv_obj_set_style_radius(sep1, 0, 0);

    // ── 4 KAFELKI (y 222..299) ───────────────────────────────────────────────
    const int cw = 182, ch = 74, gap = 8;
    const int cx0 = (800 - 4*cw - 3*gap) / 2;
    makeCard(s_scr, s_cardVal[0], "WIATR",      cx0,             222, cw, ch);
    makeCard(s_scr, s_cardVal[1], "WILGOTNOSC", cx0+cw+gap,      222, cw, ch);
    makeCard(s_scr, s_cardVal[2], "OPAD",       cx0+2*(cw+gap),  222, cw, ch);
    makeCard(s_scr, s_cardVal[3], "TEMPERATURA", cx0+3*(cw+gap), 222, cw, ch);

    // ── separator ────────────────────────────────────────────────────────────
    lv_obj_t *sep2 = lv_obj_create(s_scr);
    lv_obj_set_pos(sep2, 0, 298);
    lv_obj_set_size(sep2, 800, 1);
    lv_obj_set_style_bg_color(sep2, uiTheme::CARD, 0);
    lv_obj_set_style_border_width(sep2, 0, 0);
    lv_obj_set_style_radius(sep2, 0, 0);


    // -- PROGNOZA 3 DNI (y 302..390): karta 248x86, ikonka po prawej --------
    const int fw = 248, fh = 86, fgap = 8;
    const int fx0 = (800 - 3*fw - 2*fgap) / 2;
    for (int i = 0; i < 3; ++i) {
        int x = fx0 + i*(fw+fgap);
        lv_obj_t *card = lv_obj_create(s_scr);
        lv_obj_set_pos(card, x, 302);
        lv_obj_set_size(card, fw, fh);
        lv_obj_set_style_bg_color(card, uiTheme::CARD, 0);
        lv_obj_set_style_border_width(card, 0, 0);
        lv_obj_set_style_radius(card, 10, 0);
        lv_obj_set_style_pad_all(card, 0, 0);

        // Dzien (muted, 16px)
        s_fcastDay[i] = lv_label_create(card);
        lv_obj_set_pos(s_fcastDay[i], 10, 7);
        lv_obj_set_size(s_fcastDay[i], 180, 20);
        lv_obj_set_style_text_color(s_fcastDay[i], uiTheme::MUTED, 0);
        lv_obj_set_style_text_font(s_fcastDay[i], &lv_font_montserrat_16, 0);
        lv_label_set_text(s_fcastDay[i], "");

        // Temperatura max/min (bold 30px)
        s_fcastTemp[i] = lv_label_create(card);
        lv_obj_set_pos(s_fcastTemp[i], 10, 28);
        lv_obj_set_size(s_fcastTemp[i], 180, 36);
        lv_obj_set_style_text_color(s_fcastTemp[i], uiTheme::TEXT, 0);
        lv_obj_set_style_text_font(s_fcastTemp[i], &lv_font_montserrat_30, 0);
        lv_label_set_text(s_fcastTemp[i], "--");

        // Opad (muted, 14px)
        s_fcastPrecip[i] = lv_label_create(card);
        lv_obj_set_pos(s_fcastPrecip[i], 10, 66);
        lv_obj_set_size(s_fcastPrecip[i], 180, 16);
        lv_obj_set_style_text_color(s_fcastPrecip[i], uiTheme::MUTED, 0);
        lv_obj_set_style_text_font(s_fcastPrecip[i], &lv_font_montserrat_14, 0);
        lv_label_set_text(s_fcastPrecip[i], "");

        // Ikonka po prawej (50x50), wycentrowana pionowo
        s_fcastIconBuf[i] = (lv_color_t *)heap_caps_malloc(
            FCAST_ICON_W * FCAST_ICON_H * sizeof(lv_color_t), MALLOC_CAP_SPIRAM);
        if (!s_fcastIconBuf[i])
            s_fcastIconBuf[i] = (lv_color_t *)malloc(FCAST_ICON_W * FCAST_ICON_H * sizeof(lv_color_t));
        s_fcastIconCanvas[i] = lv_canvas_create(card);
        lv_obj_set_pos(s_fcastIconCanvas[i], fw - FCAST_ICON_W - 6, (fh - FCAST_ICON_H)/2);
        lv_obj_set_size(s_fcastIconCanvas[i], FCAST_ICON_W, FCAST_ICON_H);
        if (s_fcastIconBuf[i])
            lv_canvas_set_buffer(s_fcastIconCanvas[i], s_fcastIconBuf[i],
                                 FCAST_ICON_W, FCAST_ICON_H, LV_IMG_CF_TRUE_COLOR);
        lv_canvas_fill_bg(s_fcastIconCanvas[i], uiTheme::CARD, LV_OPA_COVER);
    }


    // ── PASEK DOLNY rotacyjny (y 392..429) — tuż nad nav barem (430) ─────────
    lv_obj_t *tickerBg = lv_obj_create(s_scr);
    lv_obj_set_pos(tickerBg, 0, 392);
    lv_obj_set_size(tickerBg, 800, 36);
    lv_obj_set_style_bg_color(tickerBg, uiTheme::CARD, 0);
    lv_obj_set_style_bg_opa(tickerBg, LV_OPA_60, 0);
    lv_obj_set_style_border_width(tickerBg, 0, 0);
    lv_obj_set_style_radius(tickerBg, 0, 0);
    lv_obj_set_style_pad_all(tickerBg, 0, 0);

    s_ticker = lv_label_create(tickerBg);
    lv_obj_set_pos(s_ticker, 48, 8);          // przesunięty w prawo — miejsce na ikonkę
    lv_obj_set_size(s_ticker, 752, 22);
    lv_obj_set_style_text_align(s_ticker, LV_TEXT_ALIGN_LEFT, 0);
    lv_obj_set_style_text_color(s_ticker, uiTheme::MUTED, 0);
    lv_obj_set_style_text_font(s_ticker, &lv_font_montserrat_16, 0);
    lv_label_set_text(s_ticker, "");

    // Moon canvas 40×40 po lewej stronie tickera
    s_moonBuf = (lv_color_t *)heap_caps_malloc(MOON_W * MOON_H * sizeof(lv_color_t), MALLOC_CAP_SPIRAM);
    if (!s_moonBuf) s_moonBuf = (lv_color_t *)malloc(MOON_W * MOON_H * sizeof(lv_color_t));
    s_moonCanvas = lv_canvas_create(tickerBg);
    lv_obj_set_pos(s_moonCanvas, 4, (36 - MOON_H) / 2);   // wycentrowany pionowo
    lv_obj_set_size(s_moonCanvas, MOON_W, MOON_H);
    if (s_moonBuf)
        lv_canvas_set_buffer(s_moonCanvas, s_moonBuf, MOON_W, MOON_H, LV_IMG_CF_TRUE_COLOR);
    lv_canvas_fill_bg(s_moonCanvas, uiTheme::CARD, LV_OPA_COVER);
    lv_obj_add_flag(s_moonCanvas, LV_OBJ_FLAG_HIDDEN);
}

void uiWeatherShow() { lv_scr_load(s_scr); }
lv_obj_t *uiWeatherScr() { return s_scr; }

// ============================================================================
// uiWeatherRefresh
// ============================================================================
void uiWeatherRefresh(const AppState &state) {
    if (lv_scr_act() != s_scr) return;

    unsigned long now = millis();

    // ── ZEGAR + DATA ─────────────────────────────────────────────────────────
    struct tm ti = {};
    bool hasTime = state.timeSynced && getLocalTime(&ti, 0);
    if (hasTime) {
        char buf[24];
        snprintf(buf, sizeof(buf), "%02d:%02d", ti.tm_hour, ti.tm_min);
        if (strcmp(buf, s_lastClock) != 0) {
            strncpy(s_lastClock, buf, sizeof(s_lastClock)-1);
            lv_label_set_text(s_clock, buf);
        }
        static const char *dow[]    = {"niedziela","poniedzialek","wtorek","sroda","czwartek","piatek","sobota"};
        static const char *months[] = {"stycznia","lutego","marca","kwietnia","maja","czerwca",
                                       "lipca","sierpnia","wrzesnia","pazdziernika","listopada","grudnia"};
        char dateBuf[52];
        snprintf(dateBuf, sizeof(dateBuf), "%s, %d %s",
                 dow[ti.tm_wday], ti.tm_mday, months[ti.tm_mon]);
        if (strcmp(dateBuf, s_lastDate) != 0) {
            strncpy(s_lastDate, dateBuf, sizeof(s_lastDate)-1);
            lv_label_set_text(s_date, dateBuf);
        }
    } else {
        if (s_lastClock[0]) { s_lastClock[0]=0; lv_label_set_text(s_clock, "--:--"); }
        if (s_lastDate[0])  { s_lastDate[0]=0;  lv_label_set_text(s_date, ""); }
    }

    // ── POGODA ───────────────────────────────────────────────────────────────
    if (state.weatherAvailable) {
        const WeatherCurrent &w = state.weather;
        char buf[32];

        if (w.weatherCode != s_lastWCode) {
            s_lastWCode = w.weatherCode;
            drawWeatherIcon(w.weatherCode, s_iconCanvas);
        }
        if (w.tempC != s_lastTempC) {
            s_lastTempC = w.tempC;
            snprintf(buf, sizeof(buf), "%.0f\u00B0C", w.tempC);
            lv_label_set_text(s_temp, buf);
        }
        if (strcmp(w.description, lv_label_get_text(s_desc)) != 0)
            lv_label_set_text(s_desc, w.description);

        // Słońce w bloku głównym
        if (w.sunrise[0] && w.sunset[0]) {
            snprintf(buf, sizeof(buf), "^%s  v%s", w.sunrise, w.sunset);
            lv_label_set_text(s_sunRow, buf);
        }

        // Kafelki
        if (w.windMs != s_lastWindMs) {
            s_lastWindMs = w.windMs;
            if (!isnan(w.windMs)) snprintf(buf, sizeof(buf), "%.1fm/s", w.windMs);
            else snprintf(buf, sizeof(buf), "--");
            lv_label_set_text(s_cardVal[0], buf);
        }
        if (w.humidity != s_lastHumidity) {
            s_lastHumidity = w.humidity;
            if (w.humidity>=0) snprintf(buf, sizeof(buf), "%d%%", w.humidity);
            else snprintf(buf, sizeof(buf), "--");
            lv_label_set_text(s_cardVal[1], buf);
        }
        float precip = (float)w.forecast[0].precip;
        if (precip != s_lastPrecip) {
            s_lastPrecip = precip;
            if (!isnan(precip)) snprintf(buf, sizeof(buf), "%.1fmm", precip);
            else snprintf(buf, sizeof(buf), "--");
            lv_label_set_text(s_cardVal[2], buf);
        }
        // 4. kafelek: dzisiaj min..max
        if (!isnan(w.forecast[0].tempMin) && !isnan(w.forecast[0].tempMax)) {
            snprintf(buf, sizeof(buf), "%.0f/%.0f\u00B0",
                     w.forecast[0].tempMin, w.forecast[0].tempMax);
        } else {
            snprintf(buf, sizeof(buf), "--");
        }
        lv_label_set_text(s_cardVal[3], buf);

        // Prognoza 3 dni — dzień + ikonka + temperatura czytelnie
        static const char *dayNames[] = {
            "Niedziela","Poniedzialek","Wtorek","Sroda","Czwartek","Piatek","Sobota"
        };
        int todayYday = hasTime ? ti.tm_yday : -1;
        for (int i = 0; i < 3; ++i) {
            const ForecastDay &fd = w.forecast[i];

            // Wyznacz dzień tygodnia
            int dayIdx = -1;
            if (todayYday >= 0 && strlen(fd.date) >= 5) {
                int month, day;
                if (sscanf(fd.date, "%d-%d", &month, &day) == 2) {
                    struct tm ft = {};
                    ft.tm_year = ti.tm_year; ft.tm_mon = month-1; ft.tm_mday = day;
                    mktime(&ft); dayIdx = ft.tm_wday;
                }
            }
            // Dla i==0 (dzisiaj): pokaż "Dzisiaj"
            const char *dayLabel = (i == 0) ? "Dzisiaj"
                                 : (dayIdx >= 0 ? dayNames[dayIdx % 7] : fd.date);
            lv_label_set_text(s_fcastDay[i], dayLabel);

            // Temperatura: "19° / 11°" (max / min)
            if (!isnan(fd.tempMin) && !isnan(fd.tempMax)) {
                snprintf(buf, sizeof(buf), "%.0f\u00B0 / %.0f\u00B0",
                         fd.tempMax, fd.tempMin);
            } else {
                snprintf(buf, sizeof(buf), "--");
            }
            lv_label_set_text(s_fcastTemp[i], buf);

            // Opad — pokaż tylko jeśli > 0.1mm
            if (s_fcastPrecip[i]) {
                if (!isnan(fd.precip) && fd.precip > 0.1f) {
                    snprintf(buf, sizeof(buf), "opad: %.1f mm", fd.precip);
                    lv_label_set_text(s_fcastPrecip[i], buf);
                } else {
                    lv_label_set_text(s_fcastPrecip[i], "");
                }
            }

            // Ikonka pogody — rysuj przy zmianie kodu
            if (fd.weatherCode != s_fcastLastCode[i] && s_fcastIconBuf[i]) {
                s_fcastLastCode[i] = fd.weatherCode;
                if (s_fcastIconCanvas[i]) {
                    lv_canvas_fill_bg(s_fcastIconCanvas[i], uiTheme::CARD, LV_OPA_COVER);
                    drawWeatherIconSmall(fd.weatherCode, s_fcastIconCanvas[i]);
                }
            }
        }
    } else {
        // brak danych
        lv_label_set_text(s_temp, "--\u00B0C");
        lv_label_set_text(s_desc, "Oczekiwanie...");
        lv_label_set_text(s_sunRow, "");
        for (int i=0;i<4;i++) lv_label_set_text(s_cardVal[i], "--");
        for (int i=0;i<3;i++) {
            lv_label_set_text(s_fcastDay[i], "");
            lv_label_set_text(s_fcastTemp[i], "");
            if (s_fcastPrecip[i]) lv_label_set_text(s_fcastPrecip[i], "");
            if (s_fcastIconCanvas[i])
                lv_canvas_fill_bg(s_fcastIconCanvas[i], uiTheme::CARD, LV_OPA_COVER);
            s_fcastLastCode[i] = -1;
        }
        if (s_lastWCode != -1) { s_lastWCode=-1; lv_canvas_fill_bg(s_iconCanvas, uiTheme::BG, LV_OPA_COVER); }
    }

    // ── PASEK DOLNY ───────────────────────────────────────────────────────────
    // Faza księżyca — przelicz raz dziennie
    if (hasTime) {
        int y=ti.tm_year+1900, mo=ti.tm_mon+1, d=ti.tm_mday;
        static int s_lastMoonDay = -1;
        if (d != s_lastMoonDay) {
            s_lastMoonDay = d;
            float phase = moonPhase(y, mo, d);

            // Mapuj fazę 0.0-1.0 na numer ikonki 0-7
            int phaseIdx;
            if      (phase < 0.063f || phase > 0.937f) phaseIdx = 0; // nów
            else if (phase < 0.187f)                   phaseIdx = 1; // przybyw. sierp
            else if (phase < 0.312f)                   phaseIdx = 2; // pierwsza kwadra
            else if (phase < 0.437f)                   phaseIdx = 3; // przybyw. garb
            else if (phase < 0.563f)                   phaseIdx = 4; // pełnia
            else if (phase < 0.687f)                   phaseIdx = 5; // ubyw. garb
            else if (phase < 0.812f)                   phaseIdx = 6; // ostatnia kwadra
            else                                        phaseIdx = 7; // ubyw. sierp

            // Załaduj ikonkę jeśli się zmieniła
            if (s_moonBuf && sdAvailable()) {
                if (phaseIdx != s_lastMoonPhase) {
                    s_lastMoonPhase = phaseIdx;
                    if (!sdLoadMoonIcon(phaseIdx, s_moonBuf))
                        s_lastMoonPhase = -1;  // reset — spróbuj ponownie następnym razem
                    else
                        lv_obj_invalidate(s_moonCanvas);
                }
                // Zawsze: jeśli faza załadowana → pokaż; inaczej ukryj
            }

            // Tekst obok ikonki: "X dni do pełni" — zapisz do bufora
            int dtFull = daysToFullMoon(y, mo, d);
            // (tekst generowany na bieżąco w sekcji wyświetlania tickera poniżej)

            // Imieniny z SD
            const char *nd = sdNameday(mo, d);
            snprintf(s_tickerNameday, sizeof(s_tickerNameday),
                     nd[0] ? "Imieniny: %s" : "", nd);

            // Swieta rodzinne — dzisiaj lub zapowiedz na jutro
            const char *famToday    = sdFamilyEvent(mo, d, false);
            const char *famTomorrow = sdFamilyEvent(mo, d, true);
            if (famToday[0])
                snprintf(s_tickerFamily, sizeof(s_tickerFamily), "Dzisiaj: %s", famToday);
            else if (famTomorrow[0])
                snprintf(s_tickerFamily, sizeof(s_tickerFamily), "Jutro: %s", famTomorrow);
            else
                s_tickerFamily[0] = '\0';

            // Popularne swieta polskie z /holidays.csv — dzisiaj lub jutro
            const char *holToday    = sdHoliday(mo, d, false);
            const char *holTomorrow = sdHoliday(mo, d, true);
            if (holToday[0])
                snprintf(s_tickerHoliday, sizeof(s_tickerHoliday), "%s", holToday);
            else if (holTomorrow[0])
                snprintf(s_tickerHoliday, sizeof(s_tickerHoliday), "Jutro: %s", holTomorrow);
            else
                s_tickerHoliday[0] = '\0';

            // Cache do wyświetlenia tekstu księżyca (faza + dni)
            // Używamy globalnego bufora — nazwijmy go s_tickerMoonText
        }
    }

    // Lot
    const bool hasFlight = state.hasBestEastCandidate || state.hasSelectedFlight;
    if (hasFlight) {
        const char *cs = state.hasBestEastCandidate
                         ? state.bestEastCandidate.aircraft.callsign
                         : state.selectedFlight.aircraft.callsign;
        snprintf(s_tickerFlight, sizeof(s_tickerFlight), "Lot w poblizu: %s", cs);
    } else {
        snprintf(s_tickerFlight, sizeof(s_tickerFlight), "Cisza w okolicy");
    }

    // Rotacja co 6 sekund — pomijaj puste sloty
    if (s_tickerMs == 0) s_tickerMs = now;
    if (now - s_tickerMs >= 6000) {
        s_tickerMs = now;
        // Sloty: 0=ksiezyc 1=imieniny 2=swieto_rodzinne 3=lot
        int tries = 0;
        do {
            s_tickerSlot = (s_tickerSlot + 1) % 5;
            tries++;
        } while (tries < 5 && (
            (s_tickerSlot == 1 && s_tickerNameday[0] == '\0') ||
            (s_tickerSlot == 2 && s_tickerFamily[0]  == '\0') ||
            (s_tickerSlot == 3 && s_tickerHoliday[0] == '\0')
        ));
    }

    // Aktualizuj widok tickera
    if (s_tickerSlot == 0) {
        // Slot księżyca: ikonka po lewej + tekst wyśrodkowany w całym pasku
        if (hasTime && s_moonCanvas) {
            int y=ti.tm_year+1900, mo=ti.tm_mon+1, d=ti.tm_mday;
            int dtFull = daysToFullMoon(y, mo, d);
            char moonTxt[48];
            const char *phaseName = moonPhaseName(moonPhase(y, mo, d));
            if (dtFull == 0)
                snprintf(moonTxt, sizeof(moonTxt), "%s  -  Pelnia!", phaseName);
            else if (dtFull > 0)
                snprintf(moonTxt, sizeof(moonTxt), "%s  -  %d dni do pelni", phaseName, dtFull);
            else
                snprintf(moonTxt, sizeof(moonTxt), "%s", phaseName);

            if (strcmp(moonTxt, lv_label_get_text(s_ticker)) != 0)
                lv_label_set_text(s_ticker, moonTxt);
            lv_obj_set_style_text_color(s_ticker, uiTheme::MUTED, 0);
            // Tekst wyśrodkowany w całym pasku (obok ikonki ale CENTER względem całości)
            lv_obj_set_pos(s_ticker, 48, 8);
            lv_obj_set_size(s_ticker, 744, 22);
            lv_obj_set_style_text_align(s_ticker, LV_TEXT_ALIGN_CENTER, 0);
        }
        // Ikonka
        if (sdAvailable() && s_lastMoonPhase >= 0)
            lv_obj_clear_flag(s_moonCanvas, LV_OBJ_FLAG_HIDDEN);
        else
            lv_obj_add_flag(s_moonCanvas, LV_OBJ_FLAG_HIDDEN);
    } else {
        // Inne sloty: ukryj ikonkę księżyca, wyśrodkuj tekst
        lv_obj_add_flag(s_moonCanvas, LV_OBJ_FLAG_HIDDEN);
        lv_obj_set_pos(s_ticker, 8, 8);
        lv_obj_set_size(s_ticker, 790, 22);
        lv_obj_set_style_text_align(s_ticker, LV_TEXT_ALIGN_CENTER, 0);
        const char *txt;
        lv_color_t  tc;
        if (s_tickerSlot == 1) {
            txt = s_tickerNameday;
            tc  = uiTheme::MUTED;
        } else if (s_tickerSlot == 2) {
            txt = s_tickerFamily;
            tc  = lv_color_make(255, 180, 100);   // zloto = rodzinne
        } else if (s_tickerSlot == 3) {
            txt = s_tickerHoliday;
            tc  = lv_color_make(100, 180, 255);   // niebieski = popularne swieto
        } else {  // slot 4 = lot
            txt = s_tickerFlight;
            tc  = hasFlight ? uiTheme::ACCENT : uiTheme::MUTED;
        }
        if (strcmp(txt, lv_label_get_text(s_ticker)) != 0)
            lv_label_set_text(s_ticker, txt);
        lv_obj_set_style_text_color(s_ticker, tc, 0);
    }
}

void uiWeatherRefreshSettings(const AppState &) {}
