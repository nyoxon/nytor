#include <assert.h>

#include "editor/input/window.h"
#include "window/action.h"

static int window_handle_normal_input
(
	Editor* editor, 
	struct normal_key key
);

static int window_handle_mouse_input(Editor* editor, struct mouse_event mouse);

int editor_handle_window_input
(
	Editor* editor, 
	struct event event
) 
{
	int ret = 1;

	switch (event.type) {
	case EVENT_KEY:
		ret = window_handle_normal_input(editor, event.key);
		break;

	case EVENT_MOUSE:
		ret = window_handle_mouse_input(editor, event.mouse);
		break;

	default:
		break;
	}

	if (editor->has_window) {
		const u32string* line = vector_get_const(
			&editor->window.content,
			editor->window.cursor.pos.y
		);

		cursor_clamp(
			&editor->window.cursor, 
			u32string_size(line),
			editor->window.content.size);
	}

	return ret;
}

static int window_handle_normal_input
(
	Editor* editor, 
	struct normal_key key
) 
{
	uint32_t content = key.content;
	uint32_t modifiers = key.modifiers;

	if (normal_key_equal(&key, &editor->config.keybinds[ACTION_QUIT])) {
		window_close(&editor->window);
		editor->has_window = 0;
	}

	else if (content == ARROW_RIGHT && modifiers == 0) {
		// log_write(&editor->log, "before: %zu", editor->window.cursor.pos.x);

		window_move_cursor_right(&editor->window);

		// log_write(&editor->log, "after: %zu", editor->window.cursor.pos.x);
	}

	else if (content == ARROW_LEFT && modifiers == 0) {
		window_move_cursor_left(&editor->window);
	}

	else if (content == ARROW_UP && modifiers == 0) {
		window_move_cursor_up(&editor->window);
	}

	else if (content == ARROW_DOWN && modifiers == 0) {
		window_move_cursor_down(&editor->window);
	}

	else if (normal_key_equal(&key, &editor->config.keybinds[ACTION_MOVE_START_LINE])) {
		window_move_cursor_start_line(&editor->window);
	}

	else if (normal_key_equal(&key, &editor->config.keybinds[ACTION_MOVE_END_LINE])) {
		window_move_cursor_end_line(&editor->window);
	}

	else if (normal_key_equal(&key, &editor->config.keybinds[ACTION_SCROLL_RIGHT])) {
		window_scroll_right(
			&editor->window, 
			editor->config.cursor_follow_scroll
		);
	}

	else if (normal_key_equal(&key, &editor->config.keybinds[ACTION_SCROLL_LEFT])) {
		window_scroll_left(
			&editor->window, 
			editor->config.cursor_follow_scroll
		);
	}

	else if (normal_key_equal(&key, &editor->config.keybinds[ACTION_SCROLL_UP])) {
		window_scroll_up(
			&editor->window, 
			editor->config.cursor_follow_scroll
		);
	}

	else if (normal_key_equal(&key, &editor->config.keybinds[ACTION_SCROLL_DOWN])) {
		window_scroll_down(
			&editor->window, 
			editor->config.cursor_follow_scroll
		);
	}

	else if ((content == U'\n' || content == U'\r' || content == U' ' ||
			content == U'\t') && 
		modifiers == 0) 
	{
		WindowResult result = (editor->window.on_select)
			? editor->window.on_select(editor)
			: WINDOW_CLOSE;

		if (result == WINDOW_CLOSE) {
			window_close(&editor->window);
			editor->has_window = 0;
		}

		else if (result == WINDOW_QUIT_PROGRAM) {
			window_close(&editor->window);
			editor->has_window = 0;

			return 0;
		}
	}

	return 1;
}

static void window_mouse_move_cursor
(
	// Editor* editor,
	Window* w,
	struct mouse_event mouse
)
{
	if (mouse.x < 0 || mouse.y < 1) {
		return;
	}

	if (mouse.x > 0) {
		mouse.x--;
	}

	if (mouse.y > 0) {
		mouse.y--;
	}

	if ((size_t) mouse.x <= w->pos.x ||
		(size_t) mouse.y < w->pos.y ||
		(size_t) mouse.x >= w->pos.x + 1 + window_width(w))
	{
		return;
	}

	/*

	SPECIAL BEHAVIOR:


	┌──────────────────────┐
	│  ↑  click = up	   │
	├──────────────────────┤
	│                      │
	│       content        │
	│                      │
	├──────────────────────┤
	│  ↓ click = down	   │
	└──────────────────────┘

	*/


	/// ↑  click = up
	if ((size_t) mouse.y == w->pos.y) {
		size_t height = window_height(w);

		if (w->view.row_offset == 0) {
			return;
		}

		if (w->view.row_offset < height) {
			w->view.row_offset = 0;
		}

		else {
			w->view.row_offset -= height;
		}

		w->cursor.pos.x = 0;
		w->cursor.pos.y = w->view.row_offset;

		window_cursor_update(w);
		return;
	}

	/// ↓ click = down
	if ((size_t) mouse.y == w->pos.y + 1 + window_height(w)) {
		size_t lines = w->content.size;
		size_t height = window_height(w);

		if (w->view.row_offset + height >= lines) {
			return;
		}

		w->view.row_offset += height;
		w->cursor.pos.x = 0;
		w->cursor.pos.y = w->view.row_offset;

		window_cursor_update(w);
		return;
	}

	size_t lines = w->content.size;

	if (lines == 0) {
		w->cursor.pos.x = 0;
		w->cursor.pos.y = 0;

		return;
	}

	size_t text_x = mouse.x;
	size_t text_y = (size_t) mouse.y;

	size_t file_x = w->view.col_offset + text_x - (w->pos.x + 1);
	size_t file_y = w->view.row_offset + text_y - (w->pos.y + 1);

	// log_write(&editor->log, "pos: %zu, %zu", w->pos.x, w->pos.y);
	// log_write(&editor->log, "offset: %zu, %zu", w->view.col_offset, w->view.row_offset);
	// log_write(&editor->log, "text: %zu, %zu", text_y, text_y);
	// log_write(&editor->log, "file: %zu, %zu", file_x, file_y);

	if (file_y >= lines) {
		file_y = lines - 1;
	}

	w->cursor.pos.y = file_y;

	// log_write(&editor->log, "y: %zu", file_y);

	const u32string* text = vector_get_const(
		&w->content,
		file_y
	);

	size_t line_size = u32string_size(text);

	size_t screen_line_size = file_to_screen_x(
		text,
		line_size,
		w->tab_size
	);

	if (file_x > screen_line_size) {
		file_x = screen_line_size;
	}

	w->cursor.pos.x = screen_to_file_x(
		text,
		file_x,
		w->tab_size);

	// log_write(&editor->log, "x: %zu", w->cursor.pos.x);

	window_cursor_update(w);
}

static int window_handle_mouse_input(Editor* editor, struct mouse_event mouse) {
	int button = mouse.button;
	int pressed = mouse.pressed;

	if (button == MOUSE_SCROLL_UP) {
		window_scroll_up(&editor->window, 
			editor->config.cursor_follow_scroll);
	}

	else if (button == MOUSE_SCROLL_DOWN) {
		window_scroll_down(&editor->window, 
			editor->config.cursor_follow_scroll);
	}

	else if (button == MOUSE_BUTTON_LEFT && pressed) {
		size_t old_y = editor->window.cursor.pos.y;
		window_mouse_move_cursor(/*editor,*/ &editor->window, mouse);

		if (editor->window.cursor.pos.y == old_y) {
			WindowResult result = (editor->window.on_select)
				? editor->window.on_select(editor)
				: WINDOW_CLOSE;

			if (result == WINDOW_CLOSE) {
				window_close(&editor->window);
				editor->has_window = 0;
			}

			else if (result == WINDOW_QUIT_PROGRAM) {
				window_close(&editor->window);
				editor->has_window = 0;

				return 0;
			}	
		}
	}

	return 1;
}