#pragma once

#include "types.h"

typedef void (*EventHandler)(XEvent*, DisplayContext*, InputState*);

void register_handler(EventHandler h);

void handle_keypress(XEvent* e, DisplayContext* ctx, InputState* state);
void handle_click(XEvent* e, DisplayContext* ctx, InputState* state);
void handle_motion(XEvent* e, DisplayContext* ctx, InputState* state);

void app_run(DisplayContext* ctx, InputState* state);
