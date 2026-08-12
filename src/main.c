#include <stdio.h>
#include <assert.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <signal.h>
#include <getopt.h>
#include <dlfcn.h>
#include <poll.h>
#include <time.h>
#include <locale.h>

#include "terminal/input.h"
#include "editor/core/scroll.h"
#include "editor/core/action.h"
#include "editor/input/input.h"
#include "editor/render/render.h"
#include "editor/editor.h"
#include "util/files.h"
#include "plugins/plugin.h"
#include "terminal/parser.h"
#include "util/types/u32string.h"
#include "window/window.h"
#include "window/render.h"

#define TARGET_FPS 40 			// it's already enough

static volatile sig_atomic_t exit_requested = 0;
static volatile sig_atomic_t terminal_resized = 0;

static void handle_exit_signal(int sig);
static void handle_sigwinch(int sig);
static void crash_handler(int sig);

static void install_handlers(void);

static void print_help();

static uint32_t frame_interval_ms(uint32_t fps);

static int parse_args
(
	int argc, 
	char* argv[],
	int* debug_mode
);

int main(int argc, char* argv[]) {
	setlocale(LC_ALL, "");

	// --- PARSE CLI ARGUMENTS ---
	int debug_mode = 0;

	int ret = parse_args(argc, argv, &debug_mode);

	if (ret == 1) { // program does not need to run
		return 0;
	}

	char** filenames = NULL;

	// ensures that the filepath will differ from the defined
	// cli arguments used by getopt_long

	if (optind != argc) {
		filenames = argv + optind;
	}

	// --- EDITOR INIT AND CONFIGURATIONS ---

	Editor editor;

	if (editor_init(
		&editor, 
		filenames,
		argc - optind,
		debug_mode) < 0)
	{
		if (editor.result.type != ERROR_OK) {
			fprintf(stderr, editor.result.reason);
		}

		fprintf(stderr, "\n");

		result_free(&editor.result);

		return 1;
	}

	// --- RENDER AND MAIN LOOP ---

	int running = 1;
	activate_terminal();

	install_handlers();

	detect_clipboard_backend();

	int need_render = 1;

	struct pollfd fds[] = {
		{
			.fd = STDIN_FILENO,
			.events = POLLIN
		},
		{
			.fd = editor.inotify_fd,
			.events = POLLIN
		}
	};

	while (running) { 
		if (exit_requested) {
			deactivate_terminal();
			editor_free(&editor);

			return 0;
		}

		if (terminal_resized && 
			update_terminal_size(&editor.tsize) == 0) 
		{
			editor_update_size(&editor);

			terminal_resized = 0;
			need_render = 1;
		}

		if (editor.suspend) {
			editor_suspend(&editor);

			need_render = 1; // maybe unnecessary
		}

		if (editor.actual_file->file.filename &&
			editor_file_changed(editor.actual_file))
		{
			editor_handle_file_change(&editor);
			need_render = 1;
		}

		int ret = poll(fds, 2, frame_interval_ms(TARGET_FPS));

		if (ret > 0) {
			if (fds[0].revents & POLLIN) {
				struct event event = parser_read_key();

				running = editor_handle_input(&editor, event);
				need_render = 1;
			}

			if (fds[1].revents & POLLIN) {
				editor_check_inotify(&editor);
			}
		}

		if (debug_mode && running) {
			editor_log_write(&editor);
		}

		if (need_render && running) {
			editor_update_syntax(&editor);
			editor_render(&editor);
		}

		need_render = 0;
	}

	deactivate_terminal();
	editor_free(&editor);

	return 0;
}

// --- AUX FUNCTIONS ---

static void handle_exit_signal(int sig) {
	(void) sig;
	exit_requested = 1;
}

static void handle_sigwinch(int sig) {
	(void) sig;
	terminal_resized = 1;
}

// this function isn't async-whatever-safe,
// but it's better than leaving the user's terminal
// behaving strangely
static void crash_handler(int sig) {
	deactivate_terminal();

	signal(sig, SIG_DFL);
	raise(sig);
}

void install_handlers(void) {
	struct sigaction sa = {0};

	sa.sa_handler = handle_exit_signal;
	sigemptyset(&sa.sa_mask);
	sa.sa_flags = 0;

	sigaction(SIGTERM, &sa, NULL);
	sigaction(SIGHUP, &sa, NULL);

	sa.sa_handler = handle_sigwinch;

	sigaction(SIGWINCH, &sa, NULL);

	sa.sa_handler = crash_handler;

	sigaction(SIGABRT, &sa, NULL);
	sigaction(SIGSEGV, &sa, NULL);
	sigaction(SIGILL, &sa, NULL);
	sigaction(SIGBUS, &sa, NULL);
	sigaction(SIGFPE, &sa, NULL);

	signal(SIGINT, SIG_IGN);
}

static uint32_t frame_interval_ms(uint32_t fps) {
	return (1000 + fps / 2) / fps;
}

static void print_help() {
	printf("--- nytor ---\n\n");
	printf("usage: nytor [ARGUMENTS] [filepaths]\n\n");
	printf("for information on how to use the editor, run the program\n");
	printf("and type \"help\" at the command prompt (ctrl + n, default keybind)\n\n");
	printf("\n--- [ARGUMENTS] --- \n\n");
	printf("  --help		-> prints this message\n\n");
	printf("  --debug=[filepath] 	-> debug_mode\n");
	printf("  				if filepath is omitted, a default file\n");
	printf("				called \"debug.ny\" will be created\n\n");
	printf("note: you must pass all arguments before passing a file;\n");
	printf("otherwise, the passed argument will be treated as a file\n");
}

static int parse_args
(
	int argc, 
	char* argv[],
	int* debug_mode
) 
{
	int opt;

	struct option long_opts[] = {
		{"debug", no_argument, 0, 'd'},
		{"help", no_argument, 0, 'h'},
		{0, 0, 0, 0}
	};

	while ((opt = getopt_long(argc, argv, "+d:h::", long_opts, NULL)) != 1) 
	{
		switch (opt) {
			case 'd':
				*debug_mode = 1;
				return 0;

			case 'h':
				print_help();
				return 1;
				break;               

			default:
				return 0;
		}
	}

	return 0;
}