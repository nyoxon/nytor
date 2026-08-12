#ifndef CLIPBOARD_H
#define CLIPBOARD_H

/*

*/

#include <stddef.h>

#include "util/types/u32string.h"

enum clipboard_backend {
	CLIPBOARD_NONE,
	CLIPBOARD_WL,
	CLIPBOARD_XCLIP,
	CLIPBOARD_XSEL
};

typedef struct {
	u32string text;
	int linewise;
} Clipboard;

Clipboard clipboard_new();
void clipboard_free(Clipboard* cb);

size_t clipboard_count_newlines(const Clipboard* cb);

void detect_clipboard_backend();

void clipboard_set(u32string* text);
u32string clipboard_get();

extern enum clipboard_backend CB_BACKEND;

#endif