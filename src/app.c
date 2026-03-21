#include "app.h"

#include "display.h"
#include "draw.h"
#include "ui.h"

#include <X11/keysym.h>
#include <stdlib.h>
#include <math.h>
#include <string.h>

#define POLY_SNAP_DIST 12

static void
apply_snap_mode(int snap_mode, int x0, int y0, int* x1, int* y1)
{
    if (snap_mode == 1)
    {
        *x1 = x0;
    }
    else if (snap_mode == 0)
    {
        *y1 = y0;
    }
    else if (snap_mode == 2)
    {
        int dx = *x1 - x0;
        int dy = *y1 - y0;
        int abs_dx = abs(dx);
        int abs_dy = abs(dy);
        int min_dist = abs_dx < abs_dy ? abs_dx : abs_dy;
        *x1 = x0 + (dx > 0 ? min_dist : -min_dist);
        *y1 = y0 + (dy > 0 ? min_dist : -min_dist);
    }
}

static int
choose_snap_mode(int x0, int y0, int x1, int y1)
{
    int dx = x1 - x0;
    int dy = y1 - y0;
    int abs_dx = abs(dx);
    int abs_dy = abs(dy);

    if (abs_dx < abs_dy / 2)
        return 1; // vertical
    if (abs_dy < abs_dx / 2)
        return 0; // horizontal
    return 2;     // diag
}

#define MAX_HANDLERS 32
static EventHandler handlers[MAX_HANDLERS];
static int handler_count = 0;

void
register_handler(EventHandler h)
{
    if (handler_count < MAX_HANDLERS)
        handlers[handler_count++] = h;
}

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
        render_frame(ctx);
    }
}

void
handle_click(XEvent* e, DisplayContext* ctx, InputState* state)
{
    if (e->type != ButtonPress)
        return;

    int x = e->xbutton.x;
    int y = e->xbutton.y;

    if (ui_handle_click(x, y, ctx, state))
        return;

    // Tool: point
    if (state->tool == 0)
    {
        put_pixel_thick(ctx, x, y, state->thickness, state->color_r, state->color_g, state->color_b);
        memcpy(ctx->fb.saved, ctx->fb.data, (size_t)ctx->w * (size_t)ctx->h * sizeof(uint32_t));
        render_ui(ctx, state);
        render_frame(ctx);
        return;
    }

    // Tool: polygon (left click adds point, right click closes if >=3)
    if (state->tool == 3)
    {
        if (e->xbutton.button == Button3)
        {
            if (state->poly_count >= 3)
            {
                // restore preview base then draw closing edge and commit
                memcpy(ctx->fb.data, ctx->fb.saved, (size_t)ctx->w * (size_t)ctx->h * sizeof(uint32_t));
                int x0 = state->poly_x[state->poly_count - 1];
                int y0 = state->poly_y[state->poly_count - 1];
                int x1 = state->poly_x[0];
                int y1 = state->poly_y[0];

                if (e->xbutton.state & ShiftMask)
                    snap_to_axis(x0, y0, &x1, &y1);

                if (state->line_style == 2)
                    draw_dotted_line(ctx, x0, y0, x1, y1, state->color_r, state->color_g, state->color_b);
                else if (state->line_style == 1)
                    draw_dashed_line_thick(
                        ctx, x0, y0, x1, y1, state->thickness, 6, 4, state->color_r, state->color_g, state->color_b
                    );
                else
                    draw_line_thick(
                        ctx, x0, y0, x1, y1, state->thickness, state->color_r, state->color_g, state->color_b
                    );

                state->have_first = false;
                state->poly_count = 0;
                memcpy(ctx->fb.saved, ctx->fb.data, (size_t)ctx->w * (size_t)ctx->h * sizeof(uint32_t));
                render_ui(ctx, state);
                render_frame(ctx);
            }
            return;
        }

        if (state->poly_count == 0)
        {
            state->poly_x[0] = x;
            state->poly_y[0] = y;
            state->poly_count = 1;
            state->have_first = true;

            put_pixel_thick(ctx, x, y, state->thickness, state->color_r, state->color_g, state->color_b);
            memcpy(ctx->fb.saved, ctx->fb.data, (size_t)ctx->w * (size_t)ctx->h * sizeof(uint32_t));
            render_ui(ctx, state);
            render_frame(ctx);
            return;
        }

        // add an edge from last point to new point
        int x0 = state->poly_x[state->poly_count - 1];
        int y0 = state->poly_y[state->poly_count - 1];
        int x1 = x;
        int y1 = y;

        // Snap to first vertex if close enough (helps closing the polygon)
        if (state->poly_count >= 2)
        {
            int fx = state->poly_x[0];
            int fy = state->poly_y[0];
            int ddx = x1 - fx;
            int ddy = y1 - fy;
            if ((ddx * ddx + ddy * ddy) <= (POLY_SNAP_DIST * POLY_SNAP_DIST))
            {
                x1 = fx;
                y1 = fy;
            }
        }

        if (e->xbutton.state & ShiftMask)
            snap_to_axis(x0, y0, &x1, &y1);

        memcpy(ctx->fb.data, ctx->fb.saved, (size_t)ctx->w * (size_t)ctx->h * sizeof(uint32_t));

        if (state->line_style == 2)
            draw_dotted_line(ctx, x0, y0, x1, y1, state->color_r, state->color_g, state->color_b);
        else if (state->line_style == 1)
            draw_dashed_line_thick(
                ctx, x0, y0, x1, y1, state->thickness, 6, 4, state->color_r, state->color_g, state->color_b
            );
        else
            draw_line_thick(ctx, x0, y0, x1, y1, state->thickness, state->color_r, state->color_g, state->color_b);

        if (state->poly_count < 256)
        {
            state->poly_x[state->poly_count] = x1;
            state->poly_y[state->poly_count] = y1;
            state->poly_count++;
        }

        // If we snapped to the first vertex, close and finish immediately
        if (x1 == state->poly_x[0] && y1 == state->poly_y[0] && state->poly_count >= 4)
        {
            state->have_first = false;
            state->poly_count = 0;
        }

        // commit as new base
        memcpy(ctx->fb.saved, ctx->fb.data, (size_t)ctx->w * (size_t)ctx->h * sizeof(uint32_t));
        render_ui(ctx, state);
        render_frame(ctx);
        return;
    }

    if (!state->have_first)
    {
        state->x0 = x;
        state->y0 = y;
        state->have_first = true;

    put_pixel_thick(ctx, x, y, state->thickness, state->color_r, state->color_g, state->color_b);
        memcpy(ctx->fb.saved, ctx->fb.data, (size_t)ctx->w * (size_t)ctx->h * sizeof(uint32_t));
        render_frame(ctx);
        return;
    }

    memcpy(ctx->fb.data, ctx->fb.saved, (size_t)ctx->w * (size_t)ctx->h * sizeof(uint32_t));

    if (e->xbutton.state & ShiftMask)
        snap_to_axis(state->x0, state->y0, &x, &y);

    // Tool: circle
    if (state->tool == 2)
    {
        int dx = x - state->x0;
        int dy = y - state->y0;
        int r = (int)(sqrt((double)dx * (double)dx + (double)dy * (double)dy) + 0.5);
        bool dashed = (state->line_style != 0);
        draw_circle(ctx,
            state->x0,
            state->y0,
            r,
            state->thickness,
            dashed,
            state->color_r,
            state->color_g,
            state->color_b);

        state->have_first = false;
        render_ui(ctx, state);
        render_frame(ctx);
        return;
    }

    if (state->line_style == 2)
    {
        draw_dotted_line(ctx, state->x0, state->y0, x, y, state->color_r, state->color_g, state->color_b);
    }
    else if (state->line_style == 1)
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
    render_frame(ctx);
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

    memcpy(ctx->fb.data, ctx->fb.saved, (size_t)ctx->w * (size_t)ctx->h * sizeof(uint32_t));

    // Shift snapping: lock snap direction to avoid jitter near thresholds
    bool shift_down = (e->xmotion.state & ShiftMask) != 0;
    if (!shift_down)
    {
        state->snap_mode = -1;
    }
    else
    {
        if (state->snap_mode < 0)
            state->snap_mode = choose_snap_mode(state->x0, state->y0, x, y);
        apply_snap_mode(state->snap_mode, state->x0, state->y0, &x, &y);
    }

    // Polygon preview edge
    if (state->tool == 3 && state->poly_count > 0)
    {
        int x0 = state->poly_x[state->poly_count - 1];
        int y0 = state->poly_y[state->poly_count - 1];
        int x1 = x;
        int y1 = y;

        // Snap preview to first vertex when close
        if (state->poly_count >= 2)
        {
            int fx = state->poly_x[0];
            int fy = state->poly_y[0];
            int ddx = x1 - fx;
            int ddy = y1 - fy;
            if ((ddx * ddx + ddy * ddy) <= (POLY_SNAP_DIST * POLY_SNAP_DIST))
            {
                x1 = fx;
                y1 = fy;
            }
        }

        if (shift_down)
        {
            // for polygon, lock relative to the last vertex
            if (state->snap_mode < 0)
                state->snap_mode = choose_snap_mode(x0, y0, x1, y1);
            apply_snap_mode(state->snap_mode, x0, y0, &x1, &y1);
        }

        if (state->line_style == 2)
            draw_dotted_line(ctx, x0, y0, x1, y1, 120, 120, 120);
        else if (state->line_style == 1)
            draw_dashed_line_thick(ctx, x0, y0, x1, y1, state->thickness, 6, 4, 120, 120, 120);
        else
            draw_line_thick(ctx, x0, y0, x1, y1, state->thickness, 120, 120, 120);
    }
    else if (state->tool == 2)
    {
        int dx = x - state->x0;
        int dy = y - state->y0;
        int r = (int)(sqrt((double)dx * (double)dx + (double)dy * (double)dy) + 0.5);
        bool dashed = (state->line_style != 0);
        draw_circle(ctx, state->x0, state->y0, r, state->thickness, dashed, 120, 120, 120);
    }
    else
    {
        if (state->line_style == 2)
            draw_dotted_line(ctx, state->x0, state->y0, x, y, 120, 120, 120);
        else if (state->line_style == 1)
            draw_dashed_line_thick(ctx, state->x0, state->y0, x, y, state->thickness, 6, 4, 120, 120, 120);
        else
            draw_line_thick(ctx, state->x0, state->y0, x, y, state->thickness, 120, 120, 120);
    }

    render_ui(ctx, state);
    render_frame(ctx);
}

void
app_run(DisplayContext* ctx, InputState* state)
{
    while (state->running)
    {
        XEvent e;
        XNextEvent(ctx->dpy, &e);
        for (int i = 0; i < handler_count; i++)
            handlers[i](&e, ctx, state);
    }
}
