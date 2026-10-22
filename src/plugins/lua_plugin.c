#include <dlfcn.h>
#include <stdio.h>
#include <string.h>
#include <stdint.h>
#include <stdlib.h>
#include <dirent.h>
#include <sys/stat.h>
#include <unistd.h>
#include <pwd.h>

#include "plugins/lua_plugin.h"
#include "editor/core/action.h"
#include "util/files.h"
#include "util/types/u32string.h"

// Lua's ABI is loaded at runtime, so 
// compiling nyt never requires Lua headers
// or a Lua development package. These 
// declarations match the public C API.

// USEFUL DOCUMENTATION:
// C closures in Lua: https://cyevgeniy.github.io/luadocs/04_API/ch02.html
// Userdata: https://www.lua.org/pil/28.1.html
// C API: https://www.lua.org/pil/24.html

// Starting with Lua 5.3, integers are considered a
// distinct type (separete from floats); these integers
// have a maximum size of 64 bits (long long) as long
// the floats (51 precision)


/// --- TYPE DEFITIONS ---

typedef struct lua_State lua_State;

typedef int (*lua_CFunction) (lua_State*);

typedef long long lua_KContext;

typedef int (*lua_KFunction) (lua_State*, int, lua_KContext);

// -1001000 = LUA_REGISTRYINDEX
// upvalue indexes are calculated based on this value
#define LUA_UPVALUE_INDEX(i) (-1001000 - (i))

typedef struct {
	void* library;
	
	// sigma based Lua state
	lua_State* (*L_newstate)(void);
	
	// load some useful Lua libraries
	void (*L_openlibs)(lua_State*);
	
	// close the Lua state
	void (*close)(lua_State*);
	
	// load a Lua file
	int (*L_loadfilex)(lua_State*, const char*, const char*);
	
	
	// call a function in a "safe" manner
	int (*pcallk)(
		lua_State*, 
		int, 
		int, 
		int, 
		lua_KContext, 
		lua_KFunction
	);
	
	
	// create a Lua table
	void (*createtable)(lua_State*, int, int);
	
	
	// create a light userdata.
	// a light userdata is like a value in Lua
	// that holds a pointer in C.
	// in this case, the Lua state bears no
	// responsability whatsoever for managing
	// the memory to which the pointer points
	void (*pushlightuserdata)(lua_State*, void*);
	
	
	// basically a function defined in C that can
	// be called by Lua
	void (*pushcclosure)(lua_State*, lua_CFunction, int);
	
	
	// set a field of a table
	void (*setfield)(lua_State*, int, const char*);
	
	
	// set something as global
	void (*setglobal)(lua_State*, const char*);
	
	
	// Lua's tointeger function
	long long (*tointegerx)(lua_State*, int, int*);
	
	
	// Lua's toboolean function
	int (*toboolean)(lua_State*, int);
	
	
	// pushes an integer onto the top of the stack
	void (*pushinteger)(lua_State*, long long);
	
	
	// pushes an string onto the top of the stack
	const char* (*pushstring)(lua_State*, const char*);
	
	
	// Lua's tostring function
	const char* (*tolstring)(lua_State*, int, size_t*);
	
	
	// create a userdata
	void* (*touserdata)(lua_State*, int);
} LuaApi;


typedef struct {
	LuaApi* api;
	Editor* editor;
} LuaContext;


static LuaApi* active_api;


// get the config dirs that exist
static size_t get_plugin_directories
(
	char directories[][PATH_MAX_LENGTH]
) 
{
	char priorities[3][PATH_MAX_LENGTH] = {{0}};
	size_t count = 0;
	const char* home = getenv("HOME");
	
	if (!home || !*home) {
		struct passwd* pw = getpwuid(getuid());
		
		if (pw) {
			home = pw->pw_dir;
		}
	}
	
	if (home && *home) {
		int written = snprintf(
			priorities[count], 
			PATH_MAX_LENGTH,
			"%s/.config/nytor/plugins", 
			home
		);
		
		if (written > 0 && 
			written < PATH_MAX_LENGTH) 
		{
			count++;
		}
	}
	
	strncpy(
		priorities[count++], 
		NYTOR_DATADIR "/plugins",
		PATH_MAX_LENGTH - 1
	);
	
	strncpy(
		priorities[count++], 
		"/usr/share/nytor/plugins",
		PATH_MAX_LENGTH - 1
	);
	
	for (size_t i = 0; i < count; i++) {
		struct stat info;
		
		if (stat(priorities[i], &info) == 0 && 
			S_ISDIR(info.st_mode)) 
		{
			memcpy(
				directories[0], 
				priorities[i], 
				PATH_MAX_LENGTH
			);
			
			return 1;
		}
	}
	
	return 0;
}


// configdir/plugin
static int make_plugin_path
(
	char* path, 
	size_t path_size,
	const char* directory, 
	const char* name
) 
{
	int written = snprintf(
		path, 
		path_size, 
		"%s/%s.lua", 
		directory, 
		name
	);
	
	if (written < 0 || (size_t) written >= path_size) {
		return 0;
	}
	
	struct stat info;
	
	return stat(path, &info) == 0 && 
		   S_ISREG(info.st_mode) && 
		   access(path, R_OK) == 0;
}


// find a plugin
int lua_plugin_find
(
	const char* name, 
	char* path, 
	size_t path_size
) 
{
	if (!name || !*name || !path || path_size == 0 ||
		strchr(name, '/') || strchr(name, '\\') ||
		strcmp(name, ".") == 0 || strcmp(name, "..") == 0)
	{
		return 0;
	}
	
	char directories[3][PATH_MAX_LENGTH] = {{0}};
	
	size_t count = get_plugin_directories(directories);
	
	for (size_t i = 0; i < count; i++) {
		if (make_plugin_path(
				path, 
				path_size, 
				directories[i], 
				name))
		{
			return 1;
		}
	}
	
	return 0;
}


// if name is already in candidates
static int has_plugin_candidate
(
	const Vector* candidates, 
	const char* name
) 
{
	for (size_t i = 0; i < candidates->size; i++) {
		const u32string* candidate = vector_get_const(
			candidates, 
			i
		);
		
		char* utf8 = u32string_into_u8(candidate);
		int equal = utf8 && strcmp(utf8, name) == 0;
		
		free(utf8);
		
		if (equal) {
			return 1;
		}
	}
	
	return 0;
}


// for autocomplete
void lua_plugin_candidates
(
	Vector* candidates, 
	const u32string* prefix
) 
{
	if (!candidates || !prefix) {
		return;
	}
	
	char directories[3][PATH_MAX_LENGTH] = {{0}};
	
	size_t directory_count = get_plugin_directories(directories);
	size_t prefix_size = u32string_size(prefix);
	
	for (size_t i = 0; i < directory_count; i++) {
		DIR* directory = opendir(directories[i]);
		
		if (!directory) {
			continue;
		}
		
		struct dirent* entry;
		
		while ((entry = readdir(directory)) != NULL) {
			size_t len = strlen(entry->d_name);
			
			if (len <= 4 || 
				len >= PATH_MAX_LENGTH ||
				strcmp(entry->d_name + len - 4, ".lua") != 0)
			{
				continue;
			}
			
			char plugin_name[PATH_MAX_LENGTH];
			memcpy(plugin_name, entry->d_name, len - 4);
			plugin_name[len - 4] = '\0';
			
			char plugin_path[PATH_MAX_LENGTH];
			
			if (!make_plugin_path(
					plugin_path, 
					sizeof(plugin_path), 
					directories[i],
					plugin_name))
			{
				continue;
			}
			
			if (has_plugin_candidate(
					candidates, 
					plugin_name) ||
				!u32string_equalnu8(
					prefix, 
					plugin_name, 
					prefix_size))
			{
				continue;
			}
			
			u32string candidate = u32string_from(plugin_name);
			vector_push(candidates, &candidate);
		}
		
		closedir(directory);
	}
}



/// --- SYSTEM FUNCTIONS ---


// the big old baldy
static void* load_symbol(void* library, const char* name) {
	return dlsym(library, name);
}

#define LOAD(member, symbol) do { \
	*(void**) (&api->member) = load_symbol(api->library, symbol); \
	if (!api->member) return -1; \
} while (0)




// min: 5.2
// load all the necessary functions
static int lua_api_load(LuaApi* api) {
	static const char* libraries[] = {
		"liblua-5.4.so.0",
		"liblua-5.3.so.0",
		"liblua-5.2.so.0",
		"liblua-5.4.so",
		"liblua-5.3.so",
		"liblua-5.2.so",
		"liblua5.4.so.0", 
		"liblua5.3.so.0", 
		"liblua5.2.so.0",
		"liblua.so.5.4", 
		"liblua.so.5.3", 
		"liblua.so"
	};
	
	memset(api, 0, sizeof(*api));
	
	for (size_t i = 0; i < ARRAY_SIZE(libraries); i++) {
		api->library = dlopen(
			libraries[i], 
			RTLD_NOW | RTLD_LOCAL
		);
		
		if (api->library) {
			break;
		}
	}
	
	if (!api->library) {
		return -1;
	}
	
	LOAD(L_newstate, "luaL_newstate");
	LOAD(L_openlibs, "luaL_openlibs");
	LOAD(close, "lua_close");
	LOAD(L_loadfilex, "luaL_loadfilex");
	LOAD(pcallk, "lua_pcallk");
	LOAD(createtable, "lua_createtable");
	LOAD(pushlightuserdata, "lua_pushlightuserdata");
	LOAD(pushcclosure, "lua_pushcclosure");
	LOAD(setfield, "lua_setfield");
	LOAD(setglobal, "lua_setglobal");
	LOAD(tointegerx, "lua_tointegerx");
	LOAD(toboolean, "lua_toboolean");
	LOAD(pushinteger, "lua_pushinteger");
	LOAD(pushstring, "lua_pushstring");
	LOAD(tolstring, "lua_tolstring");
	LOAD(touserdata, "lua_touserdata");
	
	return 0;
}

// get the context
static LuaContext* context(lua_State* state) {
	return active_api->touserdata(state, LUA_UPVALUE_INDEX(1));
}




/// --- HELPER FUNCTIONS ---


// tries to read an arguments as a integer
static long long integer_arg
(
	LuaContext* ctx, 
	lua_State* state, 
	int index,
	int* valid
) 
{
	return ctx->api->tointegerx(state, index, valid);
}

// tries to read an argument as a string
static const char* string_arg
(
	LuaContext* ctx, 
	lua_State* state, 
	int index
) 
{
	return ctx->api->tolstring(state, index, NULL);
}

// tries to read an argument as a boolean
static int boolean_arg
(
	LuaContext* ctx, 
	lua_State* state, 
	int index
) 
{
	return ctx->api->toboolean(state, index);
}


// tries to push an integer onto the top of the stack
static int push_result
(
	LuaContext* ctx, 
	lua_State* state, 
	int result
) 
{
	ctx->api->pushinteger(state, result);
	return 1;
}




/// --- API FUNCTIONS ---

// -- DETAILS --

// It's resposibility of theses functions to verify
// the validity of the values returned by Lua (out of bounds etc)


// Exposes the current content of the prompt
// prompt(void) -> string
static int api_prompt(lua_State* state) {
	LuaContext* ctx = context(state);
	
	char* utf8 = u32string_into_u8(
		&ctx->editor->status_bar.buf
	);
	
	ctx->api->pushstring(state, utf8 ? utf8 : "");
	
	free(utf8);
	return 1;
}


// Exposes the actual position of the cursor
// cursor(void) -> (integer, integer)
static int api_cursor(lua_State* state) {
	LuaContext* ctx = context(state);
	
	ctx->api->pushinteger(
		state, 
		(long long) ctx->editor->cursor.pos.x + 1
	);
	
	ctx->api->pushinteger(
		state, 
		(long long) ctx->editor->cursor.pos.y + 1
	);
	
	return 2;
}


// Moves the cursor to (x, y)
// The values returned by Lua must be indexed
// starting from 1
// set_cursor(x: integer, y: integer) -> void
static int api_set_cursor(lua_State* state) {
	LuaContext* ctx = context(state);
	int valid_x = 0, valid_y = 0;
	
	long long x = ctx->api->tointegerx(state, 1, &valid_x);
	long long y = ctx->api->tointegerx(state, 2, &valid_y);
	
	if (valid_x && valid_y && x > 0 && y > 0) {
			editor_cursor_move(ctx->editor, (Position) {
				.x = (size_t) x - 1,
				.y = (size_t) y - 1
			});
	}
	
	return 0;
}


// Just calls editor_cursor_update
// cursor_update(void) -> void
static int api_cursor_update(lua_State* state) {
	LuaContext* ctx = context(state);
	
	editor_cursor_update(ctx->editor);
	return 0;
}


// Moves the cursor to a named position
// move_cursor(position: string) -> integer
static int api_move_cursor(lua_State* state) {
	LuaContext* ctx = context(state);
	const char* direction = string_arg(ctx, state, 1);
	
	if (!direction) {
		return push_result(ctx, state, -1);
	}
	
	if (strcmp(direction, "right") == 0) {
		editor_move_cursor_right(ctx->editor);
	}
	
	else if (strcmp(direction, "left") == 0) {
		editor_move_cursor_left(ctx->editor);
	}
	
	else if (strcmp(direction, "up") == 0) {
		editor_move_cursor_up(ctx->editor);
	}
	
	else if (strcmp(direction, "down") == 0) {
		editor_move_cursor_down(ctx->editor);
	}
	
	else if (strcmp(direction, "line_start") == 0) {
		editor_move_cursor_beginning_line(ctx->editor);
	}
	
	else if (strcmp(direction, "line_end") == 0) {
		editor_move_cursor_end_line(ctx->editor);
	}
	
	else {
		return push_result(ctx, state, -1);
	}
	
	return push_result(ctx, state, 0);
}


// Exposes the size of the current terminal
// terminal_size(void) -> (integer, integer)
static int api_terminal_size(lua_State* state) {
	LuaContext* ctx = context(state);
	
	ctx->api->pushinteger(
		state, 
		(long long) ctx->editor->tsize.cols
	);
	
	ctx->api->pushinteger(
		state, 
		(long long) ctx->editor->tsize.rows
	);
	
	return 2;
}


// Just calls editor_open_terminal
// open_terminal(void) -> void
static int api_open_terminal(lua_State* state) {
	LuaContext* ctx = context(state);
	
	editor_open_terminal(ctx->editor);
	return 0;
}


// Just calls editor_suspend
// suspend(void) -> void
static int api_suspend(lua_State* state) {
	LuaContext* ctx = context(state);
	
	editor_suspend(ctx->editor);
	return 0;
}


// Just calls editor_update_size
// update_size(void) -> void
static int api_update_size(lua_State* state) {
	LuaContext* ctx = context(state);
	
	editor_update_size(ctx->editor);
	return 0;
}


// calls editor_change_next_file
// next_file(void) -> void
static int api_next_file(lua_State* state) {
	LuaContext* ctx = context(state);
	
	editor_change_next_file(ctx->editor);
	return 0;
}


// cals editor_prev_file
// prev_file(void) -> void
static int api_prev_file(lua_State* state) {
	LuaContext* ctx = context(state);
	
	editor_change_prev_file(ctx->editor);
	return 0;
}


// calls editor_create_new_file
// new_file(internal: bool) -> void
static int api_new_file(lua_State* state) {
	LuaContext* ctx = context(state);
	
	editor_create_new_file(
		ctx->editor, 
		boolean_arg(ctx, state, 1) // internal
	);
	
	return 0;
}


// creates an internal file
// internal_file(name: string, content: string[]) -> integer
static int api_internal_file(lua_State* state) {
	LuaContext* ctx = context(state);
	
	const char* name = string_arg(ctx, state, 1);
	const char* contents = string_arg(ctx, state, 2);
	
	Clipboard clipboard = clipboard_new();
	
	if (contents) {
		u32string_free(&clipboard.text);
		
		clipboard.text = u32string_from(contents);
	}
	
	int ret = editor_create_internal_file(
		ctx->editor, 
		name, 
		&clipboard
	);
	
	clipboard_free(&clipboard);
	return push_result(ctx, state, ret);
}


// opens a file
// open_file(filename: string, readonly: bool) -> integer
static int api_open_file(lua_State* state) {
	LuaContext* ctx = context(state);
	const char* path = string_arg(ctx, state, 1);
	
	if (!path) {
		return push_result(ctx, state, -1);
	}
	
	ssize_t existing = editor_has_file(ctx->editor, path);
	
	editor_open_file(
		ctx->editor, 
		path, 
		boolean_arg(ctx, state, 2)
	);
	
	if (existing >= 0 || 
	   (ctx->editor->actual_file->file.path &&
		strcmp(ctx->editor->actual_file->file.path, path) == 0))
	{
		return push_result(ctx, state, 0);
	}
	
	return push_result(ctx, state, -1);
}


// closes a file
// close_file(index: uinteger, forced: bool) -> integer
static int api_close_file(lua_State* state) {
	LuaContext* ctx = context(state);
	
	int valid = 0;
	long long index = integer_arg(ctx, state, 1, &valid);
	int force = boolean_arg(ctx, state, 2);
	
	if (!valid || 
		index < 1 || 
		(unsigned long long) index > ctx->editor->files.size)
	{
		return push_result(ctx, state, -1);
	}
	if (force) {
		editor_close_file_forced(ctx->editor, (size_t) index - 1);
	}
	
	else {
		editor_close_file(ctx->editor, (size_t) index - 1);
	}
	
	return push_result(ctx, state, 0);
}


// calls editor_close_file
// close_current_file(void) -> void
static int api_close_current_file(lua_State* state) {
	LuaContext* ctx = context(state);
	
	editor_close_file(
		ctx->editor, 
		ctx->editor->actual_file_index
	);
	
	return 0;
}


// save a file
// save_file(index: integer) -> integer
static int api_save_file(lua_State* state) {
	LuaContext* ctx = context(state);
	
	int valid = 0;
	long long index = integer_arg(ctx, state, 1, &valid);
	
	size_t file_index = (valid) 
		? (size_t) ((index > 0) ? index - 1 : 0)
		: ctx->editor->actual_file_index;
	
	if (index < 1 && valid) {
		return push_result(ctx, state, -1);
	}
	
	if (file_index >= ctx->editor->files.size) {
		return push_result(ctx, state, -1);
	}
	
	return push_result(
		ctx, 
		state, 
		editor_save_file(ctx->editor, file_index)
	);
}


// change the actual file
// change_file(index: integer) -> integer
static int api_change_file(lua_State* state) {
	LuaContext* ctx = context(state);
	
	int valid = 0;
	long long index = integer_arg(ctx, state, 1, &valid);
	
	if (!valid || 
		index < 1 || 
		(unsigned long long) index > ctx->editor->files.size)
	{
		return push_result(ctx, state, -1);
	}
	
	editor_change_actual_file(
		ctx->editor, 
		(size_t) index - 1
	);
	
	return push_result(ctx, state, 0);
}


// number of files
// file_count(void) -> integer
static int api_file_count(lua_State* state) {
	LuaContext* ctx = context(state);
	
	ctx->api->pushinteger(
		state, 
		(long long) ctx->editor->files.size
	);
	
	return 1;
}


// index of the current file
// current_file(void) -> integer
static int api_current_file(lua_State* state) {
	LuaContext* ctx = context(state);
	
	ctx->api->pushinteger(
		state,
		(long long) ctx->editor->actual_file_index + 1
	);
	
	return 1;
}


// creates a selection
// select_range(start_x: integer, start_y: integer,
//				end_x: integer, end_y: integer) -> integer
static int api_select_range(lua_State* state) {
	LuaContext* ctx = context(state);
	
	int valid[4] = {0};
	long long values[4];
	
	for (int i = 0; i < 4; i++) {
		values[i] = integer_arg(ctx, state, i + 1, &valid[i]);
	}
	
	for (int i = 0; i < 4; i++) {
		if (!valid[i] || values[i] < 1) {
			return push_result(ctx, state, -1);
		}
	}
	
	File* file = &ctx->editor->actual_file->file;
	
	if ((unsigned long long) values[1] > file->lines.size ||
		(unsigned long long) values[3] > file->lines.size)
	{
		return push_result(ctx, state, -1);
	}
	
	Position start = {
		(size_t) values[0] - 1, 
		(size_t) values[1] - 1
	};
	
	Position end = {
		(size_t) values[2] - 1, 
		(size_t) values[3] - 1
	};
	
	if (start.x > file_size_line(file, start.y) ||
		end.x > file_size_line(file, end.y))
	{
		return push_result(ctx, state, -1);
	}
	
	selection_start(&ctx->editor->sel, start);
	selection_update(&ctx->editor->sel, end);
	
	ctx->editor->selecting = 0;
	
	editor_cursor_move(ctx->editor, end);
	
	return push_result(ctx, state, 0);
}


// calls editor_start_and_create_selection
// start_seleciton(void) -> void
static int api_start_selection(lua_State* state) {
	LuaContext* ctx = context(state);
	if (!ctx->editor->sel.active)
		editor_start_and_create_selection(ctx->editor);
	return 0;
}


// clear a selection
// clear_selection(void) -> void
static int api_clear_selection(lua_State* state) {
	LuaContext* ctx = context(state);
	editor_selection_clear(ctx->editor);
	return 0;
}


// complete the selection
// complete_selection(void) -> void
static int api_complete_selection(lua_State* state) {
	LuaContext* ctx = context(state);
	
	editor_selection_complete(ctx->editor);
	return 0;
}


// select the current line
// select_line(void) -> integer
static int api_select_line(lua_State* state) {
	LuaContext* ctx = context(state);
	
	return push_result(
		ctx, 
		state, 
		editor_select_line(ctx->editor)
	);
}


// select all file
// select_all(void) -> integer
static int api_select_all(lua_State* state) {
	LuaContext* ctx = context(state);
	
	return push_result(
		ctx, 
		state, 
		editor_select_all_file(ctx->editor)
	);
}


// select a word
// select_word(void) -> void
static int api_select_word(lua_State* state) {
	LuaContext* ctx = context(state);
	
	if (file_size_line(&ctx->editor->actual_file->file,
						ctx->editor->cursor.pos.y) > 0)
	{
		editor_select_word(ctx->editor);
	}
	
	return 0;
}


// call editor_copy_selection
// copy(void) -> integer
static int api_copy(lua_State* state) {
	LuaContext* ctx = context(state);
	
	return push_result(
		ctx, 
		state, 
		editor_copy_selection(ctx->editor)
	);
}


// call editor_paste_clipboard
static int api_paste(lua_State* state) {
	LuaContext* ctx = context(state);
	
	return push_result(
		ctx, 
		state, 
		editor_paste_clipboard(ctx->editor)
	);
}


// delete the char at the current position of the cursor
// or a selection
// delete_char(void) -> integer
static int api_delete(lua_State* state) {
	LuaContext* ctx = context(state);
	
	return push_result(
		ctx,
		state,
		editor_delete_char_or_selection(ctx->editor, 0, 0)
	);
}


// call editor_del_from_cursor_left
// delete_left(void) -> void
static int api_delete_left(lua_State* state) {
	LuaContext* ctx = context(state);
	
	editor_del_from_cursor_left(ctx->editor);
	return 0;
}


// call editor_del_from_cursor_right
// delete_right(void) -> void
static int api_delete_right(lua_State* state) {
	LuaContext* ctx = context(state);
	
	editor_del_from_cursor_right(ctx->editor);
	return 0;
}


// insert a newline at the cursor's position
// insert_newline(void) -> integer
static int api_insert_newline(lua_State* state) {
	LuaContext* ctx = context(state);
	
	return push_result(
		ctx, 
		state, 
		editor_insert_newline(ctx->editor)
	);
}


// indent the actual line or a selection
// indent(void) -> integer
static int api_indent(lua_State* state) {
	LuaContext* ctx = context(state);
	
	return push_result(
		ctx, 
		state,
		editor_indent_selection_or_line(ctx->editor)
	);
}


// unindent the actual line or selection
// unindent(void) -> integer
static int api_unindent(lua_State* state) {
	LuaContext* ctx = context(state);
	
	return push_result(
		ctx, 
		state,
		editor_unindent_selection_or_line(ctx->editor)
	);
}


// dis/comment the current line or selection
// comment(void) -> integer
static int api_comment(lua_State* state) {
	LuaContext* ctx = context(state);
	
	return push_result(
		ctx, 
		state,
		editor_comment_line_or_selection(ctx->editor)
	);
}


// move the current line or selection up
// move_line_up(void) -> integer
static int api_move_line_up(lua_State* state) {
	LuaContext* ctx = context(state);
	
	return push_result(
		ctx, 
		state, 
		editor_move_line_or_selection_up(ctx->editor)
	);
}


// move the current line or selection down
// move_line_down(void) -> integer
static int api_move_line_down(lua_State* state) {
	LuaContext* ctx = context(state);
	
	return push_result(
		ctx, 
		state, 
		editor_move_line_or_selection_down(ctx->editor)
	);
}


// call editor_undo
// undo(void) -> void
static int api_undo(lua_State* state) {
	LuaContext* ctx = context(state);
	editor_undo(ctx->editor);
	return 0;
}


// call editor_redo
// redo(void) -> void
static int api_redo(lua_State* state) {
	LuaContext* ctx = context(state);
	editor_redo(ctx->editor);
	return 0;
}

// call is_word_char()
// is_word_char(codepoint: integer) -> boolean
// 0 < codepoint < UINT32_MAX
static int api_is_word_char(lua_State* state) {
	LuaContext* ctx = context(state);
	
	int valid = 0;
	long long codepoint = integer_arg(ctx, state, 1, &valid);
	
	IsWordChar is_word = editor_get_IsWordChar(ctx->editor);
	
	return push_result(
		ctx, 
		state,
		(valid && codepoint >= 0 
			   && codepoint <= UINT32_MAX 
			   && is_word)
			? is_word((uint32_t) codepoint) : 0
	);
}


// find a first occurence of a pattern
// starting at the current position of the cursor,
// the cursor will move to the end of the
// pattern and the pattern will be selected
// returns whether the pattern has been foundededede

// find(pattern: string) -> boolean
static int api_find(lua_State* state) {
	LuaContext* ctx = context(state);
	
	const char* value = string_arg(ctx, state, 1);
	
	if (!value) {
		return push_result(ctx, state, 0);
	}
	
	u32string pattern = u32string_from(value);
	
	int found = editor_find_next_pattern_match(
		ctx->editor,
		ctx->editor->cursor.pos, 
		&pattern, 
		1
	);
	
	u32string_free(&pattern);
	
	return push_result(ctx, state, found);
}


// replace a pattern
// if the a selection is active, the replacement
// will occurs only within the selection. otherwise
// it will occurs within the entire file.
// if newtext is null, all the occurences of oldtext
// will be deleted

// replace(oldtext: string, newtext: string) -> integer
static int api_replace(lua_State* state) {
	LuaContext* ctx = context(state);
	
	const char* pattern_value = string_arg(ctx, state, 1);
	const char* text_value = string_arg(ctx, state, 2);
	
	if (!pattern_value || !text_value) {
		return push_result(ctx, state, -1);
	}
	
	u32string pattern = u32string_from(pattern_value);
	u32string text = u32string_from(text_value);
	
	if (boolean_arg(ctx, state, 3)) {
		editor_replace_within_selection(
			ctx->editor, 
			&pattern, 
			&text
		);
	}
	
	else {
		editor_replace_pattern(
			ctx->editor, 
			&pattern, 
			&text
		);
	}
	
	u32string_free(&pattern);
	u32string_free(&text);
	
	return push_result(ctx, state, 0);
}


// scroll the terminal
// scroll(direction: string) -> integer
static int api_scroll(lua_State* state) {
	LuaContext* ctx = context(state);
	
	const char* direction = string_arg(ctx, state, 1);
	
	if (!direction) {
		return push_result(ctx, state, -1);
	}
	
	if (strcmp(direction, "up") == 0) {
		editor_scroll_up(ctx->editor);
	}
	
	else if (strcmp(direction, "down") == 0) {
		editor_scroll_down(ctx->editor);
	}
	
	else if (strcmp(direction, "left") == 0) {
		editor_scroll_left(ctx->editor);
	}
	
	else if (strcmp(direction, "right") == 0) {
		editor_scroll_right(ctx->editor);
	}
	
	else if (strcmp(direction, "page_up") == 0) {
		editor_scroll_up_terminal_size(ctx->editor);
	}
	
	else if (strcmp(direction, "page_down") == 0) {
		editor_scroll_down_terminal_size(ctx->editor);
	}
	
	else {
		return push_result(ctx, state, -1);
	}
	
	return push_result(ctx, state, 0);
}


// number of lines of the actual file
// line_count(void) -> integer

// obs: line_count is always >= 1
// a file is empty if only if
// line_count == 1 and line_length == 0
static int api_line_count(lua_State* state) {
	LuaContext* ctx = context(state);
	
	ctx->api->pushinteger(
		state, 
		(long long) ctx->editor->actual_file->file.lines.size
	);
	
	return 1;
}


// return a line
// line(index: integer) -> string

// the returned string is empty if
// the content of the line is empty
// or an error has occured
static int api_line(lua_State* state) {
	LuaContext* ctx = context(state);
	
	int valid = 0;
	long long number = ctx->api->tointegerx(state, 1, &valid);
	
	Vector* lines = &ctx->editor->actual_file->file.lines;
	
	if (!valid || 
		number < 1 || 
		(unsigned long long) number > lines->size) 
	{
		ctx->api->pushstring(state, "");
		
		return 1;
	}
	
	Line* line = vector_get(lines, (size_t) number - 1);
	char* utf8 = u32string_into_u8(&line->text);
	
	ctx->api->pushstring(state, utf8 ? utf8 : "");
	
	free(utf8);
	return 1;
}


// size of a line
// line_length(index: integer) -> integer
static int api_line_length(lua_State* state) {
	LuaContext* ctx = context(state);
	
	int valid = 0;
	long long number = integer_arg(ctx, state, 1, &valid);
	
	Vector* lines = &ctx->editor->actual_file->file.lines;
	
	if (!valid || 
		number < 1 || 
		(unsigned long long) number > lines->size) 
	{
		ctx->api->pushinteger(state, -1);
		
		return 1;
	}
	
	Line* line = vector_get(lines, (size_t) number - 1);
	
	ctx->api->pushinteger(
		state, 
		(long long) u32string_size(&line->text)
	);
	
	return 1;
}


// change the prompt's content as PT_INFO
// set_prompt(content: string) -> void
static int api_set_prompt(lua_State* state) {
	LuaContext* ctx = context(state);
	
	const char* value = ctx->api->tolstring(state, 1, NULL);
	
	if (value) {
		prompt_init(&ctx->editor->status_bar, value, PT_INFO);
	}
	
	return 0;
}


// insert a string into the file starting from
// the current cursor position

// insert(content: string) -> integer
static int api_insert(lua_State* state) {
	LuaContext* ctx = context(state);
	
	const char* value = ctx->api->tolstring(state, 1, NULL);
	
	if (!value) {
		return 0;
	}
	
	u32string text = u32string_from(value);
	
	int ret = 0;
	
	for (size_t i = 0; i < text.text.size; i++) {
		uint32_t* ch = vector_get(&text.text, i);
		ret = editor_insert_char(ctx->editor, *ch);
		
		if (ret < 0) {
			break;
		}
	}
	
	u32string_free(&text);
	
	return push_result(ctx, state, ret);

}


// export a function
static void add_function
(
	LuaApi* api, 
	lua_State* state, 
	LuaContext* ctx,
	const char* name, 
	lua_CFunction function
) 
{
	api->pushlightuserdata(state, ctx);
	api->pushcclosure(state, function, 1);
	api->setfield(state, -2, name);
}


// runs the script
int lua_plugin_run
(
	Editor* editor, 
	const char* path, 
	char* error, 
	size_t error_size
) 
{
	LuaApi api;
	
	if (lua_api_load(&api) < 0) {
		if (api.library) {
			dlclose(api.library);
		}
		
		snprintf(
			error, 
			error_size, 
			"Lua runtime unavailable"
		);
		
		return -1;
	}
	
	active_api = &api;
	lua_State* state = api.L_newstate();
	
	if (!state) {
		dlclose(api.library);
		
		snprintf(
			error, 
			error_size, 
			"failed to create Lua state"
		);
		
		return -1;
	}
	
	api.L_openlibs(state);
	
	LuaContext ctx = { .api = &api, .editor = editor };
	
	api.createtable(state, 0, 55);
	
	add_function(&api, state, &ctx, "prompt", api_prompt);
	add_function(&api, state, &ctx, "cursor", api_cursor);
	add_function(&api, state, &ctx, "set_cursor", api_set_cursor);
	add_function(&api, state, &ctx, "cursor_update", api_cursor_update);
	add_function(&api, state, &ctx, "move_cursor", api_move_cursor);
	add_function(&api, state, &ctx, "terminal_size", api_terminal_size);
	add_function(&api, state, &ctx, "open_terminal", api_open_terminal);
	add_function(&api, state, &ctx, "suspend", api_suspend);
	add_function(&api, state, &ctx, "update_size", api_update_size);
	add_function(&api, state, &ctx, "next_file", api_next_file);
	add_function(&api, state, &ctx, "prev_file", api_prev_file);
	add_function(&api, state, &ctx, "change_file", api_change_file);
	add_function(&api, state, &ctx, "file_count", api_file_count);
	add_function(&api, state, &ctx, "current_file", api_current_file);
	add_function(&api, state, &ctx, "new_file", api_new_file);
	add_function(&api, state, &ctx, "internal_file", api_internal_file);
	add_function(&api, state, &ctx, "open_file", api_open_file);
	add_function(&api, state, &ctx, "close_file", api_close_file);
	add_function(&api, state, &ctx, "close_current_file", api_close_current_file);
	add_function(&api, state, &ctx, "save_file", api_save_file);
	add_function(&api, state, &ctx, "start_selection", api_start_selection);
	add_function(&api, state, &ctx, "clear_selection", api_clear_selection);
	add_function(&api, state, &ctx, "complete_selection", api_complete_selection);
	add_function(&api, state, &ctx, "select_range", api_select_range);
	add_function(&api, state, &ctx, "select_line", api_select_line);
	add_function(&api, state, &ctx, "select_all", api_select_all);
	add_function(&api, state, &ctx, "select_word", api_select_word);
	add_function(&api, state, &ctx, "copy", api_copy);
	add_function(&api, state, &ctx, "paste", api_paste);
	add_function(&api, state, &ctx, "line_count", api_line_count);
	add_function(&api, state, &ctx, "line", api_line);
	add_function(&api, state, &ctx, "line_length", api_line_length);
	add_function(&api, state, &ctx, "set_prompt", api_set_prompt);
	add_function(&api, state, &ctx, "insert", api_insert);
	add_function(&api, state, &ctx, "insert_newline", api_insert_newline);
	add_function(&api, state, &ctx, "delete", api_delete);
	add_function(&api, state, &ctx, "delete_left", api_delete_left);
	add_function(&api, state, &ctx, "delete_right", api_delete_right);
	add_function(&api, state, &ctx, "indent", api_indent);
	add_function(&api, state, &ctx, "unindent", api_unindent);
	add_function(&api, state, &ctx, "comment", api_comment);
	add_function(&api, state, &ctx, "move_line_up", api_move_line_up);
	add_function(&api, state, &ctx, "move_line_down", api_move_line_down);
	add_function(&api, state, &ctx, "scroll", api_scroll);
	add_function(&api, state, &ctx, "is_word_char", api_is_word_char);
	add_function(&api, state, &ctx, "find", api_find);
	add_function(&api, state, &ctx, "replace", api_replace);
	add_function(&api, state, &ctx, "undo", api_undo);
	add_function(&api, state, &ctx, "redo", api_redo);
	
	api.setglobal(state, "nyt");

	int ret = api.L_loadfilex(state, path, NULL);
	
	if (ret == 0) {
		ret = api.pcallk(state, 0, 0, 0, 0, NULL);
	}
	
	if (ret != 0) {
		const char* message = api.tolstring(state, -1, NULL);
		
		snprintf(
			error, 
			error_size, 
			"%s", 
			(message) 
				? message 
				: "Lua plugin failed"
		);
	}
	
	api.close(state);
	active_api = NULL;
	
	dlclose(api.library);
	
	return (ret == 0) ? 0 : -1;
}
