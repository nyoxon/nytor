#ifndef CONFIG_H
#define CONFIG_H

#include <stddef.h>

#include "terminal/input.h"
#include "terminal/style.h"
#include "plugins/language_syntax.h"
#include "util/types/vector.h"

#define PATH_MAX_LENGTH 1024

struct config {
	char shell[PATH_MAX_LENGTH];
	int auto_shell;

	char theme_path[PATH_MAX_LENGTH];
	Vector language_plugins;

	size_t tab_size;
	int use_spaces;

	int line_numbers;
	int cursor_follow_scroll;
	int show_tabs;
	int left_click_end_selection;
	int auto_save_quit;
	int background_fills_all;
	// int soft_wrap; // maybe someday

	int has_background;

	struct ui ui;
	struct style theme[HL_COUNT];
	struct normal_key keybinds[ACTION_COUNT];
};

void config_default(struct config* config);
int config_load(struct config* config);

void config_check_equal_keybinds
(
	const struct config* config,
	int* index1,
	int* index2
);

int config_check_valid_keybinds
(
	const struct config* config,
	int* index
);

void vector_char_array_destroy(void* ptr);


#endif