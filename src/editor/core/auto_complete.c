#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <dirent.h>
#include <sys/stat.h>
#include <assert.h>
#include <ctype.h>
#include <math.h>

#include "editor/core/cmd.h" 
#include "editor/core/auto_complete.h" 
#include "editor/core/action.h"
#include "util/files.h"
#include "util/types/hash.h"

#define MATCHES_CAPACITY 128
#define LOC_FACTOR 0.25
#define LOC_DECAI 20
#define LOC_PROP 0.5

#define LOCALITY(dx, dy) (exp(-(dy + LOC_FACTOR * dx  / (1 + dy)) / LOC_DECAI))
#define SCORE(f, l) ((1 - LOC_PROP) * log(1 + f) + LOC_PROP * l)

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
	
	if (last_slash) {
		if (last_slash - u32string_into_ptr_const(string) == 0 &&
			u32string_char(string, 0) == U'/') 
		{
			*dirname = strdup("/");
		}
		
		else {
			*dirname = u32_to_utf8(
				u32string_into_ptr_const(string),
				last_slash - u32string_into_ptr_const(string)
			);
		}
	}
	
	else {
		*dirname = strdup(".");
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
				if (strcmp(dirname, "/") != 0) {
					u32string_appendu8(&name, dirname);
				}
				
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
	vector_init(&candidates, sizeof(u32string), u32string_destructor);

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

	float sw = 0.4;
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
	double locality;
} WordFreq;

typedef struct {
	size_t frequency;
	double locality;
} Score;

static int word_cmp(const void* a, const void* b) {
	const WordFreq* wa = a;
	const WordFreq* wb = b;

	double score_a = SCORE(wa->frequency, wa->locality);
	double score_b = SCORE(wb->frequency, wb->locality);

	if (score_a == score_b) {
		size_t a_size = u32string_size(&wa->string);
		size_t b_size = u32string_size(&wb->string);

		return (a_size > b_size) ? -1 : 1;
	}

	else {
		return (score_a > score_b) ? -1 : 1;
	}
}

static void autocomp_word
(
	Editor* editor,
	u32string* text,
	const uint32_t* suffix,
	size_t suffix_size
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

	// if (editor->config.use_autocomplete) {
	// 	editor_update_word_frequency(
	// 		editor,
	// 		u32string_into_ptr_const(text) + x - word_size,
	// 		word_size,
	// 		1
	// 	);
	// }

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
		suffix_size
	);

	return WINDOW_CLOSE;
}


static int hash_increment_or_insert
(
	HashTable* ht,
	u32string* key,
	double locality
)
{
	Score* score = hash_table_get(ht, key);

	if (score) {
		(score->frequency)++;

		if (locality < score->locality) {
			score->locality = locality;
		}

		return 1;
	}

	else {
		Score score = {
			.frequency = 1,
			.locality = locality
		};

		hash_table_insert(ht, key, &score);

		return 0;
	}
}

static HashTable find_prefixed_words
(
	const Editor* editor,
	const uint32_t* prefix,
	size_t prefix_size
) 
{
	size_t lines = file_num_lines(&editor->actual_file->file);

	// pattern = "tre"
	u32string pattern = u32string_from_raw_copy(
		prefix,
		prefix_size
	);

	IsWordChar is_word = editor_get_IsWordChar(editor);;

	HashTable words = hash_table_new(
		sizeof(u32string),			// key
		u32string_destructor,

		sizeof(Score),				// value
		NULL,

		hash_u32string,
		equals_u32string
	);

	Position cursor_pos = editor->cursor.pos;

	for (size_t i = 0; i < lines; i++) {
		const u32string* text = file_get_line_text(
			&editor->actual_file->file,
			i
		);

		size_t size = u32string_size(text);
		ssize_t index;
		size_t pos = 0;

		while ((index = u32string_find(text, pos, size, &pattern)) >= 0) {
			if (words.size == MATCHES_CAPACITY) {
				break;
			}

			// found: treant
			//           x
			size_t x = index + prefix_size;

			while (x < size && is_word(u32string_char(text, x))) {
				x++;
			}

			// found: abctre
			//           i x
			if (index > 0 && is_word(u32string_char(text, index - 1))) {
				pos = index + x;
				continue;
			}

			// found the prefix
			if (i == cursor_pos.y && 
				(size_t) index + prefix_size == cursor_pos.x) 
			{
				pos = index + x;
				continue;
			}

			// found: treant
			//             x

			u32string word = u32string_slice(text, index, x);

			double dy = labs((ssize_t) cursor_pos.y - (ssize_t) i);
			double dx = labs((ssize_t) cursor_pos.x - (ssize_t) index);

			double locality = LOCALITY(dx, dy);

			// incremented, then free word
			if (hash_increment_or_insert(&words, &word, locality) == 1) {
				u32string_free(&word);
			}

			pos = index + x;
		}
	}

	u32string_free(&pattern);

	return words;
}

// static Vector* get_contained
// (
// 	const HashTable* ht,
// 	const u32string* u32prefix
// )
// {
// 	size_t prefix_size = u32string_size(u32prefix);

// 	size_t max_size = 0;
// 	ssize_t max_prefix_index = -1;

// 	for (size_t i = 0; i < ht->capacity; i++) {
// 		HashEntry* entry = &ht->entries[i];

// 		if (entry->state != HASH_ENTRY_OCCUPIED) {
// 			continue;
// 		}

// 		const u32string* string = entry->key;
// 		size_t size = u32string_size(string);

// 		if (prefix_size < size) {
// 			continue;
// 		}

// 		if (u32string_equaln(string, u32prefix, size)) {
// 			if (size > max_size) {
// 				max_size = size;
// 				max_prefix_index = i;
// 			}

// 			return (Vector*) entry->value;
// 		}
// 	}

// 	if (max_prefix_index >= 0) {
// 		HashEntry* entry = &ht->entries[max_prefix_index];

// 		return (Vector*) entry->value;
// 	}

// 	return NULL;
// }

// static Vector keep_only_prefixed
// (
// 	const Vector* v, 
// 	const u32string* u32prefix
// )
// {
// 	Vector only_prefixed;
// 	vector_init(&only_prefixed, v->elem_size, v->destroy);

// 	size_t prefix_size = u32string_size(u32prefix);

// 	for (size_t i = 0; i < v->size; i++) {
// 		const u32string* string = vector_get_const(v, i);

// 		if (u32string_size(string) < prefix_size) {
// 			continue;
// 		}

// 		if (u32string_equaln(string, u32prefix, prefix_size)) {
// 			u32string cloned = u32string_clone(string);

// 			vector_push(&only_prefixed, &cloned);
// 		}
// 	}

// 	return only_prefixed;
// }

static Vector get_ordened_words
(
	const Editor* editor,
	const uint32_t* prefix,
	size_t prefix_size
)
{
	Vector strings;
	vector_init(&strings, sizeof(u32string), u32string_destructor);


	/// --- FIND WORDS AND SET CLONER ---
	HashTable prefixed_words = find_prefixed_words(
		editor,
		prefix,
		prefix_size
	);

	Vector words;
	vector_init(&words, sizeof(WordFreq), NULL);

	for (size_t i = 0; i < prefixed_words.capacity; i++) {
		HashEntry* entry = &prefixed_words.entries[i];

		if (entry->state != HASH_ENTRY_OCCUPIED) {
			continue;
		}

		u32string* string = entry->key;
		size_t size = u32string_size(string);

		WordFreq word;

		u32string word_string = u32string_from_raw(
			u32string_into_ptr(string),
			size
		);

		word.string = word_string;

		Score* score = entry->value;

		word.frequency = score->frequency;
		word.locality = score->locality;

		vector_push(&words, &word);
		
		free(entry->key);
		free(entry->value);
	}

	free(prefixed_words.entries);

	qsort(
		words.data,
		words.size,
		words.elem_size,
		word_cmp
	);

	for (size_t i = 0; i < words.size; i++) {
		// WordFreq* w = vector_get(&words, i);

		// log_write(&editor->log, "%f, %f, %zu",
		// 	SCORE(w->frequency, w->locality),
		// 	w->locality,
		// 	w->frequency);

		u32string* string = vector_get(&words, i);
		vector_push(&strings, string);
	}

	vector_free(&words);

	return strings;
}

int editor_autocomp_word(Editor* editor) {
	if (editor->cursor.pos.x == 0) {
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

	Vector candidates = get_ordened_words(
		editor,
		prefix, 
		prefix_size
	);

	if (candidates.size == 0) {
		vector_free(&candidates);

		return -1;
	}

	else if (candidates.size == 1) {
		u32string* text_mut = (u32string*) text;
		text = NULL;

		WordFreq* wf = vector_get(
			&candidates,
			0
		);

		u32string word = wf->string;

		uint32_t suffix_size = u32string_size(&word) - prefix_size;

		autocomp_word(
			editor,
			text_mut,
			u32string_into_ptr_const(&word) + prefix_size,
			suffix_size
		);

		u32string_free(&wf->string);
		vector_free(&candidates);

		return 0;
	}

	float sw = 0.4;
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

	editor->window.content = candidates;

	return 0;
}