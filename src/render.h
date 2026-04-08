#ifndef RENDER_H
#define RENDER_H

// to draw on the screen

#include "file.h"
#include "cursor.h"
#include "scroll.h"

void render_screen(File* file, Cursor* cursor, View* view, TerminalSize* tsize);

#endif