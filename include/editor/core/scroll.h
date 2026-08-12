#ifndef SCROLL_H
#define SCROLL_H

// definition of a View and functions to handle it

// View must be a logical representation of the part of the editor
// that are indeed visible by the user

#include "util/types/position.h"

typedef struct {
	size_t row_offset;
	size_t col_offset;
} View;

typedef struct {
	size_t rows;
	size_t cols;
} TerminalSize;

void update_scroll(
	View* view, 
	const Position pos,
	size_t cursor_screen_x, 
	TerminalSize* tsize);

// the only part of the code responsible for mutating a tsize
int update_terminal_size(TerminalSize* tsize);

#endif