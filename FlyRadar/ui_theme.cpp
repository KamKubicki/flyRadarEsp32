#include "ui_theme.h"
#include <lvgl.h>

void uiThemeInit() {
    lv_obj_t *scr = lv_scr_act();
    lv_obj_set_style_bg_color(scr, uiTheme::BG, 0);
    lv_obj_set_style_bg_opa(scr, LV_OPA_COVER, 0);
}
