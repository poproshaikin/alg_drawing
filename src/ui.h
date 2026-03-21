#pragma once

#include "types.h"

#define UI_BAR_H 48

void render_ui(DisplayContext* ctx, const InputState* state);

bool ui_handle_click(int x, int y, DisplayContext* ctx, InputState* state);
