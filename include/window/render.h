#ifndef WINDOW_RENDER_H
#define WINDOW_RENDER_H

#include "window/window.h"
#include "terminal/style.h"

#define UP_LEFT U'┌'
#define UP_RIGHT U'┐'
#define DOWN_LEFT U'└'
#define DOWN_RIGHT U'┘'
#define HORIZONTAL U'─'
#define VERTICAL U'│'

void window_draw
(
	const Window* window,
	const struct window_color* color
);

void window_move_screen_cursor(const Window* w);
void window_clamp_cursor_to_view(Window* w);

#endif