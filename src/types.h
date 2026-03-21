#pragma once

#include <X11/Xlib.h>
#include <stdint.h>
#include <stdbool.h>

typedef struct
{
    uint32_t* data;
    uint32_t* saved;
} Framebuffer;

typedef struct
{
    Display* dpy;
    Window win;
    GC gc;
    XImage* img;
    int w;
    int h;
    Framebuffer fb;
} DisplayContext;

typedef struct
{
    bool running;
    bool have_first;
    int x0, y0;
    unsigned int modifiers;

    uint8_t color_r, color_g, color_b;
    int thickness;
    int line_style; // 0=solid, 1=dashed, 2=dotted

    int tool; // 0=point, 1=line, 2=circle, 3=polygon
    int poly_count;
    int poly_x[256];
    int poly_y[256];

    int snap_mode; // -1=none, 0=horizontal, 1=vertical, 2=diag45
} InputState;

static inline uint32_t
pack_rgb(uint8_t r, uint8_t g, uint8_t b)
{
    return ((uint32_t)r << 16) | ((uint32_t)g << 8) | (uint32_t)b;
}
