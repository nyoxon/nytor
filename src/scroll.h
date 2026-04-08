#ifndef SCROLL_H
#define SCROLL_H

#include <stddef.h>
#include "cursor.h"

typedef struct {
	size_t row_offset;
	size_t col_offset;
} View;

void update_scroll(View* view, Cursor* c, size_t screen_rows);

#endif