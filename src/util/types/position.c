#include "util/types/position.h"

const Position POS_ZERO = (Position) {0, 0};

int position_equal(const Position* a, const Position* b) {
	return a->x == b->x && a->y == b->y;
}