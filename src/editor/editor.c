#include <unistd.h>
#include <stdio.h>
#include <string.h>
#include <assert.h>
#include <ctype.h>
#include <errno.h>
#include <stdlib.h>
#include <dlfcn.h>
#include <fcntl.h>
#include <signal.h>
#include <sys/inotify.h>

#include "editor/editor.h"
#include "editor/core/history.h"
#include "util/files.h"
#include "util/types/u32string.h"
#include "terminal/input.h"
#include "terminal/parser.h"

#define EVENT_BUF_LEN (1024 * sizeof(struct inotify_event) + 16)

void editor_prompt_init(Editor* editor);
static int handle_config_error(const struct config* config);
static int check_keybinds(const struct config* config);
static int editor_load_config(Editor* editor);
static void vector_editor_file_destroy(void* ptr);

static int has_equal_filename
(
	char* filenames[],
	size_t count,
	const char* filename
)
{
	for (size_t i = 0; i < count; i++) {
		if (strcmp(filename, filenames[i]) == 0) {
			return 1;
		}
	}

	return 0;
}

int editor_init
(
	Editor* editor,
	char* filenames[],
	size_t filename_count,
	int debug_mode
) 
{
	editor->debug_mode = debug_mode;

	if (debug_mode) {
		log_init(&editor->log, DEBUG_FILE);

		if (editor->log.fd < 0) {
			clean_terminal();
			disable_raw_mode();
			mouse_off();

			fprintf(stderr, "failed to start log file");

			return -1;
		}
	} 

	else {
		editor->log.fd = -1;
	}

	editor->result.reason = NULL;
	editor->result.type = EIE_OK;

	if (editor_load_config(editor) < 0) {
		if (editor->log.fd > 0) {
			close(editor->log.fd);
		}

		return -1;
	}

	if (load_lang_plugins(
		&editor->lang_plugins_data, 
		&editor->config.language_plugins) < 0)
	{
		if (editor->log.fd > 0) {
			close(editor->log.fd);
		}

		return -1;
	}

	vector_init(
		&editor->files, 
		sizeof(EditorFile), 
		vector_editor_file_destroy);


	editor->inotify_fd = inotify_init();


	/// --- CREATE PROTOTYPES ---
	if (!filenames) {
		EditorFileOptions options = {
			.filename = NULL,
			.readonly = 0,
			.default_tab_size = editor->config.tab_size,
			.default_use_spaces = editor->config.use_spaces,
			.inotify_fd = editor->inotify_fd,
			.use_autocomplete = editor->config.use_autocomplete
		};

		EditorFile ef = editor_file_prototype(&options);

		vector_push(&editor->files, &ef);
	}

	else {
		for (size_t i = 0; i < filename_count; i++) {
			if (i > 0 && has_equal_filename(
							filenames,
							i - 1,
							filenames[i])) 
			{
				continue;
			}

			EditorFileOptions options = {
				.filename = filenames[i],
				.readonly = 0,
				.default_tab_size = editor->config.tab_size,
				.default_use_spaces = editor->config.use_spaces,
				.inotify_fd = editor->inotify_fd,
				.use_autocomplete = editor->config.use_autocomplete
			};

			EditorFile ef = editor_file_prototype(&options);

			vector_push(&editor->files, &ef);
		}
	}


	/// --- THE FIRST LOADED FILE ---
	editor->actual_file = vector_get(&editor->files, 0);

	int ret = editor_file_open(
		editor->actual_file,
		&editor->lang_plugins_data,
		&editor->result
	);

	if (ret < 0) {
		vector_free(&editor->files);
		destroy_lang_plugins(&editor->lang_plugins_data);

		return -1;		
	}

	editor_create_syntax(editor);

	editor->actual_file_index = 0;

	update_terminal_size(&editor->tsize);

	editor->status_bar = prompt_new(editor->tsize); // called here once and never again
	editor_prompt_init(editor);

	editor->cb.text = u32string_new();
	editor->cb.linewise = 0;

	editor->suspend = 0;

	editor->cursor.pos = POS_ZERO;
	editor->cursor.preferred_column = 0;

	editor->view.row_offset = 0;
	editor->view.col_offset = 0;

	selection_clear(&editor->sel);
	editor->selecting = 0;

	editor->has_window = 0;

	return 0;
}

void editor_free(Editor* editor) {
	if (!editor) {
		return ;
	}

	result_free(&editor->result);
	clipboard_free(&editor->cb);
	vector_free(&editor->files);

	u32string_free(&editor->status_bar.label);
	u32string_free(&editor->status_bar.buf);

	if (editor->log.fd > 0) {
		close(editor->log.fd);
	}

	if (editor->has_window) {
		window_close(&editor->window);
	}

	if (editor->inotify_fd > 0) {
		close(editor->inotify_fd);
	}

	destroy_lang_plugins(&editor->lang_plugins_data);
}

static int handle_config_error(const struct config* config) {
	clean_terminal();
	fprintf(stderr, "failed to load theme: %s\n", config->theme_path);
	fprintf(stderr, "continue and use default theme?\n(y/n)\n");
	enable_raw_mode();

	int ret = 0;

	while (1) {
		struct event event = parser_read_key();

		if (event.type != EVENT_KEY) {
			continue;
		}

		int key = event.key.content;

		if (key == 'y' || key == 'Y') {
			ret = 0;
			break;
		}

		if (key == 'n' || key == 'N') {
			ret = -1;
			break;
		}
	}

	disable_raw_mode();
	return ret;
}

static void vector_editor_file_destroy(void* ptr) {
	EditorFile* ef = (EditorFile*) ptr;

	editor_file_free(ef);
}

static int check_keybinds(const struct config* config) {
	int invalid_kb, eqkb1, eqkb2;

	int ret = config_check_valid_keybinds(config, &invalid_kb);
	config_check_equal_keybinds(config, &eqkb1, &eqkb2);

	if (invalid_kb >= 0) {
		char* action = str_action(invalid_kb);
		fprintf(stderr, "invalid keybind: %s\n", action);
		free(action);

		struct normal_key key = config->keybinds[invalid_kb];

		char utf8[4];
		u32_encode(key.content, utf8);

		fprintf(stderr, "content: %s, ", utf8);

		char* modifier = str_modifier(key.modifiers);
		fprintf(stderr, "modifiers: %s\n\n", modifier);
		free(modifier);

		if (ret == 0) {
			fprintf(stderr, "reason: the combination of CONTENT and \n");
			fprintf(stderr, "MODIFIER produces a special character \n");
			fprintf(stderr, "in terminal and cannot be distinguished\n");
		}

		else if (ret == 1) {
			fprintf(stderr, "reason: the terminal emulator receives\n"
							"the input first and interprets SHIFT + LETTER\n"
							"as capitalizing the letter\n");
		}

		return -1;
	}

	if (eqkb1 >= 0) {
		char* action1 = str_action(eqkb1);
		char* action2 = str_action(eqkb2);

		fprintf(stderr, "%s and %s keybinds are equal\n", action1, action2);
		free(action1); free(action2);

		return -1;
	}

	return 0;
}

static EditorFile* find_file_by_watch
(
	Editor* editor, 
	int wd
) 
{
	for (size_t i = 0; i < editor->files.size; i++) {
		EditorFile* ef = vector_get(&editor->files, i);

		if (ef->type == EDITOR_FILE_PROTOTYPE) {
			continue;
		}

		if (!ef->file.filename) {
			continue;
		}

		if (ef->watch_descriptor == wd) 
		{
			return ef;
		}
	}

	return NULL;
}

void editor_check_inotify(Editor* editor) {
	if (editor->inotify_fd < 0) {
		if (editor->debug_mode) {
			log_write(&editor->log, 
				"editor_check_inotify: invalid fd");
		}	

		return;
	}

	char buffer[EVENT_BUF_LEN];

	ssize_t length = read(editor->inotify_fd, buffer, EVENT_BUF_LEN);

	if (length < 0) {
		if (editor->debug_mode) {
			log_write(&editor->log, "editor_check_inotify: length < 0");
		}	

		return;
	}

	char* ptr = buffer;

	char filename[PATH_MAX_LENGTH];

	while (ptr < buffer + length) {
		struct inotify_event* event = (struct inotify_event*) ptr;

		if (event->mask & IN_IGNORED) {
			if (editor->debug_mode) {
				log_write(&editor->log, 
					"editor_check_inotify: ignored");
			}

			return;
		}

		if (event->len == 0) {
			if (editor->debug_mode) {
				log_write(&editor->log, 
					"editor_check_inotify: event->len == 0");
			}	

			return;
		}

		EditorFile* ef = find_file_by_watch(
			editor, 
			event->wd);

		if (!ef) {
			if (editor->debug_mode) {
				log_write(&editor->log, 
					"editor_check_inotify: no corresponding ef");
			}

			return;
		}

		get_filename_after_last_slash(filename, ef->file.filename);

		if (strcmp(event->name, filename) != 0) {
			ptr += sizeof(struct inotify_event) + event->len;

			if (editor->debug_mode) {
				log_write(&editor->log, 
					"editor_check_inotify: different file %s",
					event->name);
			}

			continue;
		}

		if (event->mask & IN_MOVED_TO) {
			if (editor->debug_mode) {
				log_write(&editor->log, "editor_check_inotify: moved_to");
			}	

			handle_file_moved_into(ef);
		}

		if (event->mask & IN_CLOSE_WRITE) {
			if (editor->debug_mode) {
				log_write(&editor->log, "editor_check_inotify: close_write");
			}	

			handle_file_modified(ef);
		}

		if (event->mask & IN_MODIFY) {
			if (editor->debug_mode) {
				log_write(&editor->log, "editor_check_inotify: modify");
			}

			handle_file_modified(ef);		
		}

		if (event->mask & IN_DELETE) {
			if (editor->debug_mode) {
				log_write(&editor->log, "editor_check_inotify: delete");
			}

			handle_file_deleted(ef, editor->inotify_fd);
		}

		if (event->mask & IN_MOVED_FROM) {
			if (editor->debug_mode) {
				log_write(&editor->log, "editor_check_inotify: moved_from");
			}	

			handle_file_moved_from(ef, editor->inotify_fd);
		}

		if (event->mask & IN_ATTRIB) {
			if (editor->debug_mode) {
				log_write(&editor->log, "editor_check_inotify: attrib");
			}	

			handle_file_attrib_changed(ef);
		}

		if (event->mask & IN_CREATE) {
			if (editor->debug_mode) {
				log_write(&editor->log, "editor_check_inotify: create");
			}	

			handle_file_create(ef, editor->inotify_fd);
		}

		ptr += sizeof(struct inotify_event) + event->len;
	}	


	if (editor->debug_mode) {
		log_write(&editor->log, "editor_check_inotify: reached end");
	}	
}


ssize_t editor_has_filename(const Editor* editor, const char* filename) {
	for (size_t i = 0; i < editor->files.size; i++) {
		const EditorFile* ef = vector_get_const(&editor->files, i);

		if (ef->type == EDITOR_FILE_PROTOTYPE) {
			if (filename && 
				strcmp(ef->proto_data.options.filename, filename) == 0)
			{
				return i;
			}
		}

		else {
			if (ef->file.filename && filename &&
				strcmp(ef->file.filename, filename) == 0)
			{
				return i;
			}
		}


		if (ef->type == EDITOR_FILE_LOADED && 
			!ef->file.filename && !filename) 
		{
			return i;
		}
	}

	return -1;
}


static int editor_load_config(Editor* editor) {
	struct config config;
	config_default(&config);

	int config_ret = config_load(&config);

	if (config_ret == -2) {
		if (handle_config_error(&config) < 0) {
			vector_free(&config.language_plugins);
			return -1;
		}
	}

	else if (config_ret == 0) {
		if (check_keybinds(&config) < 0) {
			vector_free(&config.language_plugins);
			return -1;
		}
	}

	editor->config = config;

	if (editor->debug_mode) {
		log_write(&editor->log, "editor_load_config: SUCCESS");
	}

	return 0;
}

void editor_log_write(const Editor* editor) {
	if (editor->log.fd < 0 || !editor->result.reason) {
		return;
	}

	log_write(&editor->log, editor->result.reason);
}

void editor_create_syntax(Editor* editor) {
	if (!editor->actual_file->lexer || 
		editor->actual_file->tokenized) 
	{
		return;
	}

	file_create_tokens(
		&editor->actual_file->file, 
		editor->actual_file->lexer);

	editor->actual_file->tokenized = 1;

	if (editor->debug_mode) {
		log_write(&editor->log, "editor_create_syntax: SUCCESS");
	}
}

void editor_update_syntax(Editor* editor) {
	if (!editor->actual_file->lexer ||
		!editor->actual_file->tokenized) 
	{
		return;
	}

	int has_dirty = editor->actual_file->file.has_dirty_line;

	if (has_dirty) {
		log_write(&editor->log, "\neditor_update_syntax: STATES BEFORE:\n");

		for (size_t i = 0; 
			i < file_num_lines(&editor->actual_file->file); 
			i++) 
		{
			const Line* line = file_get_line(&editor->actual_file->file, i);

			log_write(&editor->log, "line %zu: in = %s, out = %s, dirty = %d",
				i + 1,
				(line->state_in == LEX_STATE_NORMAL) ? "normal" : "special",
				(line->state_out == LEX_STATE_NORMAL) ? "normal" : "special",
				line->dirty);
		}
	}

	ssize_t start = -1, end = -1;

	file_recalculate_tokens(
		&editor->actual_file->file, 
		editor->actual_file->lexer,
		&start,
		&end);

	if (has_dirty) {
		log_write(&editor->log, "\neditor_update_syntax: STATES AFTER:\n");

		for (size_t i = 0; 
			i < file_num_lines(&editor->actual_file->file); 
			i++) 
		{
			const Line* line = file_get_line(&editor->actual_file->file, i);

			log_write(&editor->log, "line %zu: in = %s, out = %s, dirty = %d",
				i + 1,
				(line->state_in == LEX_STATE_NORMAL) ? "normal" : "special",
				(line->state_out == LEX_STATE_NORMAL) ? "normal" : "special",
				line->dirty);
		}

		log_write(&editor->log, "started at %zd, stopped at %zd",
			(start < 0)?start:start+1, (end < 0)?end:end+1);

		log_write(&editor->log, "");
	}
}

size_t get_gutter_width(size_t line_count) {
	int digits = 1;
	size_t tmp = line_count;

	while (tmp >= 10) {
		tmp /= 10;
		digits++;
	}
	
	return digits + 1;	
}

void editor_prompt_init(Editor* editor) {
	if (editor->actual_file->file.filename) {
		prompt_init(
			&editor->status_bar, 
			editor->actual_file->file.filename, 
			PT_INFO);
	}

	else {
		prompt_init(&editor->status_bar, "untitled", PT_INFO);
	}

	if (editor->actual_file->internal) {
		editor->status_bar.invert_color = 1;
	}

	if (editor->debug_mode) {
		log_write(&editor->log, "editor_prompt_init: SUCCESS");
	}
}

size_t editor_cursor_screen_x(Editor* editor) {
	const u32string* line = file_get_line_text(
		&editor->actual_file->file,
		editor->cursor.pos.y
	);

	return file_to_screen_x(
		line,
		editor->cursor.pos.x,
		editor->config.tab_size
	);
}

void editor_sync_cursor(Editor* editor) {
	const u32string* line = file_get_line_text(
		&editor->actual_file->file,
		editor->cursor.pos.y
	);

	editor->cursor.preferred_column = 
		file_to_screen_x(
			line,
			editor->cursor.pos.x,
			editor->config.tab_size);

	if (editor->debug_mode) {
		log_write(&editor->log, "editor_sync_cursor: SUCCESS");
	}
}

void editor_clamp_cursor_to_view(Editor* editor) {
	const u32string* line = file_get_line_text(
		&editor->actual_file->file,
		editor->cursor.pos.y
	);

	size_t cursor_screen_x = file_to_screen_x(
		line,
		editor->cursor.pos.x,
		editor->config.tab_size
	);

	size_t left = editor->view.col_offset;
	size_t right = left + editor->tsize.cols - 1;

	if (cursor_screen_x < left) {
		editor->cursor.pos.x = screen_to_file_x(
			line,
			left,
			editor->config.tab_size
		);

		editor_sync_cursor(editor);
	}

	else if (cursor_screen_x > right) {
		editor->cursor.pos.x = screen_to_file_x(
			line,
			right,
			editor->config.tab_size
		);

		editor_sync_cursor(editor);
	}
}

const struct language_plugin* editor_get_language(Editor* editor) {
	return editor->actual_file->language;
}

const struct lexer* editor_get_lexer(Editor* editor) {
	return editor->actual_file->lexer;
}

const struct language_rules* editor_get_rules(Editor* editor) {
	if (!editor->actual_file->language) {
		return NULL;
	}

	return editor->actual_file->language->rules;
}

IsWordChar editor_get_IsWordChar(Editor* editor) {
	if (!editor_get_rules(editor)) {
		return is_utf_word_char;
	}

	if (!editor->actual_file->language->rules->is_word_char) {
		return is_utf_word_char;
	}

	else {
		return editor->actual_file->language->rules->is_word_char;
	}
}

void editor_update_word_frequency
(
	Editor* editor,
	const uint32_t* word,
	size_t size,
	ssize_t increment
)
{
	if (size == 0 || !editor->actual_file->words.root) {
		return;
	}

	if (increment > 0) {
		trie_ninsert(
			&editor->actual_file->words, 
			word, 
			size,
			increment
		);
	}

	else if (increment < 0) {
		increment *= -1;
		trie_nremove(
			&editor->actual_file->words, 
			word, 
			size,
			increment
		);
	}

	if (increment != 0) {
		if (editor->debug_mode) {
			log_write(&editor->log, "(update_frequency) words count: %zu", 
				trie_count(&editor->actual_file->words));
		}		
	}
}


/*
I thought a lot about how to insert and remove words from the
trie and discovered that if, before any insert/delete operation,
you decrement the entire line and then increment it after the op, the
word frequencies become correct.

EX:

line before: "asd bsd csd"

asd -= 1, bsd -= 1, csd -=1

line after: "as d bsd csd"

as += 1,
d += 1,
bsd += 1 (not changed)
csd +=1 (not changed)


This also works when you change the frequency using specific functions:

line before: "asd bsd csd"

asd -= 1, bsd -= 1, csd -=1

line after (replace "csd"): "asd bsd ksd "

ksd += 1
asd += 1 (not changed)
bsd +=1 (not changed)

if you add a non-word char:

line before: "asd bsd csd"

ksd -= 1
asd -= 1
bsd -=1

line after: "asd bsd ksd "

ksd += 1 (not changed)
asd += 1 (not changed)
bsd +=1  (not changed)



Of course, that trick comes at a cost, more
specific, O(line_size), though it's not a major
issue unless you're dealing with very long lines
(which generally isn't the case during normal editor usage)


FIXME:
Nonetheless 😞, there is a case where a word should be inserted/removed
into the trie but isn't. That happens exactly when the user types
a word but does not trigger any of the internal program mechanisms
used to insert/delete a word


EXAMPLE 1:

line before: "asd|"

*user types manually "bsd"*

line after: "asd bsd|"

*user moves the cursor up*

result: "bsd" will not enter the trie, because moving the cursor is
not a trigger

EXAMPLE 2:

line before: "asd |"

*user types manually "bsd" inside the last word*

line after: "absd|sd "

*user moves the cursor up*

result: "absdsd" will not enter the trie and "asd" will
not be removed
*/

void editor_increment_line_freq(Editor* editor, size_t y) {
	const u32string* text = file_get_line_text_const(
		&editor->actual_file->file,
		y
	);

	size_t size = u32string_size(text);

	if (size == 0) {
		return;
	}

	IsWordChar is_word = editor_get_IsWordChar(editor);

	size_t x = 0;

	while (x < size) {
		size_t start = x;

		while (x < size && is_word(u32string_char(text, x))) {
			x++;
		}

		const uint32_t* word = 
			u32string_into_ptr_const(text) + start;

		editor_update_word_frequency(
			editor,
			word,
			x - start,
			1
		);

		x++;
	}
}

void editor_decrement_line_freq(Editor* editor, size_t y) {
	const u32string* text = file_get_line_text_const(
		&editor->actual_file->file,
		y
	);

	size_t size = u32string_size(text);

	if (size == 0) {
		return;
	}

	IsWordChar is_word = editor_get_IsWordChar(editor);

	size_t x = 0;

	while (x < size) {
		size_t start = x;

		while (x < size && is_word(u32string_char(text, x))) {
			x++;
		}

		const uint32_t* word = 
			u32string_into_ptr_const(text) + start;

		editor_update_word_frequency(
			editor,
			word,
			x - start,
			-1
		);

		x++;
	}
}