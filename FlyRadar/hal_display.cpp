#include "hal_display.h"
#include <Arduino_GFX_Library.h>
#include "config.h"

static Arduino_ESP32RGBPanel *s_bus = nullptr;
static Arduino_RPi_DPI_RGBPanel *s_gfx = nullptr;

lv_disp_draw_buf_t s_drawBuf;
lv_disp_drv_t s_dispDrv;
lv_color_t *s_buf = nullptr;
uint32_t s_w = 0;
uint32_t s_h = 0;

namespace {

void flushCb(lv_disp_drv_t *drv, const lv_area_t *area, lv_color_t *color) {
    const int32_t w = area->x2 - area->x1 + 1;
    const int32_t h = area->y2 - area->y1 + 1;
#if (LV_COLOR_16_SWAP != 0)
    s_gfx->draw16bitBeRGBBitmap(area->x1, area->y1, (uint16_t *)&color->full, w, h);
#else
    s_gfx->draw16bitRGBBitmap(area->x1, area->y1, (uint16_t *)&color->full, w, h);
#endif
    lv_disp_flush_ready(drv);
}

}

void halDisplayBegin() {
    s_bus = new Arduino_ESP32RGBPanel(
        GFX_NOT_DEFINED, GFX_NOT_DEFINED, GFX_NOT_DEFINED,
        40, 41, 39, 42,
        45, 48, 47, 21, 14,
        5, 6, 7, 15, 16, 4,
        8, 3, 46, 9, 1
    );

    s_gfx = new Arduino_RPi_DPI_RGBPanel(
        s_bus,
        800, 0, 8, 4, 8,
        480, 0, 8, 4, 8,
        1, 16000000, true
    );

    s_gfx->begin();
    s_gfx->fillScreen(BLACK);

#ifdef TFT_BL
    pinMode(TFT_BL, OUTPUT);
    digitalWrite(TFT_BL, HIGH);
#endif

    s_gfx->setCursor(10, 10);
    s_gfx->setTextColor(RED);
    s_gfx->println("DISPLAY OK");

    s_w = s_gfx->width();
    s_h = s_gfx->height();

    lv_init();

    const size_t bufSize = s_w * 10;
    s_buf = (lv_color_t *)heap_caps_malloc(bufSize * sizeof(lv_color_t),
                                           MALLOC_CAP_INTERNAL | MALLOC_CAP_8BIT);
    if (!s_buf) {
        Serial.println("hal_display: LVGL buffer allocation failed");
        return;
    }

    lv_disp_draw_buf_init(&s_drawBuf, s_buf, NULL, bufSize);

    lv_disp_drv_init(&s_dispDrv);
    s_dispDrv.hor_res = s_w;
    s_dispDrv.ver_res = s_h;
    s_dispDrv.flush_cb = flushCb;
    s_dispDrv.draw_buf = &s_drawBuf;
    lv_disp_drv_register(&s_dispDrv);
}

uint32_t halDisplayWidth() { return s_w; }
uint32_t halDisplayHeight() { return s_h; }