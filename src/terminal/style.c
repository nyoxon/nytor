#include <stdio.h>
#include <unistd.h>
#include <stdbool.h>
#include <string.h>

#include "terminal/style.h"

int ansi_fg(enum ansi_color c) {
	if (c == DEFAULT) {
		return 39;
	}

	if (c <= WHITE) {
		return 30 + c;
	}

	return 90 + (c - BRIGHT_BLACK);
}

int ansi_bg(enum ansi_color c) {
	if (c == DEFAULT) {
		return 49;
	}

	if (c <= WHITE) {
		return 40 + c;
	}

	return 100 + (c - BRIGHT_BLACK);
}

static void append_color
(
	char* buf,
	size_t buf_size,
	size_t* len,
	bool* first,
	const struct color* c,
	bool background
)
{
	if (c->type == COLOR_ANSI) {
		if (!*first) {
			*len += snprintf(buf + *len, buf_size - *len, ";");
		}

		*len += snprintf(
			buf + *len,
			buf_size - *len,
			"%d",
			(background) ? ansi_bg(c->ansi) : ansi_fg(c->ansi)
		);

		*first = false;
	}

	else {
		if (!*first) {
			*len += snprintf(buf + *len, buf_size -*len, ";");
		}

		*len += snprintf(
			buf + *len,
			buf_size - *len,
			"%d;2;%u;%u;%u",
			background ? 48 : 38,
			c->rgb.r,
			c->rgb.g,
			c->rgb.b
		);

		*first = false;
	}
}

void write_color(const struct style* s) {
	char buf[64];
	size_t len = 0;
	bool first = true;

	len += snprintf(buf + len, sizeof(buf) - len, "\x1b[");

	#define APPEND(code)                                                \
		do {                                                            \
			if (!first)                                                 \
				len += snprintf(buf + len, sizeof(buf) - len, ";");     \
			len += snprintf(buf + len, sizeof(buf) - len, "%d", (code));\
			first = false;                                              \
		} while (0)

	if (s->modifiers & BOLD) {
		APPEND(1);
	}

	if (s->modifiers & DIM) {
		APPEND(2);
	}

	if (s->modifiers & ITALIC) {
		APPEND(3);
	}

	if (s->modifiers & UNDERLINE) {
		APPEND(4);
	}

	if (s->modifiers & REVERSE) {
		APPEND(7);
	}

	append_color(buf, sizeof(buf), &len, &first, &s->fg, false);
	append_color(buf, sizeof(buf), &len, &first, &s->bg, true);

	len += snprintf(buf + len, sizeof(buf) - len, "m");

	#undef APPEND

	write(STDOUT_FILENO, buf, len);
}

void reset_color() {
	write(STDOUT_FILENO, "\033[0m", 4);
}

void invert_color() {
	write(STDOUT_FILENO, "\033[7m", 4);
}

const struct color ANSI_COLOR_DEFAULT = {
	.type = COLOR_ANSI,
	.ansi = DEFAULT
};

const struct color RGB_COLOR_DEFAULT = {
	.type = COLOR_RGB,
	.rgb = { 255, 255, 255 }
};

const struct style STYLE_DEFAULT = {
	ANSI_COLOR_DEFAULT,
	ANSI_COLOR_DEFAULT,
	0
};

struct color color_ansi(enum ansi_color c) {
	return (struct color) {
		.type = COLOR_ANSI,
		.ansi = c
	};
}


struct color color_rgb(uint8_t r, uint8_t g, uint8_t b) {
	return (struct color) {
		.type = COLOR_RGB,
		.rgb = { r, g, b }
	};
}

int color_equal(struct color a, struct color b) {
	if (a.type != b.type) {
		return 0;
	}

	if (a.type == COLOR_ANSI) {
		return a.ansi == b.ansi;
	}

	else if (a.type == COLOR_RGB) {
		return a.rgb.r == b.rgb.r &&
			   a.rgb.g == b.rgb.g &&
			   a.rgb.b == b.rgb.b;
	}

	return 0;
}