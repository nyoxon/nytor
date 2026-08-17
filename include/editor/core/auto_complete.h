#ifndef EDITOR_AUTO_COMPLETE_H
#define EDITOR_AUTO_COMPLETE_H

#include "util/types/vector.h"
#include "util/types/u32string.h"

#include "editor/editor.h"

typedef enum {
	AUTOCOMP_CMD,
	AUTOCOMP_FILE,
	// AUTOCOMP_SHELL,
	// AUTOCOMP_LANG
} AutoCompType;

int editor_autocomp_prompt(Editor* editor);
int editor_autocomp_word(Editor* editor);

#endif