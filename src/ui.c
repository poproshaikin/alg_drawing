#include "ui.h"

#include "display.h"
#include "draw.h"

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
cycle_color(InputState* state)
{
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

static void
cycle_line_style(InputState* state)
{
    state->line_style = (state->line_style + 1) % 3;
}

static void
ui_layout(int* sx, int* sy, int* sw, int* bx, int* by, int* tx, int* ty, int* mx, int* my)
{
    const int pad = 10;
    const int size = UI_BAR_H - 2 * pad;

    *sx = pad;
    *sy = pad;
    *sw = size;

    *bx = *sx + size + pad;
    *by = pad;

    *tx = *bx + size + pad;
    *ty = pad;

    *mx = *tx + size + pad;
    *my = pad;
}

static void
cycle_tool(InputState* state)
{
    state->tool = (state->tool + 1) % 4;
    state->have_first = false;
    state->poly_count = 0;
}

void
render_ui(DisplayContext* ctx, const InputState* state)
{
    ui_fill_rect(ctx, 0, 0, ctx->w, UI_BAR_H, 32, 32, 32);
    ui_draw_border(ctx, 0, UI_BAR_H - 1, ctx->w, 1, 80, 80, 80);

    int sx, sy, sw, bx, by, tx, ty, mx, my;
    ui_layout(&sx, &sy, &sw, &bx, &by, &tx, &ty, &mx, &my);

    ui_fill_rect(ctx, sx, sy, sw, sw, state->color_r, state->color_g, state->color_b);
    ui_draw_border(ctx, sx, sy, sw, sw, 220, 220, 220);

    ui_fill_rect(ctx, bx, by, sw, sw, 48, 48, 48);
    ui_draw_border(ctx, bx, by, sw, sw, 220, 220, 220);
    int cy = by + sw / 2;
    int x_start = bx + 4;
    int x_end = bx + sw - 5;
    if (state->line_style == 2)
    {
        // dotted icon
        draw_dotted_line(ctx, x_start, cy, x_end, cy, 230, 230, 230);
    }
    else if (state->line_style == 1)
    {
        // dashed icon
        draw_dashed_line_thick(ctx, x_start, cy, x_end, cy, 1, 3, 3, 230, 230, 230);
    }
    else
    {
        // solid icon
        draw_line_thick(ctx, x_start, cy, x_end, cy, 1, 230, 230, 230);
    }

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

    // Tool button (point/line/circle/polygon)
    ui_fill_rect(ctx, mx, my, sw, sw, 48, 48, 48);
    ui_draw_border(ctx, mx, my, sw, sw, 220, 220, 220);
    int cx = mx + sw / 2;
    int cy2 = my + sw / 2;
    if (state->tool == 0)
    {
        put_pixel_thick(ctx, cx, cy2, 3, 230, 230, 230);
    }
    else if (state->tool == 1)
    {
        draw_line_thick(ctx, mx + 4, my + sw - 6, mx + sw - 5, my + 5, 2, 230, 230, 230);
    }
    else if (state->tool == 2)
    {
        draw_circle(ctx, cx, cy2, sw / 3, 1, false, 230, 230, 230);
    }
    else
    {
        // triangle-ish polygon icon
        draw_line_thick(ctx, cx, my + 6, mx + 6, my + sw - 6, 1, 230, 230, 230);
        draw_line_thick(ctx, mx + 6, my + sw - 6, mx + sw - 6, my + sw - 10, 1, 230, 230, 230);
        draw_line_thick(ctx, mx + sw - 6, my + sw - 10, cx, my + 6, 1, 230, 230, 230);
    }
}

bool
ui_handle_click(int x, int y, DisplayContext* ctx, InputState* state)
{
    if (y >= UI_BAR_H)
        return false;

    int sx, sy, sw, bx, by, tx, ty, mx, my;
    ui_layout(&sx, &sy, &sw, &bx, &by, &tx, &ty, &mx, &my);

    bool in_color = (x >= sx && x < sx + sw && y >= sy && y < sy + sw);
    bool in_style = (x >= bx && x < bx + sw && y >= by && y < by + sw);
    bool in_thick = (x >= tx && x < tx + sw && y >= ty && y < ty + sw);
    bool in_tool = (x >= mx && x < mx + sw && y >= my && y < my + sw);

    if (in_color)
        cycle_color(state);
    else if (in_style)
        cycle_line_style(state);
    else if (in_thick)
        cycle_thickness(state);
    else if (in_tool)
        cycle_tool(state);
    else
        return true;

    render_ui(ctx, state);
    render_frame(ctx);
    return true;
}
