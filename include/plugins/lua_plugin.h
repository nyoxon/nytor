#ifndef LUA_PLUGIN_H
#define LUA_PLUGIN_H

#include "editor/editor.h"

// executes a Lua plugin file when a 
// Lua shared library is available
// the editor itself has no link-time 
// or startup dependency on Lua
int lua_plugin_run(
	Editor* editor, 
	const char* path, 
	char* error, 
	size_t error_size
);

// resolve a command name using the 
// configured plugin search order
int lua_plugin_find(
	const char* name, 
	char* path, 
	size_t path_size
);

// add discoverable Lua plugin command 
// names matching prefix to candidates
void lua_plugin_candidates(
	Vector* candidates, 
	const u32string* prefix
);

#endif
