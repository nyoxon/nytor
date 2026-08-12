#include <assert.h>

#include "editor/input/window.h"
#include "window/action.h"

static void window_handle_normal_input(Editor* editor, struct normal_key key);
static void window_handle_mouse_input(Editor* editor, struct mouse_event mouse);

void editor_handle_window_input(Editor* editor, struct event event) {
	switch (event.type) {
	case EVENT_KEY:
		window_handle_normal_input(editor, event.key);
		break;

	case EVENT_MOUSE:
		window_handle_mouse_input(editor, event.mouse);
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
}

static void window_handle_normal_input(Editor* editor, struct normal_key key) {
	uint32_t content = key.content;
	uint32_t modifiers = key.modifiers;

	if (normal_key_equal(&key, &editor->config.keybinds[ACTION_QUIT])) {
		window_close(&editor->window);
		editor->has_window = 0;
	}

	else if (content == ARROW_RIGHT && modifiers == 0) {
		log_write(&editor->log, "before: %zu", editor->window.cursor.pos.x);

		window_move_cursor_right(&editor->window);

		log_write(&editor->log, "after: %zu", editor->window.cursor.pos.x);
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

	else if ((content == U'\n' || content == U'\r' || content == U' ' ||
			content == U'\t') && 
		modifiers == 0) 
	{
		WindowResult result = 
			editor->window.on_select(editor);

		if (result == WINDOW_CLOSE) {
			window_close(&editor->window);
			editor->has_window = 0;
		}
	}
}

static void window_handle_mouse_input(Editor* editor, struct mouse_event mouse) {
	(void) editor;
	(void) mouse;
}