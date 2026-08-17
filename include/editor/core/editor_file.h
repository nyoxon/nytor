#ifndef EDITOR_FILE_H
#define EDITOR_FILE_H

#include <sys/types.h>

#include "util/types/stack.h"
#include "util/types/position.h"
#include "util/types/u32string.h"
#include "editor/core/scroll.h"
#include "util/types/trie.h"
#include "file/file.h"
#include "plugins/plugin.h"

#define HISTORY_CAPACITY 200

typedef struct {
	ino_t inode;
	struct timespec mtime;
	off_t size;
} FileMetadata;

typedef struct {
	const char* filename;
	int readonly;

	size_t default_tab_size;
	int default_use_spaces;

	int inotify_fd;

	int use_autocomplete;
} EditorFileOptions;

typedef enum {
	EDITOR_FILE_LOADED,

	// A prototype is a editor file whose inner file hasn't been loaded
	// into memory yet.
	// For each filename passed to editor_init(), a corresponding
	// prototype must be created.
	// The file will actually be loaded when it becomes the actual file.
	// For this reason, the filename in the options passed to
	// editor_file_prototype MUST BE A NON-NULL pointer to a string
	// of argv.

	// To create a file when the program is running, you need
	// to call editor_file_prototype AND NECESSARILY CALL
	// editor_file_open() immediately afterwards

	// The filename inside the proto_data->options can only be
	// NULL if you call editor_file_open immediately afterwards
	EDITOR_FILE_PROTOTYPE,
} EditorFileType;

typedef struct {
	EditorFileOptions options;
} EditorFilePrototypeData;

typedef struct {
	File file;
	int new_file;
	int tokenized;	// the file was tokenized at least 1 time

	EditorFileType type;

	// should only be used when type = EDITOR_FILE_PROTOTYPE
	EditorFilePrototypeData proto_data;

	int internal;

	int readonly;

	struct lexer* lexer;
	const struct language_plugin* language;

	Cursor cursor;
	View view;

	Stack undo;
	Stack redo;

	u32string replace_pattern;
	u32string replace_text;
	int replace;

	int watch_descriptor;
	int inotify_fd;
	FileMetadata metadata;

	int externally_modified;
	int externally_deleted;
	int externally_moved_from;
	int externally_attrib_changed;

	Trie words;
} EditorFile;

/// --- File x EditorFile ---
// The difference between a File and a EditorFile is that the latter
// possesses certain flags and metadata that the former, on its own,
// does not need to know about.



// The filename inside the proto_data->options can only be
// NULL if you call editor_file_open immediately afterwards
EditorFile editor_file_prototype(const EditorFileOptions* options);



// To create a file when the program is running, you need
// to call editor_file_prototype AND NECESSARILY CALL
// editor_file_open() immediately afterwards

// The filename inside the proto_data->options can only be
// NULL if you call editor_file_open immediately afterwards
int editor_file_open
(
	EditorFile* ef, // a prototype
	const Vector* lang_plugins_data,
	Result* result
);

void editor_file_free(EditorFile* ef);

void editor_file_set_lang_plugin
(
	EditorFile* ef,
	const char* filename,
	const Vector* lang_plugins_data
);

int editor_file_change_lang
(
	EditorFile* ef,
	const char* lang_name,
	const Vector* lang_plugins_data
);

void editor_file_set_watcher
(
	EditorFile* ef, 
	const char* filename,
	int inotify_fd
);

void editor_file_del_watcher(EditorFile* ef, int inotify_fd);

void editor_file_update_metadata(EditorFile* ef);

void handle_file_modified(EditorFile* ef);
void handle_file_deleted(EditorFile* ef, int inotify_fd);
void handle_file_moved_from(EditorFile* ef, int inotify_fd);
void handle_file_moved_into(EditorFile* ef);
void handle_file_attrib_changed(EditorFile* ef);
void handle_file_create(EditorFile* ef, int inotify_fd);

void editor_file_sync(EditorFile* ef, int use_autocomplete, Result* result);

int editor_file_changed(const EditorFile* ef);

void editor_file_set_readonly(EditorFile* ef);
int editor_file_set_writeable(EditorFile* ef);

void editor_file_update_word_frequency
(
	EditorFile* ef,
	const uint32_t* word,
	size_t size,
	int increment
);

#endif