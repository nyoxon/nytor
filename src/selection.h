#ifndef SELECTION_H
#define SELECTION_H

#include "cursor.h"

typedef struct {
	int active;
	size_t start_x;
	size_t start_y;
	size_t end_x;
	size_t end_y;
} Selection;

void selection_normalize
(
	const Selection* sel,
	size_t* x1, size_t* y1,
	size_t* x2, size_t* y2
);

void selection_start(Selection* sel, Cursor* c);
void selection_update(Selection* sel, Cursor* c);
void selection_clear(Selection* sel);

#endif