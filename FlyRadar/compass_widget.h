#pragma once
// ============================================================================
// compass_widget — mały kompas 80×80 rysowany na LVGL canvas
// Wskazuje kierunek azymutu (bearing) samolotu od domu.
// Użycie:
//   lv_obj_t *cv = compassCreate(parent, x, y);
//   compassUpdate(cv, bearingDeg);  // bearing 0=N, 90=E, 180=S, 270=W
// ============================================================================
#include <lvgl.h>

lv_obj_t *compassCreate(lv_obj_t *parent, int x, int y);
void       compassUpdate(lv_obj_t *canvas, double bearingDeg);
void       compassClear(lv_obj_t *canvas);   // brak danych — czyść
