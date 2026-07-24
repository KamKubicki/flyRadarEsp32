#include "hal_touch.h"
#include <Wire.h>
#include <TAMC_GT911.h>
#include <lvgl.h>

namespace {

TAMC_GT911 ts(19, 20, -1, 38, 800, 480);
int touch_last_x = 0;
int touch_last_y = 0;

void readCb(lv_indev_drv_t *drv, lv_indev_data_t *data) {
    (void)drv;
    ts.read();
    if (ts.isTouched) {
        touch_last_x = map(ts.points[0].x, 800, 0, 0, 800 - 1);
        touch_last_y = map(ts.points[0].y, 480, 0, 0, 480 - 1);
        data->point.x = touch_last_x;
        data->point.y = touch_last_y;
        data->state = LV_INDEV_STATE_PR;
    } else {
        data->state = LV_INDEV_STATE_REL;
    }
}

} // namespace

void halTouchBegin() {
    Wire.begin(19, 20);
    ts.begin();
    ts.setRotation(ROTATION_NORMAL);

    static lv_indev_drv_t drv;
    lv_indev_drv_init(&drv);
    drv.type    = LV_INDEV_TYPE_POINTER;
    drv.read_cb = readCb;
    lv_indev_drv_register(&drv);
}