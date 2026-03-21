#pragma once

#include "types.h"

void draw_line(DisplayContext* ctx, int x0, int y0, int x1, int y1, uint8_t r, uint8_t g, uint8_t b);
void draw_dotted_line(DisplayContext* ctx,
    int x0,
    int y0,
    int x1,
    int y1,
    uint8_t r,
    uint8_t g,
    uint8_t b);

void draw_line_thick(DisplayContext* ctx,
    int x0,
    int y0,
    int x1,
    int y1,
    int thickness,
    uint8_t r,
    uint8_t g,
    uint8_t b);

void draw_dashed_line_thick(DisplayContext* ctx,
    int x0,
    int y0,
    int x1,
    int y1,
    int thickness,
    int on_len,
    int off_len,
    uint8_t r,
    uint8_t g,
    uint8_t b);

void draw_circle(DisplayContext* ctx,
    int cx,
    int cy,
    int radius,
    int thickness,
    bool dashed,
    uint8_t r,
    uint8_t g,
    uint8_t b);

void snap_to_axis(int x0, int y0, int* x1, int* y1);
