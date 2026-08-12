#ifndef CMD_H
#define CMD_H

#include <stdint.h>
#include <stddef.h>

#include "window/window.h"
#include "editor/editor.h"

typedef struct {
	const char* name;
	const char* description;
} cmd;

int editor_handle_cmd
(
	Editor* editor, 
	const u32string* u32_cmd
);

extern const cmd COMMANDS[];
extern const size_t COMMANDS_COUNT;

WindowResult show_help
(
	void* userdata
);

#endif