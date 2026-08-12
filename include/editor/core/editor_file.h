#ifndef EDITOR_FILE_H
#define EDITOR_FILE_H

#include <sys/types.h>

#include "util/types/stack.h"
#include "util/types/position.h"
#include "util/types/u32string.h"
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

} EditorFileOptions;

typedef struct {
	File file;
	int new_file;
	int tokenized;

	int internal;

	int readonly;

	struct lexer* lexer;
	const struct language_plugin* language;

	Cursor cursor;

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
} EditorFile;


int editor_file_open
(
	EditorFile* ef,
	const EditorFileOptions* options,
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

void editor_file_sync(EditorFile* ef);

int editor_file_changed(const EditorFile* ef);

void editor_file_set_readonly(EditorFile* ef);
int editor_file_set_writeable(EditorFile* ef);

#endif