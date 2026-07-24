#include "ui_radar.h"
#include "ui_theme.h"
#include "ui_router.h"
#include "geo_utils.h"
#include "config.h"
#include <Arduino.h>
#include <lvgl.h>
#include <math.h>
#include <string.h>
#include <stdio.h>

namespace {

lv_obj_t *s_scr       = nullptr;
lv_obj_t *s_canvas    = nullptr;
lv_color_t *s_cbuf    = nullptr;

// Panel boczny — przeprojektowany
lv_obj_t *s_panelTitle   = nullptr;  // "BRAK SYGNALU" / "LECI NAD NAMI"
lv_obj_t *s_infoCallsign = nullptr;
lv_obj_t *s_infoStatus   = nullptr;
lv_obj_t *s_infoRoute    = nullptr;  // trasa np "BCN -> KRK"
lv_obj_t *s_infoAlt      = nullptr;
lv_obj_t *s_infoSpd      = nullptr;
lv_obj_t *s_infoDist     = nullptr;
lv_obj_t *s_infoVario    = nullptr;  // wznoszenie/opadanie
lv_obj_t *s_infoCount    = nullptr;  // ile samolotów w zasięgu
lv_obj_t *s_lastUpd      = nullptr;

constexpr int CX = 260;
constexpr int CY = 215;
constexpr int GRID_R = 170;
constexpr int RADAR_W = 580;
constexpr int RADAR_H = 430;

unsigned long s_lastRedraw = 0;
int s_lastVersion = -1;

// ---------- helpers ----------

static void setPx(lv_obj_t *cv, int x, int y, lv_color_t col) {
    if (x >= 0 && x < RADAR_W && y >= 0 && y < RADAR_H) {
        lv_canvas_set_px_color(cv, x, y, col);
        lv_canvas_set_px_opa(cv, x, y, LV_OPA_COVER);
    }
}

static void fillCircle(lv_obj_t *cv, int cx, int cy, int r, lv_color_t col) {
    for (int dy = -r; dy <= r; ++dy)
        for (int dx = -r; dx <= r; ++dx)
            if (dx * dx + dy * dy <= r * r)
                setPx(cv, cx + dx, cy + dy, col);
}

static void drawLine(lv_obj_t *cv, int x0, int y0, int x1, int y1, lv_color_t col) {
    int dx = abs(x1 - x0), sx = x0 < x1 ? 1 : -1;
    int dy = -abs(y1 - y0), sy = y0 < y1 ? 1 : -1;
    int err = dx + dy;
    while (true) {
        setPx(cv, x0, y0, col);
        if (x0 == x1 && y0 == y1) break;
        int e2 = 2 * err;
        if (e2 >= dy) { err += dy; x0 += sx; }
        if (e2 <= dx) { err += dx; y0 += sy; }
    }
}

// Wypełniony trójkąt (scanline) — poprawna kolejność lx≤rx
static void fillTriangle(lv_obj_t *cv, int x0, int y0, int x1, int y1, int x2, int y2, lv_color_t col) {
    auto swapIf = [](int &a, int &b) { int t = a; a = b; b = t; };
    if (y0 > y1) { swapIf(x0, x1); swapIf(y0, y1); }
    if (y0 > y2) { swapIf(x0, x2); swapIf(y0, y2); }
    if (y1 > y2) { swapIf(x1, x2); swapIf(y1, y2); }
    auto drawSpan = [&](int y, int lx, int rx) {
        if (lx > rx) swapIf(lx, rx);
        if (lx < 0) lx = 0;
        if (rx >= RADAR_W) rx = RADAR_W - 1;
        if (y < 0 || y >= RADAR_H || lx > rx) return;
        for (int x = lx; x <= rx; ++x) setPx(cv, x, y, col);
    };
    auto interpX = [](int x0, int x2, float t) -> int {
        if (isnan(t) || isinf(t)) t = 0;
        return x0 + (int)((x2 - x0) * t);
    };
    int midY = y1;
    float dy1 = (float)(y1 - y0) / (float)(y2 - y0 + 1);
    int midX = interpX(x0, x2, dy1);
    // Góra (y0 → y1)
    for (int y = y0; y <= midY; ++y) {
        float t = (float)(y - y0) / (float)(midY - y0 + 1);
        drawSpan(y, interpX(x0, midX, t), interpX(x1, x2, t));
    }
    // Dół (y1 → y2)
    for (int y = midY + 1; y <= y2; ++y) {
        float t = (float)(y - midY - 1) / (float)(y2 - midY);
        drawSpan(y, interpX(midX, x2, t), interpX(x1, x2, t));
    }
}

    // Strzałka kierunkowa (trójkąt nos + statek)
static void drawHeadingTriangle(lv_obj_t *cv, int cx, int cy, double headingDeg, lv_color_t col) {
    if (isnan(headingDeg) || isinf(headingDeg)) headingDeg = 0;
    double rad = radians(headingDeg);
    double sn = sin(rad), cs = cos(rad);
    int tipX = cx + (int)(sn * 8);
    int tipY = cy - (int)(cs * 8);
    int baseX = cx - (int)(sn * 3);
    int baseY = cy + (int)(cs * 3);
    int wingX = (int)(cs * 4);
    int wingY = (int)(sn * 4);
    fillTriangle(cv, tipX, tipY, baseX + wingX, baseY + wingY, baseX - wingX, baseY - wingY, col);
}

// ---------- etykieta: tylko callsign ----------

static void drawTag(lv_obj_t *cv, int px, int py, const AircraftState &a, lv_color_t col) {
    if (strlen(a.callsign) == 0) return;

    // Szerokość proporcjonalna do długości callsign
    const int cw = 7;   // szerokość znaku ~7px (montserrat_12)
    const int ch = 14;  // wysokość linii
    int bw = (int)strlen(a.callsign) * cw + 6;
    int bh = ch + 4;

    // Po prawej lub lewej stronie samolotu
    bool right = px < CX;
    int tx = right ? px + 8 : px - bw - 8;
    int ty = py - bh / 2;

    // Zabezpieczenie przed wychodzeniem poza ekran
    if (tx < 1) tx = 1;
    if (tx + bw > RADAR_W - 1) tx = RADAR_W - bw - 1;
    if (ty < 1) ty = 1;
    if (ty + bh > RADAR_H - 1) ty = RADAR_H - bh - 1;

    // Tło — prosty prostokąt (lv_canvas_draw_rect zamiast pętli piksel-po-pikselu)
    lv_draw_rect_dsc_t rd;
    lv_draw_rect_dsc_init(&rd);
    rd.bg_color   = lv_color_make(14, 17, 22);
    rd.bg_opa     = LV_OPA_80;
    rd.radius     = 2;
    rd.border_width = 0;
    lv_canvas_draw_rect(cv, tx, ty, bw, bh, &rd);

    // Tekst callsign
    lv_draw_label_dsc_t ld;
    lv_draw_label_dsc_init(&ld);
    ld.font  = &lv_font_montserrat_12;
    ld.color = col;
    lv_canvas_draw_text(cv, tx + 3, ty + 2, bw - 4, &ld, a.callsign);
}

// ---------- wektor prędkości (algorytm 60s horizon jak w ESP32-Plane-Radar) ----------

// Stała skala referencyjna — niezależna od zoomu (jak w oryginale)
static constexpr float TRACK_HORIZON_S   = 60.0f;   // 60-sekundowy horyzont
static constexpr float TRACK_REF_KM      = 25.0f;   // referencyjny zasięg w km
static constexpr float TRACK_LEN_SCALE   = 0.35f;   // skróć wektor dla czytelności
static constexpr int   TRACK_MIN_PX      = 3;       // minimum widoczna długość

static void drawSpeedVector(lv_obj_t *cv, int cx, int cy,
                             double headingDeg, double trackDeg, double speedKt) {
    if (isnan(speedKt) || speedKt < 10.0) return;
    if (isnan(trackDeg))  trackDeg  = headingDeg;
    if (isnan(headingDeg)) return;

    // Długość wektora w pikselach
    // km/s = speedKt * 1.852 / 3600
    // km za 60s = speedKt * 1.852 / 60
    float kmIn60s = (float)speedKt * 1.852f / 60.0f * TRACK_HORIZON_S;
    float px = kmIn60s / TRACK_REF_KM * GRID_R * TRACK_LEN_SCALE;
    int len = (int)(px + 0.5f);
    if (len < TRACK_MIN_PX) len = TRACK_MIN_PX;
    if (len > GRID_R) len = GRID_R;

    // Start: nos trójkąta (8px przed centrum)
    double hRad = radians(headingDeg);
    int tipX = cx + (int)(sin(hRad) * 8);
    int tipY = cy - (int)(cos(hRad) * 8);

    // Kierunek: track (GPS ground track)
    double tRad = radians(trackDeg);
    int ex = tipX + (int)(sin(tRad) * len);
    int ey = tipY - (int)(cos(tRad) * len);

    // Przytnij do krawędzi siatki
    double ddx = ex - CX, ddy = ey - CY;
    double dist = sqrt(ddx*ddx + ddy*ddy);
    if (dist > GRID_R) {
        ex = CX + (int)(ddx / dist * GRID_R);
        ey = CY + (int)(ddy / dist * GRID_R);
    }

    drawLine(cv, tipX, tipY, ex, ey, lv_color_make(255, 80, 220));  // magenta
}

// ---------- trasy przelotu ----------
// Dla każdego samolotu przechowujemy bufor ostatnich pozycji.
// Max 60 samolotów × 60 punktów = 3600 pozycji (72KB w PSRAM)

static constexpr int TRAIL_MAX_AC   = 60;  // max samolotów z trasą
static constexpr int TRAIL_MAX_PTS  = 60;  // max punktów na trasę

struct TrailPoint { int16_t x, y; };  // pozycja na kanwasie
struct TrailEntry {
    char hex[8]   = {};
    TrailPoint pts[TRAIL_MAX_PTS];
    int count     = 0;   // ile punktów aktywnych (FIFO)
    bool active   = false;
};

static TrailEntry *s_trails = nullptr;  // alokowane w PSRAM w uiRadarBegin

static TrailEntry *findOrCreateTrail(const char *hex) {
    if (!s_trails || !hex || !hex[0]) return nullptr;
    // Szukaj istniejącego
    for (int i = 0; i < TRAIL_MAX_AC; ++i)
        if (s_trails[i].active && strcmp(s_trails[i].hex, hex) == 0)
            return &s_trails[i];
    // Szukaj wolnego slotu
    for (int i = 0; i < TRAIL_MAX_AC; ++i)
        if (!s_trails[i].active) {
            s_trails[i] = TrailEntry();
            strncpy(s_trails[i].hex, hex, 7);
            s_trails[i].active = true;
            return &s_trails[i];
        }
    return nullptr;  // brak miejsca
}

static void addTrailPoint(const char *hex, int px, int py) {
    TrailEntry *e = findOrCreateTrail(hex);
    if (!e) return;
    // Sprawdź czy punkt się zmienił (nie duplikuj)
    if (e->count > 0) {
        auto &last = e->pts[(e->count - 1) % TRAIL_MAX_PTS];
        if (last.x == px && last.y == py) return;
    }
    // Wstaw FIFO — gdy pełny, przesuń
    int idx = e->count % TRAIL_MAX_PTS;
    e->pts[idx].x = (int16_t)px;
    e->pts[idx].y = (int16_t)py;
    e->count++;
}

static void clearTrail(const char *hex) {
    if (!s_trails) return;
    for (int i = 0; i < TRAIL_MAX_AC; ++i)
        if (s_trails[i].active && strcmp(s_trails[i].hex, hex) == 0) {
            s_trails[i] = TrailEntry();
            return;
        }
}

static void clearStaleTrails(const AppState &state) {
    // Usuń trasy samolotów których już nie widać
    if (!s_trails) return;
    for (int i = 0; i < TRAIL_MAX_AC; ++i) {
        if (!s_trails[i].active) continue;
        bool found = false;
        for (int j = 0; j < state.aircraftCount; ++j)
            if (strcmp(state.aircraft[j].hex, s_trails[i].hex) == 0) { found = true; break; }
        if (!found) s_trails[i] = TrailEntry();
    }
}

static void drawTrail(lv_obj_t *cv, const TrailEntry &e,
                       uint8_t r0, uint8_t g0, uint8_t b0) {
    if (e.count < 2) return;
    int total = (e.count < TRAIL_MAX_PTS) ? e.count : TRAIL_MAX_PTS;
    int start = (e.count <= TRAIL_MAX_PTS) ? 0 : e.count % TRAIL_MAX_PTS;
    for (int i = 0; i < total - 1; ++i) {
        int ai = (start + i) % TRAIL_MAX_PTS;
        int bi = (start + i + 1) % TRAIL_MAX_PTS;
        // Starsze punkty ciemniejsze: t=0 (najstarszy) → dim, t=1 (najnowszy) → pełny kolor
        float t = (float)(i + 1) / (float)total;
        uint8_t r = (uint8_t)(r0 * t * 0.7f);
        uint8_t g = (uint8_t)(g0 * t * 0.7f);
        uint8_t b = (uint8_t)(b0 * t * 0.7f);
        drawLine(cv, e.pts[ai].x, e.pts[ai].y,
                     e.pts[bi].x, e.pts[bi].y,
                     lv_color_make(r, g, b));
    }
}



struct DrawItem {
    int idx;
    int x, y;
    int distSq;
};

static void sortFarFirst(DrawItem *items, int n) {
    for (int i = 1; i < n; ++i) {
        DrawItem k = items[i];
        int j = i;
        while (j > 0 && items[j-1].distSq < k.distSq) { items[j] = items[j-1]; --j; }
        items[j] = k;
    }
}

// ---------- siatka ----------

static void drawGrid(lv_obj_t *cv, int rangeKm) {
    lv_canvas_fill_bg(cv, lv_color_make(4, 10, 28), LV_OPA_COVER);

    auto gridCol = lv_color_make(16, 100, 32);
    auto dimCol  = lv_color_make(12, 60, 20);
    auto labelCol = lv_color_make(200, 200, 200);

    // Okręgi
    for (int i = 1; i <= 4; ++i) {
        int r = GRID_R * i / 4;
        lv_draw_arc_dsc_t ad;
        lv_draw_arc_dsc_init(&ad);
        ad.color = (i == 4) ? dimCol : gridCol;
        ad.width = 1;
        lv_canvas_draw_arc(cv, CX, CY, r, 0, 3600, &ad);
    }

    // Krzyż celowniczy
    lv_draw_line_dsc_t ld;
    lv_draw_line_dsc_init(&ld);
    ld.color = dimCol;
    ld.width = 1;
    lv_point_t ptsH[2] = {{0, CY}, {RADAR_W, CY}};
    lv_canvas_draw_line(cv, ptsH, 2, &ld);
    lv_point_t ptsV[2] = {{CX, 0}, {CX, RADAR_H}};
    lv_canvas_draw_line(cv, ptsV, 2, &ld);

    // Oznaczenia N/S (W/E na krawędziach)
    lv_draw_label_dsc_t td;
    lv_draw_label_dsc_init(&td);
    td.color = labelCol;
    td.font = &lv_font_montserrat_14;
    td.align = LV_TEXT_ALIGN_CENTER;

    lv_canvas_draw_text(cv, CX - 8, 2, 16, &td, "N");
    lv_canvas_draw_text(cv, CX - 8, RADAR_H - 18, 16, &td, "S");
    td.align = LV_TEXT_ALIGN_LEFT;
    lv_canvas_draw_text(cv, 2, CY - 8, 20, &td, "W");
    td.align = LV_TEXT_ALIGN_RIGHT;
    lv_canvas_draw_text(cv, RADAR_W - 22, CY - 8, 20, &td, "E");

    // Skala (dynamiczna z ustawienia)
    char scaleBuf[16];
    snprintf(scaleBuf, sizeof(scaleBuf), "%d km", rangeKm);
    td.align = LV_TEXT_ALIGN_RIGHT;
    td.color = gridCol;
    td.font = &lv_font_montserrat_12;
    lv_canvas_draw_text(cv, CX + GRID_R - 50, CY - 8, 48, &td, scaleBuf);

    // Centrum "DOM"
    td.align = LV_TEXT_ALIGN_CENTER;
    td.color = lv_color_make(60, 200, 255);
    td.font = &lv_font_montserrat_12;
    lv_canvas_draw_text(cv, CX - 16, CY - 6, 32, &td, "DOM");
}

// ---------- marker EPKK ----------
static void drawAirportMarker(lv_obj_t *cv, int rangeKm) {
    // Oblicz pozycję EPKK na kanwasie
    double dist   = geo::distanceKm(HOME_LAT, HOME_LON, AIRPORT_LAT, AIRPORT_LON);
    double bear   = geo::bearingDeg(HOME_LAT, HOME_LON, AIRPORT_LAT, AIRPORT_LON);
    double maxR   = (double)rangeKm;
    if (maxR < 1) maxR = 1;
    double rPx    = (dist / maxR) * GRID_R;
    double rad    = radians(bear);
    int ax        = CX + (int)(rPx * sin(rad));
    int ay        = CY - (int)(rPx * cos(rad));

    // Pasek startowy: dwie równoległe kreski w kierunku pasa 07/25 (72°)
    auto runwayCol = lv_color_make(255, 200, 60);  // żółty
    double rwRad = radians(RWY_HEADING_07);
    int dx = (int)(sin(rwRad) * 8);
    int dy = (int)(-cos(rwRad) * 8);
    // Kreska pasa
    for (int i = -1; i <= 1; i++) {
        int ox = (int)(cos(rwRad) * i * 2);
        int oy = (int)(sin(rwRad) * i * 2);
        lv_draw_line_dsc_t ld; lv_draw_line_dsc_init(&ld);
        ld.color = runwayCol; ld.width = 2;
        lv_point_t pts[] = {{(lv_coord_t)(ax-dx+ox),(lv_coord_t)(ay-dy+oy)},
                            {(lv_coord_t)(ax+dx+ox),(lv_coord_t)(ay+dy+oy)}};
        lv_canvas_draw_line(cv, pts, 2, &ld);
    }
    // Etykieta lotniska
    lv_draw_label_dsc_t td; lv_draw_label_dsc_init(&td);
    td.font  = &lv_font_montserrat_12;
    td.color = runwayCol;
    td.align = LV_TEXT_ALIGN_CENTER;
    lv_canvas_draw_text(cv, ax - 20, ay + 10, 40, &td, AIRPORT_ICAO);
}

// ---------- rysowanie --------

static void drawAircraftAll(lv_obj_t *cv, const AppState &state) {
    const bool hasFlight = state.hasSelectedFlight || state.hasBestEastCandidate;
    const FlightCandidate &cand = state.hasBestEastCandidate
                                  ? state.bestEastCandidate
                                  : state.selectedFlight;

    // Krok 1: zbierz punkty do narysowania (heap, nie stack)
    int maxAircraft = state.aircraftCount;
    if (maxAircraft > 120) maxAircraft = 120;
    DrawItem *inside = (DrawItem *)calloc(maxAircraft, sizeof(DrawItem));
    DrawItem *rim    = (DrawItem *)calloc(maxAircraft, sizeof(DrawItem));
    if (!inside || !rim) { free(inside); free(rim); return; }
    int inCnt = 0, rimCnt = 0;

    double maxR = state.settings.radarRadiusKm;
    if (maxR < 1) maxR = 1;

    for (int i = 0; i < state.aircraftCount; ++i) {
        const AircraftState &a = state.aircraft[i];
        if (!a.hasPosition) continue;

        int rPx = (int)((a.distanceKm / maxR) * GRID_R);
        double bearing = geo::bearingDeg(HOME_LAT, HOME_LON, a.lat, a.lon);
        double rad = radians(bearing);
        int px = CX + (int)(rPx * sin(rad));
        int py = CY - (int)(rPx * cos(rad));

        if (rPx <= GRID_R && px >= 0 && px < RADAR_W && py >= 0 && py < RADAR_H) {
            inside[inCnt].idx = i;
            inside[inCnt].x = px;
            inside[inCnt].y = py;
            inside[inCnt].distSq = (px - CX) * (px - CX) + (py - CY) * (py - CY);
            inCnt++;
        } else {
            // Kropka na krawędzi (właściwy azymut)
            int rx = CX + (int)(sin(rad) * (GRID_R - 2));
            int ry = CY - (int)(cos(rad) * (GRID_R - 2));
            rim[rimCnt].idx = i;
            rim[rimCnt].x = rx;
            rim[rimCnt].y = ry;
            rim[rimCnt].distSq = (rx - CX) * (rx - CX) + (ry - CY) * (ry - CY);
            rimCnt++;
        }
    }

    // Krok 2: dodaj punkty do tras (przed rysowaniem)
    for (int i = 0; i < state.aircraftCount; ++i) {
        const AircraftState &a = state.aircraft[i];
        if (!a.hasPosition) continue;
        double bearing_ac = geo::bearingDeg(HOME_LAT, HOME_LON, a.lat, a.lon);
        double rad_ac = radians(bearing_ac);
        double maxR_ac = state.settings.radarRadiusKm;
        if (maxR_ac < 1) maxR_ac = 1;
        int rPx_ac = (int)((a.distanceKm / maxR_ac) * GRID_R);
        int px_ac = CX + (int)(rPx_ac * sin(rad_ac));
        int py_ac = CY - (int)(rPx_ac * cos(rad_ac));
        if (rPx_ac <= GRID_R)
            addTrailPoint(a.hex, px_ac, py_ac);
        else if (a.onGround)
            clearTrail(a.hex);   // wylądował — wyczyść trasę
    }
    clearStaleTrails(state);

    // Krok 3: narysuj trasy (pod samolotami)
    if (s_trails) {
        for (int i = 0; i < TRAIL_MAX_AC; ++i) {
            if (!s_trails[i].active || s_trails[i].count < 2) continue;
            // Znajdź kolor tego samolotu
            bool isMain = hasFlight && (strcmp(cand.aircraft.hex, s_trails[i].hex) == 0);
            // Sprawdź bearing
            bool isEast = false;
            for (int j = 0; j < state.aircraftCount; ++j)
                if (strcmp(state.aircraft[j].hex, s_trails[i].hex) == 0) {
                    isEast = geo::isBearingEast(state.aircraft[j].bearingDeg);
                    break;
                }
            if (isMain)       drawTrail(cv, s_trails[i], 60,  169, 252);  // niebieski
            else if (isEast)  drawTrail(cv, s_trails[i], 74,  222, 128);  // zielony
            else              drawTrail(cv, s_trails[i], 120, 120, 120);  // szary
        }
    }

    // Krok 4: narysuj kropki na krawędzi
    auto rimCol = lv_color_make(200, 0, 0);
    for (int d = 0; d < rimCnt; ++d)
        fillCircle(cv, rim[d].x, rim[d].y, 3, rimCol);

    // Krok 5: sortuj dalekie → bliskie
    sortFarFirst(inside, inCnt);

    // Krok 6: trójkąty + wektory prędkości
    // Kolory altitudinalne jak w esp32flight — intuicyjne dla obserwatora
    // Żółty = nisko (startuje/ląduje), zielony = średnio, niebieski = wysoko, fioletowy = przelot
    auto altColor = [](double altFt, bool onGround) -> lv_color_t {
        if (onGround || isnan(altFt))  return lv_color_make(120, 120, 120);  // szary = na ziemi
        if (altFt < 5000)             return lv_color_make(255, 209, 102);  // żółty
        if (altFt < 15000)            return lv_color_make(138, 201, 38);   // zielony
        if (altFt < 30000)            return lv_color_make(77, 163, 255);   // niebieski
        return                               lv_color_make(179, 136, 255);  // fioletowy = wysoki przelot
    };
    auto mainCol = lv_color_make(255, 255, 255);  // wybrany samolot = biały (odróżnia się)

    for (int d = 0; d < inCnt; ++d) {
        const AircraftState &a = state.aircraft[inside[d].idx];
        bool isMain = hasFlight && (strcmp(cand.aircraft.hex, a.hex) == 0);
        lv_color_t col = isMain ? mainCol : altColor(a.altitudeFt, a.onGround);
        // Wektor prędkości (magenta) — pod trójkątem
        drawSpeedVector(cv, inside[d].x, inside[d].y, a.trackDeg, a.trackDeg, a.speedKt);
        // Trójkąt kierunkowy
        drawHeadingTriangle(cv, inside[d].x, inside[d].y, a.trackDeg, col);
    }

    // Krok 7: etykiety callsign
    for (int d = 0; d < inCnt; ++d) {
        const AircraftState &a = state.aircraft[inside[d].idx];
        bool isMain = hasFlight && (strcmp(cand.aircraft.hex, a.hex) == 0);
        lv_color_t col = isMain ? mainCol : altColor(a.altitudeFt, a.onGround);
        drawTag(cv, inside[d].x, inside[d].y, a, col);
    }

    // Krok 8: marker lotniska EPKK (na wierzchu wszystkiego)
    drawAirportMarker(cv, (int)state.settings.radarRadiusKm);

    free(inside);
    free(rim);
}

// ---------- panel boczny ----------

void updateInfoPanel(const AppState &state) {
    if (!s_infoCallsign) return;

    const bool hasFlight = state.hasSelectedFlight || state.hasBestEastCandidate;
    const FlightCandidate *cand = nullptr;
    if (state.hasBestEastCandidate) cand = &state.bestEastCandidate;
    else if (state.hasSelectedFlight) cand = &state.selectedFlight;

    // Liczba samolotów w zasięgu (zawsze widoczna)
    char countBuf[32];
    snprintf(countBuf, sizeof(countBuf), "W zasiegu: %d", state.aircraftCount);
    if (s_infoCount) lv_label_set_text(s_infoCount, countBuf);

    if (!hasFlight || !cand) {
        // Brak ciekawego lotu — wyświetl "cisza"
        if (s_panelTitle)   lv_label_set_text(s_panelTitle,   "BRAK LOTOW");
        if (s_infoCallsign) lv_label_set_text(s_infoCallsign, "");
        if (s_infoStatus)   lv_label_set_text(s_infoStatus,   "");
        if (s_infoRoute)    lv_label_set_text(s_infoRoute,    "");
        if (s_infoAlt)      lv_label_set_text(s_infoAlt,      "");
        if (s_infoSpd)      lv_label_set_text(s_infoSpd,      "");
        if (s_infoDist)     lv_label_set_text(s_infoDist,     "");
        if (s_infoVario)    lv_label_set_text(s_infoVario,    "");
        return;
    }

    const AircraftState &a = cand->aircraft;
    char buf[64];

    // Tytuł — co robi samolot
    const char *title;
    switch (cand->kind) {
        case FlightKind::Arrival:   title = "LADUJE"; break;
        case FlightKind::Departure: title = "STARTUJE"; break;
        case FlightKind::Transit:   title = "PRZELOT"; break;
        default:                    title = "W POBLIZU"; break;
    }
    if (s_panelTitle)   lv_label_set_text(s_panelTitle,   title);
    if (s_infoCallsign) lv_label_set_text(s_infoCallsign, a.callsign);

    // Status z kolorem
    if (s_infoStatus) {
        lv_label_set_text(s_infoStatus, strlen(cand->airline) > 0 ? cand->airline : "");
    }

    // Trasa
    if (s_infoRoute) {
        if (cand->route.valid && cand->route.origin[0] && cand->route.dest[0])
            snprintf(buf, sizeof(buf), "%s -> %s", cand->route.origin, cand->route.dest);
        else
            buf[0] = '\0';
        lv_label_set_text(s_infoRoute, buf);
    }

    // Wysokość
    if (s_infoAlt) {
        if (!isnan(a.altitudeFt)) snprintf(buf, sizeof(buf), "%.0f ft", a.altitudeFt);
        else snprintf(buf, sizeof(buf), "-- ft");
        lv_label_set_text(s_infoAlt, buf);
    }

    // Prędkość
    if (s_infoSpd) {
        if (!isnan(a.speedKt)) snprintf(buf, sizeof(buf), "%.0f kt", a.speedKt);
        else snprintf(buf, sizeof(buf), "-- kt");
        lv_label_set_text(s_infoSpd, buf);
    }

    // Dystans
    if (s_infoDist) {
        if (!isnan(a.distanceKm)) snprintf(buf, sizeof(buf), "%.1f km", a.distanceKm);
        else snprintf(buf, sizeof(buf), "-- km");
        lv_label_set_text(s_infoDist, buf);
    }

    // Wznoszenie/opadanie
    if (s_infoVario) {
        if (!isnan(a.verticalRateFpm)) {
            if      (a.verticalRateFpm >  100) snprintf(buf, sizeof(buf), "^ %.0f ft/min", a.verticalRateFpm);
            else if (a.verticalRateFpm < -100) snprintf(buf, sizeof(buf), "v %.0f ft/min", fabs(a.verticalRateFpm));
            else                               snprintf(buf, sizeof(buf), "= poziomo");
        } else {
            snprintf(buf, sizeof(buf), "");
        }
        lv_label_set_text(s_infoVario, buf);
    }
}

} // namespace

// ---------- API ----------

void uiRadarBegin(AppState &state) {
    (void)state;
    if (s_scr) return;
    s_scr = lv_obj_create(NULL);
    lv_obj_set_style_bg_color(s_scr, uiTheme::BG, 0);

    s_cbuf = (lv_color_t *)heap_caps_malloc(RADAR_W * RADAR_H * sizeof(lv_color_t), MALLOC_CAP_SPIRAM);
    if (!s_cbuf) {
        Serial.println("[RADAR] PSRAM unavailable, trying internal heap");
        s_cbuf = (lv_color_t *)malloc(RADAR_W * RADAR_H * sizeof(lv_color_t));
    }

    // Bufor tras w PSRAM (60 samolotów × 60 punktów × 4 bajty ≈ 14KB)
    s_trails = (TrailEntry *)heap_caps_calloc(TRAIL_MAX_AC, sizeof(TrailEntry), MALLOC_CAP_SPIRAM);
    if (!s_trails)
        s_trails = (TrailEntry *)calloc(TRAIL_MAX_AC, sizeof(TrailEntry));
    if (!s_trails) Serial.println("[RADAR] Trail buffer FAILED");
    if (s_cbuf) {
        s_canvas = lv_canvas_create(s_scr);
        lv_obj_set_pos(s_canvas, 0, 0);
        lv_obj_set_size(s_canvas, RADAR_W, RADAR_H);
        lv_canvas_set_buffer(s_canvas, s_cbuf, RADAR_W, RADAR_H, LV_IMG_CF_TRUE_COLOR);
        drawGrid(s_canvas, state.settings.radarRadiusKm);
    } else {
        Serial.println("[RADAR] Canvas buffer allocation FAILED");
    }

    lv_obj_t *panel = lv_obj_create(s_scr);
    lv_obj_set_pos(panel, RADAR_W, 0);
    lv_obj_set_size(panel, 220, RADAR_H);
    lv_obj_set_style_bg_color(panel, uiTheme::CARD, 0);
    lv_obj_set_style_border_width(panel, 0, 0);
    lv_obj_set_style_radius(panel, 0, 0);
    lv_obj_set_style_pad_all(panel, 0, 0);

    // ── Separator pionowy ────────────────────────────────────────────────────
    lv_obj_t *sep = lv_obj_create(panel);
    lv_obj_set_pos(sep, 0, 0); lv_obj_set_size(sep, 2, RADAR_H);
    lv_obj_set_style_bg_color(sep, uiTheme::BG, 0);
    lv_obj_set_style_border_width(sep, 0, 0); lv_obj_set_style_radius(sep, 0, 0);

    int y = 12;

    // ── Tytuł stanu (LADUJE / STARTUJE / BRAK LOTOW) ────────────────────────
    s_panelTitle = lv_label_create(panel);
    lv_obj_set_pos(s_panelTitle, 10, y); y += 28;
    lv_obj_set_size(s_panelTitle, 200, 24);
    lv_obj_set_style_text_color(s_panelTitle, uiTheme::MUTED, 0);
    lv_obj_set_style_text_font(s_panelTitle, &lv_font_montserrat_16, 0);
    lv_label_set_text(s_panelTitle, "RADAR");

    // ── Callsign — duży ──────────────────────────────────────────────────────
    s_infoCallsign = lv_label_create(panel);
    lv_obj_set_pos(s_infoCallsign, 10, y); y += 38;
    lv_obj_set_size(s_infoCallsign, 200, 34);
    lv_obj_set_style_text_color(s_infoCallsign, uiTheme::ACCENT, 0);
    lv_obj_set_style_text_font(s_infoCallsign, &lv_font_montserrat_30, 0);
    lv_label_set_text(s_infoCallsign, "");

    // ── Linia lotnicza ───────────────────────────────────────────────────────
    s_infoStatus = lv_label_create(panel);
    lv_obj_set_pos(s_infoStatus, 10, y); y += 22;
    lv_obj_set_size(s_infoStatus, 200, 18);
    lv_obj_set_style_text_color(s_infoStatus, uiTheme::TEXT, 0);
    lv_obj_set_style_text_font(s_infoStatus, &lv_font_montserrat_16, 0);
    lv_label_set_text(s_infoStatus, "");

    // ── Trasa ────────────────────────────────────────────────────────────────
    s_infoRoute = lv_label_create(panel);
    lv_obj_set_pos(s_infoRoute, 10, y); y += 28;
    lv_obj_set_size(s_infoRoute, 200, 24);
    lv_obj_set_style_text_color(s_infoRoute, uiTheme::EAST_OK, 0);
    lv_obj_set_style_text_font(s_infoRoute, &lv_font_montserrat_20, 0);
    lv_label_set_text(s_infoRoute, "");

    // ── separator ────────────────────────────────────────────────────────────
    lv_obj_t *sep2 = lv_obj_create(panel);
    lv_obj_set_pos(sep2, 10, y); lv_obj_set_size(sep2, 200, 1); y += 10;
    lv_obj_set_style_bg_color(sep2, uiTheme::BG, 0);
    lv_obj_set_style_border_width(sep2, 0, 0);

    // ── 3 dane liczbowe (każde z etykietą) ───────────────────────────────────
    auto makeDataRow = [&](lv_obj_t *&lbl, const char *label, lv_color_t col) {
        lv_obj_t *hdr = lv_label_create(panel);
        lv_obj_set_pos(hdr, 10, y);
        lv_obj_set_style_text_color(hdr, uiTheme::MUTED, 0);
        lv_obj_set_style_text_font(hdr, &lv_font_montserrat_12, 0);
        lv_label_set_text(hdr, label);

        lbl = lv_label_create(panel);
        lv_obj_set_pos(lbl, 10, y + 14);
        lv_obj_set_size(lbl, 200, 26);
        lv_obj_set_style_text_color(lbl, col, 0);
        lv_obj_set_style_text_font(lbl, &lv_font_montserrat_20, 0);
        lv_label_set_text(lbl, "--");
        y += 42;
    };

    makeDataRow(s_infoAlt,  "WYSOKOSC",  uiTheme::TEXT);
    makeDataRow(s_infoSpd,  "PREDKOSC",  uiTheme::TEXT);
    makeDataRow(s_infoDist, "DYSTANS",   uiTheme::TEXT);

    // ── Wznoszenie/opadanie ──────────────────────────────────────────────────
    s_infoVario = lv_label_create(panel);
    lv_obj_set_pos(s_infoVario, 10, y); y += 22;
    lv_obj_set_size(s_infoVario, 200, 18);
    lv_obj_set_style_text_color(s_infoVario, uiTheme::MUTED, 0);
    lv_obj_set_style_text_font(s_infoVario, &lv_font_montserrat_16, 0);
    lv_label_set_text(s_infoVario, "");

    // ── separator ────────────────────────────────────────────────────────────
    lv_obj_t *sep3 = lv_obj_create(panel);
    lv_obj_set_pos(sep3, 10, y); lv_obj_set_size(sep3, 200, 1); y += 10;
    lv_obj_set_style_bg_color(sep3, uiTheme::BG, 0);
    lv_obj_set_style_border_width(sep3, 0, 0);

    // ── Liczba samolotów w zasięgu ───────────────────────────────────────────
    s_infoCount = lv_label_create(panel);
    lv_obj_set_pos(s_infoCount, 10, y);
    lv_obj_set_size(s_infoCount, 200, 18);
    lv_obj_set_style_text_color(s_infoCount, uiTheme::MUTED, 0);
    lv_obj_set_style_text_font(s_infoCount, &lv_font_montserrat_14, 0);
    lv_label_set_text(s_infoCount, "W zasiegu: --");

    // ── Czas ostatniej aktualizacji (na dole panelu) ─────────────────────────
    s_lastUpd = lv_label_create(panel);
    lv_obj_set_pos(s_lastUpd, 10, RADAR_H - 22);
    lv_obj_set_size(s_lastUpd, 200, 18);
    lv_obj_set_style_text_color(s_lastUpd, uiTheme::MUTED, 0);
    lv_obj_set_style_text_font(s_lastUpd, &lv_font_montserrat_12, 0);
    lv_label_set_text(s_lastUpd, "");
}

void uiRadarShow() { lv_scr_load(s_scr); }
lv_obj_t *uiRadarScr() { return s_scr; }

void uiRadarRefresh(const AppState &state) {
    if (lv_scr_act() != s_scr) return;

    if (s_lastUpd) {
        char buf[48];
        unsigned long sec = (millis() - state.lastAdsBFetchMs) / 1000UL;
        unsigned long staleThreshold = (unsigned long)state.settings.adsbRefreshSec * 3UL;
        if (sec > staleThreshold) {
            // Dane mogą być nieaktualne — pokaż ostrzeżenie
            snprintf(buf, sizeof(buf), "! brak odp. %lu s", sec);
            lv_obj_set_style_text_color(s_lastUpd, uiTheme::WEST_OFF, 0);  // czerwony
        } else {
            if (sec < 60) snprintf(buf, sizeof(buf), "akt. %lu s temu", sec);
            else          snprintf(buf, sizeof(buf), "akt. %lu min temu", sec / 60UL);
            lv_obj_set_style_text_color(s_lastUpd, uiTheme::MUTED, 0);
        }
        lv_label_set_text(s_lastUpd, buf);
    }

    updateInfoPanel(state);

    if (!s_canvas) return;

    unsigned long now = millis();
    bool needRedraw = false;
    if (state.adsbVersion != s_lastVersion) {
        s_lastVersion = state.adsbVersion;
        needRedraw = true;
    }
    if (!needRedraw && (now - s_lastRedraw >= 15000)) needRedraw = true;
    if (!needRedraw) return;
    if (now - s_lastRedraw < 2000) return;
    s_lastRedraw = now;

    drawGrid(s_canvas, state.settings.radarRadiusKm);
    drawAircraftAll(s_canvas, state);
    lv_obj_invalidate(s_canvas);
}
