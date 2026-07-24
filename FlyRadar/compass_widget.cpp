#include "compass_widget.h"
#include "ui_theme.h"
#include <math.h>
#include <string.h>
#include <Arduino.h>

// ============================================================================
// Kompas 80×80 px
// ── Anatomia ──────────────────────────────────────────────────────────────
//   Tło: ciemny okrąg
//   Siatka: N/S/E/W ticksy co 90°, małe co 45°
//   Wskazówka: czerwona strzałka w kierunku samolotu
//   Centrum: biała kropka
// ============================================================================

static constexpr int CW = 80;
static constexpr int CH = 80;
static constexpr int CR = 36;   // promień okręgu kompasu
static constexpr int CX = 40;
static constexpr int CY = 40;

// Bufor PSRAM per canvas — alokujemy globalnie przy compassCreate
// Ponieważ może być wiele kompasów (lot + radar), każdy ma swój bufor.
// Dla prostoty: max 2 kompasy × 80×80×2 = 25.6KB

static lv_color_t *s_bufs[2]     = {};
static lv_obj_t   *s_canvases[2] = {};
static int         s_count        = 0;

static void setPx(lv_obj_t *cv, int x, int y, lv_color_t col) {
    if (x >= 0 && x < CW && y >= 0 && y < CH) {
        lv_canvas_set_px_color(cv, x, y, col);
        lv_canvas_set_px_opa(cv, x, y, LV_OPA_COVER);
    }
}

static void drawCircleOutline(lv_obj_t *cv, int cx, int cy, int r, lv_color_t col) {
    // Midpoint circle
    int x = 0, y = r, d = 1 - r;
    while (x <= y) {
        for (int s = -1; s <= 1; s += 2) {
            setPx(cv, cx+x*s, cy+y,   col); setPx(cv, cx+x*s, cy-y,   col);
            setPx(cv, cx+y*s, cy+x,   col); setPx(cv, cx+y*s, cy-x,   col);
        }
        if (d < 0) d += 2*x+3;
        else       { d += 2*(x-y)+5; y--; }
        x++;
    }
}

static void drawLine(lv_obj_t *cv, int x0, int y0, int x1, int y1, lv_color_t col) {
    int dx = abs(x1-x0), sx = x0<x1?1:-1;
    int dy = -abs(y1-y0), sy = y0<y1?1:-1;
    int err = dx+dy;
    while (true) {
        setPx(cv, x0, y0, col);
        if (x0==x1 && y0==y1) break;
        int e2 = 2*err;
        if (e2 >= dy) { err += dy; x0 += sx; }
        if (e2 <= dx) { err += dx; y0 += sy; }
    }
}

lv_obj_t *compassCreate(lv_obj_t *parent, int x, int y) {
    if (s_count >= 2) return nullptr;
    int idx = s_count++;

    lv_color_t *buf = (lv_color_t *)heap_caps_malloc(
        CW * CH * sizeof(lv_color_t), MALLOC_CAP_SPIRAM);
    if (!buf) buf = (lv_color_t *)malloc(CW * CH * sizeof(lv_color_t));
    if (!buf) return nullptr;

    s_bufs[idx] = buf;

    lv_obj_t *cv = lv_canvas_create(parent);
    lv_obj_set_pos(cv, x, y);
    lv_obj_set_size(cv, CW, CH);
    lv_canvas_set_buffer(cv, buf, CW, CH, LV_IMG_CF_TRUE_COLOR);
    s_canvases[idx] = cv;

    compassClear(cv);
    return cv;
}

void compassClear(lv_obj_t *canvas) {
    if (!canvas) return;
    lv_canvas_fill_bg(canvas, uiTheme::BG, LV_OPA_COVER);
    // Narysuj pusty okrąg z N/S/E/W ale bez strzałki
    auto dimCol  = lv_color_make(40, 50, 65);
    auto tickCol = lv_color_make(80, 90, 110);
    drawCircleOutline(canvas, CX, CY, CR, dimCol);
    // Ticksy kierunkowe
    static const char *dirs[] = {"N","E","S","W"};
    static const int   angles[] = {0, 90, 180, 270};
    lv_draw_label_dsc_t ld; lv_draw_label_dsc_init(&ld);
    ld.font  = &lv_font_montserrat_12;
    ld.color = tickCol;
    ld.align = LV_TEXT_ALIGN_CENTER;
    for (int i = 0; i < 4; ++i) {
        double r = angles[i] * 3.14159265 / 180.0;
        int tx = CX + (int)(sin(r) * (CR - 4));
        int ty = CY - (int)(cos(r) * (CR - 4));
        lv_canvas_draw_text(canvas, tx-6, ty-6, 12, &ld, dirs[i]);
    }
    lv_obj_invalidate(canvas);
}

void compassUpdate(lv_obj_t *canvas, double bearingDeg) {
    if (!canvas) return;
    if (isnan(bearingDeg) || isinf(bearingDeg)) { compassClear(canvas); return; }

    // Wyczyść i narysuj tło
    lv_canvas_fill_bg(canvas, uiTheme::BG, LV_OPA_COVER);

    auto ringCol  = lv_color_make(40, 50, 65);
    auto tickCol  = lv_color_make(80, 90, 110);
    auto arrowCol = lv_color_make(60, 169, 252);   // niebieski — kolor ACCENT
    auto nCol     = lv_color_make(240, 70,  60);   // czerwone N

    // Okrąg
    drawCircleOutline(canvas, CX, CY, CR, ringCol);

    // Ticksy kardynalne + litery
    lv_draw_label_dsc_t ld; lv_draw_label_dsc_init(&ld);
    ld.font  = &lv_font_montserrat_12;
    ld.align = LV_TEXT_ALIGN_CENTER;

    static const char *dirs[]  = {"N","E","S","W"};
    static const int   angs[]  = { 0, 90, 180, 270};
    for (int i = 0; i < 4; ++i) {
        double r = angs[i] * 3.14159265 / 180.0;
        // Tick kreska
        int x0 = CX + (int)(sin(r) * (CR - 0));
        int y0 = CY - (int)(cos(r) * (CR - 0));
        int x1 = CX + (int)(sin(r) * (CR - 5));
        int y1 = CY - (int)(cos(r) * (CR - 5));
        drawLine(canvas, x0, y0, x1, y1, tickCol);
        // Etykieta
        int tx = CX + (int)(sin(r) * (CR - 12));
        int ty = CY - (int)(cos(r) * (CR - 12));
        ld.color = (i == 0) ? nCol : tickCol;
        lv_canvas_draw_text(canvas, tx-6, ty-6, 12, &ld, dirs[i]);
    }

    // Małe ticksy co 45°
    for (int a = 45; a < 360; a += 90) {
        double r = a * 3.14159265 / 180.0;
        int x0 = CX + (int)(sin(r) * CR);
        int y0 = CY - (int)(cos(r) * CR);
        int x1 = CX + (int)(sin(r) * (CR - 3));
        int y1 = CY - (int)(cos(r) * (CR - 3));
        drawLine(canvas, x0, y0, x1, y1, tickCol);
    }

    // Strzałka w kierunku samolotu (bearing)
    double brad = bearingDeg * 3.14159265 / 180.0;
    int arrowLen = CR - 8;
    int tipX  = CX + (int)(sin(brad) * arrowLen);
    int tipY  = CY - (int)(cos(brad) * arrowLen);
    int tailX = CX - (int)(sin(brad) * 8);
    int tailY = CY + (int)(cos(brad) * 8);

    // Grubsza strzałka: 3 linie równoległe
    for (int offset = -1; offset <= 1; ++offset) {
        int ox = (int)(cos(brad) * offset);
        int oy = (int)(sin(brad) * offset);
        drawLine(canvas, tailX+ox, tailY+oy, tipX+ox, tipY+oy, arrowCol);
    }

    // Grot strzałki (trójkąt)
    int wingLen = 5;
    int w1x = tipX - (int)(sin(brad) * wingLen) - (int)(cos(brad) * wingLen);
    int w1y = tipY + (int)(cos(brad) * wingLen) - (int)(sin(brad) * wingLen);
    int w2x = tipX + (int)(sin(brad) * wingLen) - (int)(cos(brad) * wingLen);
    int w2y = tipY - (int)(cos(brad) * wingLen) - (int)(sin(brad) * wingLen);
    drawLine(canvas, tipX, tipY, w1x, w1y, arrowCol);
    drawLine(canvas, tipX, tipY, w2x, w2y, arrowCol);

    // Środkowa kropka
    for (int dy = -2; dy <= 2; ++dy)
        for (int dx = -2; dx <= 2; ++dx)
            if (dx*dx+dy*dy <= 4)
                setPx(canvas, CX+dx, CY+dy, lv_color_make(200,200,200));

    // Wartość kątowa w centrum na dole
    char degBuf[8];
    snprintf(degBuf, sizeof(degBuf), "%.0f°", bearingDeg);
    ld.font  = &lv_font_montserrat_12;
    ld.color = arrowCol;
    ld.align = LV_TEXT_ALIGN_CENTER;
    lv_canvas_draw_text(canvas, CX-18, CY+18, 36, &ld, degBuf);

    lv_obj_invalidate(canvas);
}
