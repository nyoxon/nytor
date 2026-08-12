#ifndef PLUGIN_H
#define PLUGIN_H

#include "plugins/language_syntax.h"
#include "util/types/vector.h"

struct plugin_result {
	void* handle;
	int error;
};

struct plugin_result plugin_language_load(const char* name);
void plugin_destroy(void* plugin);

typedef const struct language_plugin* (*plugin_init_t)(void);

struct pair {
	char open;
	char close;

	int auto_indent;
	int auto_complete;
};

struct language_rules {
	const struct pair* pairs;
	size_t pair_count;

	const char* comment_fmt;

	int auto_indent;
};

struct language_plugin {
	const char* name;

	const char* const* extensions;
	size_t extension_count;
	
	const char* const* reserved_filenames;
	size_t reserved_filename_count;

	struct lexer *(*create_lexer)(void);

	const struct language_rules* rules;
};

typedef struct {
	void* handle;

	struct lexer* lexer;
	const struct language_plugin* language;
} LangPluginData;

int load_lang_plugins(Vector* out, Vector* plugin_names);
void destroy_lang_plugins(Vector* lang_plugins);


#endif