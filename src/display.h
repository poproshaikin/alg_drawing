#pragma once

#include "types.h"

DisplayContext init_display(int w, int h);
void cleanup_display(DisplayContext* ctx);

void fill_framebuffer(const DisplayContext* ctx, uint8_t r, uint8_t g, uint8_t b);
void clear_framebuffer(const DisplayContext* ctx);

void render_frame(DisplayContext* ctx);

void put_pixel(DisplayContext* ctx, int x, int y, uint8_t r, uint8_t g, uint8_t b);
void put_pixel_thick(DisplayContext* ctx, int x, int y, int thickness, uint8_t r, uint8_t g, uint8_t b);
