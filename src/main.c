#include <stdio.h>
#include "input.h"
#include "scroll.h"
#include "editor.h"
#include <assert.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <signal.h>

// contains the main loop

volatile sig_atomic_t exit_requested = 0;

void handle_exit_signal() {
	exit_requested = 1;
}

int main(int argc, const char* argv[]) {
	if (argc < 2) {
		printf("USAGE: %s [filename]\n", argv[0]);
		return -1;
	}

	signal(SIGTERM, handle_exit_signal);
	enable_raw_mode();

	Editor editor;
	editor_init(&editor, argv[1]);

	int running = 1;

	while (running) { 
		editor_render(&editor);

		int key = read_key();

		if (exit_requested) {
			clean_terminal();

			editor_free(&editor);
			disable_raw_mode();
			exit(0);
		}

		running = editor_handle_input(&editor, key);

		Line* line = vector_get(&editor.file.lines, editor.cursor.y);
		cursor_clamp(&editor.cursor, line->len, editor.file.lines.size);

		update_terminal_size(&editor.tsize);
		update_scroll(&editor.view, &editor.cursor, &editor.tsize);
	}

	clean_terminal();

	editor_free(&editor);
	return 0;
}