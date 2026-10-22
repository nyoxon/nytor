#include <stdio.h>
#include <unistd.h>
#include <stdbool.h>
#include <string.h>
#include <stdlib.h>

#include "terminal/style.h"

static int has_substring(const char* value, const char* needle) {
	return value && strstr(value, needle) != NULL;
}

int terminal_supports_truecolor(void) {
	const char* term = getenv("TERM");
	const char* color = getenv("COLORTERM");

	return has_substring(term, "truecolor") || 
		   has_substring(term, "24bit") ||
		   (color && (!strcmp(color, "truecolor") || 
		   !strcmp(color, "24bit")));
}

int terminal_supports_256_colors(void) {
	const char* term = getenv("TERM");

	return terminal_supports_truecolor() || 
		   has_substring(term, "256color") ||
		   has_substring(term, "256-color");
}

int terminal_supports_xterm_extensions(void) {
	const char* term = getenv("TERM");

	return has_substring(term, "xterm") || 
		   has_substring(term, "screen") ||
		   has_substring(term, "tmux") || 
		   has_substring(term, "rxvt") ||
		   has_substring(term, "kitty") || 
		   has_substring(term, "wezterm") ||
		   has_substring(term, "alacritty");
}

static int rgb_to_256(uint8_t r, uint8_t g, uint8_t b) {
	static const uint8_t levels[] = { 0, 95, 135, 175, 215, 255 };

	int ri = (r < 48) 
			 ? 0 
			 : (r < 115) 
			 	? 1 
			 	: (r < 155) 
			 		? 2 
			 		: (r < 195) 
			 			? 3 
			 			: (r < 235) 
			 				? 4 
			 				: 5;

	int gi = (g < 48) 
			 ? 0 
			 : (g < 115) 
			 	? 1 
			 	: (g < 155) 
			 		? 2 
			 		: (g < 195) 
			 			? 3 
			 			: (g < 235) 
			 				? 4 
			 				: 5;

	int bi = (b < 48) 
			 ? 0 
			 : (b < 115) 
			 	? 1 
			 	: (b < 155) 
			 		? 2 
			 		: (b < 195) 
			 			? 3 
			 			: (b < 235) 
			 				? 4 
			 				: 5;

	int cube = 16 + 36 * ri + 6 * gi + bi;
	int gray = (int) ((r + g + b) / 3);

	int gi_gray = (gray < 8) 
				  ? 0 
				  : (gray > 238) 
				  	? 23 
				  	: (gray - 8 + 5) / 10;

	int gv = 8 + 10 * gi_gray;

	int dr = (int) r - gv, dg = (int) g - gv, db = (int) b - gv;

	int cr = (int) r - levels[ri], cg = (int) g - levels[gi], cb = (int) b - levels[bi];

	return (cr * cr + 
			   cg * cg + 
			   cb * cb <= dr * dr + dg * dg + db * db)
		? cube 
		: 232 + gi_gray;
}

static int rgb_to_ansi(uint8_t r, uint8_t g, uint8_t b) {
	int bright = (r > 127 || g > 127 || b > 127);

	int code = (r > 95 ? 1 : 0) | (g > 95 ? 2 : 0) | (b > 95 ? 4 : 0);

	if (bright) code += 8;

	return code;
}

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

	else if (terminal_supports_truecolor()) {
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
	else {
		int code;
		if (terminal_supports_256_colors()) {
			code = rgb_to_256(c->rgb.r, c->rgb.g, c->rgb.b);

			if (!*first) {
				*len += snprintf(buf + *len, buf_size - *len, ";");
			}

			*len += snprintf(
				buf + *len, buf_size - *len, 
				"%d;5;%d",
				(background) ? 48 : 38, 
				code
			);
		} 

		else {
			code = rgb_to_ansi(c->rgb.r, c->rgb.g, c->rgb.b);

			if (!*first) {
				*len += snprintf(buf + *len, buf_size - *len, ";");
			}

			*len += snprintf(
				buf + *len, 
				buf_size - *len, 
				"%d",
				(background) 
					? ansi_bg((enum ansi_color) code) 
					: ansi_fg((enum ansi_color) code)
			);
		}
		
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
