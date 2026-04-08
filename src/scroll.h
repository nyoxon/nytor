#ifndef SCROLL_H
#define SCROLL_H

#include <stddef.h>
#include "cursor.h"

typedef struct {
	size_t row_offset;
	size_t col_offset;
} View;

typedef struct {
	size_t rows;
	size_t cols;
} TerminalSize;

void update_scroll(View* view, Cursor* c, TerminalSize* tsize);
int update_terminal_size(TerminalSize* tsize);

#endif