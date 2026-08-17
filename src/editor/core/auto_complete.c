#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <dirent.h>
#include <sys/stat.h>
#include <assert.h>
#include <ctype.h>

#include "editor/core/cmd.h" 
#include "editor/core/auto_complete.h" 
#include "editor/core/action.h"
#include "util/files.h"

static int u32string_cmp(const void* a, const void* b) {
	const u32string* sa = a;
	const u32string* sb = b;

	int ret = (int) u32string_size(sa) - (int) u32string_size(sb);

	return ret;
}

static void get_cmd_candidates(Vector* candidates, const u32string* string) {
	size_t string_size = u32string_size(string);

	for (size_t i = 0; i < COMMANDS_COUNT; i++) {
		const char* cmd_name = COMMANDS[i].name;

		if (strlen(cmd_name) < string_size) {
			continue;
		}

		if (u32string_equalnu8(string, cmd_name, string_size)) {
			u32string candidate = u32string_from(cmd_name);

			vector_push(candidates, &candidate);
		}
	}
}

static void get_file_and_dirname
(
	const u32string* string,
	char** filename,
	char** dirname
) 
{
	if (!filename || !dirname || u32string_is_empty(string)) {
		return;
	}

	size_t string_size = u32string_size(string);

	uint32_t* last_slash = u32string_rchr(string, U'/');

	*filename = u32_to_utf8(
		(last_slash == NULL)
			? u32string_into_ptr_const(string)
			: last_slash + 1,

		(last_slash == NULL)
			? string_size
			: string_size - (last_slash -
				u32string_into_ptr((u32string*) string)) - 1 
	);

	*dirname = u32_to_utf8(
		u32string_into_ptr_const(string),

		(last_slash == NULL)
			? 0
			: last_slash - u32string_into_ptr((u32string*) string)		
	);

	last_slash = NULL;

	if (strlen(*dirname) == 0) {
		char* tmp = realloc(*dirname, 2);

		if (!tmp) {
			free(*dirname);
			free(*filename);

			*dirname = NULL;
			*filename = NULL;

			return;
		}

		*dirname = tmp;

		(*dirname)[0] = '.';
		(*dirname)[1] = '\0';
	}
}

static void get_file_candidates
(
	Vector* candidates, 
	const u32string* string
) 
{
	char* filename = NULL; char* dirname = NULL;
	get_file_and_dirname(string, &filename, &dirname);

	if (!filename || !dirname) {
		return;
	}

	DIR* dir = opendir(dirname);

	if (!dir) {
		free(filename);
		free(dirname);

		return;
	}

	struct dirent* entry;
	size_t filename_size = strlen(filename);

	while ((entry = readdir(dir)) != NULL) {
		if (strcmp(entry->d_name, ".") == 0 ||
			strcmp(entry->d_name, "..") == 0)
		{
			continue;
		} 

		if (strncmp(filename, entry->d_name, filename_size) == 0) {
			u32string name = u32string_new();

			if (u32string_size(string) > strlen(filename)) {
				u32string_appendu8(&name, dirname);
				u32string_push(&name, U'/');
			}

			u32string_appendu8(&name, entry->d_name);

			vector_push(candidates, &name);
		}
	}

	free(filename);
	free(dirname);

	closedir(dir);	
}

static Vector get_autocomp_candidates
(
	AutoCompType type,
	const u32string* string
) 
{
	Vector candidates;
	vector_init(&candidates, sizeof(u32string), u32string_vector_destroy);

	switch (type) {
	case AUTOCOMP_CMD:
		get_cmd_candidates(&candidates, string);
		break;

	case AUTOCOMP_FILE:
		get_file_candidates(&candidates, string);
		break;

	default:
		break;
	}

	qsort(
		candidates.data, 
		candidates.size, 
		candidates.elem_size, 
		u32string_cmp);

	return candidates;
}

/// --- AUTOCOMP_PROMPT ---

static void autocomp_prompt
(
	Editor* editor,
	const uint32_t* suffix,
	size_t suffix_size,
	size_t prefix_size,
	AutoCompType type
)
{
	u32string* text = &editor->status_bar.buf;

	size_t x = (editor->status_bar.cursor.pos.x == 0)
		? 1
		: editor->status_bar.cursor.pos.x;

	u32string_insert_range_raw(
		text,
		x,
		suffix,
		suffix_size
	);

	uint32_t additional_char[] = {U' '};

	if (type == AUTOCOMP_FILE) {
		char* name = u32_to_utf8(
			u32string_into_ptr_const(text) + x - prefix_size,
			prefix_size + suffix_size
		);

		if (isdir(name)) {
			additional_char[0] = U'/';
		}

		free(name);
	}

	x += suffix_size;

	u32string_insert_range_raw(
		text,
		x,
		additional_char,
		1
	);

	x++;

	editor->status_bar.cursor.pos.x = x;
	prompt_cursor_update(&editor->status_bar);
}

static WindowResult on_select_autocomp_prompt(void* userdata) {
	assert(userdata != NULL);

	Editor* editor = userdata;
	Vector* candidates = &editor->window.content;
	u32string* buf = &editor->status_bar.buf;

	size_t x = editor->status_bar.cursor.pos.x;

	if (x == u32string_size(buf)) {
		x--;
	}

	if (x > 0) {
		x--;
	}

	while (is_utf_word_char(u32string_char(buf, x))) {
		if (x == 0) {
			break;
		}

		x--;
	}

	if (x > 0) {
		x++;
	}

	const u32string* selected = vector_get_const(
		candidates,
		editor->window.cursor.pos.y
	);

	size_t prefix_size = editor->status_bar.cursor.pos.x - x;
	size_t suffix_size = u32string_size(selected) - prefix_size;

	AutoCompType type = (u32string_size(buf) == prefix_size)
		? AUTOCOMP_CMD
		: AUTOCOMP_FILE;

	autocomp_prompt(
		editor,
		u32string_into_ptr_const(selected) + prefix_size,
		suffix_size,
		prefix_size,
		type
	);

	return WINDOW_CLOSE;
}

int editor_autocomp_prompt(Editor* editor) {
	size_t x = editor->status_bar.cursor.pos.x;

	if (x == 0) {
		return -1;
	}

	const u32string* buf = &editor->status_bar.buf;

	if (x == u32string_size(buf)) {
		x--;
	}

	if (!is_utf_word_char(u32string_char(buf, x))) {
		return -1;
	}

	while (is_utf_word_char(u32string_char(buf, x))) {
		if (x == 0) {
			break;
		}

		x--;
	}

	if (x > 0) {
		x++;
	}

	const uint32_t* prefix = u32string_into_ptr_const(buf) + x;
	size_t prefix_size = editor->status_bar.cursor.pos.x - x;

	int search_cmd = (u32string_size(buf) == prefix_size);

	Vector candidates;

	u32string u32prefix = u32string_from_raw_copy(
		prefix,
		prefix_size
	);

	AutoCompType type;

	if (search_cmd) {
		type = AUTOCOMP_CMD;
		candidates = get_autocomp_candidates(type, &u32prefix);
	}

	else {
		type = AUTOCOMP_FILE;

		candidates = get_autocomp_candidates(type, &u32prefix);
	}

	u32string_free(&u32prefix);

	if (candidates.size == 0) {
		vector_free(&candidates);
		return -1;
	}

	else if (candidates.size == 1) {
		const u32string* word = vector_get_const(
			&candidates,
			0
		);

		uint32_t suffix_size = u32string_size(word) - prefix_size;

		autocomp_prompt(
			editor,
			u32string_into_ptr_const(word) + prefix_size,
			suffix_size,
			prefix_size,
			type
		);

		vector_free(&candidates);

		return 0;		
	}

	float sw = 0.2;
	float sh = 0.5;

	Position pos = {
		u32string_size(&editor->status_bar.label) +
			editor->status_bar.cursor.pos.x,

		editor->tsize.rows * ( 1 - sh) - 2
	};

	WindowOptions options = {
		.pos_type = WINDOWPOS_CUSTOM,
		.pos = pos,
		.sw = sw,
		.sh = sh,
		.tab_size = editor->actual_file->file.tab_size,
		.tsize = editor->tsize,
		.on_select = on_select_autocomp_prompt
	};

	editor->window = window_new(&options);
	editor->has_window = 1;

	editor->window.content = candidates;

	return 0;	
}


/// --- AUTOCOMP_WORD ---

typedef struct {
	u32string string;
	size_t frequency;
} WordFreq;

static void collect_word
(
	const uint32_t* word,
	size_t word_size,
	TrieNode* node,
	void* userdata
)
{
	(void) node;

	Vector* candidates = userdata;

	u32string u32word = u32string_from_raw_copy(
		word,
		word_size
	);

	WordFreq w = {
		.string = u32word,
		.frequency = node->frequency
	};

	vector_push(candidates, &w);
}

static int word_cmp(const void* a, const void* b) {
	const WordFreq* wa = a;
	const WordFreq* wb = b;

	int ret = (int) wb->frequency - (int) wa->frequency;

	return ret;
}

static void autocomp_word
(
	Editor* editor,
	u32string* text,
	const uint32_t* suffix,
	size_t suffix_size,
	size_t prefix_size
)
{
	// necessarily greater than 0, otherwise
	// this function would not have been called	
	size_t x = editor->cursor.pos.x;

	Position cursor_remove = (Position) { x, editor->cursor.pos.y };

	u32string_insert_range_raw(
		text,
		x,
		suffix,
		suffix_size
	);

	x += suffix_size;

	size_t word_size = prefix_size + suffix_size;

	if (editor->config.use_autocomplete) {
		editor_update_word_frequency(
			editor,
			u32string_into_ptr_const(text) + x - word_size,
			word_size,
			1
		);
	}

	u32string_insert_range_raw(
		text,
		x,
		U" ",
		1
	);

	x++;
	Position cursor_insert = (Position) { x, editor->cursor.pos.y };

	editor->cursor.pos.x = x;
	editor_cursor_update(editor);

	Position start = cursor_remove;
	Position end = cursor_insert;

	u32string op_text = u32string_slice(
		text,
		start.x,
		end.x
	);

	Operation op = operation_create_insert(
		start, end,
		cursor_remove, cursor_insert,
		op_text
	);

	stack_clear(&editor->actual_file->redo);
	stack_push(&editor->actual_file->undo, &op);

	file_set_line_dirty(&editor->actual_file->file, editor->cursor.pos.y);
	file_set_has_dirty_line(&editor->actual_file->file);
	editor->actual_file->file.dirty = 1;
}

static WindowResult on_select_autocomp(void* userdata) {
	assert(userdata != NULL);

	Editor* editor = userdata;
	Vector* candidates = &editor->window.content;

	u32string* text = file_get_line_text(
		&editor->actual_file->file,
		editor->cursor.pos.y
	);

	size_t cursor_x = editor->cursor.pos.x;

	// necessarily greater than 0, otherwise
	// this function would not have been called
	size_t size = u32string_size(text);

	size_t x = cursor_x;

	if (x == size) {
		x--;
	}
	
	IsWordChar is_word = editor_get_IsWordChar(editor);

	// necessarily in a valid position, otherwise
	// this function would not have been called
	while (x > 0 && is_word(u32string_char(text, x - 1))) {
		x--;
	}

	const u32string* selected = vector_get_const(
		candidates,
		editor->window.cursor.pos.y
	);

	size_t prefix_size = editor->cursor.pos.x - x;
	size_t suffix_size = u32string_size(selected) - prefix_size;

	autocomp_word(
		editor,
		text,
		u32string_into_ptr_const(selected) + prefix_size,
		suffix_size,
		prefix_size
	);

	return WINDOW_CLOSE;
}

int editor_autocomp_word(Editor* editor) {
	if (!editor->actual_file->words.root ||
		editor->cursor.pos.x == 0) 
	{
		return -1;
	}

	const u32string* text = file_get_line_text(
		&editor->actual_file->file,
		editor->cursor.pos.y
	);

	size_t cursor_x = editor->cursor.pos.x;
	size_t size = u32string_size(text);

	if (size == 0) {
		return -1;
	}

	size_t x = cursor_x;

	// asd|
	if (x == size) {
		x--;
	}
	
	IsWordChar is_word = editor_get_IsWordChar(editor);

	// if the cursor is not over a word_char, is it
	// immediately after a word_char?
	if (!is_word(u32string_char(text, x))) {

		// no
		if (x == 0 || !is_word(u32string_char(text, x - 1))) {
			return -1;
		}

		// yes
		x--;
	}

	while (x > 0 && is_word(u32string_char(text, x - 1))) {
		x--;
	}

	size_t word_start = x;
	size_t word_end = cursor_x;

	// now: asd a
	// -----x----


	// "asd"
	// -p--
	const uint32_t* prefix = u32string_into_ptr_const(text) + word_start;
	size_t prefix_size = word_end - word_start;

	Vector words;
	vector_init(&words, sizeof(WordFreq), NULL);

	trie_prefix_search(
		&editor->actual_file->words,
		prefix,
		prefix_size,
		collect_word,
		&words
	);

	if (words.size == 0) {
		vector_free(&words);

		return -1;
	}

	else if (words.size == 1) {
		u32string* text_mut = (u32string*) text;
		text = NULL;

		WordFreq* wf = vector_get(
			&words,
			0
		);

		u32string word = wf->string;

		uint32_t suffix_size = u32string_size(&word) - prefix_size;

		autocomp_word(
			editor,
			text_mut,
			u32string_into_ptr_const(&word) + prefix_size,
			suffix_size,
			prefix_size
		);

		u32string_free(&wf->string);
		vector_free(&words);

		return 0;
	}

	qsort(
		words.data,
		words.size,
		words.elem_size,
		word_cmp
	);

	float sw = 0.2;
	float sh = 0.6;

	size_t height = editor->tsize.rows * sh;

	size_t pos_x = editor->cursor.pos.x;
	size_t pos_y = (editor->cursor.pos.y > editor->tsize.rows / 2)
		? (editor->cursor.pos.y == 0)
			? 0
			// 2 = 1 (window_border) + 1 (editor->cursor.pos.y)
			: (editor->cursor.pos.y > height + 2)
				? editor->cursor.pos.y - height - 2
				: editor->cursor.pos.y - 1
		: editor->cursor.pos.y + 1;

	WindowOptions options = {
		.pos_type = WINDOWPOS_CUSTOM,
		.pos = (Position) { pos_x, pos_y },
		.sw = sw,
		.sh = sh,
		.tab_size = editor->actual_file->file.tab_size,
		.tsize = editor->tsize,
		.on_select = on_select_autocomp
	};

	editor->window = window_new(&options);
	editor->has_window = 1;

	for (size_t i = 0; i < words.size; i++) {
		WordFreq* wf = vector_get(
			&words,
			i
		);

		vector_push(&editor->window.content, &wf->string);
	}

	vector_free(&words);

	return 0;	
}