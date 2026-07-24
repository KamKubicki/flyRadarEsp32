#pragma once

#include "app_state.h"
#include <lvgl.h>

void uiWeatherBegin(AppState &state);
void uiWeatherRefresh(const AppState &state);
void uiWeatherShow();
lv_obj_t *uiWeatherScr();