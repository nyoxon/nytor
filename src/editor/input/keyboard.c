#include <string.h>
#include <stdlib.h>
#include <assert.h>

#include "editor/input/keyboard.h"
#include "editor/core/action.h"
#include "editor/core/cmd.h"
#include "editor/core/auto_complete.h"
#include "util/files.h"

static int editor_handle_language_input
(
	Editor* editor,
	const struct normal_key* key
);

static void editor_handle_replace_pattern
(
	Editor* editor,
	struct normal_key key
);

static void editor_handle_find_pattern
(
	Editor* editor,
	struct normal_key key
);

static void editor_handle_goto
(
	Editor* editor,
	struct normal_key key
);

int editor_open_cmd(Editor* editor, struct normal_key key);


// --- MAIN FUNCTION ---

int editor_handle_normal_input(Editor* editor, struct normal_key key) {
	uint32_t content = key.content;
	uint32_t modifiers = key.modifiers;

	if (editor->status_bar.active) {
		if (prompt_has_label(&editor->status_bar, PROMPT_FIND)) {
			editor_handle_find_pattern(editor, key);
			return 1;
		}

		if (prompt_has_label(&editor->status_bar, PROMPT_GOTO)) {
			editor_handle_goto(editor, key);
			return 1;
		}

		if (prompt_has_label(&editor->status_bar, PROMPT_CMD)) {
			return editor_open_cmd(editor, key);
		}

		if (prompt_has_label(&editor->status_bar, PROMPT_REPLACE)||
			prompt_has_label(&editor->status_bar, PROMPT_INSERT))
		{
			editor_handle_replace_pattern(editor, key);
			return 1;
		}
	}

	int ret;

	ret = editor_handle_language_input(editor, &key);

	if (ret == 1) {
		return 1;
	}

	else if (ret == EIE_FATAL_ERROR) {
		return 0;
	}


	/// --- NORMAL INPUT --- 

	if (normal_key_equal(&key, &editor->config.keybinds[ACTION_QUIT])) {
		int ret = editor_quit(editor);

		if (ret == 1) {
			return 1;
		}

		return 0;
	}

	if (normal_key_equal(&key, &editor->config.keybinds[ACTION_GOTO])) {
		prompt_init(&editor->status_bar, PROMPT_GOTO, PT_INTERACTIVE);
		return 1;		
	}

	if (normal_key_equal(&key, &editor->config.keybinds[ACTION_FIND])) {
		prompt_init(&editor->status_bar, PROMPT_FIND, PT_INTERACTIVE);
		return 1;	
	}

	if (normal_key_equal(&key, &editor->config.keybinds[ACTION_SHOW_TABS])) {
		editor->config.show_tabs = !editor->config.show_tabs;
		return 1;
	}

	if (normal_key_equal(&key, &editor->config.keybinds[ACTION_MOVE_START_LINE])) 
	{
		editor_move_cursor_beginning_line(editor);
		return 1;
	}

	if (normal_key_equal(&key, &editor->config.keybinds[ACTION_MOVE_INDENT])) {
		editor_move_cursor_indent_line(editor);
		return 1;
	}

	if (normal_key_equal(&key, &editor->config.keybinds[ACTION_MOVE_END_LINE])) {
		editor_move_cursor_end_line(editor);
		return 1;
	}

	if (normal_key_equal(&key, &editor->config.keybinds[ACTION_SELECTION])) {
		editor_start_and_create_selection(editor);
		return 1;
	}

	if (normal_key_equal(&key, &editor->config.keybinds[ACTION_SELECT_LINE])) {
		if (editor->sel.active &&
			editor->sel.start.y != editor->sel.end.y)
		{
			editor_selection_complete(editor);
			return 1;
		}

		ret = editor_select_line(editor);

		if (ret == EIE_FATAL_ERROR) {
			return 0;
		}

		return 1;
	}

	if (normal_key_equal(&key, &editor->config.keybinds[ACTION_SELECT_ALL])) {
		ret = editor_select_all_file(editor);

		if (ret == EIE_FATAL_ERROR) {
			return 0;
		}

		return 1;
	}

	if (normal_key_equal(&key, &editor->config.keybinds[ACTION_COPY])) {
		ret = editor_copy_selection(editor);

		if (ret == EIE_FATAL_ERROR) {
			return 0;
		}

		return 1;
	}

	if (normal_key_equal(&key, &editor->config.keybinds[ACTION_PASTE])) {
		ret = editor_paste_clipboard(editor);

		if (ret == EIE_FATAL_ERROR) {
			return 0;
		}

		return 1;
	}

	if (normal_key_equal(&key, &editor->config.keybinds[ACTION_DEL_FROM_CURSOR_LEFT])) {
		editor_del_from_cursor_left(editor);

		return 1;
	}

	if (normal_key_equal(&key, &editor->config.keybinds[ACTION_DEL_FROM_CURSOR_RIGHT])) {
		editor_del_from_cursor_right(editor);

		return 1;
	}

	if (content == U'\t')
	{
		if (editor->sel.active) {
			ret = editor_indent_selection_or_line(editor);

			if (ret == EIE_FATAL_ERROR) {
				return 0;
			}

			return 1;
		} 

		else {
			ret = editor_insert_tab(editor);
			
			if (ret == EIE_FATAL_ERROR) {
				return 0;
			}

			return 1;
		}
	}

	if (normal_key_equal(&key, &editor->config.keybinds[ACTION_INDENT])) {
		ret = editor_indent_selection_or_line(editor);

		if (ret == EIE_FATAL_ERROR) {
			return 0;
		}

		return 1;
	}

	if (normal_key_equal(&key, &editor->config.keybinds[ACTION_UNINDENT])) {
		ret = editor_unindent_selection_or_line(editor);

		if (ret == EIE_FATAL_ERROR) {
			return 0;
		}

		return 1;
	}

	if (normal_key_equal(&key, &editor->config.keybinds[ACTION_COMMENT])) {
		ret = editor_comment_line_or_selection(editor);

		if (ret == EIE_FATAL_ERROR) {
			return 0;
		}

		return 1;
	}

	if (content == U'\n' || content == U'\r') {
		ret = editor_insert_newline(editor);

		if (ret == EIE_FATAL_ERROR) {
			return 0;
		}

		return 1;
	}

	if (normal_key_equal(&key, &editor->config.keybinds[ACTION_MATCH])) {
		editor_match(editor);
		return 1;
	}

	if (normal_key_equal(&key, &editor->config.keybinds[ACTION_SAVE])) {
		ret = editor_save_file(editor);

		if (ret == EIE_FATAL_ERROR) {
			return 0;
		}

		return 1;
	}

	if (content == 127) {
		if (!editor->sel.active &&
			editor->cursor.pos.x > 0 && 
			editor->cursor.pos.x < file_size_line(
									&editor->actual_file->file, 
									editor->cursor.pos.y)) 
		{
			const u32string* line = file_get_line_text(
				&editor->actual_file->file,
				editor->cursor.pos.y);

			ret = editor_delete_char_or_selection(editor,
				u32string_char(line, editor->cursor.pos.x - 1),
				u32string_char(line, editor->cursor.pos.x));
		} 

		else {
			ret = editor_delete_char_or_selection(editor,
				U' ',
				U' ');
		}

		if (ret == EIE_FATAL_ERROR) {
			return 0;
		}

		return 1;
	}

	if (content == ARROW_UP && modifiers == 0) {
		editor_move_cursor_up(editor);
		return 1;
	}

	if (normal_key_equal(&key, &editor->config.keybinds[ACTION_SCROLL_UP])) {
		editor_scroll_up(editor);
		return 1;
	}

	if (normal_key_equal(&key, &editor->config.keybinds[ACTION_SCROLL_TERMINAL_UP])) {
		editor_scroll_up_terminal_size(editor);
		return 1;
	}

	if (normal_key_equal(&key, &editor->config.keybinds[ACTION_SELECTION_MOVE_UP])) {
		ret = editor_move_line_or_selection_up(editor);

		if (ret == EIE_FATAL_ERROR) {
			return 0;
		}

		return 1;
	}

	if (content == ARROW_DOWN && modifiers == 0) {
		editor_move_cursor_down(editor);
		return 1;
	}

	if (normal_key_equal(&key, &editor->config.keybinds[ACTION_SCROLL_DOWN])) {
		editor_scroll_down(editor);
		return 1;
	}

	if (normal_key_equal(&key, &editor->config.keybinds[ACTION_SCROLL_TERMINAL_DOWN])) 
	{
		editor_scroll_down_terminal_size(editor);
		return 1;
	}

	if (normal_key_equal(&key, &editor->config.keybinds[ACTION_SELECTION_MOVE_DOWN])) 
	{
		ret = editor_move_line_or_selection_down(editor);

		if (ret == EIE_FATAL_ERROR) {
			return 0;
		}

		return 1;
	}

	if (content == ARROW_RIGHT && modifiers == 0) {
		editor_move_cursor_right(editor);
		return 1;
	}

	if (normal_key_equal(&key, &editor->config.keybinds[ACTION_MOVE_WORD_RIGHT])) {
		editor_move_between_words_right(editor, is_alnum);
		return 1;
	}

	if (normal_key_equal(&key, &editor->config.keybinds[ACTION_MOVE_FULLWORD_RIGHT])) 
	{
		editor_move_between_words_right(editor, is_word_char);
		return 1;
	}

	if (content == ARROW_LEFT && modifiers == 0) {
		editor_move_cursor_left(editor);
		return 1;
	}

	if (normal_key_equal(&key, &editor->config.keybinds[ACTION_MOVE_WORD_LEFT])) {
		editor_move_between_words_left(editor, is_alnum);
		return 1;
	}

	if (normal_key_equal(&key, &editor->config.keybinds[ACTION_MOVE_FULLWORD_LEFT])) 
	{
		editor_move_between_words_left(editor, is_word_char);
		return 1;
	}

	if (normal_key_equal(&key, &editor->config.keybinds[ACTION_SUSPEND])) {
		editor->suspend = 1;
		return 1;
	}

	if (normal_key_equal(&key, &editor->config.keybinds[ACTION_SCROLL_RIGHT])) {
		editor_scroll_right(editor);
		return 1;
	}

	if (normal_key_equal(&key, &editor->config.keybinds[ACTION_SCROLL_LEFT])) {
		editor_scroll_left(editor);

		return 1;
	}

	if (normal_key_equal(&key, &editor->config.keybinds[ACTION_REPLACE])) {
		prompt_init(&editor->status_bar, PROMPT_REPLACE, PT_INTERACTIVE);
		return 1;
	}

	if (normal_key_equal(&key, &editor->config.keybinds[ACTION_UNDO])) {
		editor_undo(editor);
		return 1;
	}

	if (normal_key_equal(&key, &editor->config.keybinds[ACTION_REDO])) {
		editor_redo(editor);
		return 1;
	}

	if (normal_key_equal(&key, &editor->config.keybinds[ACTION_NEXT_FILE])) {
		editor_change_next_file(editor);
		return 1;
	}

	if (normal_key_equal(&key, &editor->config.keybinds[ACTION_PREV_FILE])) {
		editor_change_prev_file(editor);
		return 1;
	}

	if (normal_key_equal(&key, &editor->config.keybinds[ACTION_NEW_FILE])) {
		editor_create_new_file(editor);
		return 1;
	}

	if (normal_key_equal(&key, &editor->config.keybinds[ACTION_CLOSE_FILE])) {
		editor_close_file(editor);
		return 1;
	}

	if (normal_key_equal(&key, &editor->config.keybinds[ACTION_OPEN_CMD])) {
		prompt_init(&editor->status_bar, PROMPT_CMD, PT_INTERACTIVE);
		return 1;
	}

	if (normal_key_equal(&key, &editor->config.keybinds[ACTION_TERMINAL])) {
		editor_open_terminal(editor);
		return 1;
	}

	if (u32_is_printable(content) && modifiers == 0) {
		if (editor->sel.active) {
			editor_selection_clear(editor);
		}

		ret = editor_insert_char(editor, content);

		if (ret == EIE_FATAL_ERROR) {
			return 0;
		}

		if (editor->debug_mode) {
			log_write(&editor->log, 
			"editor_handle_input: a char has been written (%c)", 
				content);
		}

		return 1;
	}

	return 1;	
}


/// --- FUNCTIONS --- 




/// --- LANGUAGE ---

static int editor_handle_open_delimiters(Editor* editor, uint32_t key) {
	uint32_t open = key;
	uint32_t close;

	const struct language_rules* rules = (editor->actual_file->language) ?
		editor->actual_file->language->rules : NULL;

	if (!rules) {
		int ret = editor_insert_char(editor, key);

		if (ret < 0) {
			return ret;
		}

		if (editor->debug_mode) {
			log_write(&editor->log, 
				"editor_handle_open_delimiters: SUCCESS");
		}

		return 0;
	}

	for (size_t i = 0; i < rules->pair_count; i++) {
		if (open == (unsigned char) rules->pairs[i].open) {
			u32_decode(&rules->pairs[i].close, 1, &close);

			break;
		}
	}

	Position cursor_remove = editor->cursor.pos;

	Line* line = vector_get(
		&editor->actual_file->file.lines, 
		editor->cursor.pos.y);

	u32string* line_text = &line->text;

	u32string_insert(line_text, open, editor->cursor.pos.x);
	u32string_insert(line_text, close, editor->cursor.pos.x + 1);

	file_set_line_dirty(&editor->actual_file->file, editor->cursor.pos.y);

	editor_cursor_move(
		editor,
		(Position) { editor->cursor.pos.x + 1, editor->cursor.pos.y } );

	Position cursor_insert = editor->cursor.pos;
	Position start = cursor_remove;
	Position end = (Position) { 
		editor->cursor.pos.x + 1, editor->cursor.pos.y };

	u32string text = u32string_from_raw_copy(
		(uint32_t[]) { open, close }, 2 );

	Operation op = operation_create_insert(
		start, end,
		cursor_remove, cursor_insert,
		text
	);

	stack_clear(&editor->actual_file->redo);
	stack_push(&editor->actual_file->undo, &op);

	editor->actual_file->file.dirty = 1;

	if (editor->debug_mode) {
		log_write(&editor->log, 
			"editor_handle_open_delimiters: SUCCESS");
	}

	return 0;
}

static int editor_handle_close_delimiters
(
	Editor* editor, 
	uint32_t key
) 
{	
	const u32string* line = file_get_line_text(
		&editor->actual_file->file, 
		editor->cursor.pos.y);

	size_t size = u32string_size(line);

	if (editor->cursor.pos.x < size &&
		u32string_char(line, editor->cursor.pos.x) == key) 
	{
		editor_move_cursor_right(editor);
	} else {
		int ret = editor_insert_char(editor, key);

		if (ret < 0) {
			return ret;
		}
	}

	if (editor->debug_mode) {
		log_write(&editor->log, 
			"editor_handle_close_delimiters: SUCCESS");
	}

	return 0;
}

static int editor_handle_language_input
(
	Editor* editor,
	const struct normal_key* key
)
{
	uint32_t content = key->content;
	uint32_t modifiers = key->modifiers;

	const struct language_rules* rules = (editor->actual_file->language) ?
		(editor->actual_file->language->rules) : NULL;

	if (!rules) {
		return 0;;
	}

	for (size_t i = 0; i < rules->pair_count; i++) {
		if (content == (uint32_t) rules->pairs[i].open &&
			modifiers == 0) 
		{
			if (rules->pairs[i].auto_complete) {
				int ret = editor_handle_open_delimiters(editor, content);

				if (ret == EIE_FATAL_ERROR) {
					return ret;
				}

				return 1;
			} 

			else {
				return 0;;
			}
		}

		else if (content == (uint32_t) rules->pairs[i].close &&
				 modifiers == 0) 
		{
			if (rules->pairs[i].auto_complete) {
				int ret = editor_handle_close_delimiters(editor, content);

				if (ret == EIE_FATAL_ERROR) {
					return ret;
				}

				return 1;
			} 

			else {
				return 0;;
			}
		}
	}

	return 0;
}



/// --- GOTO ---

static void editor_handle_goto(Editor* editor, struct normal_key key) {
	if ((key.content == U'\n' || key.content == U'\r')) {
		long y;

		if (u32string_is_empty(&editor->status_bar.buf)) {
			editor->cursor.pos.y = 0;
			editor->cursor.pos.x = 0;

			editor_cursor_update(editor);
			selection_update(&editor->sel, editor->cursor.pos);
		}

		else if ((y = u32stol(&editor->status_bar.buf)) >= 0) 
		{
			if (y == 0) {
				editor->cursor.pos.y = file_num_lines(
					&editor->actual_file->file) - 1;
			} 

			else {
				editor->cursor.pos.y = ((size_t) y >= file_num_lines(
												&editor->actual_file->file))
					? file_num_lines(&editor->actual_file->file) - 1
					: (size_t) y - 1;	
			}

			editor->cursor.pos.x = 0;
			
			editor_cursor_update(editor);
			selection_update(&editor->sel, editor->cursor.pos);
		}

		else {
			char* filename;

			int ret = prompt_buf_to_u8string(
				&editor->status_bar, 
				&filename);

			if (ret < 0) {
				prompt_init(
					&editor->status_bar,
					PROMPT_INVALID_ARG,
					PT_INFO
				);

				return;
			}

			ssize_t index = editor_has_filename(editor, filename);

			free(filename);

			if (index < 0) {
				prompt_init(
					&editor->status_bar,
					PROMPT_INVALID_FILENAME,
					PT_INFO);
			}

			else {
				editor_change_actual_file(editor, index);
			}
		}

		if (editor->status_bar.type == PT_INTERACTIVE) {
			editor->status_bar.active = 0;
		}
	}

	else if (normal_key_equal(&key, &editor->config.keybinds[ACTION_QUIT]) ||
			 normal_key_equal(&key, &editor->config.keybinds[ACTION_GOTO])) 
	{
		editor->status_bar.active = 0;
	} 

	else {
		prompt_handle_normal_input(
			&editor->status_bar, 
			key, 
			editor->config.keybinds);
	}
}



// -- FIND --

static void editor_handle_find_pattern
(
	Editor* editor, 
	struct normal_key key
) 
{
	if (key.content == U'\n' || key.content == U'\r') {
		if (u32string_is_empty(&editor->status_bar.buf)) {
			editor->status_bar.active = 0;

			prompt_init(
				&editor->status_bar, 
				PROMPT_EMPTY_STRING, 
				PT_INFO);

			return;
		}

		u32string pattern;
		int ret = prompt_buf_to_u32string(
			&editor->status_bar,
			&pattern
		);

		if (ret == -1) {
			prompt_init(
				&editor->status_bar, 
				PROMPT_EMPTY_STRING, 
				PT_INFO);

			return;			
		}

		else if (ret == -2) {
			prompt_init(
				&editor->status_bar, 
				PROMPT_EMPTY_STRING, 
				PT_INFO);

			return;			
		}

		int found;

		if (editor->sel.active) {
			found = editor_find_next_pattern_within_selection(
				editor,
				&pattern,
				1
			);
		}

		else {
			found = editor_find_next_pattern_match(
				editor, 
				POS_ZERO,
				&pattern,
				1);
		}


		if (!found) {
			prompt_init(&editor->status_bar, "not found", PT_INFO);
		}

		else {
			editor->status_bar.active = 0;
		}

		u32string_free(&pattern);
	}

	else if (normal_key_equal(&key, &editor->config.keybinds[ACTION_QUIT]) ||
			 normal_key_equal(&key, &editor->config.keybinds[ACTION_FIND]))
	{
		editor->status_bar.active = 0;
	}

	else {
		prompt_handle_normal_input(
			&editor->status_bar, 
			key, 
			editor->config.keybinds);
	}
}




/// --- REPLACE ---

static void editor_handle_replace_pattern
(
	Editor* editor, 
	struct normal_key key
) 
{
	static int selected_before = 0;
	static int already_checked = 0;

	if (key.content == U'\n' || key.content == U'\r') {
		if (editor->actual_file->replace) {
			size_t selected = selected_before;

			selected_before = 0;
			already_checked = 0;

			if (u32string_is_empty(&editor->status_bar.buf)) {
				editor->actual_file->replace_text = u32string_new();
			}

			else {
				int ret = prompt_buf_to_u32string(
					&editor->status_bar,
					&editor->actual_file->replace_text);

				if (ret == -1) {
					prompt_init(
						&editor->status_bar,
						PROMPT_INVALID_ARG,
						PT_INFO
					);

					editor->actual_file->replace = 0;
					u32string_free(&editor->actual_file->replace_pattern);
					return;
				}
			}

			// // debug
			// char* a = u32string_into_u8(&editor->actual_file->replace_pattern);
			// char* b = u32string_into_u8(&editor->actual_file->replace_text);

			// log_write(&editor->log, "pat: %s, size: %zu",
			// 	a, strlen(a));
			// log_write(&editor->log, "text: %s, size: %zu",
			// 	b, strlen(b));

			// u32string_free(&editor->actual_file->replace_pattern);
			// u32string_free(&editor->actual_file->replace_text);

			// free(a); free(b);

			// editor->status_bar.active = 0;
			// editor->actual_file->replace = 0;

			// return;

			if (selected) {
				editor_replace_within_selection(
					editor, 
					&editor->actual_file->replace_pattern,
					&editor->actual_file->replace_text);
			}

			else {
				editor_replace_pattern(
					editor, 
					&editor->actual_file->replace_pattern,
					&editor->actual_file->replace_text);
			}

			u32string_free(&editor->actual_file->replace_pattern);
			u32string_free(&editor->actual_file->replace_text);

			editor->status_bar.active = 0;
			editor->actual_file->replace = 0;
		}

		else {
			if (u32string_is_empty(&editor->status_bar.buf)) {
				prompt_init(
					&editor->status_bar, 
					PROMPT_EMPTY_STRING,
					PT_INFO);

				already_checked = 0;
				selected_before = 0;
				return;		
			}

			int ret = prompt_buf_to_u32string(
				&editor->status_bar, 
				&editor->actual_file->replace_pattern);

			if (ret == -1) {
				prompt_init(
					&editor->status_bar,
					PROMPT_INVALID_ARG,
					PT_INFO
				);

				already_checked = 0;
				selected_before = 0;
				return;
			}

			else if (ret == -2) {
				prompt_init(
					&editor->status_bar,
					PROMPT_EMPTY_STRING,
					PT_INFO
				);

				already_checked = 0;
				selected_before = 0;
				return;				
			}

			int found;

			if (editor->sel.active) {

				found = editor_find_next_pattern_within_selection(
					editor,
					&editor->actual_file->replace_pattern,
					0
				);
			}

			else {
				found = editor_find_next_pattern_match(
					editor,
					POS_ZERO,
					&editor->actual_file->replace_pattern,
					1);
			}


			if (found) {
				prompt_init(
					&editor->status_bar, 
					PROMPT_INSERT, 
					PT_INTERACTIVE);
				
				editor->actual_file->replace = 1;
			}

			else {
				u32string_free(&editor->actual_file->replace_pattern);
				prompt_init(
					&editor->status_bar, 
					PROMPT_NOT_FOUND,
					PT_INFO);

				already_checked = 0;
				selected_before = 0;
			}
		}
	}

	else if (normal_key_equal(&key, &editor->config.keybinds[ACTION_QUIT]) ||
			 normal_key_equal(&key, &editor->config.keybinds[ACTION_REPLACE]))
	{
		editor->status_bar.active = 0;

		already_checked = 0;
		selected_before = 0;

		if (editor->actual_file->replace) {
			u32string_free(&editor->actual_file->replace_pattern);
			editor->actual_file->replace = 0;
		}
	} 

	else {
		if (!already_checked) {
			selected_before = editor->sel.active;
			already_checked = 1;
		}

		prompt_handle_normal_input(
			&editor->status_bar, 
			key, 
			editor->config.keybinds);
	}
}

static size_t get_start_of_last_space
(
	const u32string* buf,
	uint32_t* last_space,
	u32string* last_segment // out
) 
{
	size_t buf_size = u32string_size(buf);

	size_t size = buf_size - (
		last_space - u32string_into_ptr_const(buf));

	if (last_segment) {
		*last_segment = u32string_from_raw_copy(
			last_space,
			size
		);
	}

	return last_space - u32string_into_ptr_const(buf);
}

static WindowResult on_select_fill_prompt(void* userdata) {
	assert(userdata != NULL);

	Editor* editor = userdata;

	u32string* buf = &editor->status_bar.buf;
	uint32_t* space = u32string_chr(buf, U' ');
	const Vector* candidates = &editor->window.content;

	size_t start;
	AutoCompType type;

	if (!space) {
		start = 0;
		type = AUTOCOMP_CMD;
	}

	else {
		uint32_t* last_space = u32string_rchr(
			&editor->status_bar.buf, 
			U' '
		);

		last_space++;

		start = get_start_of_last_space(buf, last_space, NULL);
		type = AUTOCOMP_FILE;
	}

	size_t buf_size = u32string_size(buf);

	const u32string* selected = vector_get_const(
		candidates,
		editor->window.cursor.pos.y
	);

	u32string_remove_range(
		buf,
		start,
		buf_size
	);

	u32string_insert_range_raw(
		buf,
		start,
		u32string_into_ptr_const(selected),
		u32string_size(selected)
	);

	/// FIXME (temporary assert)
	assert(u32string_size(selected) >= (buf_size - start));

	editor->status_bar.cursor.pos.x += u32string_size(selected) -
		(buf_size - start);

	uint32_t additional_char;

	if (type == AUTOCOMP_FILE) {
		char* name = u32string_into_u8(selected);

		if (isdir(name)) {
			additional_char = U'/';
		}

		else {
			additional_char = U' ';
		}

		free(name);
	}

	else {
		additional_char = U' ';
	}

	u32string_push(buf, additional_char);
	editor->status_bar.cursor.pos.x += 1;

	return WINDOW_CLOSE;
}

static void editor_create_window_candidates
(
	Editor* editor,
	Vector candidates
)
{
	WindowOptions options = {
		.pos_type = WINDOWPOS_CUSTOM,
		.sw = 0.2,
		.sh = 0.5,
		.tab_size = editor->config.tab_size,
		.tsize = editor->tsize,
		.on_select = on_select_fill_prompt
	};

	Position pos = (Position) {
		u32string_size(&editor->status_bar.label) + 
			u32string_size(&editor->status_bar.buf),

		options.tsize.rows - (options.tsize.rows * options.sh) - 2
	};

	options.pos = pos;

	editor->window = window_new(&options);
	editor->window.content = candidates;
	editor->has_window = 1;
}

int editor_open_cmd(Editor* editor, struct normal_key key) {
	if ((key.content == U'\n' || key.content == U'\r')) {
		if (u32string_is_empty(&editor->status_bar.buf)) {
			prompt_init(
				&editor->status_bar,
				PROMPT_INVALID_ARG,
				PT_INFO
			);

			return 1;
		}

		int ret = editor_handle_cmd(
			editor,
			&editor->status_bar.buf);

		if (editor->status_bar.type == PT_INTERACTIVE) {
			editor->status_bar.active = 0;
		}

		if (ret < 0) {
			return 0;
		}

		return 1;
	}

	else if (normal_key_equal(&key, &editor->config.keybinds[ACTION_QUIT]) ||
			 normal_key_equal(&key, &editor->config.keybinds[ACTION_OPEN_CMD])) 
	{
		editor->status_bar.active = 0;
	}

	else if (key.content == U'\t') {
		const u32string* buf = &editor->status_bar.buf;

		uint32_t* space = u32string_chr(buf, U' ');

		AutoCompResult result;

		if (!space) {
			u32string buf_cloned = u32string_clone(buf);

			result = autocomp_cmd(&buf_cloned, &editor->status_bar);
			u32string_free(&buf_cloned);
		}

		else {
			uint32_t* last_space =
				u32string_rchr(buf, U' ');

			if (last_space) {
				last_space++;

				u32string last_segment;

				size_t start = get_start_of_last_space(
					buf,
					last_space,
					&last_segment
				);

				result = autocomp_file(
					&last_segment, 
					&editor->status_bar,
					start);

				u32string_free(&last_segment);
			}
		}

		if (result.type == AUTOCOMP_RESULT_SHOW_CANDIDATES) {
			editor_create_window_candidates(editor, result.candidates);
		}
	} 

	else {
		prompt_handle_normal_input(
			&editor->status_bar, 
			key, 
			editor->config.keybinds);
	}

	return 1;	
}