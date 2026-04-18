#ifndef SCROLL_H
#define SCROLL_H

// definition of a View and functions to handle it

// View must be a logical representation of the part of the editor
// that are indeed visible by the user

#include "cursor.h"

typedef struct {
	size_t row_offset;
	size_t col_offset;
} View;

typedef struct {
	size_t rows;
	size_t cols;
} TerminalSize;

// the only part of the code responsible for mutating a view
void update_scroll(View* view, Cursor* c, TerminalSize* tsize);

// the only part of the code responsible for mutating a tsize
int update_terminal_size(TerminalSize* tsize);

#endif