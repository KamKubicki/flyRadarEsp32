#pragma once

#include "app_state.h"
#include <lvgl.h>

void uiRadarBegin(AppState &state);
void uiRadarRefresh(const AppState &state);
void uiRadarShow();
lv_obj_t *uiRadarScr();