#ifndef EDITOR_INPUT_KEYBOARD_H
#define EDITOR_INPUT_KEYBOARD_H

#include "terminal/input.h"
#include "editor/editor.h"

int editor_handle_normal_input(Editor* editor, struct normal_key key);

#endif