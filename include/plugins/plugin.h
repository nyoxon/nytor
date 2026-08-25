#ifndef PLUGIN_H
#define PLUGIN_H

#include "plugins/language_syntax.h"
#include "util/types/vector.h"

// IMPORTANT: after changing something that
// the .so files depend on, don't forget
// to rebuild them

// helper
struct plugin_result {
	void* handle;
	int error;
};

struct plugin_result plugin_language_load(const char* name);

void plugin_destroy(void* plugin);


/// --- LANGUAGE PLUGIN API ---

// init function
typedef const struct language_plugin* (*plugin_init_t)(void);


// ex in C: {}, (), "", ''
struct pair {
	char open;
	char close;

	/*
	-- AUTO_INDENT --
	if you press ENTER after a open pair, the
	next line will be auto indented

	ex ('|' represents the cursor):
	
	{|

	press enter:

	{
			|

	if you press ENTER between a pair, the result
	will be:
	
	{
			|
	}

	*/
	int auto_indent;


	/*
	-- AUTO_COMPLETE
	if you type the open pair, the close pair
	will be automatically typed too and the cursor
	will be positioned between them

	ex ('|' represents the cursor)s:

	type " -> "|"

	if the cursor is between a pair with auto_complete and
	you press BACKSPACE, both characters will
	be deleted

	"|" 	->		|	
		 delete
	*/
	int auto_complete;
};


/*

a function that determines what is a "word" in the language.
word is not necessarily, for example, the valid name of a variable,
but could be.

that function will be used in autocomplete, in construction
of the internal Trie that contains the words in the file,
in the ACTION_MOVE_WORD functions and in the
editor_select_word() function.

ex in C: int is_c_word(uint32_t c) { return isalnum(c) || c == U'_'; }

if not defined, the default function will be:

int is_utf_word(uint32_t c) { return u32_is_printable(c) && !isspace(c); }

*/
typedef int (*IsWordChar)(uint32_t c);


// some general rules
struct language_rules {
	const struct pair* pairs;
	size_t pair_count;

	// in C: "//"
	const char* comment_fmt;

	/*
	-- RULES AUTO_INDENT --
	if the cursor is on a line with a certain indentation and
	you insert a newline, the newline will have the same
	indentation as the previous one

	ex ('|' represents the cursor):

	\t\tline|

	insert a newline:

	\t\tline
	\t\t|
	*/
	int auto_indent;

	IsWordChar is_word_char;
};

// a monster
struct language_plugin {
	const char* name;

	// determine whether a file should use this plugin
	const char* const* extensions;
	size_t extension_count;
	
	// determine whether a file should use this plugin
	const char* const* reserved_filenames;
	size_t reserved_filename_count;

	// syntax highlighting
	struct lexer *(*create_lexer)(void);

	const struct language_rules* rules;
};


/// --- THE API ENDS HERE ---

// helper
typedef struct {
	void* handle;

	struct lexer* lexer;
	const struct language_plugin* language;
} LangPluginData;

int load_lang_plugins(Vector* out, Vector* plugin_names);
void destroy_lang_plugins(Vector* lang_plugins);



extern int is_utf_word_char(uint32_t c);



// functions and macros that can help

#define ARRAY_SIZE(x) (sizeof(x) / sizeof((x)[0]))

#endif