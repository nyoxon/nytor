#ifndef WINDOW_ACTION_H
#define WINDOW_ACTION_H

#include "window/window.h"

void window_cursor_update(Window* w);

void window_move_cursor_right(Window* window);
void window_move_cursor_left(Window* window);
void window_move_cursor_up(Window* window);
void window_move_cursor_down(Window* window);
void window_move_cursor_start_line(Window* window);
void window_move_cursor_end_line(Window* window);

void window_scroll_up(Window* window, int follow_scroll);
void window_scroll_down(Window* window, int follow_scroll);
void window_scroll_right(Window* window, int follow_scroll);
void window_scroll_left(Window* window, int follow_scroll);

#endif