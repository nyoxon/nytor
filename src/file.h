#ifndef FILE_H
#define FILE_H

// to read and handle from files

// File must be a logical representation of the file itself
// it contains a vector of Lines (maybe it's not too efficient)

// maybe the use of Result is useless inside these functions, but
// i don't care

#include "vector.h"
#include "cursor.h"
#include "buffer.h"
#include "selection.h"
#include "error.h"

#define TAB_SIZE 4

typedef struct {
	char* filename;
	Vector lines; // Vector of Line's
	int dirty;
} File;

int file_open
(
	File* file, 
	const char* filename, 
	int* new_file,
	Result* result
);

int file_save(File* file, Result* result);

// inserts a char in the c->x position of the (c->y + 1)º line
int file_insert_char
(
	File* file, 
	Cursor* cursor, 
	char c,
	Result* result
);

// deletes a char in the c->x position of the (c->y + 1)º line
int file_delete_char(File* file, Cursor* cursor, Result* result);

int file_insert_newline(File* file, Cursor* cursor, Result* result);
int file_move_line_down(File* file, size_t* y, Result* result);
int file_move_line_up(File* file, size_t* y, Result* result);

// used to handle the case where the user deletes a char at
// the begginning of a line
int file_merge_lines(File* file, Cursor* cursor, Result* result);

int file_indent_a_line(File* file, Cursor* cursor, Result* result);
int file_unindent_a_line(File* file, Cursor* cursor, Result* result);

int file_comment_line
(
	File* file, 
	Cursor* cursor,
	int move_cursor,
	int force_comment_or_discomment,
	Result* result
);

void file_free(File* file);

int file_delete_selection
(
	File* file, 
	Cursor* c, 
	Selection* sel, 
	Result* result
);

int file_copy_selection
(
	File* file, 
	Clipboard* cb, 
	Selection* sel,
	Result* result
);

int file_paste_clipboard
(
	File* file, 
	Clipboard* cb, 
	Cursor* c,
	Result* result
);

int file_select_line
(
	File* file, 
	size_t y, 
	Selection* sel,
	Result* result
);

int file_select_all_file
(
	File* file, 
	Selection* sel, 
	Cursor* c,
	Result* result
);


#endif