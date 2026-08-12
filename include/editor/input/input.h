#ifndef EDITOR_INPUT_H
#define EDITOR_INPUT_H

#include "terminal/input.h"
#include "editor/input/keyboard.h"
#include "editor/input/mouse.h"
#include "editor/input/window.h"

int editor_handle_input(Editor* editor, struct event event);
void editor_handle_file_change(Editor* editor);

#endif
