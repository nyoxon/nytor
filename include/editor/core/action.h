#ifndef EDITOR_ACTION_H
#define EDITOR_ACTION_H

#include "editor/editor.h"
#include "editor/core/history.h"


typedef int (*WordFinder)(uint32_t c);

int is_alnum(uint32_t c);
int is_word_char(uint32_t c);

void editor_open_terminal(Editor* editor);

void editor_update_size(Editor* editor);
void editor_suspend(Editor* editor);

void editor_cursor_update(Editor* editor);
void editor_cursor_move(Editor* editor, Position pos);


void editor_change_actual_file(Editor* editor,size_t index);
void editor_change_next_file(Editor* editor);
void editor_change_prev_file(Editor* editor);
void editor_create_new_file(Editor* editor);

int editor_create_internal_file
(
	Editor* editor,
	const char* name,
	Clipboard* content
);

void editor_open_file
(
	Editor* editor, 
	const char* filename,
	int readonly
);

void editor_close_file(Editor* editor);

void editor_handle_deleted(Editor* editor);
void editor_handle_moved_from(Editor* editor);
void editor_handle_created(Editor* editor);
void editor_handle_modified(Editor* editor);
void editor_handle_attrib_changed(Editor* editor);


int editor_save_file(Editor* editor);
int editor_quit(Editor* editor);


void editor_start_and_create_selection(Editor* editor);
void editor_selection_clear(Editor* editor);
void editor_selection_complete(Editor* editor);
int editor_select_line(Editor* editor);
int editor_select_all_file(Editor* editor);
void editor_select_word(Editor* editor);


int editor_copy_selection(Editor* editor);
int editor_paste_clipboard(Editor* editor);


void editor_move_cursor_right(Editor* editor);
void editor_move_cursor_left(Editor* editor);
void editor_move_cursor_up(Editor* editor);
void editor_move_cursor_down(Editor* editor);
void editor_move_cursor_beginning_line(Editor* editor);
void editor_move_cursor_indent_line(Editor* editor);
void editor_move_cursor_end_line(Editor* editor);


void editor_scroll_right(Editor* editor);
void editor_scroll_left(Editor* editor);
void editor_scroll_up(Editor* editor);
void editor_scroll_down(Editor* editor);
void editor_scroll_up_terminal_size(Editor* editor);
void editor_scroll_down_terminal_size(Editor* editor);


void editor_move_between_words_right
(
	Editor* editor,
	WordFinder finder
);

void editor_move_between_words_left
(
	Editor* editor,
	WordFinder finder
);


void editor_operation_insert(Editor* editor, const Operation* op);
void editor_operation_remove(Editor* editor, const Operation* op);
void editor_operation_indent(Editor* editor, const Operation* op);
void editor_operation_unindent(Editor* editor, const Operation* op);
void editor_operation_comment(Editor* editor, const Operation* op);
void editor_operation_replace(Editor* editor, Operation* op);
void editor_operation_linemove(Editor* editor, Operation* op);


int editor_insert_char(Editor* editor, uint32_t c);
int editor_insert_tab(Editor* editor);
int editor_insert_newline(Editor* editor);


int editor_delete_char_or_selection
(
	Editor* editor,
	char prev, // used only if !editor->sel.active
	char current
);

void editor_del_from_cursor_left(Editor* editor);
void editor_del_from_cursor_right(Editor* editor);

int editor_indent_line
(
	Editor* editor,
	size_t y
);

int editor_indent_selection_or_line(Editor* editor);

int editor_unindent_line
(
	Editor* editor,
	size_t y
);

int editor_unindent_selection_or_line(Editor* editor);


int editor_comment_line_or_selection(Editor* editor);

int editor_move_line_or_selection_up(Editor* editor);
int editor_move_line_or_selection_down(Editor* editor);


// replace a pattern considering all the file
void editor_replace_pattern
(
	Editor* editor,
	const u32string* pattern,
	const u32string* text
);

// replace a pattern just considering a selection
void editor_replace_within_selection
(
	Editor* editor,
	const u32string* pattern,
	const u32string* text
);


int editor_find_next_pattern_match
(
	Editor* editor,
	Position start,
	const u32string* pattern,
	int select_if_find
);

int editor_find_next_pattern_within_selection
(
	Editor* editor,
	const u32string* pattern,
	int select_if_find
);

int editor_find_actual_block(Editor* editor);

void editor_match(Editor* editor);


void editor_undo(Editor* editor);
void editor_redo(Editor* editor);

#endif