#ifndef POSITION_H
#define POSITION_H

#include <stddef.h>

typedef struct {
	size_t x;
	size_t y;
} Position;

int position_equal(const Position* a, const Position* b);

extern const Position POS_ZERO;

#endif