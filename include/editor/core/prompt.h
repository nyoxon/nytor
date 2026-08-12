#ifndef PROMPT_H
#define PROMPT_H

#include <stddef.h>
#include <stdint.h>

#include "terminal/style.h"
#include "editor/core/scroll.h"
#include "terminal/input.h"

extern const char PROMPT_SAVE[];
extern const char PROMPT_REPLACE[];
extern const char PROMPT_INSERT[];
extern const char PROMPT_EMPTY_STRING[];
extern const char PROMPT_NOT_FOUND[];
extern const char PROMPT_FIND[];
extern const char PROMPT_GOTO[];
extern const char PROMPT_CMD[];
extern const char PROMPT_READONLY[];
extern const char PROMPT_INVALID_ARG[];
extern const char PROMPT_INVALID_COMMAND[];
extern const char PROMPT_NO_ARGS[];
extern const char PROMPT_FILE_EXISTS[];
extern const char PROMPT_FILE_DNT_EXIST[];
extern const char PROMPT_OUT_OF_BOUNDS[];
extern const char PROMPT_INSUFFICIENT_ARGS[];
extern const char PROMPT_LANG_PLUGIN_NULL[];
extern const char PROMPT_LANG_COMMENT_NULL[];
extern const char PROMPT_LANG_RULES_NULL[];
extern const char PROMPT_LANG_PAIRS_NULL[];
extern const char PROMPT_INVALID_FILENAME[];
extern const char PROMPT_MULTILINE_ON_MATCH[];
extern const char PROMPT_EMPTY_ON_MATCH[];
extern const char PROMPT_ARG_TOO_LONG[];
extern const char PROMPT_INVALID_LANGUAGE[];
extern const char PROMPT_PERM_DENIED[];

typedef enum {
	PT_INTERACTIVE,
	PT_INFO,
	PT_STATUS
} PromptType;

typedef struct {
	int active;

	u32string buf;
	u32string label;

	PromptType type;

	int invert_color; // must be set manually

	Cursor cursor;
	View view;
	TerminalSize tsize;
} Prompt;

Prompt prompt_new();
void prompt_init(Prompt* pt, const char* label, PromptType type);
void prompt_init_u32string(Prompt* pt, const u32string* lb, PromptType type);

void prompt_drawn
(
	Prompt* pt,
	const struct style* style,
	const TerminalSize* tsize
);

int prompt_has_label(const Prompt* pt, const char* label);

u32string prompt_create_status_text
(
	const char* filename,
	const char* language_name,
	int readonly,
	const TerminalSize* tsize
);

int prompt_buf_to_u32string(const Prompt* pt, u32string* string);
int prompt_buf_to_u8string(const Prompt* pt, char** string);

void prompt_update_size(Prompt* pt, TerminalSize tsize);

void prompt_handle_normal_input
(
	Prompt* pt, 
	struct normal_key key,
	const struct normal_key* keybinds
);

#endif