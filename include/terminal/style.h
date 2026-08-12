#ifndef STYLE_H
#define STYLE_H

#include <stddef.h>
#include <stdint.h>

#define CURSOR_STYLE_DEFAULT "\x1b[0 q"
#define CURSOR_STYLE_BLINKING_BLOCK "\x1b[1 q"
#define CURSOR_STYLE_STEADY_BLOCK "\x1b[2 q"
#define CURSOR_STYLE_BLINKING_UNDERLINE "\x1b[3 q"
#define CURSOR_STYLE_STEADY_UNDERLINE "\x1b[4 q"
#define CURSOR_STYLE_BLINKING_BAR "\x1b[5 q"
#define CURSOR_STYLE_STEADY_BAR "\x1b[6 q"

#define CURSOR_STYLE_SIZE 5

enum color_type {
	COLOR_ANSI,
	COLOR_RGB
};

enum ansi_color {
	BLACK,
	RED,
	GREEN,
	YELLOW,
	BLUE,
	MAGENTA,
	CYAN,
	WHITE,
	BRIGHT_BLACK,
	BRIGHT_RED,
	BRIGHT_GREEN,
	BRIGHT_YELLOW,
	BRIGHT_BLUE,
	BRIGHT_MAGENTA,
	BRIGHT_CYAN,
	BRIGHT_WHITE,
	DEFAULT
};


struct color {
	enum color_type type;

	union {
		enum ansi_color ansi;

		struct {
			uint8_t r;
			uint8_t g;
			uint8_t b;
		} rgb;
	};
};

int color_equal(struct color a, struct color b);

enum modifier {
	BOLD		= 1 << 0,
	ITALIC		= 1 << 1,
	UNDERLINE	= 1 << 2,
	REVERSE		= 1 << 3,
	DIM			= 1 << 4
};

struct style {
	struct color fg;
	struct color bg;

	uint32_t modifiers;
};

struct window_color {
	struct style border;
	struct style text;
	struct style current_line;
};

struct ui {
	struct style normal;
	struct style selection;
	struct style line_number;
	struct style line_number_current;
	struct style status_bar;
	struct style spaces_and_tabs;
	struct window_color window_color;

	char* cursor_style; // 5 + '\0'
};

struct color color_ansi(enum ansi_color);
struct color color_rgb(uint8_t r, uint8_t g, uint8_t b);

void write_color(const struct style* s);
void reset_color();
void invert_color();

int ansi_fg(enum ansi_color c);
int ansi_bg(enum ansi_color c);

extern const struct style STYLE_DEFAULT;
extern const struct color ANSI_COLOR_DEFAULT;
extern const struct color RGB_COLOR_DEFAULT;

#endif