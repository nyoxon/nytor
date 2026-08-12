#define _GNU_SOURCE
#include <wchar.h>

#include "util/types/cursor.h"

// certains conditions must be satisfied for the program to work
// specifically, c != NULL.

// PRE: c != NULL for all the functions

void cursor_move_to(Cursor*c, size_t cx, size_t cy) {
	c->pos.x = cx;
	c->pos.y = cy;
}

void cursor_move_left(Cursor* c) {
	if (c->pos.x > 0) c->pos.x--;
}

void cursor_move_right(Cursor* c, size_t line_size) {
	if (c->pos.x < line_size) c->pos.x++;
}

void cursor_move_up(Cursor* c) {
	if (c->pos.y > 0) c->pos.y--;
}

void cursor_move_down(Cursor* c, size_t line_count) {
	if (c->pos.y < line_count - 1) c->pos.y++;
}

void cursor_clamp(Cursor* c, size_t line_size, size_t line_count) {
	if (line_count == 0) {
		c->pos.x = 0;
		c->pos.y = 0;
		return;
	}

	if (c->pos.y >= line_count) {
		c->pos.y = line_count - 1;
	}

	if (c->pos.x > line_size) {
		c->pos.x = line_size;
	}
}

// perhaps a more modern implementation in the future, in
// addition to considering support for grapheme clusters
static size_t unicode_width(uint32_t c) {
	int w = wcwidth((wchar_t) c);

	if (w < 0) {
		return 1;
	}

	return (size_t) w;
}

size_t char_screen_width
(
	uint32_t c,
	size_t screen_x,
	size_t tab_size
)
{
	if (c == U'\t' && tab_size > 0) {
		return tab_size - (screen_x % tab_size);
	}

	// First, the value will be converted to uint8_t, and
	// then the byte will be printed in hexadecimal format.
	// width = 2 because an unsigned byte has a maximum value
	// of 255 (or BF em hex)
	if (!u32_is_printable(c)) {
		return 2;
	}

	return unicode_width(c);
}

size_t file_to_screen_x
(
	const u32string* line,
	size_t file_x,
	size_t tab_size
)
{
	size_t screen_x = 0;
	size_t end = file_x;

	if (end > u32string_size(line)) {
		end = u32string_size(line);
	}

	for (size_t i = 0; i < end; i++) {
		uint32_t c = u32string_char(line, i);
		screen_x += char_screen_width(c, screen_x, tab_size);
	}

	return screen_x;
}

size_t screen_to_file_x
(
	const u32string* line,
	size_t desired_screen_x,
	size_t tab_size
)
{
	size_t screen_x = 0;
	size_t size = u32string_size(line);

	for (size_t i = 0; i < size; i++) {
		uint32_t c = u32string_char(line, i);

		size_t width = char_screen_width(c, screen_x, tab_size);

		if (desired_screen_x < screen_x + width) {
			if (c == U'\t') {
				size_t middle = screen_x + width / 2;

				return desired_screen_x < middle
					? i
					: i + 1;
			}

			return i;
		}

		screen_x += width;
	}

	return size;
}