#ifndef EDITOR_INPUT_MOUSE_H
#define EDITOR_INPUT_MOUSE_H

#include "terminal/mouse.h"
#include "editor/editor.h"

int editor_handle_mouse_input
(
	Editor* editor,
	struct mouse_event mouse
);

#endif