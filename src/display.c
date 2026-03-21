#include "display.h"

#include <X11/Xutil.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static void
terminate(const char* message)
{
    fprintf(stderr, "execution terminated, reason: %s\n", message);
}

static Framebuffer
init_framebuffer(int w, int h)
{
    uint32_t* data = calloc((size_t)w * (size_t)h, sizeof(uint32_t));
    uint32_t* saved = calloc((size_t)w * (size_t)h, sizeof(uint32_t));

    Framebuffer fb;
    fb.data = data;
    fb.saved = saved;

    return fb;
}

void
fill_framebuffer(const DisplayContext* ctx, uint8_t r, uint8_t g, uint8_t b)
{
    uint32_t color = pack_rgb(r, g, b);
    for (int i = 0; i < ctx->w * ctx->h; ++i)
        ctx->fb.data[i] = color;
}

void
clear_framebuffer(const DisplayContext* ctx)
{
    fill_framebuffer(ctx, 0, 0, 0);
}

void
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
        (unsigned)w,
        (unsigned)h,
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
        (unsigned)w,
        (unsigned)h,
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

    free(ctx->fb.data);
    free(ctx->fb.saved);
}

void
put_pixel(DisplayContext* ctx, int x, int y, uint8_t r, uint8_t g, uint8_t b)
{
    if ((unsigned)x >= (unsigned)ctx->w || (unsigned)y >= (unsigned)ctx->h)
        return;

    ctx->fb.data[y * ctx->w + x] = pack_rgb(r, g, b);
}

void
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
