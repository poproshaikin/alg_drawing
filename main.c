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
    bool have_first; // first point is selected
    int x0, y0; // cursor position
    unsigned int modifiers; // shift / control
} InputState;

typedef void (*EventHandler)(XEvent*, DisplayContext*, InputState*);

#define MAX_HANDLERS 32
EventHandler handlers[MAX_HANDLERS];
int handler_count = 0;

void register_handler(EventHandler h)
{
    if (handler_count < MAX_HANDLERS)
        handlers[handler_count++] = h;
}

void terminate(const char* message)
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

void cleanup_display(DisplayContext* ctx)
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

    uint32_t color = (r << 16) | (g << 8) | b;
    ctx->fb.data[y * ctx->w + x] = color;
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
draw_dotted_line(DisplayContext* ctx, int x0, int y0, int x1, int y1, uint8_t r, uint8_t g, uint8_t b)
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
    if (e->type != KeyPress) return;

    KeySym sym = XLookupKeysym(&e->xkey, 0);
    if (sym == XK_Escape || sym == XK_q)
        state->running = false;
    else if (sym == XK_c || sym == XK_C) {
        clear_framebuffer(ctx);
        state->have_first = false;
        XPutImage(ctx->dpy, ctx->win, ctx->gc, ctx->img, 0,0,0,0, ctx->w, ctx->h);
    }
}

void
handle_click(XEvent* e, DisplayContext* ctx, InputState* state)
{
    if (e->type != ButtonPress) return;

    int x = e->xbutton.x;
    int y = e->xbutton.y;

    if (!state->have_first) {
        state->x0 = x; state->y0 = y;
        state->have_first = true;
        put_pixel(ctx, x, y, 255,255,255);

        memcpy(ctx->fb.saved, ctx->fb.data, ctx->w * ctx->h * sizeof(uint32_t));

        XPutImage(ctx->dpy, ctx->win, ctx->gc, ctx->img, 0,0,0,0, ctx->w, ctx->h);
    } else {
        memcpy(ctx->fb.data, ctx->fb.saved, ctx->w*ctx->h*sizeof(uint32_t));

        if (e->xbutton.state & ShiftMask)
            snap_to_axis(ctx, state->x0, state->y0, &x, &y);

        if (e->xbutton.state & ControlMask)
            draw_dotted_line(ctx, state->x0,state->y0,x,y,255,255,255);
        else
            draw_line(ctx, state->x0, state->y0, x, y, 255, 255, 255);

        state->have_first = false;
        XPutImage(ctx->dpy, ctx->win, ctx->gc, ctx->img, 0,0,0,0, ctx->w, ctx->h);
    }
}

int main()
{
    DisplayContext ctx = init_display(W, H);
    InputState state = { .running = true, .have_first = false };
    render_frame(&ctx);

    register_handler(handle_keypress);
    register_handler(handle_click);

    while (state.running)
    {
        XEvent e;
        XNextEvent(ctx.dpy, &e);

        for (int i=0; i<handler_count; i++)
            handlers[i](&e, &ctx, &state);

    }

    cleanup_display(&ctx);
}
