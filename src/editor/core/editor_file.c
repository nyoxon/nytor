#include <sys/inotify.h>
#include <sys/stat.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <fcntl.h>
#include <assert.h>
#include <ctype.h>

#include "editor/core/editor_file.h"
#include "editor/core/history.h"
#include "util/debug/error.h"
#include "util/types/vector.h"
#include "util/files.h"

static ssize_t check_has_extension
(
	const char* filename,
	const Vector* lang_plugins_data
);

static ssize_t check_has_reserved_filename
(
	const char* filename,
	const Vector* lang_plugins_data
);

EditorFile editor_file_prototype(const EditorFileOptions* options) {
	assert(options != NULL);

	EditorFile ef;

	ef.type = EDITOR_FILE_PROTOTYPE;
	ef.proto_data = (EditorFilePrototypeData) { *options };

	return ef;
}

int editor_file_open
(
	EditorFile* ef, // a prototype
	const Vector* lang_plugins_data,
	Result* result
) 
{
	// you must check the type before calling this function
	assert(ef->type == EDITOR_FILE_PROTOTYPE);

	EditorFileOptions options = ef->proto_data.options;

	const char* filename = options.filename;
	size_t tab_size = options.default_tab_size;
	int use_spaces = options.default_use_spaces;
	int readonly = options.readonly;
	int inotify_fd = options.inotify_fd;
	int use_autocomplete = options.use_autocomplete;

	ef->lexer = NULL;
	ef->language = NULL;
	ef->tokenized = 0;



	/// --- LOAD PLUGINS ---
	editor_file_set_lang_plugin(
		ef,
		filename,
		lang_plugins_data);



	/// --- AUTOCOMPLETE ---
	Trie* trie = NULL;

	if (use_autocomplete && !readonly) {
		trie = &ef->words;

		trie->root = trie_node_create();
	}

	else {
		ef->words.root = NULL;
	}

	IsWordChar is_word_char = is_utf_word_char;

	if (use_autocomplete &&
		ef->language && 
		ef->language->rules &&
		ef->language->rules->is_word_char)
	{
		is_word_char = ef->language->rules->is_word_char;
	}



	/// --- FILE_OPEN ---
	FileOptions foptions = {
		.filename = filename,
		.use_spaces = use_spaces,
		.tab_size = tab_size,
		.trie = trie,
		.is_word_char = is_word_char
	};

	if (file_open(
		&ef->file,
		&foptions,
		result) < 0)
	{
		return -1;
	}



	/// --- INOTIFY ---
	if (filename != NULL) {
		editor_file_set_watcher(
			ef, 
			filename, 
			inotify_fd);

		int exists = file_exists(filename);

		if (exists) {
			editor_file_update_metadata(ef);

			ef->new_file = 0;	

			ef->internal = 0;	
		}

		else {
			ef->internal = 1;
			ef->new_file = 1;
			ef->metadata = (FileMetadata) {0};
		}
	}

	else {
		ef->internal = 1;
		ef->new_file = 1;
		ef->watch_descriptor = -1;
		ef->metadata = (FileMetadata) {0};
	}



	/// --- UNDO/REDO STACKS ---
	if (filename && !ef->file.dirty) {
		int fd = open(filename, O_RDWR);
		size_t capacity;

		if (fd == -1) {
			ef->readonly = 1;
			capacity = 0;
		}

		else {
			ef->readonly = readonly;

			if (readonly) {
				capacity = 0;
			}

			else {
				capacity = HISTORY_CAPACITY;
			}
		}

		stack_init(
			&ef->undo,
			capacity,
			sizeof(Operation), 
			vector_operation_destroy);

		stack_init(
			&ef->redo, 
			capacity,
			sizeof(Operation), 
			vector_operation_destroy);
	}

	else {
		ef->readonly = readonly;
		size_t capacity = (ef->readonly)
			? 0
			: HISTORY_CAPACITY;

		stack_init(
			&ef->undo,
			capacity,
			sizeof(Operation), 
			vector_operation_destroy);

		stack_init(
			&ef->redo, 
			capacity,
			sizeof(Operation), 
			vector_operation_destroy);		
	}


	/// --- OTHER INIT ---
	ef->type = EDITOR_FILE_LOADED;

	ef->cursor = (Cursor) { POS_ZERO, 0 };
	ef->view = (View) { 0, 0 };

	ef->replace_pattern = u32string_new();
	ef->replace_text = u32string_new();
	ef->replace = 0;

	ef->externally_modified = 0;
	ef->externally_deleted = 0;
	ef->externally_moved_from = 0;
	ef->externally_attrib_changed = 0;

	return 0;
}

void editor_file_free(EditorFile* ef) {
	if (!ef || ef->type == EDITOR_FILE_PROTOTYPE) {
		return;
	}

	file_free(&ef->file);
	stack_free(&ef->undo);
	stack_free(&ef->redo);

	if (ef->replace) {
		u32string_free(&ef->replace_text);
		u32string_free(&ef->replace_pattern);
	}

	trie_free(&ef->words);
}

static ssize_t check_has_extension
(
	const char* filename,
	const Vector* lang_plugins_data
)
{
	for (size_t i = 0; i < lang_plugins_data->size; i++) {
		const LangPluginData* data = vector_get_const(
			lang_plugins_data, 
			i);

		for (size_t j = 0; j < data->language->extension_count; j++) {
			if (has_extension(filename, data->language->extensions[j])) {
				return i;
			}
		}
	}

	return -1;
}

static ssize_t check_has_reserved_filename
(
	const char* filename,
	const Vector* lang_plugins_data
)
{
	for (size_t i = 0; i < lang_plugins_data->size; i++) {
		const LangPluginData* data = vector_get_const(
			lang_plugins_data, 
			i);

		if (!data->language->reserved_filenames ||
			data->language->reserved_filename_count == 0)
		{
			continue;
		}

		for (size_t j = 0; j < data->language->reserved_filename_count; j++) {
			if (strcmp(filename, data->language->reserved_filenames[j]) == 0) {
				return i;
			}
		}
	}

	return -1;
}

void editor_file_set_lang_plugin
(
	EditorFile* ef,
	const char* filename,
	const Vector* lang_plugins_data
)
{
	if (filename && lang_plugins_data) {
		ssize_t index = check_has_extension(
			filename,
			lang_plugins_data);


		if (index >= 0) {
			const LangPluginData* data = vector_get_const(
				lang_plugins_data, 
				index);

			ef->lexer = data->lexer;
			ef->language = data->language;

			file_set_has_dirty_line(&ef->file);

			return;
		}

		index = check_has_reserved_filename(
			filename,
			lang_plugins_data
		);

		if (index >= 0) {
			const LangPluginData* data = vector_get_const(
				lang_plugins_data, 
				index);

			ef->lexer = data->lexer;
			ef->language = data->language;

			file_set_has_dirty_line(&ef->file);

			return;
		}		
	}
}

int editor_file_change_lang
(
	EditorFile* ef,
	const char* lang_name,
	const Vector* lang_plugins_data
)
{
	if (lang_name && lang_plugins_data) {
		for (size_t i = 0; i < lang_plugins_data->size; i++) {
			const LangPluginData* data = vector_get_const(
				lang_plugins_data,
				i
			);

			if (data->language && 
				strcmp(lang_name, data->language->name) == 0) 
			{
				ef->language = data->language;
				ef->lexer = data->lexer;

				file_set_has_dirty_line(&ef->file);
				file_set_all_lines_dirty(&ef->file);

				return 0;
			}
		}
	}

	return -1;
}


void editor_file_set_watcher
(
	EditorFile* ef, 
	const char* filename,
	int inotify_fd
)
{
	// see editor_check_inotify() in src/editor/editor.c
	// for more information

	if (!filename || inotify_fd < 0) {
		return;
	}

	// the watcher is placed on the parent directory because
	// the editor may handle files that do not yet exist,
	// and in that case, it's not possible to place a watcher
	// on a non-existent file.
	char* absolute = absolute_parent_directory(filename);

	ef->watch_descriptor = inotify_add_watch(
		inotify_fd,
		absolute,
		IN_CREATE |			// a file with the same name was created
		IN_MOVED_TO |		// someone did "mv something actual_file"
		IN_MOVED_FROM |		// someone did "mv actual_file somewhere"
		IN_CLOSE_WRITE |	// the file was modified
		IN_DELETE |			// the file was deleted
		IN_ATTRIB |			// the rx permissions were changed
		IN_MODIFY
	);

	free(absolute);
}

void editor_file_del_watcher(EditorFile* ef, int inotify_fd) {
	if (ef->watch_descriptor < 0 || inotify_fd < 0) {
		return;
	}

	inotify_rm_watch(
		inotify_fd,
		ef->watch_descriptor
	);

	ef->watch_descriptor = -1; // important
}

void editor_file_update_metadata(EditorFile* ef) {
	struct stat st;

	if (stat(ef->file.filename, &st) == 0) {
		ef->metadata.inode = st.st_ino;
		ef->metadata.mtime = st.st_mtim;
		ef->metadata.size = st.st_size;
	}
}

void handle_file_modified(EditorFile* ef) {
	struct stat st;

	if (stat(ef->file.filename, &st) < 0) {
		ef->externally_modified = 1;
		return;
	}

	if (st.st_mtim.tv_sec != ef->metadata.mtime.tv_sec ||
		st.st_size != ef->metadata.size ||
		st.st_ino != ef->metadata.inode)
	{
		ef->externally_modified = 1;
	}
}

void handle_file_deleted(EditorFile* ef, int inotify_fd) {
	if (ef->readonly) {
		return;
	}

	// dunno if removing the watcher here is the best approach

	// given that a user, upon realizing the file has been
	// deleted, might keep an internal file with the same name,
	// it might be worth keeping the watcher alive and only
	// deleting it if the user decides to close the file
	editor_file_del_watcher(ef, inotify_fd);

	ef->externally_deleted = 1;
}

void handle_file_moved_from(EditorFile* ef, int inotify_fd) {
	if (ef->readonly) {
		return;
	}

	// dunno if removing the watcher here is the best approach

	// given that a user, upon realizing the file has been
	// moved, might keep an internal file with the same name,
	// it might be worth keeping the watcher alive and only
	// deleting it if the user decides to close the file
	editor_file_del_watcher(ef, inotify_fd);

	ef->externally_moved_from = 1;
}

void handle_file_moved_into(EditorFile* ef) {
	if (ef->readonly) {
		return;
	}

	ef->externally_modified = 1;
}

void handle_file_attrib_changed(EditorFile* ef) {
	if (ef->readonly) {
		int fd = open(ef->file.filename, O_RDONLY);

		if (fd < 0) {
			ef->externally_attrib_changed = 1;
		}

		close(fd);
	}

	else {
		int fd = open(ef->file.filename, O_RDONLY);

		if (fd < 0) {
			ef->readonly = 1;
			ef->externally_attrib_changed = 1;

			return;
		}

		close(fd);

		fd = open(ef->file.filename, O_WRONLY);

		if (fd < 0) {
			ef->externally_attrib_changed = 1;
		}

		close(fd);
	}
}

void handle_file_create(EditorFile* ef, int inotify_fd) {
	if (!ef->new_file) {
		return;
	}

	ef->externally_modified = 1;

	if (ef->watch_descriptor < 0) {
		editor_file_set_watcher(ef, ef->file.filename, inotify_fd);		
	}
}

void editor_file_sync
(
	EditorFile* ef,
	int use_autocomplete,
	Result* result
) 
{
	ef->new_file = 0;

	ef->cursor = (Cursor) { POS_ZERO, 0 };

	stack_clear(&ef->undo);
	stack_clear(&ef->redo);

	if (use_autocomplete && !ef->readonly) {
		trie_free(&ef->words);
		ef->words.root = trie_node_create();
	}

	if (ef->replace) {
		u32string_free(&ef->replace_text);
		u32string_free(&ef->replace_pattern);
	}

	IsWordChar is_word_char = 
		(ef->language && 
			ef->language->rules && 
			ef->language->rules->is_word_char)
				? ef->language->rules->is_word_char
				: is_utf_word_char;

	Trie* trie = (use_autocomplete && !ef->readonly)
		? &ef->words
		: NULL;

	file_sync(
		&ef->file, 
		trie,
		is_word_char, 
		result);

	editor_file_update_metadata(ef);

	if (ef->lexer) {
		file_create_tokens(&ef->file, ef->lexer);
	}

	ef->externally_modified = 0;
	ef->externally_moved_from = 0;
	ef->externally_deleted = 0;
	ef->externally_attrib_changed = 0;

	ef->new_file = 0;
	ef->internal = 0;
}

int editor_file_changed(const EditorFile* ef) {
	return ef->externally_deleted ||
		   ef->externally_moved_from ||
		   ef->externally_modified ||
		   ef->externally_attrib_changed;
}

void editor_file_set_readonly(EditorFile* ef) {
	if (ef->undo.capacity > 0) {
		stack_free(&ef->undo); stack_free(&ef->redo);

		stack_init(
			&ef->undo,
			0,
			sizeof(Operation),
			vector_operation_destroy
		);

		stack_init(
			&ef->redo,
			0,
			sizeof(Operation),
			vector_operation_destroy
		);
	}

	ef->readonly = 1;
}

int editor_file_set_writeable(EditorFile* ef) {
	const char* filename = ef->file.filename;

	// i don't know what the use would be, but it is
	// allowed.
	if (!filename) {
		ef->readonly = 0;
		goto end;
	}

	// EAFP
	else {
		int fd = open(filename, O_WRONLY);

		if (fd < 0) {
			return -1;
		}

		else {
			ef->readonly = 0;

			close(fd);
			goto end;
		}
	}

end:
	if (ef->undo.capacity == 0) {
		stack_free(&ef->undo); stack_free(&ef->redo);

		stack_init(
			&ef->undo,
			HISTORY_CAPACITY,
			sizeof(Operation),
			vector_operation_destroy
		);

		stack_init(
			&ef->redo,
			HISTORY_CAPACITY,
			sizeof(Operation),
			vector_operation_destroy
		);			
	}

	return 0;
}


/// --- AUTOCOMP TRIE ---
void editor_file_update_word_frequency
(
	EditorFile* ef,
	const uint32_t* word,
	size_t size,
	int increment
)
{
	if (size == 0 || !ef->words.root) {
		return;
	}

	if (increment > 0) {
		trie_insert(&ef->words, word, size);
	}

	else if (increment < 0) {
		trie_remove(&ef->words, word, size);
	}
}

