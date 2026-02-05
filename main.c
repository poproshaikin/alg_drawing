#include <X11/Xlib.h>
#include <X11/Xutil.h>
#include <stdlib.h>
#include <assert.h>
#include <stdint.h>
#include <unistd.h>
#include <X11/keysym.h>
#include <stdbool.h>
#include <string.h>

#define W 800
#define H 600

uint32_t framebuffer[W * H];

static inline void
put_pixel(int x, int y, uint8_t r, uint8_t g, uint8_t b)
{
    if ((unsigned)x >= W || (unsigned)y >= H)
        return;

    uint32_t color = (r << 16) | (g << 8) | b;
    framebuffer[y * W + x] = color;
}

static void
clear_framebuffer(uint8_t r, uint8_t g, uint8_t b)
{
    uint32_t color = (r << 16) | (g << 8) | b;
    for (int i = 0; i < W * H; ++i)
        framebuffer[i] = color;
}

/* Bresenham's line algorithm */
static void
draw_line(int x0, int y0, int x1, int y1, uint8_t r, uint8_t g, uint8_t b)
{
    int dx = abs(x1 - x0);
    int sx = x0 < x1 ? 1 : -1;
    int dy = -abs(y1 - y0);
    int sy = y0 < y1 ? 1 : -1;
    int err = dx + dy; /* error value e_xy */

    while (1)
    {
        put_pixel(x0, y0, r, g, b);
        if (x0 == x1 && y0 == y1) break;
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
snap_to_axis(int x0, int y0, int *x1, int *y1)
{
    int dx = *x1 - x0;
    int dy = *y1 - y0;
    int abs_dx = abs(dx);
    int abs_dy = abs(dy);
    
    /* Find which axis is closest */
    if (abs_dx < abs_dy / 2) {
        /* Vertical */
        *x1 = x0;
    } else if (abs_dy < abs_dx / 2) {
        /* Horizontal */
        *y1 = y0;
    } else {
        /* 45° diagonal - make dx == dy */
        int min_dist = abs_dx < abs_dy ? abs_dx : abs_dy;
        *x1 = x0 + (dx > 0 ? min_dist : -min_dist);
        *y1 = y0 + (dy > 0 ? min_dist : -min_dist);
    }
}

int
main(void)
{
    Display* dpy = XOpenDisplay(NULL);
    if (!dpy) return 1;

    int screen = DefaultScreen(dpy);

    Window win = XCreateSimpleWindow(
        dpy,
        RootWindow(dpy, screen),
        100,
        100,
        W,
        H,
        1,
        BlackPixel(dpy, screen),
        WhitePixel(dpy, screen)
    );

    XSelectInput(dpy, win, ExposureMask | KeyPressMask | ButtonPressMask | PointerMotionMask);
    XMapWindow(dpy, win);

    GC gc = XCreateGC(dpy, win, 0, 0);
    XImage* img = XCreateImage(
        dpy,
        DefaultVisual(dpy, screen),
        DefaultDepth(dpy, screen),
        ZPixmap,
        0,
        (char*)framebuffer,
        W,
        H,
        32,
        0
    );

    clear_framebuffer(0, 0, 0);

    bool running = true;
    bool have_first = false;
    bool shift_pressed = false;
    int x0 = 0, y0 = 0;
    int preview_x = 0, preview_y = 0;

    XPutImage(dpy, win, gc, img, 0, 0, 0, 0, W, H);

    while (running)
    {
        XEvent e;
        XNextEvent(dpy, &e);

        if (e.type == Expose)
        {
            XPutImage(dpy, win, gc, img, 0, 0, 0, 0, W, H);
        }
        else if (e.type == KeyPress)
        {
            XKeyEvent *kev = &e.xkey;
            KeySym sym = XLookupKeysym(kev, 0);
            if (sym == XK_Escape || sym == XK_q)
                running = false;
            else if (sym == XK_c || sym == XK_C)
            {
                clear_framebuffer(0, 0, 0);
                have_first = false;
                XPutImage(dpy, win, gc, img, 0, 0, 0, 0, W, H);
            }
        }
        else if (e.type == ButtonPress)
        {
            int x = e.xbutton.x;
            int y = e.xbutton.y;

            if (!have_first)
            {
                x0 = x; y0 = y;
                have_first = true;
                put_pixel(x0, y0, 255, 255, 255);
                XPutImage(dpy, win, gc, img, 0, 0, 0, 0, W, H);
            }
            else
            {
                if (e.xbutton.state & ShiftMask) {
                    snap_to_axis(x0, y0, &x, &y);
                }
                
                draw_line(x0, y0, x, y, 255, 255, 255);
                have_first = false;
                XPutImage(dpy, win, gc, img, 0, 0, 0, 0, W, H);
            }
        }
    }

    img->data = NULL;
    XDestroyImage(img);
    XDestroyWindow(dpy, win);
    XCloseDisplay(dpy);

    return 0;
}
