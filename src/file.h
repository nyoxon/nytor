#ifndef FILE_H
#define FILE_H

// to read and handle from files

#include <stddef.h>
#include "vector.h"
#include "cursor.h"

typedef struct {
	char* text;
	size_t len;
	// int dirty;
} Line;

typedef struct {
	int active;
	size_t start_x;
	size_t start_y;
	size_t end_x;
	size_t end_y;
} Selection;

typedef struct {
	char* filename;
	Vector lines; // Vector of Line's
	int dirty;
} File;

File* file_open(const char* filename);
int file_save(File* file);
void file_free(File* file);
// int file_insert_line(File* file, size_t index, const char* text);
// int file_delete_line(File* file, size_t index);
int file_insert_char(File* file, Cursor* cursor, char c);
int file_delete_char(File* file, Cursor* cursor);
int file_insert_newline(File* file, Cursor* cursor);
int file_merge_lines(File* file, Cursor* cursor);
void file_copy_line(File* file, Line* clipboard, size_t y);
void file_paste_line(File* file, Line* clipboard, size_t y);
void file_copy_selection(File* file, Selection* sel);
void file_paste_selection(File* file, Selection* sel);

#endif