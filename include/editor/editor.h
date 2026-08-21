#ifndef EDITOR_H
#define EDITOR_H

#include <sys/types.h>

#include "file/file.h"
#include "editor/core/scroll.h"
#include "editor/core/config.h"
#include "editor/core/prompt.h"
#include "editor/core/editor_file.h"
#include "window/window.h"
#include "util/types/clipboard.h"
#include "util/types/stack.h"
#include "util/debug/error.h"
#include "util/debug/debug.h"
#include "terminal/style.h"
#include "terminal/input.h"
#include "plugins/plugin.h"

#define HORIZONTAL_SCROLL_OVERLAP 8

#define DEFAULT_TERMINAL_WIDTH 50
#define DEFAULT_TERMINAL_HEIGHT 24

#define DEBUG_FILE "debug.ny"

// a Editor represents the editor itself
// it contains the global state of the program
typedef struct {
	Vector files;
	EditorFile* actual_file;
	size_t actual_file_index;

	int inotify_fd;

	struct config config;

	Cursor cursor;
	
	View view;
	TerminalSize tsize;

	Selection sel; // used on normal selection situations

	Clipboard cb;

	int suspend;

	int selecting;
	int debug_mode;

	Result result;

	Log log;

	Prompt status_bar;

	Vector lang_plugins_data;

	int has_window;
	Window window;
} Editor;


// --- EDITOR FUNCTIONS ---


// create a Editor
int editor_init
(
	Editor* editor,
	char* filenames[],
	size_t filename_count,
	int debug_mode
);

void editor_free(Editor* editor);

void editor_prompt_init(Editor* editor);

void editor_file_set_lang_plugin
(
	EditorFile* ef,
	const char* filename,
	const Vector* lang_plugins_data
);

void editor_check_inotify(Editor* editor);


// to check if editor has ef whose index
// is different from the passed one
// and has the same filename as the passed one
ssize_t editor_has_equal_filename
(
	const Editor* editor,
	const char* filename,
	size_t index
);

// to check if editor has a ef whose path
// is the same as the passed one
ssize_t editor_has_file(const Editor* editor, const char* path);

// write a message into the editor's log
void editor_log_write(const Editor* editor);

void editor_create_syntax(Editor* editor);
void editor_update_syntax(Editor* editor);

size_t editor_cursor_screen_x(Editor* editor);
void editor_sync_cursor(Editor* editor);
void editor_clamp_cursor_to_view(Editor* editor);

size_t get_gutter_width(size_t line_count);

const struct language_plugin* editor_get_language(const Editor* editor);
const struct language_rules* editor_get_rules(const Editor* editor);
const struct lexer* editor_get_lexer(const Editor* editor);
IsWordChar editor_get_IsWordChar(const Editor* editor);

#endif