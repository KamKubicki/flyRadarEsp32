#pragma once

#include "app_state.h"
#include <lvgl.h>

void uiHomeBegin(AppState &state);
void uiHomeRefresh(const AppState &state);
void uiHomeShow();
lv_obj_t *uiHomeScr();