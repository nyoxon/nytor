#include <stdio.h>
#include <ctype.h>
#include <string.h>
#include <stdlib.h>
#include <assert.h>

#include "editor/core/config.h"
#include "util/files.h"

void vector_char_array_destroy(void* ptr) {
	char** name = (char**) ptr;
	free(*name);
}

static void set_background_color(struct config* config, struct color color) {
	for (size_t i = 0; i < HL_COUNT; i++) {
		struct color* bg = &config->theme[i].bg;

		if (color_equal(*bg, ANSI_COLOR_DEFAULT))
		{
			*bg = color;
		}
	}

	struct color* bg = &config->ui.window_color.border.bg;

	if (color_equal(*bg, ANSI_COLOR_DEFAULT))
	{
		*bg = color;
	}

	bg = &config->ui.window_color.text.bg;

	if (color_equal(*bg, ANSI_COLOR_DEFAULT))
	{
		*bg = color;
	}

	bg = &config->ui.window_color.current_line.bg;

	if (color_equal(*bg, ANSI_COLOR_DEFAULT))
	{
		*bg = color;
	}	

	if (config->background_fills_all) {
		bg = &config->ui.selection.bg;

		if (color_equal(*bg, ANSI_COLOR_DEFAULT))
		{
			*bg = color;
		}

		bg = &config->ui.line_number.bg;

		if (color_equal(*bg, ANSI_COLOR_DEFAULT))
		{
			*bg = color;
		}

		bg = &config->ui.line_number_current.bg;

		if (color_equal(*bg, ANSI_COLOR_DEFAULT))
		{
			*bg = color;
		}

		if (color_equal(*bg, ANSI_COLOR_DEFAULT))
		{
			*bg = color;
		}

		bg = &config->ui.status_bar.bg;

		if (color_equal(*bg, ANSI_COLOR_DEFAULT))
		{
			*bg = color;
		}				
	}
}

void config_default(struct config* config) {
	strcpy(config->shell, "");
	strcpy(config->theme_path, "");

	vector_init(
		&config->language_plugins, 
		sizeof(char*), 
		vector_char_array_destroy);

	config->tab_size = 4;
	config->use_spaces = 0;

	config->line_numbers = 1;
	config->cursor_follow_scroll = 1;
	config->show_tabs = 0;
	config->left_click_end_selection = 0;
	config->auto_save_quit = 0;
	config->background_fills_all = 0;
	config->has_background = 0;
	config->auto_shell = 0;
	config->use_autocomplete = 1;
	config->select_line_selects_next = 1;


	config->ui.normal = (struct style) {
		color_ansi(DEFAULT),
		color_ansi(DEFAULT),
		0
	};

	config->ui.selection = (struct style) {
		color_ansi(BLUE),
		color_ansi(BRIGHT_BLUE),
		0
	};

	config->ui.line_number = (struct style) {
		color_ansi(BLUE),
		color_ansi(DEFAULT),
		0
	};

	config->ui.line_number_current = (struct style) {
		color_ansi(GREEN),
		color_ansi(BRIGHT_BLUE),
		0
	};

	config->ui.status_bar = (struct style) {
		color_ansi(BLACK),
		color_ansi(RED),
		0
	};

	config->ui.cursor_style = CURSOR_STYLE_DEFAULT;

	config->ui.window_color.border = (struct style) {
		color_ansi(BLUE),
		color_ansi(DEFAULT),
		0
	};

	config->ui.window_color.text = (struct style) {
		color_ansi(RED),
		color_ansi(DEFAULT),
		0
	};

	config->ui.window_color.current_line = (struct style) {
		color_ansi(YELLOW),
		color_ansi(BLUE),
		0
	};

	config->theme[HL_NONE] = STYLE_DEFAULT;
	config->theme[HL_NORMAL] = STYLE_DEFAULT;
	config->theme[HL_WHITESPACE] = STYLE_DEFAULT;
	config->theme[HL_KEYWORD] = STYLE_DEFAULT;
	config->theme[HL_TYPE] = STYLE_DEFAULT;
	config->theme[HL_LIB_TYPE] = STYLE_DEFAULT;
	config->theme[HL_POSIX_TYPE] = STYLE_DEFAULT;
	config->theme[HL_PREPROCESSOR] = STYLE_DEFAULT;
	config->theme[HL_CONSTANT] = STYLE_DEFAULT;
	config->theme[HL_NUMBER] = STYLE_DEFAULT;
	config->theme[HL_STRING] = STYLE_DEFAULT;
	config->theme[HL_CHAR] = STYLE_DEFAULT;
	config->theme[HL_COMMENT] = STYLE_DEFAULT;
	config->theme[HL_OPERATOR] = STYLE_DEFAULT;
	config->theme[HL_PUNCTUATION] = STYLE_DEFAULT;
	config->theme[HL_FUNCTION] = STYLE_DEFAULT;
	config->theme[HL_LIB_FUNCTION] = STYLE_DEFAULT;
	config->theme[HL_SPECIFIER] = STYLE_DEFAULT;
	config->theme[HL_POSIX] = STYLE_DEFAULT;
	config->theme[HL_METHOD_OR_ATTRIB] = STYLE_DEFAULT;

	config->keybinds[ACTION_SAVE] = (struct normal_key) {
		's', KEY_MOD_CTRL
	};

	config->keybinds[ACTION_FIND] = (struct normal_key) {
		'f', KEY_MOD_CTRL
	};

	config->keybinds[ACTION_GOTO] = (struct normal_key) {
		'g', KEY_MOD_CTRL
	};

	config->keybinds[ACTION_QUIT] = (struct normal_key) {
		'q', KEY_MOD_CTRL
	};

	config->keybinds[ACTION_SHOW_TABS] = (struct normal_key) {
		't', KEY_MOD_ALT
	};

	config->keybinds[ACTION_MOVE_START_LINE] = (struct normal_key) {
		'd', KEY_MOD_ALT
	};

	config->keybinds[ACTION_MOVE_END_LINE] = (struct normal_key) {
		'd', KEY_MOD_CTRL
	};

	config->keybinds[ACTION_SELECT_LINE] = (struct normal_key) {
		'l', KEY_MOD_CTRL
	};

	config->keybinds[ACTION_SELECT_ALL] = (struct normal_key) {
		'a', KEY_MOD_CTRL
	};

	config->keybinds[ACTION_SELECTION] = (struct normal_key) {
		'\0', KEY_MOD_CTRL
	};

	config->keybinds[ACTION_COPY] = (struct normal_key) {
		'c', KEY_MOD_CTRL
	};

	config->keybinds[ACTION_PASTE] = (struct normal_key) {
		'v', KEY_MOD_CTRL
	};

	config->keybinds[ACTION_DEL_FROM_CURSOR_LEFT] = (struct normal_key) {
		'u', KEY_MOD_CTRL
	};

	config->keybinds[ACTION_DEL_FROM_CURSOR_RIGHT] = (struct normal_key) {
		'u', KEY_MOD_ALT
	};

	config->keybinds[ACTION_INDENT] = (struct normal_key) {
		'p', KEY_MOD_CTRL
	};

	config->keybinds[ACTION_UNINDENT] = (struct normal_key) {
		'o', KEY_MOD_CTRL
	};

	config->keybinds[ACTION_COMMENT] = (struct normal_key) {
		'k', KEY_MOD_CTRL
	};

	config->keybinds[ACTION_SCROLL_UP] = (struct normal_key) {
		ARROW_UP, KEY_MOD_CTRL
	};

	config->keybinds[ACTION_SCROLL_DOWN] = (struct normal_key) {
		ARROW_DOWN, KEY_MOD_CTRL
	};

	config->keybinds[ACTION_SCROLL_RIGHT] = (struct normal_key) {
		ARROW_RIGHT, KEY_MOD_CTRL
	};

	config->keybinds[ACTION_SCROLL_LEFT] = (struct normal_key) {
		ARROW_LEFT, KEY_MOD_CTRL
	};

	config->keybinds[ACTION_SCROLL_TERMINAL_UP] = (struct normal_key) {
		ARROW_UP, KEY_MOD_ALT
	};

	config->keybinds[ACTION_SCROLL_TERMINAL_DOWN] = (struct normal_key) {
		ARROW_DOWN, KEY_MOD_ALT
	};

	config->keybinds[ACTION_SELECTION_MOVE_UP] = (struct normal_key) {
		ARROW_UP, KEY_MOD_CTRL_SHIFT
	};

	config->keybinds[ACTION_SELECTION_MOVE_DOWN] = (struct normal_key) {
		ARROW_DOWN, KEY_MOD_CTRL_SHIFT
	};

	config->keybinds[ACTION_MOVE_WORD_RIGHT] = (struct normal_key) {
		ARROW_RIGHT, KEY_MOD_CTRL_SHIFT
	};

	config->keybinds[ACTION_MOVE_WORD_LEFT] = (struct normal_key) {
		ARROW_LEFT, KEY_MOD_CTRL_SHIFT
	};

	config->keybinds[ACTION_MOVE_FULLWORD_RIGHT] = (struct normal_key) {
		ARROW_RIGHT, KEY_MOD_CTRL_ALT
	};

	config->keybinds[ACTION_MOVE_FULLWORD_LEFT] = (struct normal_key) {
		ARROW_LEFT, KEY_MOD_CTRL_ALT
	};

	config->keybinds[ACTION_MATCH] = (struct normal_key) {
		'r', KEY_MOD_CTRL
	};

	config->keybinds[ACTION_SUSPEND] = (struct normal_key) {
		'b', KEY_MOD_ALT
	};

	config->keybinds[ACTION_REPLACE] = (struct normal_key) {
		'x', KEY_MOD_CTRL
	};

	config->keybinds[ACTION_UNDO] = (struct normal_key) {
		'z', KEY_MOD_CTRL
	};

	config->keybinds[ACTION_REDO] = (struct normal_key) {
		'y', KEY_MOD_CTRL
	};

	config->keybinds[ACTION_NEXT_FILE] = (struct normal_key) {
		ARROW_RIGHT, KEY_MOD_ALT
	};

	config->keybinds[ACTION_PREV_FILE] = (struct normal_key) {
		ARROW_LEFT, KEY_MOD_ALT
	};

	config->keybinds[ACTION_NEW_FILE] = (struct normal_key) {
		't', KEY_MOD_CTRL
	};

	config->keybinds[ACTION_CLOSE_FILE] = (struct normal_key) {
		'w', KEY_MOD_CTRL
	};

	config->keybinds[ACTION_OPEN_CMD] = (struct normal_key) {
		'n', KEY_MOD_CTRL
	};

	config->keybinds[ACTION_TERMINAL] = (struct normal_key) {
		'b', KEY_MOD_CTRL
	};

	config->keybinds[ACTION_QUIT_FORCED] = (struct normal_key) {
		'q', KEY_MOD_ALT
	};

	config->keybinds[ACTION_CLOSE_FILE_FORCED] = (struct normal_key) {
		'w', KEY_MOD_ALT
	};
}

void config_check_equal_keybinds
(
	const struct config* config,
	int* index1,
	int* index2
) 
{
	for (int i = 0; i < ACTION_COUNT; i++) {
		for (int j = i + 1; j < ACTION_COUNT; j++) {
			struct normal_key a = config->keybinds[i];
			struct normal_key b = config->keybinds[j];

			if (a.content == b.content &&
				a.modifiers == b.modifiers)
			{
				*index1 = i;
				*index2 = j;
				return;
			}
		}
	}

	*index1 = -1;
	*index2 = -1;
}

static int is_control_char(uint32_t content) {
	return (content == U'i' || content == U'm' ||
			content == U'j' || content == U'[');
}

int config_check_valid_keybinds
(
	const struct config* config,
	int* index
) 
{
	for (int i = 0; i < ACTION_COUNT; i++) {
		struct normal_key key = config->keybinds[i];

		if (is_control_char(key.content) &&
			key.modifiers == KEY_MOD_CTRL)
		{
			*index = i;
			return 0;
		}

		if ((key.content >= U'a' && key.content <= U'z') ||
			(key.content >= U'A' && key.content <= U'Z'))
		{
			if (key.modifiers == KEY_MOD_SHIFT ||
				key.modifiers == KEY_MOD_CTRL_SHIFT ||
				key.modifiers == KEY_MOD_SHIFT_ALT ||
				key.modifiers == KEY_MOD_CTRL_SHIFT_ALT) 
			{
				*index = i;
				return 1;
			}
		}
	}

	*index = -1;
	return -1;
}

static enum ansi_color parse_ansi(char* color, size_t len) {
	if (strncmp(color, "default", len) == 0) {
		return DEFAULT;
	}

	if (strncmp(color, "black", len) == 0) {
		return BLACK;
	}

	if (strncmp(color, "red", len) == 0) {
		return RED;
	}

	if (strncmp(color, "green", len) == 0) {
		return GREEN;
	}

	if (strncmp(color, "yellow", len) == 0) {
		return YELLOW;
	}

	if (strncmp(color, "blue", len) == 0) {
		return BLUE;
	}

	if (strncmp(color, "magenta", len) == 0) {
		return MAGENTA;
	}

	if (strncmp(color, "cyan", len) == 0) {
		return CYAN;
	}

	if (strncmp(color, "white", len) == 0) {
		return WHITE;
	}

	if (strncmp(color, "bright_black", len) == 0) {
		return BRIGHT_BLACK;;
	}

	if (strncmp(color, "bright_red", len) == 0) {
		return BRIGHT_RED;
	}

	if (strncmp(color, "bright_green", len) == 0) {
		return BRIGHT_GREEN;
	}

	if (strncmp(color, "bright_yellow", len) == 0) {
		return BRIGHT_YELLOW;
	}

	if (strncmp(color, "bright_blue", len) == 0) {
		return BRIGHT_BLUE;
	}

	if (strncmp(color, "bright_magenta", len) == 0) {
		return BRIGHT_MAGENTA;
	}

	if (strncmp(color, "bright_cyan", len) == 0) {
		return BRIGHT_CYAN;
	}

	if (strncmp(color, "bright_white", len) == 0) {
		return BRIGHT_WHITE;
	}

	return DEFAULT;
}

static int is_hex_color(const char* color) {
	if (strlen(color) != 7) {
		return 0;
	}

	for (int i = 1; i <= 6; i++) {
		if (!isxdigit((unsigned char) color[i])) {
			return 0;
		}
	}

	return 1;
}

static struct color parse_rgb(char* color) {
	uint8_t r, g, b;

	if (*color == '#' && is_hex_color(color)) {
		unsigned long hex = strtoul(color + 1, NULL, 16);

		r = (hex >> 16) & 0xFF;
		g = (hex >> 8) & 0xFF;
		b = hex & 0xFF;

		return color_rgb(r, g, b);
	}

	else if (*color == '(') {
		if (sscanf(color, "(%hhu, %hhu, %hhu)", &r, &g, &b) == 3) {
			r = r % 256;
			g = g % 256;
			b = b % 256;

			return color_rgb(r, g, b);
		}

		else {
			return ANSI_COLOR_DEFAULT;
		}		
	}

	return ANSI_COLOR_DEFAULT;
}

static struct color parse_color(char* color, size_t len) {
	if (*color == '(' || *color == '#') {
		return parse_rgb(color);
	}

	else {
		return color_ansi(parse_ansi(color, len));
	}
}

static int get_property
(
	const char* line, 
	const char* name,
	struct color* c
) 
{
	char* value = strstr(line, name);

	if (value) {
		if (!c) {
			return 0;
		}

		value = strchr(value, '=');

		if (value) {
			value++;

			while (isspace((unsigned char) *value)) {
				value++;
			}

			if (*value == '"') {
				value++;

				char* end = strchr(value, '"');

				if (end) {
					size_t len = end - value;

					char color[len + 1];
					strncpy(color, value, len);
					color[len] = '\0';

					*c = parse_color(color, len);

					return 0;
				}
			} 
		}
	}

	return -1;
}

static int parse_assignment
(
	char* line, 
	char* key,
	struct style* s
) 
{
	if (!line) {
		return -1;
	}

	char* eq = strchr(line, '=');

	if (!eq) {
		return -1;
	}

	*eq = '\0';

	char* key_a = trim(line);
	strcpy(key, key_a);

	char* value = trim(eq + 1);

	struct color color;

	if (get_property(value, "fg", &color) == 0) {
		s->fg = color;
	} else {
		s->fg = ANSI_COLOR_DEFAULT;
	}

	if (get_property(value, "bg", &color) == 0) {
		s->bg = color;
	} else {
		s->bg = ANSI_COLOR_DEFAULT;
	}

	if (get_property(value, "bold", NULL) == 0) {
		s->modifiers |= BOLD;
	}

	if (get_property(value, "italic", NULL) == 0) {
		s->modifiers |= ITALIC;
	}

	if (get_property(value, "underline", NULL) == 0) {
		s->modifiers |= UNDERLINE;
	}

	if (get_property(value, "reverse", NULL) == 0) {
		s->modifiers |= REVERSE;
	}

	if (get_property(value, "dim", NULL) == 0) {
		s->modifiers |= DIM;
	}

	return 0;
}


enum section {
	SEC_NONE,
	SEC_UI,
	SEC_SYNTAX,
	SEC_KEYBINDS,
	SEC_PLUGINS
};

static enum section parse_section(const char* line) {
	if (*line != '[') {
		return SEC_NONE;
	}

	line++;

	const char* end = strchr(line, ']');

	if (!end) {
		return SEC_NONE;
	}

	size_t len = end - line;

	if (strncmp(line, "ui", len) == 0) {
		return SEC_UI;
	}

	if (strncmp(line, "syntax", len) == 0) {
		return SEC_SYNTAX;
	}

	if (strncmp(line, "keybinds", len) == 0) {
		return SEC_KEYBINDS;
	}

	if (strncmp(line, "plugins", len) == 0) {
		return SEC_PLUGINS;
	}

	return SEC_NONE;
}

static void handle_ui(const char* key, struct style s, struct ui* ui) {
	if (strcmp(key, "normal") == 0) {
		ui->normal = s;
	}

	if (strcmp(key, "selection") == 0) {
		ui->selection = s;
	}

	if (strcmp(key, "line_number") == 0) {
		ui->line_number = s;
	}

	if (strcmp(key, "line_number_current") == 0) {
		ui->line_number_current = s;
	}

	if (strcmp(key, "status_bar") == 0) {
		ui->status_bar = s;
	}

	if (strcmp(key, "window_border") == 0) {
		ui->window_color.border = s;
	}

	if (strcmp(key, "window_text") == 0) {
		ui->window_color.text = s;
	}

	if (strcmp(key, "window_current_line") == 0) {
		ui->window_color.current_line = s;
	}
}


static void handle_syntax
(
	const char* key, 
	struct style s, 
	struct style* style
) 
{
	if  (strcmp(key, "normal") == 0) {
		style[HL_NORMAL] = s;
		style[HL_NONE] = s;
	}

	else if  (strcmp(key, "whitespace") == 0) {
		style[HL_WHITESPACE] = s;
	}

	else if  (strcmp(key, "comment") == 0) {
		style[HL_COMMENT] = s;
	}

	else if  (strcmp(key, "keyword") == 0) {
		style[HL_KEYWORD] = s;
	}

	else if  (strcmp(key, "preprocessor") == 0) {
		style[HL_PREPROCESSOR] = s;
	}

	else if  (strcmp(key, "type") == 0) {
		style[HL_TYPE] = s;
	}

	else if  (strcmp(key, "constant") == 0) {
		style[HL_CONSTANT] = s;
	}

	else if  (strcmp(key, "number") == 0) {
		style[HL_NUMBER] = s;
	}

	else if  (strcmp(key, "string") == 0) {
		style[HL_STRING] = s;
	}

	else if  (strcmp(key, "char") == 0) {
		style[HL_CHAR] = s;
	}

	else if  (strcmp(key, "operator") == 0) {
		style[HL_OPERATOR] = s;
	}

	else if  (strcmp(key, "punctuation") == 0) {
		style[HL_PUNCTUATION] = s;
	}

	else if  (strcmp(key, "function") == 0) {
		style[HL_FUNCTION] = s;
	}

	else if  (strcmp(key, "lib_function") == 0) {
		style[HL_LIB_FUNCTION] = s;
	}

	else if 	(strcmp(key, "lib_type") == 0) {
		style[HL_LIB_TYPE] = s;
	}

	else if (strcmp(key, "posix_type") == 0) {
		style[HL_POSIX_TYPE] = s;
	}

	else if (strcmp(key, "method_or_attrib") == 0) {
		style[HL_METHOD_OR_ATTRIB] = s;
	}

	else if 	(strcmp(key, "posix") == 0) {
		style[HL_POSIX] = s;
	}

	else if  (strcmp(key, "specifier") == 0) {
		style[HL_SPECIFIER] = s;
	}
}

int config_load_specific_theme(struct config* config, const char* theme) {
	if (strcmp(theme, "") == 0) {
		return -1;
	}

	char path[PATH_MAX_LENGTH];

	if (make_theme_path(path, PATH_MAX_LENGTH, theme) < 0) {
		return -2;
	}

	FILE* file = fopen(path, "r");

	if (!file) {
		sprintf(path, "/usr/local/share/nytor/themes/%s", theme);

		file = fopen(path, "r");

		if (!file) {
			sprintf(path, "/usr/share/nytor/themes/%s", theme);

			file = fopen(path, "r");

			if (!file) {
				return -2;
			}
		}
	}

	char line[256];
	enum section current = SEC_NONE;

	while (fgets(line, sizeof(line), file)) {
		struct style s = STYLE_DEFAULT;

		char* trimmed_line = trim(line);
		char key[64];

		if (trimmed_line[0] == '\0') {
			continue;
		}

		if (trimmed_line[0] == '#') {
			continue;
		}

		if (trimmed_line[0] == '[') {
			current = parse_section(line);
			continue;
		}

		parse_assignment(trimmed_line, key, &s);

		if (current == SEC_UI) {
			handle_ui(key, s, &config->ui);
		}

		if (current == SEC_SYNTAX) {
			handle_syntax(key, s, config->theme);
		}
	}

	if (!color_equal(config->ui.normal.bg, ANSI_COLOR_DEFAULT)) 
	{
		config->has_background = 1;

		set_background_color(config, config->ui.normal.bg);
	}

	if (strcmp(config->theme_path, theme) != 0) {
		strcpy(config->theme_path, theme);
	}

	fclose(file);

	return 0;	
}

static void parse_assignment_config
(
	char* line,
	char* key,
	char* value
)
{
	if (!line) {
		return;
	}

	char* eq = strchr(line, '=');

	if (!eq) {
		return;
	}

	*eq = '\0';

	char* key_a = trim(line);
	strcpy(key, key_a);

	char* value_a = trim(eq + 1);
	strcpy(value, value_a);
}

static void handle_ui_config
(
	char* key,
	char* value,
	struct config* config
)
{
	if (!key || !value) {
		return;
	}

	if (strcmp(key, "theme") == 0) {
		if (*value == '"') {
			value++;

			char* end = strchr(value, '"');

			if (end) {
				size_t len = end - value; 
				strncpy(config->theme_path, value, len);
				config->theme_path[len] = '\0';
			}
		}
	}

	if (strcmp(key, "shell") == 0) {
		if (*value == '"') {
			value++;

			char* end = strchr(value, '"');

			if (end) {
				size_t len = end - value; 
				strncpy(config->shell, value, len);
				config->shell[len] = '\0';

				if (strcmp(config->shell, "auto") == 0) {
					config->auto_shell = 1;
				}
			}
		}
	}

	if (strcmp(key, "use_autocomplete") == 0) {
		if (strcmp(value, "true") == 0) {
			config->use_autocomplete = 1;
		}

		else if (strcmp(value, "false") == 0) {
			config->use_autocomplete = 0;
		}		
	}

	if (strcmp(key, "tab_size") == 0) {
		int v = atoi(value);

		if (v >= 0) {
			config->tab_size = v;
		}
	}

	if (strcmp(key, "use_spaces") == 0) {
		if (strcmp(value, "true") == 0) {
			config->use_spaces = 1;
		}

		else if (strcmp(value, "false") == 0) {
			config->use_spaces = 0;
		}
	}

	if (strcmp(key, "line_numbers") == 0) {
		if (strcmp(value, "false") == 0) {
			config->line_numbers = 0;
		}

		else if (strcmp(value, "true") == 0) {
			config->line_numbers = 1;
		}
	}

	if (strcmp(key, "cursor_follow_scroll") == 0) {
		if (strcmp(value, "false") == 0) {
			config->cursor_follow_scroll = 0;
		}

		else if (strcmp(value, "true") == 0) {
			config->cursor_follow_scroll = 1;
		}
	}

	if (strcmp(key, "show_tabs") == 0) {
		if (strcmp(value, "false") == 0) {
			config->show_tabs = 0;
		}

		else if (strcmp(value, "true") == 0) {
			config->show_tabs = 1;
		}
	}

	if (strcmp(key, "left_click_end_selection") == 0) {
		if (strcmp(value, "false") == 0) {
			config->left_click_end_selection = 0;
		}

		else if (strcmp(value, "true") == 0) {
			config->left_click_end_selection = 1;
		}
	}

	if (strcmp(key, "select_line_selects_next") == 0) {
		if (strcmp(value, "false") == 0) {
			config->select_line_selects_next = 0;
		}

		else if (strcmp(value, "true") == 0) {
			config->select_line_selects_next = 1;
		}
	}

	if (strcmp(key, "auto_save_quit") == 0) {
		if (strcmp(value, "false") == 0) {
			config->auto_save_quit = 0;
		}

		else if (strcmp(value, "true") == 0) {
			config->auto_save_quit = 1;
		}
	}

	if (strcmp(key, "background_fills_all") == 0) {
		if (strcmp(value, "false") == 0) {
			config->background_fills_all = 0;
		}

		else if (strcmp(value, "true") == 0) {
			config->background_fills_all = 1;
		}
	}

	if (strcmp(key, "cursor_style") == 0) {
		if (strcmp(value, "default") == 0) {
			config->ui.cursor_style = CURSOR_STYLE_DEFAULT;
		}

		else if (strcmp(value, "steady_bar") == 0) {
			config->ui.cursor_style = CURSOR_STYLE_STEADY_BAR;
		}

		else if (strcmp(value, "blinking_bar") == 0) {
			config->ui.cursor_style = CURSOR_STYLE_BLINKING_BAR;
		}

		else if (strcmp(value, "steady_underline") == 0) {
			config->ui.cursor_style = CURSOR_STYLE_STEADY_UNDERLINE;
		}

		else if (strcmp(value, "blinking_underline") == 0) {
			config->ui.cursor_style = CURSOR_STYLE_BLINKING_UNDERLINE;
		}

		else if (strcmp(value, "steady_block") == 0) {
			config->ui.cursor_style = CURSOR_STYLE_STEADY_BLOCK;
		}

		else if (strcmp(value, "blinking_block") == 0) {
			config->ui.cursor_style = CURSOR_STYLE_BLINKING_BLOCK;
		}

		else {
			config->ui.cursor_style = CURSOR_STYLE_DEFAULT;
		}
	}
}

static struct normal_key parse_key
(
	char* str
) 
{
	struct normal_key key = (struct normal_key) {
		-1, 0
	};

	if (!str || strlen(str) == 0) {
		return key;
	}

	if (strcmp(str, "up") == 0) {
		key.content = ARROW_UP;
	}

	else if (strcmp(str, "down") == 0) {
		key.content = ARROW_DOWN;
	}

	else if (strcmp(str, "right") == 0) {
		key.content = ARROW_RIGHT;
	}

	else if (strcmp(str, "left") == 0) {
		key.content = ARROW_LEFT;
	}

	else if (strcmp(str, " ") == 0) {
		key.content = '\0';
	}

	else {
		key.content = str[0];
	}

	return key;
}

static void parse_key_part
(
	struct normal_key* kb, 
	char* part,
	int* ctrl,
	int* shift,
	int* alt
) 
{
	if (strcmp(part, "ctrl") == 0) {
		if (ctrl) {
			*ctrl = 1;
		}
	}

	if (strcmp(part, "shift") == 0) {
		if (shift) {
			*shift = 1;
		}
	}

	if (strcmp(part, "alt") == 0) {
		if (alt) {
			*alt = 1;
		}
	}

	else {
		*kb = parse_key(part);
	}
}

static struct normal_key parse_keybind(const char* str) {
	struct normal_key kb = (struct normal_key) {
		-1, 0
	};

	const char* start = str + 1;

	int ctrl = 0;
	int shift = 0;
	int alt = 0;

	while (*str) {
		if (*str == '+') {
			size_t len = str - start;
			char part[len + 1];
			part[len] = '\0';
			strncpy(part, start, len);

			parse_key_part(&kb, part, 
				&ctrl, &shift, &alt);

			start = str + 1;
		}

		str++;
	}

	size_t len = str - start - 1;

	if (len == 0) {
		return (struct normal_key) {
			-1, 0
		};	
	}

	char part[len + 1];
	part[len] = '\0';
	strncpy(part, start, len);

	parse_key_part(&kb, part, NULL, NULL, NULL);

	uint32_t modifiers = 0;

	if (ctrl && shift && alt) {
		modifiers = KEY_MOD_CTRL_SHIFT_ALT;
	} 

	else if (ctrl && shift) {
		modifiers = KEY_MOD_CTRL_SHIFT;
	}

	else if (ctrl && alt) {
		modifiers = KEY_MOD_CTRL_ALT;
	}

	else if (shift && alt) {
		modifiers = KEY_MOD_SHIFT_ALT;
	}

	else if (ctrl) {
		modifiers = KEY_MOD_CTRL;
	} 

	else if (shift) {
		modifiers = KEY_MOD_SHIFT;
	} 

	else if (alt) {
		modifiers = KEY_MOD_ALT;
	}

	if (!ctrl && !shift && !alt) {
		return (struct normal_key) {
			-1, 0
		};
	}

	kb.modifiers = modifiers;

	return kb;
}

static void handle_keybinds
(
	const char* key,
	const char* value,
	struct config* config
)
{
	if (strcmp(key, "save") == 0) {
		config->keybinds[ACTION_SAVE] = parse_keybind(value);
	}

	else if (strcmp(key, "quit") == 0) {
		config->keybinds[ACTION_QUIT] = parse_keybind(value);
	}

	else if (strcmp(key, "quit_forced") == 0) {
		config->keybinds[ACTION_QUIT_FORCED] = parse_keybind(value);
	}

	else if (strcmp(key, "find") == 0) {
		config->keybinds[ACTION_FIND] = parse_keybind(value);
	}

	else if (strcmp(key, "match") == 0) {
		config->keybinds[ACTION_MATCH] = parse_keybind(value);
	}

	else if (strcmp(key, "goto") == 0) {
		config->keybinds[ACTION_GOTO] = parse_keybind(value);
	}

	else if (strcmp(key, "indent") == 0) {
		config->keybinds[ACTION_INDENT] = parse_keybind(value);
	}

	else if (strcmp(key, "unindent") == 0) {
		config->keybinds[ACTION_UNINDENT] = parse_keybind(value);
	}

	else if (strcmp(key, "comment") == 0) {
		config->keybinds[ACTION_COMMENT] = parse_keybind(value);
	}

	else if (strcmp(key, "move_start_line") == 0) {
		config->keybinds[ACTION_MOVE_START_LINE] = parse_keybind(value);
	}

	else if (strcmp(key, "move_end_line") == 0) {
		config->keybinds[ACTION_MOVE_END_LINE] = parse_keybind(value);
	}

	else if (strcmp(key, "show_tabs") == 0) {
		config->keybinds[ACTION_SHOW_TABS] = parse_keybind(value);
	}

	else if (strcmp(key, "selection") == 0) {
		config->keybinds[ACTION_SELECTION] = parse_keybind(value);
	}

	else if (strcmp(key, "select_all") == 0) {
		config->keybinds[ACTION_SELECT_ALL] = parse_keybind(value);
	}

	else if (strcmp(key, "select_line") == 0) {
		config->keybinds[ACTION_SELECT_LINE] = parse_keybind(value);
	}

	else if (strcmp(key, "copy") == 0) {
		config->keybinds[ACTION_COPY] = parse_keybind(value);
	}

	else if (strcmp(key, "paste") == 0) {
		config->keybinds[ACTION_PASTE] = parse_keybind(value);
	}

	else if (strcmp(key, "del_from_cursor_left") == 0) {
		config->keybinds[ACTION_DEL_FROM_CURSOR_LEFT] = parse_keybind(value);
	}

	else if (strcmp(key, "del_from_cursor_right") == 0) {
		config->keybinds[ACTION_DEL_FROM_CURSOR_RIGHT] = parse_keybind(value);
	}

	else if (strcmp(key, "selection_move_up") == 0) {
		config->keybinds[ACTION_SELECTION_MOVE_UP] = parse_keybind(value);
	}

	else if (strcmp(key, "selection_move_down") == 0) {
		config->keybinds[ACTION_SELECTION_MOVE_DOWN] = parse_keybind(value);
	}

	else if (strcmp(key, "scroll_up") == 0) {
		config->keybinds[ACTION_SCROLL_UP] = parse_keybind(value);
	}

	else if (strcmp(key, "scroll_down") == 0) {
		config->keybinds[ACTION_SCROLL_DOWN] = parse_keybind(value);
	}

	else if (strcmp(key, "scroll_terminal_up") == 0) {
		config->keybinds[ACTION_SCROLL_TERMINAL_UP] = parse_keybind(value);
	}

	else if (strcmp(key, "scroll_terminal_down") == 0) {
		config->keybinds[ACTION_SCROLL_TERMINAL_DOWN] = parse_keybind(value);
	}

	else if (strcmp(key, "move_word_right") == 0) {
		config->keybinds[ACTION_MOVE_WORD_RIGHT] = parse_keybind(value);
	}

	else if (strcmp(key, "move_word_left") == 0) {
		config->keybinds[ACTION_MOVE_WORD_LEFT] = parse_keybind(value);
	}

	else if (strcmp(key, "move_fullword_right") == 0) {
		config->keybinds[ACTION_MOVE_FULLWORD_RIGHT] = parse_keybind(value);
	}

	else if (strcmp(key, "move_fullword_left") == 0) {
		config->keybinds[ACTION_MOVE_FULLWORD_LEFT] = parse_keybind(value);
	}

	else if (strcmp(key, "suspend") == 0) {
		config->keybinds[ACTION_SUSPEND] = parse_keybind(value);
	}

	else if (strcmp(key, "scroll_right") == 0) {
		config->keybinds[ACTION_SCROLL_RIGHT] = parse_keybind(value);
	}

	else if (strcmp(key, "scroll_left") == 0) {
		config->keybinds[ACTION_SCROLL_LEFT] = parse_keybind(value);
	}

	else if (strcmp(key, "replace") == 0) {
		config->keybinds[ACTION_REPLACE] = parse_keybind(value);
	}

	else if (strcmp(key, "undo") == 0) {
		config->keybinds[ACTION_UNDO] = parse_keybind(value);
	}

	else if (strcmp(key, "redo") == 0) {
		config->keybinds[ACTION_REDO] = parse_keybind(value);
	}

	else if (strcmp(key, "next_file") == 0) {
		config->keybinds[ACTION_NEXT_FILE] = parse_keybind(value);
	}

	else if (strcmp(key, "prev_file") == 0) {
		config->keybinds[ACTION_PREV_FILE] = parse_keybind(value);
	}

	else if (strcmp(key, "new_file") == 0) {
		config->keybinds[ACTION_NEW_FILE] = parse_keybind(value);
	}

	else if (strcmp(key, "close_file") == 0) {
		config->keybinds[ACTION_CLOSE_FILE] = parse_keybind(value);
	}

	else if (strcmp(key, "close_file_forced") == 0) {
		config->keybinds[ACTION_CLOSE_FILE_FORCED] = parse_keybind(value);
	}

	else if (strcmp(key, "open_cmd") == 0) {
		config->keybinds[ACTION_OPEN_CMD] = parse_keybind(value);
	}

	else if (strcmp(key, "terminal") == 0) {
		config->keybinds[ACTION_TERMINAL] = parse_keybind(value);
	}
}

static void handle_plugins
(
	const char* key,
	const char* value,
	struct config* config
)
{
	if (strcmp(key, "language") == 0) {
		char* v = malloc(PATH_MAX_LENGTH);
		strcpy(v, value);

		vector_push(&config->language_plugins, &v);
	}
}

int config_load(struct config* config) {
	char path[PATH_MAX_LENGTH];

	if (make_config_path(path, PATH_MAX_LENGTH) < 0) {
		return -2;
	}

	FILE* file = fopen(path, "r");

	if (!file) {
		return -1;
	}

	char line[256];

	enum section current = SEC_NONE;

	while (fgets(line, sizeof(line), file)) {
		char* trimmed_line = trim(line);
		char key[64] = "";
		char value[64] = "";

		if (trimmed_line[0] == '\0') {
			continue;
		}

		if (trimmed_line[0] == '#') {
			continue;
		}

		if (trimmed_line[0] == '[') {
			current = parse_section(line);
			continue;
		}

		parse_assignment_config(trimmed_line, key, value);

		if (current == SEC_UI) {
			handle_ui_config(key, value, config);
		} 

		else if (current == SEC_KEYBINDS) {
			handle_keybinds(key, value, config);
		}

		else if (current == SEC_PLUGINS) {
			handle_plugins(key, value, config);
		}
	}

	fclose(file);

	if (config_load_specific_theme(config, config->theme_path) < -1) {
		return -2;
	}

	return 0;
}