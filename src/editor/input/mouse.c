#include "editor/input/mouse.h"
#include "editor/core/action.h"

static void editor_mouse_move_cursor
(
	Editor* editor, 
	struct mouse_event mouse,
	size_t gutter
);



/// --- MAIN FUNCTION ---

int editor_handle_mouse_input
(
	Editor* editor,
	struct mouse_event mouse
)
{
	int button = mouse.button;
	int pressed = mouse.pressed;


	int ret;

	if (button == MOUSE_SCROLL_UP) {
		editor_scroll_up(editor);

		return 1;
	}

	if (button == MOUSE_SCROLL_DOWN) {
		editor_scroll_down(editor);

		return 1;
	}

	if (button == MOUSE_BUTTON_LEFT && pressed) {
		size_t line_count = file_num_lines(&editor->actual_file->file);
		size_t gutter = (editor->config.line_numbers)
			? get_gutter_width(line_count)
			: 0;

		editor_mouse_move_cursor(editor, mouse, gutter);

		if (editor->sel.active &&
			editor->config.left_click_end_selection)
		{
			editor_selection_clear(editor);
		}

		if (editor->selecting) {
			selection_update(&editor->sel, editor->cursor.pos);
		}

		if ((size_t) mouse.x <= gutter &&
			(size_t) mouse.y <= editor->tsize.rows) 
		{
			if (!editor->selecting) {
				ret = editor_select_line(editor);

				if (ret == EIE_FATAL_ERROR) {
					return 0;
				}
			}

			else {
				editor->cursor.pos.x = file_size_line(
					&editor->actual_file->file,
					editor->cursor.pos.y);

				selection_update(&editor->sel, editor->cursor.pos);
			}
		}

		return 1;
	}

	if (button == MOUSE_BUTTON_RIGHT && pressed) {
		if (editor->sel.active) {
			editor_selection_clear(editor);
		}

		else {
			size_t line_count = file_num_lines(&editor->actual_file->file);
			size_t gutter = (editor->config.line_numbers)
				? get_gutter_width(line_count)
				: 0;

			editor_mouse_move_cursor(editor, mouse, gutter);
			editor_start_and_create_selection(editor);
		}
	}

	return 1;
}



/// --- MOVE CURSOR ---


static void editor_mouse_move_cursor
(
	Editor* editor, 
	struct mouse_event mouse,
	size_t gutter
) 
{
	if (mouse.x < 0 || mouse.y < 1) {
		return;
	}

	if ((size_t) mouse.x >= editor->tsize.cols ||
		(size_t) mouse.y > editor->tsize.rows)
	{
		return;
	}

	size_t lines = file_num_lines(&editor->actual_file->file);

	if (lines == 0) {
		editor->cursor.pos.x = 0;
		editor->cursor.pos.y = 0;
		return;
	}

	size_t text_x = (mouse.x > (int) gutter) 
		? (size_t) (mouse.x - gutter)
		: 0;

	size_t text_y = (size_t) (mouse.y - 1);

	size_t file_x = editor->view.col_offset + text_x;
	size_t file_y = editor->view.row_offset + text_y;

	if (file_y >= lines) {
		file_y = lines - 1;
	}

	editor->cursor.pos.y = file_y;


	if (file_x > 0) {
		file_x--;
	}

	size_t line_size = file_size_line(&editor->actual_file->file, file_y);

	const u32string* text = file_get_line_text(
		&editor->actual_file->file,
		file_y
	);

	size_t screen_line_size = file_to_screen_x(
		text,
		line_size,
		editor->actual_file->file.tab_size
	);

	if (file_x > screen_line_size) {
		file_x = screen_line_size;
	}


	editor->cursor.pos.x = screen_to_file_x(
		text,
		file_x,
		editor->actual_file->file.tab_size);

	editor_cursor_update(editor);
}