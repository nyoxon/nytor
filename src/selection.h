#ifndef SELECTION_H
#define SELECTION_H

#include "cursor.h"

// definition of a Selection and functions to handle it

// a Selection must be a logical representation of a text selection
// inside the editor

// someone can remove a selected bunch of text or copy a bunch
// of text to a clibpoard (see clipboard.*)

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