#include <assert.h>

#include "window/window.h"

#define WINDOW_MAX_WIDTH_MULTIPLIER 0.5
#define WINDOW_MAX_HEIGHT_MULTIPLIER 0.5

static void vector_destroy_string(void* ptr) {
	u32string* string = ptr;

	u32string_free(string);
}

size_t window_width(const Window* w) {
	return w->tsize.cols * w->sw;
}

size_t window_height(const Window* w) {
	return w->tsize.rows * w->sh;
}

static void set_valid_size(Window* w)
{
	if (w->sw > WINDOW_MAX_WIDTH_MULTIPLIER) {
		w->sw = WINDOW_MAX_WIDTH_MULTIPLIER;
	}

	if (w->sh > WINDOW_MAX_HEIGHT_MULTIPLIER) {
		w->sh = WINDOW_MAX_WIDTH_MULTIPLIER;
	}
}

static void set_valid_pos(Window* w) {
	size_t width = window_width(w);
	size_t height = window_height(w);

	if (w->pos.x + width > w->tsize.cols) {
		w->pos.x = w->tsize.cols - width;
	}

	if (w->pos.y + height > w->tsize.rows) {
		w->pos.y = w->tsize.rows - height;
	}
}

Window window_new(WindowOptions* options) {
	Window w = {
		.sw = options->sw, 
		.sh = options->sh,
		.tab_size = options->tab_size,
		.tsize = options->tsize,
		.on_select = options->on_select
	};

	w.width = w.tsize.cols * w.sw;
	w.height = w.tsize.rows * w.sh;

	set_valid_size(&w);

	switch (options->pos_type) {
	case WINDOWPOS_CENTRALIZED:
		w.pos = (Position) {
			(w.tsize.cols - w.width) / 2,
			(w.tsize.rows - w.height) / 2
		};

		break;

	case WINDOWPOS_CUSTOM:
		w.pos = options->pos;
		break;

	default:
		assert(1 == 2);
	}

	set_valid_pos(&w);

	vector_init(&w.content, sizeof(u32string), vector_destroy_string);

	w.cursor.pos = POS_ZERO;
	w.cursor.preferred_column = 0;

	w.view.row_offset = 0;
	w.view.col_offset = 0;

	return w;
}

void window_close(Window* w) {
	vector_free(&w->content);

	w->on_select = NULL;
}

size_t window_cursor_screen_x(const Window* w) {
	if (w->content.size == 0) {
		return 0;
	}

	const u32string* line = vector_get_const(
		&w->content,
		w->cursor.pos.y
	);

	return file_to_screen_x(
		line,
		w->cursor.pos.x,
		w->tab_size
	);
}

void window_sync_cursor(Window* w) {
	w->cursor.preferred_column = window_cursor_screen_x(w);
}

void window_update_size(Window* w, TerminalSize* tsize) {
	w->tsize = *tsize;

	update_scroll(
		&w->view,
		w->cursor.pos,
		window_cursor_screen_x(w),
		&w->tsize
	);
}