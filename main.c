#include <X11/Xlib.h>
#include <X11/Xutil.h>
#include <X11/keysym.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

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
    bool have_first;        // first point is selected
    int x0, y0;             // cursor position
    unsigned int modifiers; // shift / control
    uint8_t color_r, color_g, color_b;
    int thickness; // 1/3/6
    bool dashed;
} InputState;

typedef void (*EventHandler)(XEvent*, DisplayContext*, InputState*);

#define MAX_HANDLERS 32
EventHandler handlers[MAX_HANDLERS];
int handler_count = 0;

void
register_handler(EventHandler h)
{
    if (handler_count < MAX_HANDLERS)
        handlers[handler_count++] = h;
}

void
terminate(const char* message)
{
    fprintf(stderr, "execution terminated, reason: %s", message);
}

Framebuffer
init_framebuffer(int w, int h)
{
    uint32_t* data = calloc(w * h, sizeof(uint32_t));
    uint32_t* saved = calloc(w * h, sizeof(uint32_t));

    Framebuffer fb;
    fb.data = data;
    fb.saved = saved;

    return fb;
}

void
fill_framebuffer(const DisplayContext* ctx, uint8_t r, uint8_t g, uint8_t b)
{
    uint32_t color = (r << 16) | (g << 8) | b;
    for (int i = 0; i < ctx->w * ctx->h; ++i)
        ctx->fb.data[i] = color;
}

void
clear_framebuffer(const DisplayContext* ctx)
{
    fill_framebuffer(ctx, 0, 0, 0);
}

static inline uint32_t
pack_rgb(uint8_t r, uint8_t g, uint8_t b)
{
    return ((uint32_t)r << 16) | ((uint32_t)g << 8) | (uint32_t)b;
}

static inline void
render_frame(DisplayContext* ctx)
{
    XPutImage(ctx->dpy, ctx->win, ctx->gc, ctx->img, 0, 0, 0, 0, ctx->w, ctx->h);
}

DisplayContext
init_display(int w, int h)
{
    Display* dpy = XOpenDisplay(NULL);
    if (!dpy)
        terminate("cannot open display");

    int screen = DefaultScreen(dpy);

    Window win = XCreateSimpleWindow(
        dpy,
        RootWindow(dpy, screen),
        100,
        100,
        w,
        h,
        1,
        BlackPixel(dpy, screen),
        WhitePixel(dpy, screen)
    );

    XSelectInput(dpy, win, ExposureMask | KeyPressMask | ButtonPressMask | PointerMotionMask);
    XMapWindow(dpy, win);

    Framebuffer fb = init_framebuffer(w, h);

    GC gc = XCreateGC(dpy, win, 0, 0);
    XImage* img = XCreateImage(
        dpy,
        DefaultVisual(dpy, screen),
        DefaultDepth(dpy, screen),
        ZPixmap,
        0,
        (char*)fb.data,
        w,
        h,
        32,
        0
    );
    if (!img)
        terminate("cannot create image");

    DisplayContext ctx;
    ctx.dpy = dpy;
    ctx.win = win;
    ctx.gc = gc;
    ctx.img = img;
    ctx.w = w;
    ctx.h = h;
    ctx.fb = fb;

    clear_framebuffer(&ctx);

    return ctx;
}

void
cleanup_display(DisplayContext* ctx)
{
    ctx->img->data = NULL;
    XDestroyImage(ctx->img);
    XDestroyWindow(ctx->dpy, ctx->win);
    XCloseDisplay(ctx->dpy);
}

static inline void
put_pixel(DisplayContext* ctx, int x, int y, uint8_t r, uint8_t g, uint8_t b)
{
    if ((unsigned)x >= ctx->w || (unsigned)y >= ctx->h)
        return;

    ctx->fb.data[y * ctx->w + x] = pack_rgb(r, g, b);
}

static void draw_line_thick(DisplayContext* ctx,
    int x0,
    int y0,
    int x1,
    int y1,
    int thickness,
    uint8_t r,
    uint8_t g,
    uint8_t b);

static void draw_dashed_line_thick(DisplayContext* ctx,
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

static void
put_pixel_thick(DisplayContext* ctx, int x, int y, int thickness, uint8_t r, uint8_t g, uint8_t b)
{
    if (thickness <= 1)
    {
        put_pixel(ctx, x, y, r, g, b);
        return;
    }

    int half = thickness / 2;
    for (int yy = y - half; yy <= y + half; ++yy)
        for (int xx = x - half; xx <= x + half; ++xx)
            put_pixel(ctx, xx, yy, r, g, b);
}

// --- Simple UI (top toolbar) ---
#define UI_BAR_H 48

static void
ui_fill_rect(DisplayContext* ctx, int x, int y, int w, int h, uint8_t r, uint8_t g, uint8_t b)
{
    if (w <= 0 || h <= 0)
        return;

    int x0 = x < 0 ? 0 : x;
    int y0 = y < 0 ? 0 : y;
    int x1 = x + w;
    int y1 = y + h;
    if (x1 > ctx->w)
        x1 = ctx->w;
    if (y1 > ctx->h)
        y1 = ctx->h;

    uint32_t color = pack_rgb(r, g, b);
    for (int yy = y0; yy < y1; ++yy)
    {
        uint32_t* row = &ctx->fb.data[yy * ctx->w];
        for (int xx = x0; xx < x1; ++xx)
            row[xx] = color;
    }
}

static void
ui_draw_border(DisplayContext* ctx, int x, int y, int w, int h, uint8_t r, uint8_t g, uint8_t b)
{
    if (w <= 0 || h <= 0)
        return;

    int x0 = x;
    int y0 = y;
    int x1 = x + w - 1;
    int y1 = y + h - 1;

    for (int xx = x0; xx <= x1; ++xx)
    {
        put_pixel(ctx, xx, y0, r, g, b);
        put_pixel(ctx, xx, y1, r, g, b);
    }
    for (int yy = y0; yy <= y1; ++yy)
    {
        put_pixel(ctx, x0, yy, r, g, b);
        put_pixel(ctx, x1, yy, r, g, b);
    }
}

static void
render_ui(DisplayContext* ctx, const InputState* state)
{
    // Background bar
    ui_fill_rect(ctx, 0, 0, ctx->w, UI_BAR_H, 32, 32, 32);
    ui_draw_border(ctx, 0, UI_BAR_H - 1, ctx->w, 1, 80, 80, 80);

    // Color swatch (click to cycle)
    const int pad = 10;
    const int sw = UI_BAR_H - 2 * pad;
    const int sx = pad;
    const int sy = pad;

    ui_fill_rect(ctx, sx, sy, sw, sw, state->color_r, state->color_g, state->color_b);
    ui_draw_border(ctx, sx, sy, sw, sw, 220, 220, 220);

    // Style button (solid/dashed)
    const int bx = sx + sw + pad;
    const int by = sy;
    ui_fill_rect(ctx, bx, by, sw, sw, 48, 48, 48);
    ui_draw_border(ctx, bx, by, sw, sw, 220, 220, 220);
    int cy = by + sw / 2;
    int x_start = bx + 4;
    int x_end = bx + sw - 5;
    if (state->dashed)
        draw_dashed_line_thick(ctx, x_start, cy, x_end, cy, 1, 3, 3, 230, 230, 230);
    else
        draw_line_thick(ctx, x_start, cy, x_end, cy, 1, 230, 230, 230);

    // Thickness button (3 levels)
    const int tx = bx + sw + pad;
    const int ty = by;
    ui_fill_rect(ctx, tx, ty, sw, sw, 48, 48, 48);
    ui_draw_border(ctx, tx, ty, sw, sw, 220, 220, 220);
    int y1 = ty + 8;
    int y2 = ty + sw / 2;
    int y3 = ty + sw - 9;
    draw_line_thick(ctx, tx + 4, y1, tx + sw - 5, y1, 1, 230, 230, 230);
    draw_line_thick(ctx, tx + 4, y2, tx + sw - 5, y2, 3, 230, 230, 230);
    draw_line_thick(ctx, tx + 4, y3, tx + sw - 5, y3, 6, 230, 230, 230);
    int sel_y = (state->thickness <= 1) ? y1 : (state->thickness <= 3 ? y2 : y3);
    ui_fill_rect(ctx, tx + sw - 7, sel_y - 2, 3, 5, 0, 180, 255);
}

static bool
ui_hit_color_swatch(int x, int y)
{
    const int pad = 10;
    const int sw = UI_BAR_H - 2 * pad;
    const int sx = pad;
    const int sy = pad;
    return (x >= sx && x < (sx + sw) && y >= sy && y < (sy + sw));
}

static bool
ui_hit_style_button(int x, int y)
{
    const int pad = 10;
    const int sw = UI_BAR_H - 2 * pad;
    const int sx = pad;
    const int bx = sx + sw + pad;
    const int by = pad;
    return (x >= bx && x < (bx + sw) && y >= by && y < (by + sw));
}

static bool
ui_hit_thickness_button(int x, int y)
{
    const int pad = 10;
    const int sw = UI_BAR_H - 2 * pad;
    const int sx = pad;
    const int bx = sx + sw + pad;
    const int tx = bx + sw + pad;
    const int ty = pad;
    return (x >= tx && x < (tx + sw) && y >= ty && y < (ty + sw));
}

static void
cycle_color(InputState* state)
{
    // blue -> red -> green -> blue ...
    if (state->color_r == 0 && state->color_g == 128 && state->color_b == 255)
    {
        state->color_r = 255;
        state->color_g = 64;
        state->color_b = 64;
    }
    else if (state->color_r == 255 && state->color_g == 64 && state->color_b == 64)
    {
        state->color_r = 64;
        state->color_g = 220;
        state->color_b = 64;
    }
    else
    {
        state->color_r = 0;
        state->color_g = 128;
        state->color_b = 255;
    }
}

static void
cycle_thickness(InputState* state)
{
    if (state->thickness <= 1)
        state->thickness = 3;
    else if (state->thickness <= 3)
        state->thickness = 6;
    else
        state->thickness = 1;
}

/* Bresenham's line algorithm */
static void
draw_line(DisplayContext* ctx, int x0, int y0, int x1, int y1, uint8_t r, uint8_t g, uint8_t b)
{
    int dx = abs(x1 - x0);
    int sx = x0 < x1 ? 1 : -1;
    int dy = -abs(y1 - y0);
    int sy = y0 < y1 ? 1 : -1;
    int err = dx + dy; /* error value e_xy */

    while (1)
    {
        put_pixel(ctx, x0, y0, r, g, b);

        if (x0 == x1 && y0 == y1)
            break;

        int e2 = 2 * err;
        if (e2 >= dy)
        {
            err += dy;
            x0 += sx;
        }
        if (e2 <= dx)
        {
            err += dx;
            y0 += sy;
        }
    }
}

static void
draw_dotted_line(
    DisplayContext* ctx, int x0, int y0, int x1, int y1, uint8_t r, uint8_t g, uint8_t b
)
{
    int dx = abs(x1 - x0);
    int sx = x0 < x1 ? 1 : -1;
    int dy = -abs(y1 - y0);
    int sy = y0 < y1 ? 1 : -1;
    int err = dx + dy; /* error value e_xy */

    int counter = 0;

    while (1)
    {
        if ((counter % 10) < 2)
            put_pixel(ctx, x0, y0, r, g, b);

        counter++;

        if (x0 == x1 && y0 == y1)
            break;

        int e2 = 2 * err;
        if (e2 >= dy)
        {
            err += dy;
            x0 += sx;
        }
        if (e2 <= dx)
        {
            err += dx;
            y0 += sy;
        }
    }
}

static void
draw_line_thick(DisplayContext* ctx,
    int x0,
    int y0,
    int x1,
    int y1,
    int thickness,
    uint8_t r,
    uint8_t g,
    uint8_t b)
{
    int dx = abs(x1 - x0);
    int sx = x0 < x1 ? 1 : -1;
    int dy = -abs(y1 - y0);
    int sy = y0 < y1 ? 1 : -1;
    int err = dx + dy;

    while (1)
    {
        put_pixel_thick(ctx, x0, y0, thickness, r, g, b);

        if (x0 == x1 && y0 == y1)
            break;

        int e2 = 2 * err;
        if (e2 >= dy)
        {
            err += dy;
            x0 += sx;
        }
        if (e2 <= dx)
        {
            err += dx;
            y0 += sy;
        }
    }
}

static void
draw_dashed_line_thick(DisplayContext* ctx,
    int x0,
    int y0,
    int x1,
    int y1,
    int thickness,
    int on_len,
    int off_len,
    uint8_t r,
    uint8_t g,
    uint8_t b)
{
    int dx = abs(x1 - x0);
    int sx = x0 < x1 ? 1 : -1;
    int dy = -abs(y1 - y0);
    int sy = y0 < y1 ? 1 : -1;
    int err = dx + dy;

    int counter = 0;
    int period = on_len + off_len;
    if (period <= 0)
        period = 1;

    while (1)
    {
        if ((counter % period) < on_len)
            put_pixel_thick(ctx, x0, y0, thickness, r, g, b);
        counter++;

        if (x0 == x1 && y0 == y1)
            break;

        int e2 = 2 * err;
        if (e2 >= dy)
        {
            err += dy;
            x0 += sx;
        }
        if (e2 <= dx)
        {
            err += dx;
            y0 += sy;
        }
    }
}

/* Snap line to nearest axis (horizontal/vertical/45°) */
static void
snap_to_axis(DisplayContext* ctx, int x0, int y0, int* x1, int* y1)
{
    int dx = *x1 - x0;
    int dy = *y1 - y0;
    int abs_dx = abs(dx);
    int abs_dy = abs(dy);

    /* Find which axis is closest */
    if (abs_dx < abs_dy / 2)
    {
        /* Vertical */
        *x1 = x0;
    }
    else if (abs_dy < abs_dx / 2)
    {
        /* Horizontal */
        *y1 = y0;
    }
    else
    {
        /* 45° diagonal - make dx == dy */
        int min_dist = abs_dx < abs_dy ? abs_dx : abs_dy;
        *x1 = x0 + (dx > 0 ? min_dist : -min_dist);
        *y1 = y0 + (dy > 0 ? min_dist : -min_dist);
    }
}

#define W 600
#define H 800

void
handle_keypress(XEvent* e, DisplayContext* ctx, InputState* state)
{
    if (e->type != KeyPress)
        return;

    KeySym sym = XLookupKeysym(&e->xkey, 0);
    if (sym == XK_Escape || sym == XK_q)
        state->running = false;
    else if (sym == XK_c || sym == XK_C)
    {
        clear_framebuffer(ctx);
        state->have_first = false;
        render_ui(ctx, state);
        XPutImage(ctx->dpy, ctx->win, ctx->gc, ctx->img, 0, 0, 0, 0, ctx->w, ctx->h);
    }
}

void
handle_click(XEvent* e, DisplayContext* ctx, InputState* state)
{
    if (e->type != ButtonPress)
        return;

    int x = e->xbutton.x;
    int y = e->xbutton.y;

    if (y < UI_BAR_H)
    {
        if (ui_hit_color_swatch(x, y))
        {
            cycle_color(state);
            render_ui(ctx, state);
            XPutImage(ctx->dpy, ctx->win, ctx->gc, ctx->img, 0, 0, 0, 0, ctx->w, ctx->h);
        }
        else if (ui_hit_style_button(x, y))
        {
            state->dashed = !state->dashed;
            render_ui(ctx, state);
            XPutImage(ctx->dpy, ctx->win, ctx->gc, ctx->img, 0, 0, 0, 0, ctx->w, ctx->h);
        }
        else if (ui_hit_thickness_button(x, y))
        {
            cycle_thickness(state);
            render_ui(ctx, state);
            XPutImage(ctx->dpy, ctx->win, ctx->gc, ctx->img, 0, 0, 0, 0, ctx->w, ctx->h);
        }
        return;
    }

    if (!state->have_first)
    {
        state->x0 = x;
        state->y0 = y;
        state->have_first = true;
    put_pixel_thick(ctx, x, y, state->thickness, state->color_r, state->color_g, state->color_b);

        memcpy(ctx->fb.saved, ctx->fb.data, ctx->w * ctx->h * sizeof(uint32_t));

        XPutImage(ctx->dpy, ctx->win, ctx->gc, ctx->img, 0, 0, 0, 0, ctx->w, ctx->h);
    }
    else
    {
        memcpy(ctx->fb.data, ctx->fb.saved, ctx->w * ctx->h * sizeof(uint32_t));

        if (e->xbutton.state & ShiftMask)
            snap_to_axis(ctx, state->x0, state->y0, &x, &y);

        if (e->xbutton.state & ControlMask)
        {
            draw_dotted_line(ctx, state->x0, state->y0, x, y, state->color_r, state->color_g, state->color_b);
        }
        else if (state->dashed)
        {
            draw_dashed_line_thick(
                ctx,
                state->x0,
                state->y0,
                x,
                y,
                state->thickness,
                6,
                4,
                state->color_r,
                state->color_g,
                state->color_b
            );
        }
        else
        {
            draw_line_thick(
                ctx,
                state->x0,
                state->y0,
                x,
                y,
                state->thickness,
                state->color_r,
                state->color_g,
                state->color_b
            );
        }

        state->have_first = false;
        render_ui(ctx, state);
        XPutImage(ctx->dpy, ctx->win, ctx->gc, ctx->img, 0, 0, 0, 0, ctx->w, ctx->h);
    }
}

void
handle_motion(XEvent* e, DisplayContext* ctx, InputState* state)
{
    if (e->type != MotionNotify)
        return;
    if (!state->have_first)
        return;

    int x = e->xmotion.x;
    int y = e->xmotion.y;

    memcpy(ctx->fb.data, ctx->fb.saved, ctx->w * ctx->h * sizeof(uint32_t));

    if (e->xmotion.state & ShiftMask)
        snap_to_axis(ctx, x, y, &x, &y);

    if (e->xmotion.state & ControlMask)
        draw_dotted_line(ctx, state->x0, state->y0, x, y, 120, 120, 120);
    else if (state->dashed)
        draw_dashed_line_thick(ctx, state->x0, state->y0, x, y, state->thickness, 6, 4, 120, 120, 120);
    else
        draw_line_thick(ctx, state->x0, state->y0, x, y, state->thickness, 120, 120, 120);

    render_ui(ctx, state);
    XPutImage(ctx->dpy, ctx->win, ctx->gc, ctx->img, 0, 0, 0, 0, ctx->w, ctx->h);
}

int
main()
{
    DisplayContext ctx = init_display(W, H);
    InputState state = {
        .running = true,
        .have_first = false,
        .color_r = 0,
        .color_g = 128,
        .color_b = 255,
    .thickness = 1,
    .dashed = false,
    };

    render_ui(&ctx, &state);
    render_frame(&ctx);

    register_handler(handle_keypress);
    register_handler(handle_click);
    register_handler(handle_motion);

    while (state.running)
    {
        XEvent e;
        XNextEvent(ctx.dpy, &e);

        for (int i = 0; i < handler_count; i++)
            handlers[i](&e, &ctx, &state);
    }

    cleanup_display(&ctx);
}
