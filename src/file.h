#ifndef FILE_H
#define FILE_H

// to read and handle from files

#include "vector.h"
#include "cursor.h"
#include "buffer.h"
#include "selection.h"

typedef struct {
	char* filename;
	Vector lines; // Vector of Line's
	int dirty;
} File;

int file_open(File* file, const char* filename, int* new_file);

int file_insert_line(File* file, size_t index, const char* text);
// int file_delete_line(File* file, size_t index);
int file_save(File* file);
int file_insert_char(File* file, Cursor* cursor, char c);
int file_delete_char(File* file, Cursor* cursor);
int file_insert_newline(File* file, Cursor* cursor);
int file_move_line_down(File* file, size_t* y);
int file_move_line_up(File* file, size_t* y);
int file_merge_lines(File* file, Cursor* cursor);

void file_free(File* file);
void file_delete_selection(File* file, Cursor* c, Selection* sel);
void file_select_line(File* file, size_t y, Selection* sel);
void file_select_all_file(File* file, Selection* sel, Cursor* c);
void file_copy_selection(File* file, Clipboard* cb, Selection* sel);
void file_paste_clipboard(File* file, Clipboard* cb, Cursor* c);


#endif