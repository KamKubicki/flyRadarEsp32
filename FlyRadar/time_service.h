#pragma once

#include <Arduino.h>
#include "app_state.h"

void timeServiceBegin();
void timeServiceLoop(AppState &state);
