#include <assert.h>

#include "editor/input/input.h"
#include "editor/core/action.h"

int editor_handle_input(Editor* editor, struct event event) {
	int ret = 1;

	if (editor->has_window) {
		return editor_handle_window_input(
			editor, 
			event
		);
	}

	if (editor->status_bar.active && editor->status_bar.type == PT_INFO) {
		if (event.type == EVENT_KEY)
		{
			editor->status_bar.active = 0;
		}

		if (event.type == EVENT_MOUSE && event.mouse.pressed)
		{
			editor->status_bar.active = 0;
		}
	}

	switch(event.type) {
	case EVENT_KEY:
		ret = editor_handle_normal_input(editor, event.key);
		break;

	case EVENT_MOUSE:
		ret = editor_handle_mouse_input(editor, event.mouse);
		break;

	default:
		ret =  0;
		break;
	}

	cursor_clamp(&editor->cursor, 
		file_size_line(&editor->actual_file->file, editor->cursor.pos.y), 
		file_num_lines(&editor->actual_file->file));

	if (editor->config.cursor_follow_scroll) {
		update_scroll(
			&editor->view, 
			editor->cursor.pos,
			editor_cursor_screen_x(editor),
			&editor->tsize);
	}

	return ret;
}

void editor_handle_file_change(Editor* editor) {
	if (editor->actual_file->externally_deleted) {
		editor_handle_deleted(editor);

		return;
	}

	if (editor->actual_file->externally_moved_from) {
		editor_handle_moved_from(editor);

		return;
	}

	if (editor->actual_file->externally_modified) {
		editor_handle_modified(editor);

		return;
	}

	if (editor->actual_file->externally_attrib_changed) {
		editor_handle_attrib_changed(editor);

		return;
	}
}