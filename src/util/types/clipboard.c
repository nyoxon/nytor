#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <unistd.h>
#include <limits.h>

#include "util/types/clipboard.h"

enum clipboard_backend CB_BACKEND;

static int programs_exists(const char* program)
{
	const char* path = getenv("PATH");

	if (!path) {
		return 0;
	}

	char* copy = strdup(path);

	if (!copy) {
		return 0;
	}

	for (char* dir = strtok(copy, ":");
		 dir != NULL;
		 dir = strtok(NULL, ":"))
	{
		char fullpath[1024];

		snprintf(fullpath, sizeof(fullpath), "%s/%s", dir, program);

		if (access(fullpath, X_OK) == 0) {
			free(copy);

			return 1;
		}
	}

	free(copy);
	return 0;
}

void detect_clipboard_backend() {
	const char* session =  getenv("XDG_SESSION_TYPE");

	if (session && strcmp(session, "wayland") == 0) {
		if (programs_exists("wl-paste") &&
			programs_exists("wl-copy"))
		{
			CB_BACKEND = CLIPBOARD_WL;
		}

		else {
			CB_BACKEND = CLIPBOARD_NONE;
		}
	}

	else if (session && strcmp(session, "x11") == 0) {
		if (programs_exists("xclip")) {
			CB_BACKEND = CLIPBOARD_XCLIP;
		}

		else if (programs_exists("xsel")) {
			CB_BACKEND = CLIPBOARD_XSEL;
		}

		else {
			CB_BACKEND = CLIPBOARD_NONE;
		}
	}

	else {
		CB_BACKEND = CLIPBOARD_NONE;
	}
}

Clipboard clipboard_new() {
	Clipboard cb;
	cb.text = u32string_new();
	cb.linewise = 0;

	return cb;
}

void clipboard_free(Clipboard* cb) {
	u32string_free(&cb->text);
}

size_t clipboard_count_newlines(const Clipboard* cb) {
	size_t count = 0;

	for (size_t i = 0; i < u32string_size(&cb->text); i++) {
		if (u32string_char(&cb->text, i) == U'\n') {
			count++;
		}
	}

	return count;
}

static void wl_set(u32string* text) {
	FILE* fp = popen("wl-copy", "w");

	if (!fp) {
		return;
	}

	char* text_u8 = u32string_into_u8(text);
	fputs(text_u8, fp);

	free(text_u8);
	pclose(fp);
}

static void xclip_set(u32string* text) {
	FILE* fp = popen("xclip -selection clipboard", "w");

	if (!fp) {
		return;
	}

	char* text_u8 = u32string_into_u8(text);
	fputs(text_u8, fp);

	free(text_u8);
	pclose(fp);	
}

static void xsel_set(u32string* text) {
	FILE* fp = popen("xsel --clipboard --input", "w");

	if (!fp) {
		return;
	}

	char* text_u8 = u32string_into_u8(text);
	fputs(text_u8, fp);

	free(text_u8);
	pclose(fp);
}

void clipboard_set(u32string* text) {
	switch (CB_BACKEND) {
	case CLIPBOARD_NONE:
		return;

	case CLIPBOARD_WL:
		wl_set(text);
		return;

	case CLIPBOARD_XCLIP:
		xclip_set(text);
		return;

	case CLIPBOARD_XSEL:
		xsel_set(text);
		return;

	default:
		return;
	}
}

static char* get_text(FILE* fp) {
	size_t cap = 1024;
	size_t len = 0;

	char* buf = malloc(cap);

	int c;

	while ((c = fgetc(fp)) != EOF) {
		if (len + 1 >= cap) {
			cap *= 2;
			buf = realloc(buf, cap);
		}

		buf[len++] = c;
	}

	buf[len] = '\0';

	return buf;	
}

static u32string wl_get() {
	FILE* fp = popen("wl-paste --no-newline", "r");

	if (!fp) {
		return u32string_new();
	}

	char* buf = get_text(fp);

	pclose(fp);

	u32string text = u32string_from(buf);
	free(buf);

	return text;
}

static u32string xclip_get() {
	FILE* fp = popen("xclip -selection clipboard -o", "r");

	if (!fp) {
		return u32string_new();
	}

	char* buf = get_text(fp);

	pclose(fp);

	u32string text = u32string_from(buf);
	free(buf);

	return text;	
}

static u32string xsel_get() {
	FILE* fp = popen("xsel --clipboard --output", "r");

	if (!fp) {
		return u32string_new();
	}

	char* buf = get_text(fp);

	pclose(fp);

	u32string text = u32string_from(buf);
	free(buf);

	return text;	
}

u32string clipboard_get() {
	switch (CB_BACKEND) {
	case CLIPBOARD_NONE:
		return u32string_new();

	case CLIPBOARD_WL:
		return wl_get();

	case CLIPBOARD_XCLIP:
		return xclip_get();

	case CLIPBOARD_XSEL:
		return xsel_get();

	default:
		return u32string_new();
	}	
}