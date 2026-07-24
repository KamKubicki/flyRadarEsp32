#pragma once

#include "app_state.h"
#include <lvgl.h>

enum class UiScreenId : uint8_t {
    Home,
    Radar,
    Weather,
    Settings,
};

void uiThemeInit();
void uiNavBuild();
void uiBarUpdate(const AppState &state);
void uiRouterBegin(AppState &state);
void uiRouterShow(UiScreenId id);
void uiRouterShowBest(const AppState &state);
void uiRouterRefresh(const AppState &state);
UiScreenId uiRouterCurrent();