#include "draw.h"

#include "display.h"

#include <stdlib.h>

void
draw_line(DisplayContext* ctx, int x0, int y0, int x1, int y1, uint8_t r, uint8_t g, uint8_t b)
{
    int dx = abs(x1 - x0);
    int sx = x0 < x1 ? 1 : -1;
    int dy = -abs(y1 - y0);
    int sy = y0 < y1 ? 1 : -1;
    int err = dx + dy;

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

void
draw_dotted_line(DisplayContext* ctx,
    int x0,
    int y0,
    int x1,
    int y1,
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

void
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

void
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

void
snap_to_axis(int x0, int y0, int* x1, int* y1)
{
    int dx = *x1 - x0;
    int dy = *y1 - y0;
    int abs_dx = abs(dx);
    int abs_dy = abs(dy);

    if (abs_dx < abs_dy / 2)
    {
        *x1 = x0;
    }
    else if (abs_dy < abs_dx / 2)
    {
        *y1 = y0;
    }
    else
    {
        int min_dist = abs_dx < abs_dy ? abs_dx : abs_dy;
        *x1 = x0 + (dx > 0 ? min_dist : -min_dist);
        *y1 = y0 + (dy > 0 ? min_dist : -min_dist);
    }
}

static void
circle_plot8(DisplayContext* ctx,
    int cx,
    int cy,
    int x,
    int y,
    int thickness,
    uint8_t r,
    uint8_t g,
    uint8_t b)
{
    put_pixel_thick(ctx, cx + x, cy + y, thickness, r, g, b);
    put_pixel_thick(ctx, cx - x, cy + y, thickness, r, g, b);
    put_pixel_thick(ctx, cx + x, cy - y, thickness, r, g, b);
    put_pixel_thick(ctx, cx - x, cy - y, thickness, r, g, b);
    put_pixel_thick(ctx, cx + y, cy + x, thickness, r, g, b);
    put_pixel_thick(ctx, cx - y, cy + x, thickness, r, g, b);
    put_pixel_thick(ctx, cx + y, cy - x, thickness, r, g, b);
    put_pixel_thick(ctx, cx - y, cy - x, thickness, r, g, b);
}

void
draw_circle(DisplayContext* ctx,
    int cx,
    int cy,
    int radius,
    int thickness,
    bool dashed,
    uint8_t r,
    uint8_t g,
    uint8_t b)
{
    if (radius < 0)
        radius = -radius;
    if (radius == 0)
    {
        put_pixel_thick(ctx, cx, cy, thickness, r, g, b);
        return;
    }

    int x = 0;
    int y = radius;
    int d = 1 - radius;

    int step = 0;
    const int on_len = 6;
    const int off_len = 4;
    const int period = on_len + off_len;

    while (x <= y)
    {
        if (!dashed || ((step % period) < on_len))
            circle_plot8(ctx, cx, cy, x, y, thickness, r, g, b);

        if (d < 0)
        {
            d += 2 * x + 3;
        }
        else
        {
            d += 2 * (x - y) + 5;
            y -= 1;
        }

        x += 1;
        step += 1;
    }
}
