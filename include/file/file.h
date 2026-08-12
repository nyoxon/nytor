#ifndef FILE_H
#define FILE_H

// to read and handle from files

// the idea is to use any signature so that it is not so 
// complicated or that it is not necessary to 
// change the editor/editor.h functions if the storage logic changes

#include "file/line.h"
#include "util/types/vector.h"
#include "util/debug/error.h"
#include "util/types/u32string.h"
#include "util/types/position.h"
#include "util/types/cursor.h"
#include "util/types/clipboard.h"
#include "util/types/selection.h"
#include "plugins/plugin.h"
#include "plugins/language_syntax.h"

#define MAX_LINE_TOKENS 256

/// --- DEFINITION OF A FILE ---
// File must be a logical representation of the file itself.
// In this implementation, it contains a vector of Lines,
// but a better implementation should simply have a way
// to define what constitutes a line in the file.


/// --- File x EditorFile ---
// The difference between a File and a EditorFile is that the latter
// possesses certain flags and metadata that the former, on its own,
// does not need to know about.


/// --- TYPES OF FILES ---
// In total, there are three types of File that can exist
// withing an Editor. What distinguishes them are the
// 'filename' (in File struct) and 'new_file' (in EditorFile struct)
// fields (see editor/core/editor_file.h).
//
// 1º type: untitled and new file
// It is an untitled (filename = NULL) and new (new_file = 1) file.
// - The file does not exist on the user's system until it is saved.
// - file.dirty = 1 by default.
// - The program will ask for a name whenever the user attempt to save it.
// - It's always created as writable by default.
// - It will not have an associated language plugin by default.
//
// 2º type: titled and new file
// It is an titled (filename != NULL) and new (new_file = 1) file.
// - The file does not exist on the user's system until it is saved.
// - file.dirty = 1 by default.
// - It's always created as writable by default.
// - The associated language plugin will be the first one to recognize
// the file extension.
//
// 3º type: titled and not new file
// It is an titled (filename != NULL) and not new (new_file = 0) file.
// - The file exists on the user's system.
// - file.dirty = 0 by default.
// - Must be a valid file.
// - The program always attempts to open an existing file with write
// access, but whether or not this actually happens depends on the
// permissions the user has for file on their system.
// - The associated language plugin will be the first one to recognize
// the file extension.


/// --- VALID FILES AND FILE PERMISSIONS ---
// A valid file (fd) for the editor is a file for which the user
// has, at least, read permission; which means that if the user
// loses read permission while the program is running, the file
// will be removed from the editor.

// The program uses the EAFP principle to determine whether or not
// the user can read from the file.

// See editor_handle_attrib_changed() in editor/core/action.c 
// for more informations on how the program handles permissions
// and permission changes.
typedef struct {
	char* filename;
	Vector lines; // Vector of Lines

	int dirty;

	// WARNING ABOUT tab_size: in various parts of the code
	// that handle the logical position of the cursor within
	// the file (not the one drawn on the screen), the following
	// pattern or equivalent is found:
	//	size_t tab_size = (file->use_spaces) ? file->tab_size : 1;

	// Perhaps the name "tab_size" isn't appropriate: "tab_level"
	// or something similar might be better.

	// As i said, this is used only when dealing with the
	// CURSOR INSIDE THE FILE.

	// file->tab_size will ALWAYS represent the number of visual
	// columns (on screen) that a tab should occupy.
	// The confusion arises when use_spaces == 1, because the visual
	// number corresponds exactly to the logical one.

	// For this reason, using the pattern above in any type
	// of operation fundamental to rendering is a serious error.
	size_t tab_size;
	int use_spaces;
} File;

void file_init(File* file);
void file_push(File* file, Line* line);

// creates a File given the filename and tab_size.
// if the file does not exist, the function
// tries to create it for system permission purposes.
// if filename == NULL, creates a empty File.
// non-existing and empty files are dirty by default
int file_open
(
	File* file, 
	const char* filename,
	size_t tab_size,
	int use_spaces,
	Result* result
);



// must be used only if the lexer exists
// iterates through the lines and calculate tokens
// must be used just after file_open
void file_create_tokens(File* file, struct lexer* lexer);


// similar to file_create_tokens, but used
// when the file changes.
// start and end form the range [,) to iterate
void file_recalculate_tokens
(
	File* file,
	struct lexer* lexer
);

void file_sync(File* file);


void file_convert_spaces_to_tabs(File* file);
void file_convert_tabs_to_spaces(File* file);


// saves the file in disk.
// responsible for creating a non-existing file.
int file_save(File* file, Result* result);

// inserts a char in the c->x position of the (c->y)º line
int file_insert_char
(
	File* file, 
	const Position pos, 
	uint32_t c,
	Result* result
);

// deletes a char in the c->x position of the (c->y)º line
int file_delete_char
(
	File* file, 
	const Position pos, 
	Result* result
);


// inserts a newline in the file taking into account
// the content of current line before and after the cursor
int file_insert_newline
(
	File* file, 
	const Position pos, 
	Result* result
);


// move a line up in the archive
int file_move_line_down(File* file, size_t y, Result* result);


// move a line down in the archive
int file_move_line_up(File* file, size_t y, Result* result);


// used to handle the case where the user deletes a char at
// the begginning of a line
int file_merge_lines(File* file, const Position pos, Result* result);


// add tab_size spaces at the beginning of a line
int file_indent_a_line
(
	File* file, 
	size_t y,
	Result* result
);

int file_indent_selection
(
	File* file,
	Selection* sel,
	Result* result
);

// remove tab_size spaces from the beginning of a line
int file_unindent_a_line
(
	File* file, 
	size_t y,
	Result* result
);

int file_unindent_selection
(
	File* file,
	Selection* sel,
	Result* result
);

// add or remove a comment_fmt from the beginning of the line.
// in the program, this function is only called if there is
// a language plugin that defines what a comment should be
int file_comment_line
(
	File* file,
	size_t y,
	const char* comment_fmt,
	ssize_t* move_cursor,
	Result* result
);

int file_comment_selection
(
	File* file,
	Selection* sel,
	const char* comment_fmt,
	size_t cursor_y,
	ssize_t* move_cursor
);


void file_free(File* file);


// delete a selection in a file
int file_delete_selection
(
	File* file, 
	Selection* sel, 
	Result* result
);


// copy a selection to clipboard
int file_copy_selection
(
	File* file, 
	Clipboard* cb, 
	Selection* sel,
	Result* result
);


// paste the clipboard into the file
int file_paste_clipboard
(
	File* file, 
	Clipboard* cb, 
	Cursor* c,
	Result* result
);


// create a selection selecting a line
int file_select_line
(
	File* file, 
	size_t y, 
	Selection* sel,
	Result* result
);


// create a selection selecting all file
int file_select_all_file
(
	File* file, 
	Selection* sel, 
	Result* result
);

u32string* file_get_line_text(File* file, size_t y);
Vector* file_get_line_tokens(File* file, size_t y);

Line* file_get_line
(
	File* file,
	size_t y
);

void file_set_line_dirty(File* file, size_t y);


size_t file_num_lines
(
	const File* file
);


size_t file_size_line
(
	File* file,
	size_t y
);


#endif