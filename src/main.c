#include <stdio.h>
#include "input.h"
#include "scroll.h"
#include "editor.h"
#include <assert.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <signal.h>
#include <getopt.h>

// contains the main loop

volatile sig_atomic_t exit_requested = 0;

void handle_exit_signal() {
	exit_requested = 1;
}

void print_help() {
	printf("--- nytor ---\n");
	printf("usage: nytor [filepath] [ARGUMENTS]\n");
	printf("[filepath] must always be passed\n");
	printf("\n--- [ARGUMENTS] --- \n");
	printf("--debug=[filepath] -> debug_mode\n");
	printf("--no-lines -> number of the lines will not be rendered\n");
}

int parse_args
(
	int argc, 
	char* argv[],
	int* debug_mode,
	char** debug_file,
	int* render_line_enumeration
) 
{
	int opt;

	struct option long_opts[] = {
		{"debug", required_argument, 0, 'd'},
		{"no-lines", no_argument, 0, 'l'},
		{0, 0, 0, 0}
	};

	while ((opt = getopt_long(argc, argv, "d", long_opts, NULL)) != 1) {
		switch (opt) {
			case 'd':
				*debug_mode = 1;
				*debug_file = optarg;
				break;

			case 'l':
				*render_line_enumeration = 0;
				break;

			default:
				fprintf(stderr, "see --help for valid arguments\n");
				return 1;
		}
	}

	return 0;
}

int main(int argc, char* argv[]) {
	if (argc < 2) {
		printf("USAGE: %s [filename]\n", argv[0]);
		return -1;
	}

	if (strcmp(argv[1], "--help") == 0 || strcmp(argv[1], "-h") == 0) {
		print_help();
		return 0;
	}

	char* filepath = argv[1];
	optind = 2;

	int debug_mode = 0;
	char* debug_file;
	int render_line_enumeration = 1;

	int ret = parse_args(argc, argv, &debug_mode, &debug_file, 
		&render_line_enumeration);

	if (ret < 0) {
		exit(1);
	}

	signal(SIGTERM, handle_exit_signal);
	enable_raw_mode();

	Editor editor;
	
	if (editor_init(&editor, filepath, 
			render_line_enumeration, debug_mode) < 0) {
		disable_raw_mode();
		fprintf(stderr, "%s\n", editor.result.reason);
		result_free(&editor.result);

		exit(1);
	}

	if (debug_mode) {
		log_init(&editor.log, debug_file);

		if (editor.log.fd < 0) {
			disable_raw_mode();
			fprintf(stderr, "failed to start log file");
			editor_free(&editor);

			exit(1);
		}
	} else {
		editor.log.fd = -1;
	}

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

		if (debug_mode) {
			editor_log_write(&editor);
		}

		Line* line = vector_get(&editor.file.lines, editor.cursor.y);
		cursor_clamp(&editor.cursor, line->len, editor.file.lines.size);

		update_terminal_size(&editor.tsize);
		update_scroll(&editor.view, &editor.cursor, &editor.tsize);
	}

	clean_terminal();

	editor_free(&editor);
	return 0;
}