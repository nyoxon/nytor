#ifndef FILE_H
#define FILE_H

// to read and handle from files

#include "vector.h"
#include "cursor.h"
#include "buffer.h"
#include "selection.h"
#include "error.h"

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

int file_insert_char
(
	File* file, 
	Cursor* cursor, 
	char c,
	Result* result
);

int file_delete_char(File* file, Cursor* cursor, Result* result);
int file_insert_newline(File* file, Cursor* cursor, Result* result);
int file_move_line_down(File* file, size_t* y, Result* result);
int file_move_line_up(File* file, size_t* y, Result* result);
int file_merge_lines(File* file, Cursor* cursor, Result* result);

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