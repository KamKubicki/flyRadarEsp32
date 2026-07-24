#pragma once

#include "app_state.h"
#include <lvgl.h>

void uiSettingsBegin(AppState &state);
void uiSettingsRefresh(const AppState &state);
void uiSettingsShow();
lv_obj_t *uiSettingsScr();