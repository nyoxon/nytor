#ifndef SELECTION_H
#define SELECTION_H

// definition of a Selection and functions to handle it

// a Selection must be a logical representation of a text selection
// inside the editor

// someone can remove a selected bunch of text or copy a bunch
// of text to a clibpoard (see clipboard.*)

#include "util/types/position.h"

typedef struct {
	Position start;
	Position end;

	int active;
	int linewise;
} Selection;

int selection_start_before_end(const Selection* sel);

void selection_normalize
(
	const Selection* sel,
	Position* a,
	Position* b
);

void selection_start(Selection* sel, Position pos);
void selection_update(Selection* sel, Position pos);
void selection_clear(Selection* sel);

#endif