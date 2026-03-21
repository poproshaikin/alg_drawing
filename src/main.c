#include "app.h"
#include "display.h"
#include "ui.h"

#define W 600
#define H 800

int
main(void)
{
    DisplayContext ctx = init_display(W, H);
    InputState state = {
        .running = true,
        .have_first = false,
        .color_r = 0,
        .color_g = 128,
        .color_b = 255,
        .thickness = 1,
    .line_style = 0,
    .tool = 1,
    .poly_count = 0,
    .snap_mode = -1,
    };

    render_ui(&ctx, &state);
    render_frame(&ctx);

    register_handler(handle_keypress);
    register_handler(handle_click);
    register_handler(handle_motion);

    app_run(&ctx, &state);

    cleanup_display(&ctx);
    return 0;
}
