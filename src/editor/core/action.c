#include <fcntl.h>
#include <ctype.h>
#include <string.h>
#include <assert.h>
#include <signal.h>
#include <stdlib.h>
#include <stdio.h>
#include <errno.h>
#include <sys/inotify.h>
#include <poll.h>
#include <sys/wait.h>
#include <pty.h>
#include <pwd.h>

#include "editor/core/action.h"
#include "editor/core/prompt.h"
#include "editor/render/render.h"
#include "util/files.h"
#include "terminal/parser.h"
#include "terminal/input.h"


/// --- TERMINAL ---

void editor_open_terminal(Editor* editor) {
	const char* shell = NULL;

	if (editor->config.auto_shell) {
		struct passwd* pw = getpwuid(getuid());

		if (pw == NULL) {
			return;
		}

		shell = pw->pw_shell;
	}

	else {
		shell = editor->config.shell;
	}

	clean_terminal();
	mouse_off();

	// struct winsize ws = {
	// 	.ws_row = editor->tsize.rows / 2,
	// 	.ws_col = editor->tsize.cols / 2
	// };

	int master;
	pid_t pid;

	// pid = forkpty(&master, NULL, NULL, &ws);

	pid = forkpty(&master, NULL, NULL, NULL);

	if (pid == -1) {
		prompt_init(
			&editor->status_bar,
			strerror(errno),
			PT_INFO
		);

		return;
	}

	if (pid == 0) {
		execlp(shell, shell, "-i", NULL);

		_exit(1);
	}

	struct pollfd fds[2] = {
		{
			.fd = STDIN_FILENO,
			.events = POLLIN
		},

		{
			.fd = master,
			.events = POLLIN
		}
	};

	char buffer[4096];

	while (1) {
		int ret = poll(fds, 2, 100);

		if (ret == -1) {
			perror("poll");
			break;
		}

		if (fds[0].revents & POLLIN) {
			ssize_t n = read(STDIN_FILENO, buffer, sizeof buffer);

			if (n <= 0) {
				break;
			}

			write(master, buffer, n);
		}

		if (fds[1].revents & POLLIN) {
			ssize_t n = read(master, buffer, sizeof buffer);

			if (n <= 0) {
				break;
			}

			write(STDIN_FILENO, buffer, n);
		}

		int status;
		pid_t r = waitpid(pid, &status, WNOHANG);

		if (r == pid) {
			char buf[128];

			if (WIFEXITED(status)) {
				sprintf(buf, "exit: %d", WEXITSTATUS(status));

				prompt_init(
					&editor->status_bar,
					buf,
					PT_INFO
				);
			}

			else if (WIFSIGNALED(status)) {
				sprintf(buf, "exit: %d", WTERMSIG(status));

				prompt_init(
					&editor->status_bar,
					buf,
					PT_INFO
				);
			}

			break;
		}
	}

	close(master);

	mouse_on();	
}


/// --- SUSPEND ---

void editor_update_size(Editor* editor) {
	update_scroll(&editor->view, 
		editor->cursor.pos,
		editor_cursor_screen_x(editor),
		&editor->tsize);


	if (editor->has_window) {
		window_update_size(
			&editor->window,
			&editor->tsize
		);
	}

	prompt_update_size(&editor->status_bar, editor->tsize);
}

void editor_suspend(Editor* editor) {
	clean_terminal();
	disable_raw_mode();
	mouse_off();

	struct sigaction sa = {0};
	sa.sa_handler = SIG_DFL;

	sigaction(SIGTSTP, &sa, NULL);
	raise(SIGTSTP);

	enable_raw_mode();
	clean_terminal();
	mouse_on();

	update_terminal_size(&editor->tsize);

	editor_update_size(editor);

	editor->suspend = 0;
}


/// --- CURSOR ---

void editor_cursor_update(Editor* editor)
{
	size_t text_rows = editor->tsize.rows;

	while (editor->cursor.pos.y >= 
		text_rows + editor->view.row_offset)
	{
		editor->view.row_offset += text_rows;	
	}

	while (editor->cursor.pos.y  < editor->view.row_offset) {
		if (editor->view.row_offset >= text_rows) {
			editor->view.row_offset -= text_rows;
		} 

		else {
			editor->view.row_offset = 0;
		}
	}

	size_t text_cols = (editor->config.line_numbers)
		? editor->tsize.cols - get_gutter_width(
			file_num_lines(&editor->actual_file->file))
		: editor->tsize.cols;

	while (editor_cursor_screen_x(editor) >= 
		text_cols + editor->view.col_offset)
	{
		editor->view.col_offset += text_cols -
			HORIZONTAL_SCROLL_OVERLAP;		
	}

	while (editor_cursor_screen_x(editor)  < editor->view.col_offset) {
		if (editor->view.col_offset >= text_cols) {
			editor->view.col_offset -= text_cols
				- HORIZONTAL_SCROLL_OVERLAP;
		} 

		else {
			editor->view.col_offset = 0;
		}
	}


	editor_sync_cursor(editor);
}

void editor_cursor_move(Editor* editor, Position pos)
{
	size_t lines = file_num_lines(&editor->actual_file->file);

	if (pos.y >= lines) {
		return;
	}

	const Line* line = vector_get(&editor->actual_file->file.lines, pos.y);
	size_t line_size = u32string_size(&line->text);

	if (pos.x > line_size) {
		return;
	}

	editor->cursor.pos = pos;
	editor_cursor_update(editor);
}


static void editor_clean
(
	Editor* editor
)
{
	editor_selection_clear(editor);

	result_free(&editor->result);
	editor->result.reason = NULL;	
}



/// --- EDITOR FILE ---

void editor_change_actual_file
(
	Editor* editor,
	size_t index
)
{
	size_t files = editor->files.size;

	if (index >= files || index == editor->actual_file_index) {
		return;
	}

	EditorFile* ef = vector_get(&editor->files, index);

	if (!ef) {
		return;
	}

	/// --- SAVING USEFUL DATA AND RESETING OTHERS ---

	editor_clean(editor);
	editor->actual_file->cursor = editor->cursor;

	/// --- CHANGING ---

	editor->actual_file = ef; editor->actual_file_index = index;

	editor->cursor = editor->actual_file->cursor;
	editor_cursor_update(editor);

	editor_prompt_init(editor);

	editor_create_syntax(editor);
}

void editor_change_next_file
(
	Editor* editor
)
{
	if (editor->files.size <= 1) {
		return;
	}

	size_t index = editor->actual_file_index;

	if (index >= editor->files.size - 1) {
		index = 0;
	}

	else {
		index++;
	}

	editor_change_actual_file(editor, index);
}

void editor_change_prev_file
(
	Editor* editor
)
{
	if (editor->files.size <= 1) {
		return;
	}

	size_t index = editor->actual_file_index;

	if (index == 0) {
		index = editor->files.size - 1;
	}

	else {
		index--;
	}

	editor_change_actual_file(editor, index);
}

void editor_create_new_file
(
	Editor* editor
)
{
	EditorFile new_file;

	EditorFileOptions options = {
		.filename = NULL,
		.readonly = 0,
		.default_tab_size = editor->config.tab_size,
		.default_use_spaces = editor->config.use_spaces,
		.inotify_fd = -1
	};

	int ret = editor_file_open(
		&new_file,
		&options,
		NULL,
		&editor->result
	);

	if (ret < 0) {
		return;
	}

	if (editor->files.size == 0) {
		editor_clean(editor);
		vector_push(&editor->files, &new_file);

		editor->actual_file = vector_get(&editor->files, 0);
		editor->actual_file_index = 0;
		editor->cursor.pos = POS_ZERO;

		editor_cursor_update(editor);

		editor_prompt_init(editor);
	}

	else {
		size_t index = editor->actual_file_index;

		if (index < editor->files.size - 1) {
			vector_insert(&editor->files, index + 1, &new_file);
		}

		else {
			vector_push(&editor->files, &new_file);
		}

		editor_change_actual_file(editor, index + 1);
	}
}

int editor_create_internal_file
(
	Editor* editor,
	const char* name,
	Clipboard* content
)
{
	editor_create_new_file(editor);

	int ret = file_paste_clipboard(
		&editor->actual_file->file,
		content,
		&editor->cursor,
		&editor->result
	);

	if (ret < 0) {
		return ret;
	}

	editor_cursor_move(editor, POS_ZERO);
	
	editor->actual_file->file.dirty = 0;
	editor->actual_file->file.filename = (name != NULL)
		? strdup(name)
		: NULL;

	editor->actual_file->new_file = 0;
	editor->actual_file->internal = 1;

	editor->actual_file->readonly = 1;

	if (name) {
		editor_prompt_init(editor);
	}

	return EIE_OK;		
}

void editor_open_file
(
	Editor* editor, 
	const char* filename,
	int readonly
)
{
	EditorFile ef;

	EditorFileOptions options = {
		.filename = filename,
		.readonly = readonly,
		.default_tab_size = editor->config.tab_size,
		.default_use_spaces = editor->config.use_spaces,
		.inotify_fd = editor->inotify_fd
	};

	int ret = editor_file_open(
		&ef,
		&options,
		&editor->lang_plugins_data,
		&editor->result
	);

	if (ret < 0) {
		prompt_init(
			&editor->status_bar,
			strerror(errno),
			PT_INFO);

		return;
	}

	vector_insert(
		&editor->files,
		editor->actual_file_index + 1,
		&ef
	);

	editor_change_actual_file(editor, editor->actual_file_index + 1);
}


static void editor_file_quit(Editor* editor) {
	reset_color();

	if (editor->actual_file->file.dirty) {
		clean_terminal();
		char msg[] = "save before close?\n(y/n)";       
		write(STDOUT_FILENO, msg, strlen(msg));

		while (1) { // blocks the main thread

			struct event event = parser_read_key();

			if (event.type != EVENT_KEY) {
				continue;
			}

			struct normal_key key = event.key;

			if (key.content == U'y' || key.content == U'Y') {
				editor_save_file(editor);

				break;
			}

			if (key.content == U'n' || key.content == U'N') {
				break;
			}
		}
	}
}

void editor_close_file
(
	Editor* editor
)
{
	if (editor->files.size == 1 &&
		!editor->actual_file->file.filename &&
		file_num_lines(&editor->actual_file->file) == 1 &&
		file_size_line(&editor->actual_file->file, 0) == 0)
	{
		return;
	}

	// this flag is intended to make the file-closing operation
	// a bit more convenient for the user
	int untitled_with_something =
		!editor->actual_file->file.filename &&
		!(file_num_lines(&editor->actual_file->file) == 1 &&
		 file_size_line(&editor->actual_file->file, 0) == 0);

	if (untitled_with_something || editor->actual_file->file.filename) {
		editor_file_quit(editor);
	}

	size_t index = editor->actual_file_index;
	vector_remove_and_destroy(&editor->files, index);

	// editor->actual_file->watch_descriptor is not a file descriptor,
	// so not calling del_watcher does not result in a fd leak.
	// however, i thought it would be a good practice to call it
	// anyway
	editor_file_del_watcher(
		editor->actual_file,
		editor->inotify_fd
	);

	// switches from the current file to the previous one
	if (editor->files.size > 0) {
		editor_clean(editor);

		editor->actual_file_index = (index == 0)
			? editor->files.size - 1
			: index - 1;

		editor->actual_file = vector_get(&editor->files,
			editor->actual_file_index);

		editor->cursor = editor->actual_file->cursor;
		editor_cursor_update(editor);

		editor_prompt_init(editor);

		editor_create_syntax(editor);
	}

	// creates a new, internal and untitled file
	else {
		editor_create_new_file(editor);
	}
}



/// --- EDITOR CHANGE HANDLERS ---

void editor_handle_deleted(Editor* editor) {
	clean_terminal();
	hide_cursor();

	printf("%s\n\n", editor->actual_file->file.filename);

	char msg[] = "file has been deleted externally\n"
				 "what do you wanna do?\n\n";
	write(STDOUT_FILENO, msg, sizeof(msg));

	char other_msg[] = "- continue and create a new file later (1)\n"
					   "- close file (2)\n";
	write(STDOUT_FILENO, other_msg, sizeof(other_msg));

	while (1) {
		struct event event = parser_read_key();

		if (event.type != EVENT_KEY) {
			continue;
		}

		struct normal_key key = event.key;

		if (key.content == U'1') {
			editor->actual_file->externally_deleted = 0;
			editor->actual_file->file.dirty = 1;
			editor->actual_file->new_file = 1;
			editor->actual_file->internal = 1;

			editor_file_set_watcher(
				editor->actual_file,
				editor->actual_file->file.filename,
				editor->inotify_fd
			);

			show_cursor();
			return;
		}

		else if (key.content == U'2') {
			editor->actual_file->file.dirty = 0;
			editor_close_file(editor);
			show_cursor();
			return;
		}
	}
}

void editor_handle_moved_from(Editor* editor) {
	clean_terminal();
	hide_cursor();

	printf("%s\n\n", editor->actual_file->file.filename);

	char msg[] = "actual file has been moved externally\n"
				 "what do you wanna do?\n\n";
	write(STDOUT_FILENO, msg, sizeof(msg));

	char other_msg[] = "- continue and create a new file later (1)\n"
					   "- close file (2)\n";
	write(STDOUT_FILENO, other_msg, sizeof(other_msg));

	while (1) {
		struct event event = parser_read_key();

		if (event.type != EVENT_KEY) {
			continue;
		}

		struct normal_key key = event.key;

		if (key.content == U'1') {
			editor->actual_file->externally_moved_from = 0;
			editor->actual_file->file.dirty = 1;
			editor->actual_file->new_file = 1;
			editor->actual_file->internal = 1;

			editor_file_set_watcher(
				editor->actual_file,
				editor->actual_file->file.filename,
				editor->inotify_fd
			);

			show_cursor();
			return;
		}

		else if (key.content == U'2') {
			editor->actual_file->file.dirty = 0;
			editor_close_file(editor);
			show_cursor();
			return;
		}		
	}	
}


// helper for editor_handle_modified and editor_handle_attrib_changed
static int change_filename(Editor* editor, struct normal_key key) {
	if ((key.content == U'\n' || key.content == U'\r')) {
		char* new_filename = u32string_into_u8(
			&editor->status_bar.buf); 

		if (file_exists(new_filename)) {
			char msg[1024];
			sprintf(msg, 
			"%s already exists. try again\n", new_filename);

			prompt_init(&editor->status_bar,
				msg,
				PT_INFO);

			free(new_filename);
			return -1;
		}

		int new_fd = open(new_filename,
			O_RDWR | O_CREAT, 0644);

		if (new_fd < 0) {
			free(new_filename);
			return -2;
		}

		editor->actual_file->externally_deleted = 0;
		editor->actual_file->externally_moved_from = 0;
		editor->actual_file->externally_modified = 0;

		inotify_rm_watch(
			editor->inotify_fd,
			editor->actual_file->watch_descriptor);

		editor->actual_file->file.dirty = 1;
		editor->actual_file->file.filename = new_filename;

		file_save(
			&editor->actual_file->file,
			&editor->result
		);

		editor_file_update_metadata(
			editor->actual_file);

		char* absolute = absolute_parent_directory(
			new_filename
		);

		editor->actual_file->watch_descriptor = 
			inotify_add_watch(
			editor->inotify_fd,
			absolute,
			IN_CREATE |
			IN_MOVED_TO |
			IN_MOVED_FROM |
			IN_CLOSE_WRITE |
			IN_DELETE |
			IN_ATTRIB				
		);

		free(absolute);

		close(new_fd);

		editor_prompt_init(editor);

		return 0;
	}

	else if (u32_is_printable(key.content) && key.modifiers == 0) {
		u32string_push(&editor->status_bar.buf, key.content);
	} 

	else if (key.content == 127) {
		if (!u32string_is_empty(&editor->status_bar.buf)) {
			u32string_remove(
				&editor->status_bar.buf,
				u32string_size(&editor->status_bar.buf) - 1,
				NULL);
		}
	}

	prompt_drawn(
		&editor->status_bar,
		&editor->config.ui.status_bar,
		&editor->tsize);

	return 1;
}

void editor_handle_modified(Editor* editor) {
	clean_terminal();
	hide_cursor();

	char msg[] = "actual file has been modified externally\n"
				 "what do you wanna do?\n\n";
	write(STDOUT_FILENO, msg, sizeof(msg));

	char other_msg[] = "- sync file (1)\n- close file (2)\n"
					   "- create a new file (3)\n";
	write(STDOUT_FILENO, other_msg, sizeof(other_msg));

	int create = 0;

	while (1) {
		struct event event = parser_read_key();

		if (event.type != EVENT_KEY) {
			continue;
		}

		struct normal_key key = event.key;

		if (create) {
			int ret = change_filename(editor, key);

			if (ret == 0) {
				show_cursor();
				return;
			}

			else if (ret == 1) {
				continue;
			}

			else if (ret == 2) {
				prompt_init(
					&editor->status_bar,
					strerror(errno),
					PT_INFO);

				show_cursor();

				return;
			}
		}

		if (key.content == U'1') {
			editor_file_sync(editor->actual_file);
			show_cursor();
			return;
		}

		else if (key.content == U'2') {
			editor->actual_file->file.dirty = 0;
			editor_close_file(editor);
			show_cursor();
			return;
		}

		else if (key.content == U'3') {
			create = 1;
			continue;
		}
	}		
}


void editor_handle_attrib_changed(Editor* editor) {
	editor->actual_file->externally_attrib_changed = 0;

	char* filename = editor->actual_file->file.filename;
	int fd = open(filename, O_RDWR);

	if (fd < 0) {
		fd = open(filename, O_RDONLY);

		if (fd < 0) {
			close(fd);
			clean_terminal();
			hide_cursor();

			char msg[] = 
			"file's permissions have changed and you cannot read it anymore.\n"
			"what do you wanna do?\n\n";

			write(STDOUT_FILENO, msg, sizeof(msg));

			char other_msg[] = "- create a new file (1)\n"
							   "- close file (2)\n";

			write(STDOUT_FILENO, other_msg, sizeof(other_msg));

			int change = 0;

			while (1) {
				struct event event = parser_read_key();

				if (event.type != EVENT_KEY) {
					continue;
				}

				struct normal_key key = event.key;

				if (change) {
					int ret = change_filename(editor, key);

					if (ret == 1) {
						continue;
					}

					if (ret == 0) {
						return;
					}

					else {
						prompt_init(
							&editor->status_bar,
							strerror(errno),
							PT_INFO
						);

						return;
					}
				}

				if (key.content == U'1') {
					show_cursor();

					prompt_init(
						&editor->status_bar,
						"filename: ",
						PT_INTERACTIVE);

					char msg[] = "\ninsert a new filename at the prompt "
								 "below\n";

					write(STDOUT_FILENO, msg, sizeof(msg));

					prompt_drawn(
						&editor->status_bar,
						&editor->config.ui.status_bar,
						&editor->tsize);

					free(filename);
					change = 1;

					filename = NULL;

					editor->actual_file->file.filename = NULL;

					continue;
				}

				else if (key.content == U'2') {
					editor->actual_file->file.dirty = 0;
					editor_close_file(editor);
					show_cursor();
					return;
				}
			}
		}

		if (!editor->actual_file->readonly && 
			editor->actual_file->file.dirty) 
		{
			clean_terminal();
			hide_cursor();

			char msg[] = 
			"file's permissions have changed and you cannot modify it anymore.\n\n"
			"this message is appearing because you were modifying the \n"
			"file and the changes made haven't been saved, so\n"
			"what do you wanna do?\n\n";

			write(STDOUT_FILENO, msg, sizeof(msg));

			char other_msg[] = "- create a new file (1)\n"
							   "- close file (2)\n"
							   "- reopen file as readonly (3)\n";

			write(STDOUT_FILENO, other_msg, sizeof(other_msg));

			int change = 0;

			while (1) {
				struct event event = parser_read_key();

				if (event.type != EVENT_KEY) {
					continue;
				}

				struct normal_key key = event.key;

				if (change) {
					int ret = change_filename(editor, key);

					if (ret == 0) {
						return;
					}

					else if (ret == 1) {
						continue;
					}

					else {
						prompt_init(
							&editor->status_bar,
							strerror(errno),
							PT_INFO);

						return;
					}
				}

				if (key.content == U'1') {
					show_cursor();

					free(filename);
					filename = NULL;

					editor->actual_file->file.filename = NULL;

					change = 1;
					continue;
				}

				else if (key.content == U'2') {
					editor->actual_file->file.dirty = 0;
					editor_close_file(editor);
					show_cursor();
					return;
				}

				else if (key.content == U'3') {
					show_cursor();
					editor->actual_file->file.dirty = 0;

					filename = strdup(editor->actual_file->file.filename);
					editor_close_file(editor);
					editor_open_file(editor, filename, 1);

					free(filename);
					return;
				}
			}
		}

		else {
			editor->actual_file->readonly = 1;
		}
	}
}



/// --- SAVE ---

// helper for editor_save_file
static int editor_handle_create_file(Editor* editor, struct normal_key key) {
	if ((key.content == U'\n' || key.content == U'\r')) {
		char* filename = u32string_into_u8(
			&editor->status_bar.buf); 

		if (file_exists(filename)) {
			char msg[1024];
			sprintf(msg, "%s already exists. try again\n", filename);

			prompt_init(&editor->status_bar,
				msg,
				PT_INFO);

			free(filename);
			return 0;
		}

		int fd = open(filename, // TODO
			O_RDWR | O_CREAT, 0644);

		if (fd < 0) {
			free(filename);
			return 2;
		}

		editor_file_set_lang_plugin(
			editor->actual_file,
			filename,
			&editor->lang_plugins_data);

		if (editor->actual_file->file.filename) {
			free(editor->actual_file->file.filename);
		}

		editor->actual_file->file.filename = filename;

		editor_prompt_init(editor);

		close(fd);

		return 0;
	}

	else if (key.content == editor->config.keybinds[ACTION_QUIT].content &&
		key.modifiers == editor->config.keybinds[ACTION_QUIT].modifiers) 
	{
		editor->status_bar.active = 0;

		return -1;
	} 

	else if (u32_is_printable(key.content) && key.modifiers == 0) {
		u32string_push(&editor->status_bar.buf, key.content);
	} 

	else if (key.content == 127) {
		if (!u32string_is_empty(&editor->status_bar.buf)) {
			u32string_remove(
				&editor->status_bar.buf,
				u32string_size(&editor->status_bar.buf) - 1,
				NULL);
		}
	}

	return 1;	
}

int editor_save_file(Editor* editor) {
	if (editor->actual_file->readonly) {
		prompt_init(
			&editor->status_bar,
			PROMPT_READONLY,
			PT_INFO);

		return EIE_NOT_FATAL_ERROR;
	}

	if (!editor->actual_file->file.filename &&
		editor->actual_file->new_file) 
	{
		clean_terminal();
		hide_cursor();

		char msg[] = "file does not exit\n" 
					 "create file?\n(y/n)";

		write(STDOUT_FILENO, msg, sizeof(msg));

		int create = 0;

		while (1) {
			struct event event = parser_read_key();

			if (event.type != EVENT_KEY) {
				continue;
			}

			struct normal_key key = event.key;

			if (create) {
				int ret = editor_handle_create_file(editor, key);

				prompt_drawn(
					&editor->status_bar, 
					&editor->config.ui.status_bar,
					&editor->tsize);

				if (ret == 0) {
					editor->actual_file->internal = 0;
					editor->actual_file->new_file = 0;
					goto save;
				}

				else if (ret == 1) {
					continue;
				}

				else if (ret == 2) {
					prompt_init(
						&editor->status_bar,
						strerror(errno),
						PT_INFO);

					show_cursor();

					return EIE_NOT_FATAL_ERROR;
				}

				else {
					show_cursor();
					return EIE_NOT_AN_ERROR;
				}
			}

			if (key.content == U'y' || key.content == U'Y') {
				char msg[] = 
					"\n\nenter the filename at the prompt below\n";
				write(STDOUT_FILENO, msg, sizeof(msg));

				prompt_init(
					&editor->status_bar, 
					"filename: ", 
					PT_INTERACTIVE);

				create = 1;

				prompt_drawn(
					&editor->status_bar, 
					&editor->config.ui.status_bar,
					&editor->tsize);

				continue;
			}

			if (key.content == U'n' || key.content == U'N') {
				show_cursor();
				return EIE_NOT_AN_ERROR;
			}
		}

		return 0;
	}

save:
	show_cursor();
	int ret = file_save(&editor->actual_file->file, &editor->result);

	if (ret < 0) {
		return ret;
	}

	// if (ret == 0) {
	// 	prompt_init(&editor->status_bar, "saved", PT_INFO);
	// }

	if (editor->actual_file->watch_descriptor < 0) {
		editor_file_set_watcher(
			editor->actual_file,
			editor->actual_file->file.filename,
			editor->inotify_fd
		);
	}

	if (editor->actual_file->new_file) {
		editor->actual_file->new_file = 0;
	}

	if (editor->actual_file->internal) {
		editor->actual_file->internal = 0;
	}

	editor_file_update_metadata(editor->actual_file);

	if (editor->debug_mode) {
		log_write(&editor->log, "editor_save_file: SUCCESS");
	}

	return ret;
}



/// --- QUIT ---

static int show_and_handle_dirty_files(Editor* editor) {
	size_t first_dirty;
	int any_dirty = 0;

	for (size_t i = 0; i < editor->files.size; i++) {
		EditorFile* ef = vector_get(&editor->files, i);

		if (ef->file.dirty) {
			if (!ef->file.filename && 
				file_num_lines(&ef->file) == 1 && 
				file_size_line(&ef->file, 0) == 0) 
			{
				continue;
			}

			if (!any_dirty) {
				first_dirty = i;

				clean_terminal();
				hide_cursor();

				char msg[] = "the following files haven't been saved:\n\n";
				write(STDOUT_FILENO, msg, strlen(msg));

				any_dirty = 1;
			}

			write(STDOUT_FILENO, "- ", 2);

			if (ef->file.filename) {
				write(
					STDOUT_FILENO, 
					ef->file.filename, 
					strlen(ef->file.filename));
			}

			else {
				char filename[] = "untitled (internal)";

				write(STDOUT_FILENO, filename, strlen(filename));
			}

			write(STDOUT_FILENO, "\n", 1);
		}
	}

	if (any_dirty) {
		char msg[] = "\nwhat do you wanna do?\n\n";
		write(STDOUT_FILENO, msg, strlen(msg));

		char other_msg[] = "- quit anyway (1)\n"
						   "- save all, but untitled files, and quit (2)\n"
						   "- manually save them (3)\n";

		write(STDOUT_FILENO, other_msg, strlen(other_msg));

		while (1) { // blocks the main thread

			struct event event = parser_read_key();

			if (event.type != EVENT_KEY) {
				continue;
			}

			struct normal_key key = event.key;

			if (key.content == U'3') {
				if (editor->actual_file_index != first_dirty) {
					editor_change_actual_file(editor, first_dirty);
				}

				show_cursor();
				return 1;
			}

			if (key.content == U'1') {
				show_cursor();
				break;
			}

			if (key.content == U'2') {
				for (size_t i = first_dirty; i < editor->files.size; i++) {
					EditorFile* ef = vector_get(&editor->files, i);

					if (ef->file.dirty && ef->file.filename) {
						editor->actual_file = ef;

						editor_save_file(editor);
					}
				}

				show_cursor();
				break;
			}
		}
	}

	return 0;
}

int editor_quit(Editor* editor) {
	reset_color();

	int ret = 0;

	if (!editor->config.auto_save_quit) {
		ret = show_and_handle_dirty_files(editor);
	}

	else {
		for (size_t i = 0; i < editor->files.size; i++) {
			EditorFile* ef = vector_get(&editor->files, i);

			if (ef->file.dirty && ef->file.filename) {
				file_save(&ef->file, &editor->result);
			}			
		}
	}

	if (ret == 1) {
		return 1;
	}

	if (editor->debug_mode) {
		log_write(&editor->log, "editor_quit: SUCCESS");
	}

	return 0;
}



/// --- SELECT ---

void editor_start_and_create_selection(Editor* editor) {
	if (editor->sel.active) {
		editor_selection_clear(editor);
	} 

	else {
		prompt_init(
			&editor->status_bar,
			"selection: active (right-click or ACTION_SELECTION to cancel)",
			PT_INFO);

		selection_start(&editor->sel, editor->cursor.pos);
		editor->selecting = 1;

		if (editor->debug_mode) {
			log_write(&editor->log, 
				"editor_start_and_create_selection: SUCCESS");
		}
	}
}


void editor_selection_clear(Editor* editor) {
	selection_clear(&editor->sel);
	editor->selecting = 0;
}

void editor_selection_complete(Editor* editor) {
	if (!editor->sel.active) {
		return;
	}

	editor->selecting = 0;

	Position a, b;
	selection_normalize(&editor->sel, &a, &b);

	size_t line_size = file_size_line(
			&editor->actual_file->file,
			b.y);

	size_t lines = file_num_lines(&editor->actual_file->file);
	int select_after = (lines > 1 && b.y < lines - 1);

	if (selection_start_before_end(&editor->sel)) {
		editor->sel.start.x = 0;

		editor->sel.end = (select_after)
			? (Position) { 0, b.y + 1 }
			: (Position) { line_size, b.y };

		editor_cursor_move(editor, editor->sel.end);
	}

	else {
		editor->sel.end.x = 0;

		editor->sel.start = (select_after)
			? (Position) { 0, b.y + 1 }
			: (Position) { line_size, b.y };

		editor_cursor_move(editor, editor->sel.start);
	}

}


int editor_select_line
(
	Editor* editor
) 
{
	if (editor->sel.active) {
		editor_selection_clear(editor);

		return 0;
	}

	else {
		size_t y = editor->cursor.pos.y;

		int ret = file_select_line(
			&editor->actual_file->file, 
			y, 
			&editor->sel,
			&editor->result);

		if (ret < 0) {
			return ret;
		}

		editor->selecting = 0;

		editor_cursor_move(editor, editor->sel.end);

		if (editor->debug_mode) {
			log_write(&editor->log, "editor_select_line: SUCCESS");
		}

		return 0;
	}	
}

int editor_select_all_file(Editor* editor) {
	if (editor->sel.active) {
		editor_selection_clear(editor);

		return 0;
	} 

	else {
		int ret = file_select_all_file(
			&editor->actual_file->file, 
			&editor->sel, 
			&editor->result);

		if (ret < 0) {
			return ret;
		}

		if (ret == EIE_NOT_AN_ERROR) {
			return 0;
		}

		editor->selecting = 0;

		size_t last = file_num_lines(&editor->actual_file->file) - 1;

		editor_cursor_move(editor,
			(Position) { 
				file_size_line(&editor->actual_file->file, last),
				last});

		if (editor->debug_mode) {
			log_write(&editor->log, "editor_select_all_file: SUCCESS");
		}

		return 0;
	}	
}

void editor_select_word(Editor* editor) {
	size_t start = 0;
	size_t end = 0;
	size_t y = editor->cursor.pos.y;
	size_t x = editor->cursor.pos.x;

	const u32string* line = file_get_line_text(&editor->actual_file->file, y);

	size_t size = u32string_size(line);

	if (x == size) {
		return;
	}

	if (is_word_char(u32string_char(line, x))) {
		for (size_t i = x; i-- > 0;) {
			if (!is_word_char(u32string_char(line, i))) {
				start = i + 1;
				break;
			}
		}

		for (size_t i = x; i < size; i++) {
			if (!is_word_char(u32string_char(line, i))) {
				end = i;
				break;
			}

			if (i == (size - 1)) {
				end = size;
				break;
			}
		}
	}

	else {
		for (size_t i = x; i-- > 0;) {
			if (is_word_char(u32string_char(line, i))) {
				start = i + 1;
				break;
			}
		}

		for (size_t i = x; i < size; i++) {
			if (is_word_char(u32string_char(line, i))) {
				end = i;
				break;
			}

			if (i == (size - 1)) {
				end = size;
			}
		}



		if (size > 1 && end == 0) {
			end = (x == size - 1) ?
				x : x + 1;
		}
	}


	if (start != end) {
		editor->sel.active = 1;
		editor->selecting = 0;
		editor->sel.start.x = start;
		editor->sel.end.x = end;
		editor->sel.start.y = y;
		editor->sel.end.y = y;
	} else {
		editor_selection_clear(editor);
	}
}



/// --- COPY AND PASTE ---

int editor_copy_selection(Editor* editor) {
	int ret = EIE_OK;

	if (editor->sel.active) {
		ret = file_copy_selection(&editor->actual_file->file, 
			&editor->cb, 
			&editor->sel,
			&editor->result);

		if (CB_BACKEND != CLIPBOARD_NONE) {
			clipboard_set(&editor->cb.text);
		}

		// editor_selection_clear(editor); --- OPTIONAL ---
		editor->selecting = 0;
	}

	if (ret < 0) {
		return ret;
	}

	if (editor->debug_mode) {
		log_write(&editor->log, "editor_copy_selection: SUCCESS");
	}

	return ret;
}

int editor_paste_clipboard(Editor* editor) {
	if (editor->actual_file->readonly) {
		prompt_init(
			&editor->status_bar,
			PROMPT_READONLY,
			PT_INFO);

		return EIE_NOT_FATAL_ERROR;
	}

	if (CB_BACKEND != CLIPBOARD_NONE) {
		u32string_free(&editor->cb.text);
		editor->cb.text = clipboard_get();
	}

	if (!u32string_is_empty(&editor->cb.text)) {
		Position cursor_remove = editor->cursor.pos;

		int ret = file_paste_clipboard(
			&editor->actual_file->file, 
			&editor->cb, 
			&editor->cursor, 
			&editor->result);

		editor_cursor_update(editor);

		if (ret < 0) {
			return ret;
		}

		if (ret == EIE_NOT_AN_ERROR) {
			return 0;
		}

		Position cursor_insert = editor->cursor.pos;
		Position start = cursor_remove;
		Position end = editor->cursor.pos;

		u32string text = u32string_clone(&editor->cb.text);

		Operation op = operation_create_insert(
			start, end,
			cursor_remove, cursor_insert,
			text
		);

		stack_clear(&editor->actual_file->redo);
		stack_push(&editor->actual_file->undo, &op);
	}

	if (editor->debug_mode) {
		log_write(&editor->log, "editor_paste_clipboard: SUCCESS");
	}

	return 0;
}



/// --- MOVE_CURSOR ---

void editor_move_cursor_right(Editor* editor) 
{
	if (editor->cursor.pos.x > 
		file_size_line(&editor->actual_file->file, editor->cursor.pos.y)) 
	{
		return;
	}

	if (editor->sel.active && !editor->selecting) {
		editor_selection_clear(editor);
	}

	editor_cursor_move(
		editor, 
		(Position) { editor->cursor.pos.x + 1, editor->cursor.pos.y });

	if (editor->selecting) {
		selection_update(&editor->sel, editor->cursor.pos);
	}

	if (editor->debug_mode) {
		log_write(&editor->log, "editor_move_cursor_right: SUCCESS");
	}   
}

void editor_move_cursor_left(Editor* editor) {
	if (editor->cursor.pos.x == 0) {
		return;
	}

	if (editor->sel.active && !editor->selecting) {
		editor_selection_clear(editor);
	}

	editor_cursor_move(
		editor,
		(Position) { editor->cursor.pos.x - 1, editor->cursor.pos.y });

	if (editor->selecting) {
		selection_update(&editor->sel, editor->cursor.pos);
	}

	if (editor->debug_mode) {
		log_write(&editor->log, "editor_move_cursor_left: SUCCESS");
	}    
}

void editor_move_cursor_up(Editor* editor) {
	if (editor->cursor.pos.y == 0) {
		return;
	}

	if (editor->sel.active && !editor->selecting) {
		editor_selection_clear(editor);
	}

	size_t y = editor->cursor.pos.y - 1;

	const u32string* text = file_get_line_text(
		&editor->actual_file->file,
		y
	);

	size_t x = screen_to_file_x(
		text,
		editor->cursor.preferred_column,
		editor->config.tab_size
	);

	editor_cursor_move(editor, (Position) { x, y } );

	if (editor->selecting) {
		selection_update(&editor->sel, editor->cursor.pos);
	}

	if (editor->debug_mode) {
		log_write(&editor->log, "editor_move_cursor_up: SUCCESS");
	}
}

void editor_move_cursor_down(Editor* editor) 
{
	if (editor->cursor.pos.y >= file_num_lines(&editor->actual_file->file) - 1) {
		return;
	}

	if (editor->sel.active && !editor->selecting) {
		editor_selection_clear(editor);
	}

	size_t y = editor->cursor.pos.y + 1;

	const u32string* text = file_get_line_text(
		&editor->actual_file->file,
		y
	);

	size_t x = screen_to_file_x(
		text,
		editor->cursor.preferred_column,
		editor->config.tab_size
	);

	editor_cursor_move(editor, (Position) { x, y } );

	if (editor->selecting) {
		selection_update(&editor->sel, editor->cursor.pos);
	}

	if (editor->debug_mode) {
		log_write(&editor->log, "editor_move_cursor_down: SUCCESS");
	}	
}

void editor_move_cursor_beginning_line(Editor* editor) {
	if (editor->sel.active && !editor->selecting) {
		editor_selection_clear(editor);
	}

	editor_cursor_move(
		editor,
		(Position) { 0, editor->cursor.pos.y } );

	if (editor->sel.active) {
		editor->sel.end.x = 0;
	}

	if (editor->debug_mode) {
		log_write(&editor->log, 
			"editor_move_cursor_to_beginning_of_line: SUCCESS");
	}
}

void editor_move_cursor_indent_line(Editor* editor) 
{
	if (editor->sel.active && !editor->selecting) {
		editor_selection_clear(editor);
	}

	u32string* line = file_get_line_text(
		&editor->actual_file->file, 
		editor->cursor.pos.y);

	size_t indent = u32string_get_indent(
		line,
		editor->actual_file->file.use_spaces);

	editor->cursor.pos.x = indent;

	editor_cursor_move(
		editor,
		(Position) { indent, editor->cursor.pos.y });

	if (editor->sel.active) {
		editor->sel.end.y = indent;
	}

	if (editor->debug_mode) {
		log_write(&editor->log, 
			"editor_move_cursor_to_indent_of_line: SUCCESS");
	}
}

void editor_move_cursor_end_line(Editor* editor) 
{
	if (editor->sel.active && !editor->selecting) {
		editor_selection_clear(editor);
	}

	size_t line_size = file_size_line(
		&editor->actual_file->file, 
		editor->cursor.pos.y);

	editor_cursor_move(
		editor,
		(Position) { line_size, editor->cursor.pos.y });

	if (editor->sel.active && editor->selecting) {
		editor->sel.end.x = line_size;
	}

	if (editor->debug_mode) {
		log_write(&editor->log, 
			"editor_move_cursor_to_end_of_line: SUCCESS");
	}
}



/// --- SCROLL ---

void editor_scroll_up(Editor* editor) {
	if (editor->view.row_offset > 0) {
		editor->view.row_offset--;

		if (editor->config.cursor_follow_scroll &&
			editor->cursor.pos.y > editor->view.row_offset + 
			editor->tsize.rows - 1) 
		{
			editor->cursor.pos.y--;
			editor->cursor.pos.x = 0;
		}

		if (editor->debug_mode) {
			log_write(&editor->log, "editor_scroll_up: SUCCESS");
		}
	}
}

void editor_scroll_left(Editor* editor) {
	if (editor->view.col_offset > 0) {
		editor->view.col_offset--;

		if (editor->config.cursor_follow_scroll) {
			editor_clamp_cursor_to_view(editor);
		}
	}
}

void editor_scroll_down(Editor* editor) {
	size_t lines = file_num_lines(&editor->actual_file->file);

	if (editor->view.row_offset + editor->tsize.rows < lines) {
		editor->view.row_offset++;

		if (editor->config.cursor_follow_scroll &&
			editor->cursor.pos.y < editor->view.row_offset) 
		{
			editor->cursor.pos.y++;
			editor->cursor.pos.x = 0;
		}

		if (editor->debug_mode) {
			log_write(&editor->log, 
				"editor_scroll_down: SUCCESS");
		}
	}	
}

void editor_scroll_right(Editor* editor) {
	if (editor->view.col_offset == file_size_line(
		&editor->actual_file->file, editor->cursor.pos.y))
	{
		return;
	}

	editor->view.col_offset++;

	if (editor->config.cursor_follow_scroll) {
		editor_clamp_cursor_to_view(editor);
	}
}

void editor_scroll_up_terminal_size(Editor* editor) {
	if (editor->view.row_offset > 0) {
		size_t delta = editor->tsize.rows;

		if (delta > editor->view.row_offset) {
			delta = editor->view.row_offset;
		}

		editor->view.row_offset -= delta;

		if (editor->config.cursor_follow_scroll) {
			if (editor->cursor.pos.y >= delta) {
				editor->cursor.pos.y -= delta;
			} else {
				editor->cursor.pos.y = 0;
			}
		}

		if (editor->debug_mode) {
			log_write(&editor->log, 
				"editor_scroll_up_terminal_size: SUCCESS");
		}
	}
}

void editor_scroll_down_terminal_size
(
	Editor* editor
)
{
	size_t lines = file_num_lines(&editor->actual_file->file);

	size_t max_offset = (lines > editor->tsize.rows) 
		? lines - editor->tsize.rows : 0;

	if (editor->view.row_offset < max_offset) {
		size_t delta = editor->tsize.rows;

		if (editor->view.row_offset + delta > max_offset) {
			delta = max_offset - editor->view.row_offset;
		}

		editor->view.row_offset += delta;

		if (editor->config.cursor_follow_scroll) {
			editor->cursor.pos.y += delta;

			if (editor->cursor.pos.y >= lines) {
				editor->cursor.pos.y = lines - 1;
			}
		}

		if (editor->debug_mode) {
			log_write(&editor->log, 
				"editor_scroll_down_terminal_size: SUCCESS");
		}
	}
}



/// --- MOVE_BETWEEN CALLBACKS ---

int is_alnum(uint32_t c) {
	return isalnum((unsigned char) c);
}

int is_word_char(uint32_t c) {
	return isalnum((unsigned char) c) ||
		   ispunct((unsigned char) c);	
}

/// --- MOVE_BETWEEN ---

void editor_move_between_words_right
(
	Editor* editor,
	WordFinder finder
) 
{
	size_t end = 0;
	size_t y = editor->cursor.pos.y;
	size_t x = editor->cursor.pos.x;

	const u32string* line = file_get_line_text(&editor->actual_file->file, y);
	size_t size = u32string_size(line);

	if (x == size) {
		return;
	}

	if (finder(u32string_char(line, x))) {
		for (size_t i = x; i < size; i++) {
			if (!finder(u32string_char(line, i))) {
				end = i;
				break;
			}

			if (i == (size - 1)) {
				end = size;
			}
		}
	} 

	else {
		for (size_t i = x; i < size; i++) {
			if (finder(u32string_char(line, i))) {
				end = i;
				break;
			}

			if (i == (size - 1)) {
				end = size;
			}
		}
	}

	// if (end == 0) {
	// 	return;
	// }

	editor->cursor.pos.x = end;

	selection_update(&editor->sel, editor->cursor.pos);
}

void editor_move_between_words_left
(
	Editor* editor,
	WordFinder finder
) 
{
	if (editor->cursor.pos.x == 0) {
		return;
	}

	size_t start = 0;
	size_t y = editor->cursor.pos.y;
	size_t x = editor->cursor.pos.x;

	const u32string* line = file_get_line_text(&editor->actual_file->file, y);

	if (finder(u32string_char(line, x - 1))) {
		for (size_t i = x; i-- > 0;) {
			if (!finder(u32string_char(line, i))) {
				start = i + 1;
				break;
			}
		}
	} 

	else {
		for (size_t i = x; i-- > 0;) {
			if (finder(u32string_char(line, i))) {
				start = i + 1;
				break;
			}
		}
	}

	// if (start == u32string_size(line) - 1) {
	// 	return;
	// }

	editor->cursor.pos.x = start;

	selection_update(&editor->sel, editor->cursor.pos);
}


/// --- OPERATION ---

void editor_operation_insert
(
	Editor* editor,
	const Operation* op
)
{
	if (u32string_is_empty(&op->insert.text)) {
		return;
	}

	editor->cursor.pos = op->cursor_remove;

	Clipboard cb;
	cb.text = u32string_clone(&op->insert.text);
	cb.linewise = u32string_char(
		&op->insert.text,
		u32string_size(&op->insert.text) - 1) == U'\n';

	file_paste_clipboard(
		&editor->actual_file->file,
		&cb,
		&editor->cursor,
		&editor->result);

	clipboard_free(&cb);

	editor_cursor_move(editor, op->cursor_insert);
}

void editor_operation_remove
(
	Editor* editor,
	const Operation* op
)
{
	Selection sel;
	selection_start(&sel, POS_ZERO);

	sel.start = op->delete.start;
	sel.end = op->delete.end;

	editor->cursor.pos = op->cursor_insert;

	file_delete_selection(
		&editor->actual_file->file, 
		&sel, 
		&editor->result);

	editor_cursor_move(editor, op->cursor_remove);
}

void editor_operation_indent
(
	Editor* editor,
	const Operation* op
)
{
	size_t start = op->unindent.start.y;
	size_t end = op->unindent.end.y;

	int move = start <= editor->cursor.pos.y ||
			   editor->cursor.pos.y <= end;

	if (start != end) {
		file_indent_selection(
			&editor->actual_file->file,
			&editor->sel,
			&editor->result
		);
	}

	else {
		file_indent_a_line(
			&editor->actual_file->file,
			op->indent.start.y,
			&editor->result
		);

		if (editor->sel.active) {
			size_t tab_size = editor->actual_file->file.tab_size;

			if (editor->sel.start.y == start) {
				editor->sel.start.x += tab_size;
			}

			if (editor->sel.end.y == start) {
				editor->sel.end.x += tab_size;
			}
		}
	}

	if (move) {
		editor_cursor_move(editor, op->cursor_insert);
	}
}

void editor_operation_unindent
(
	Editor* editor,
	const Operation* op
)
{
	size_t start = op->unindent.start.y;
	size_t end = op->unindent.end.y;

	int move = start <= editor->cursor.pos.y ||
			   editor->cursor.pos.y <= end;

	if (start != end) {
		file_unindent_selection(
			&editor->actual_file->file,
			&editor->sel,
			&editor->result
		);
	}

	else {
		file_unindent_a_line(
			&editor->actual_file->file,
			start,
			&editor->result
		);

		if (editor->sel.active) {
			size_t tab_size = 
				(editor->actual_file->file.use_spaces)
				? editor->actual_file->file.tab_size
				: 1;

			size_t move = 0;

			u32string* text = file_get_line_text(
				&editor->actual_file->file,
				start);

			size_t indent = u32string_get_indent(
				text,
				editor->actual_file->file.use_spaces);

			move = (indent >= tab_size)
				? tab_size
				: indent;

			if (editor->sel.start.y == start) {
				editor->sel.start.x -= move;
			}

			if (editor->sel.end.y == start) {
				editor->sel.end.x -= move;
			}
		}
	}

	if (move) {
		editor_cursor_move(editor, op->cursor_remove);	
	}
}

void editor_operation_comment
(
	Editor* editor,
	const Operation* op
) 
{
	if (!editor->actual_file->language) {
		return;
	}

	const struct language_rules* rules = editor->actual_file->language->rules;

	if (!rules) {
		return;
	}

	const char* comment_fmt = rules->comment_fmt;

	if (!comment_fmt) {
		return;
	}

	size_t start = op->comment.start.y;
	size_t end = op->comment.end.y;
	ssize_t move_cursor;

	if (start != end) {
		file_comment_selection(
			&editor->actual_file->file,
			&editor->sel,
			comment_fmt,
			editor->cursor.pos.y,
			&move_cursor
		);
	}

	else {
		file_comment_line(
			&editor->actual_file->file,
			op->comment.start.y,
			comment_fmt,
			&move_cursor,
			&editor->result
		);
	}


	if (move_cursor < 0) {
		editor_cursor_move(editor, op->cursor_remove);
	}

	else {
		editor_cursor_move(editor, op->cursor_insert);
	}
}

// helper for editor_operation_replace
static void delete_from_replacements
(
	Editor* editor,
	const u32string* pattern,
	Vector* replacements
)
{
	size_t pattern_size = u32string_size(pattern);
	size_t y = 0;
	size_t deletes = 0;

	int first = 1;
	u32string* line_text = vector_get(&editor->actual_file->file.lines, y);

	Vector new_replacements;
	vector_init(&new_replacements, sizeof(Position), NULL);

	for (size_t i = 0; i < replacements->size; i++) {
		const Position* pos = vector_get_const(replacements, i);

		if (y != pos->y) {
			y = pos->y;
			line_text = vector_get(&editor->actual_file->file.lines, y);

			deletes = 0;
			first = 1;
		}

		if (first) {
			file_set_line_dirty(&editor->actual_file->file, y);
			first = 0;
		}

		size_t offset = deletes * pattern_size;
		size_t begin = pos->x - offset;
		size_t end = pos->x + pattern_size - offset;

		Position new_pos = (Position) { begin, y };

		vector_push(&new_replacements, &new_pos);

		u32string_remove_range(
			line_text,
			begin,
			end
		);

		deletes++;
	}

	assert(replacements->size == new_replacements.size);

	vector_free(replacements);
	*replacements = new_replacements;
}

// helper for editor_operation_replace
static void replace_from_replacements
(
	Editor* editor,
	const u32string* pattern,
	const u32string* text,
	Vector* replacements
)
{
	size_t pattern_size = u32string_size(pattern);
	size_t text_size = u32string_size(text);
	size_t y = 0;
	size_t replaces = 0;
	ssize_t size_offset = text_size - pattern_size;

	int first = 1;
	u32string* line_text = vector_get(&editor->actual_file->file.lines, y);

	Vector new_replacements;
	vector_init(&new_replacements, sizeof(Position), NULL);

	for (size_t i = 0; i < replacements->size; i++) {
		const Position* pos = vector_get_const(replacements, i);

		if (y != pos->y) {
			y = pos->y;
			line_text = vector_get(&editor->actual_file->file.lines, y);

			replaces = 0;
			first = 1;
		}

		if (first) {
			file_set_line_dirty(&editor->actual_file->file, y);
			first = 0;
		}

		ssize_t replace_offset = replaces * size_offset;
		size_t begin = pos->x + replace_offset;
		size_t end = pos->x + pattern_size + replace_offset;

		Position new_pos = (Position) { begin, y };

		vector_push(&new_replacements, &new_pos);

		u32string_remove_range(
			line_text,
			begin,
			end
		);

		u32string_insert_range_raw(
			line_text,
			begin,
			u32string_into_ptr_const(text),
			text_size);

		replaces++;
	}

	assert(replacements->size == new_replacements.size);
	vector_free(replacements);
	*replacements = new_replacements;
}

void editor_operation_replace
(
	Editor* editor,
	Operation* op
)
{
	if (u32string_is_empty(&op->replace.old_text) &&
		u32string_is_empty(&op->replace.new_text))
	{
		return;
	}

	if (u32string_is_empty(&op->replace.new_text)) {
		delete_from_replacements(
			editor,
			&op->replace.old_text,
			&op->replace.replacements
		);
	}

	else {
		replace_from_replacements(
			editor,
			&op->replace.old_text,
			&op->replace.new_text,
			&op->replace.replacements
		);
	}

	size_t pattern_size = u32string_size(&op->replace.old_text);
	size_t text_size = u32string_size(&op->replace.new_text);

	ssize_t offset = text_size - pattern_size;

	if (editor->sel.active) {
		editor->sel.end.x += offset;
	}

	editor->cursor.pos.x += offset;
	editor_cursor_update(editor);
}

void editor_operation_linemove
(
	Editor* editor,
	Operation* op
)
{
	size_t start = op->linemove.start.y;
	size_t end = op->linemove.end.y;

	log_write(&editor->log, "%zu, %zu", start, end);

	if (op->linemove.direction == LINEMOVE_UP) {
		if (start != end) {
			for (size_t i = start; i < end + 1; i++) {
				file_move_line_up(
					&editor->actual_file->file,
					i,
					&editor->result);
			}
		}

		else {
			file_move_line_up(
				&editor->actual_file->file,
				start,
				&editor->result);
		}

		if (editor->sel.active) {
			if (editor->sel.start.y > 0) {
				editor->sel.start.y--;
				editor->sel.end.y--;	
			}
		}

		if (op->linemove.start.y > 0) {
			op->linemove.start.y--;
			op->linemove.end.y--;

			if (editor->sel.active) {
				editor->sel.start = op->linemove.start;
				editor->sel.end = op->linemove.end;
			}
		}

		editor_cursor_move(editor, op->cursor_remove);
	}

	else {
		if (start != end) {
			size_t index = end;

			while (1) {
				file_move_line_down(
					&editor->actual_file->file,
					index,
					&editor->result);

				if (index == start) {
					break;
				}		

				index--;			
			}
		}

		else {
			file_move_line_down(
				&editor->actual_file->file,
				start,
				&editor->result);
		}

		if (op->linemove.start.y < file_num_lines(&editor->actual_file->file) - 1) 
		{
			op->linemove.start.y++;
			op->linemove.end.y++;

			if (editor->sel.active) {
				editor->sel.start = op->linemove.start;
				editor->sel.end = op->linemove.end;
			}
		}

		editor_cursor_move(editor, op->cursor_insert);
	}
}



/// --- INSERT ---

int editor_insert_char(Editor* editor, uint32_t c) {
	if (editor->actual_file->readonly) {
		prompt_init(
			&editor->status_bar,
			PROMPT_READONLY,
			PT_INFO);

		return EIE_NOT_FATAL_ERROR;
	}

	Position cursor_remove = editor->cursor.pos;

	int ret = file_insert_char(
		&editor->actual_file->file,
		editor->cursor.pos,
		c,
		&editor->result
	);

	if (ret < 0) {
		return ret;
	}


	editor_move_cursor_right(editor);

	Position cursor_insert = editor->cursor.pos;
	Position start = cursor_remove;
	Position end = cursor_insert;
	u32string text = u32string_from_raw_copy(&c, 1);

	Operation op = operation_create_insert(
		start, end,
		cursor_remove, cursor_insert,
		text
	);

	Operation* last = stack_peek(&editor->actual_file->undo);

	stack_clear(&editor->actual_file->redo);

	const struct language_rules* rules = (editor->actual_file->language)
		? editor->actual_file->language->rules
		: NULL;

	if (operation_can_merge(last, &op, rules)) {
		operation_merge(last, &op);
	}

	else {
		stack_push(&editor->actual_file->undo, &op);
	}


	if (editor->selecting) {
		editor->selecting = 0;
	}

	if (editor->debug_mode) {
		log_write(&editor->log, "editor_insert_char: SUCCESS");
	}

	return 0;
}


int editor_insert_tab(Editor* editor) {
	if (editor->actual_file->readonly) {
		prompt_init(
			&editor->status_bar,
			PROMPT_READONLY,
			PT_INFO);

		return EIE_NOT_FATAL_ERROR;
	}

	Position cursor_remove = editor->cursor.pos;

	u32string* line_text = file_get_line_text(
		&editor->actual_file->file,
		editor->cursor.pos.y);

	u32string text;
	size_t move;

	if (editor->actual_file->file.use_spaces) {
		text = u32string_new();

		size_t tab_size = editor->config.tab_size;

		for (size_t i = 0; i < tab_size; i++) {
			u32string_push(&text, U' ');
			u32string_insert(line_text, U' ', i + editor->cursor.pos.x);
		}

		move = tab_size;
	}

	else {
		text = u32string_from("\t");

		u32string_insert(line_text, U'\t', editor->cursor.pos.x);

		move = 1;
	}

	editor_cursor_move(editor,
		(Position) { editor->cursor.pos.x + move,
					 editor->cursor.pos.y } );

	file_set_line_dirty(&editor->actual_file->file, editor->cursor.pos.y);

	Position cursor_insert = editor->cursor.pos;
	Position start = cursor_remove;
	Position end = editor->cursor.pos;

	Operation op = operation_create_insert(
		start, end,
		cursor_remove, cursor_insert,
		text
	);

	stack_clear(&editor->actual_file->redo);
	stack_push(&editor->actual_file->undo, &op);

	editor->actual_file->file.dirty = 1;


	if (editor->debug_mode) {
		log_write(&editor->log, "editor_insert_tab: SUCCESS");
	}

	return EIE_OK;
}


// helper for editor_insert_newline
static int only_insert_a_newline
(
	Editor* editor
)
{
	Position cursor_remove = editor->cursor.pos;

	int ret = file_insert_newline(
		&editor->actual_file->file,
		editor->cursor.pos,
		&editor->result);

	if (ret < 0) {
		return ret;
	}

	editor_cursor_move(editor,
		(Position) { 0, editor->cursor.pos.y + 1 });

	Position cursor_insert = editor->cursor.pos;
	Position start = cursor_remove;
	Position end = editor->cursor.pos;

	u32string text = u32string_from(
		(char []) { '\n', '\0' });

	Operation op = operation_create_insert(
		cursor_remove, cursor_insert,
		start, end,
		text
	);

	stack_clear(&editor->actual_file->redo);
	stack_push(&editor->actual_file->undo, &op);

	return ret;
}

// helper for editor_insert_newline
static int insert_delimiter_pair
(
	Editor* editor,
	Position cursor_remove
)
{
	int use_spaces = editor->actual_file->file.use_spaces;

	size_t indent;
	size_t tab_size = (use_spaces)
		? editor->actual_file->file.tab_size
		: 1;

	uint32_t c = (use_spaces) 
		? U' '
		: U'\t';

	{
		u32string* prev_text = file_get_line_text(
			&editor->actual_file->file,
			editor->cursor.pos.y - 1
		);

		indent = u32string_get_indent(
			prev_text,
			use_spaces);
	}

	size_t new_indent = indent + tab_size;

	int ret; 

	{
		u32string* text = file_get_line_text(
			&editor->actual_file->file,
			editor->cursor.pos.y);

		for (size_t i = 0; i < new_indent; i++) {
			u32string_insert(text, c, i);
		}

		editor->cursor.pos.x += new_indent;
	}

	ret = file_insert_newline(
		&editor->actual_file->file, 
		editor->cursor.pos,
		&editor->result);

	if (ret < 0) {
		return ret;
	}

	editor->cursor.pos.y++;

	u32string* closing = file_get_line_text(
		&editor->actual_file->file, 
		editor->cursor.pos.y);

	u32string_set_indent(
		closing, 
		indent,
		use_spaces);

	editor_cursor_move(
		editor,
		(Position) { new_indent, editor->cursor.pos.y - 1 });

	Position cursor_insert = editor->cursor.pos;

	Position start = cursor_remove;
	Position end = (Position) { indent, editor->cursor.pos.y + 1 };

	u32string text = u32string_from((char[]) { '\n', '\0' });

	for (size_t i = 0; i < new_indent; i++) {
		u32string_push(&text, c);
	}

	u32string_push(&text, U'\n');

	Operation op = operation_create_insert(
		start, end,
		cursor_remove, cursor_insert,
		text
	);

	stack_clear(&editor->actual_file->redo);
	stack_push(&editor->actual_file->undo, &op);

	return ret;
}

// helper for has_open_delimiter_in_line
static int is_open_delimiter_with_auto_indent(Editor* editor, int c) {
	const struct language_rules* rules = 
		editor->actual_file->language->rules;

	for (size_t i = 0; i < rules->pair_count; i++) {
		if (c == rules->pairs[i].open && rules->pairs[i].auto_indent) {
			return 1;
		}
	}

	return 0;
}

// helper for insert_newline_and_indent
static int has_open_delimiter_in_line
(
	Editor* editor,
	size_t x, 
	const u32string* line
)

{
	if (x == 0) {
		return 0;
	}

	for (size_t i = 1; i <= x; i++) {
		char c = u32string_char(line, x - i);

		if (is_open_delimiter_with_auto_indent(editor, c)) {
			return 1;
		} else if (isspace((unsigned) c)) {
			continue;
		} else {
			return 0;
		}
	}

	return 0;
}

// helper for editor_insert_newline
static void insert_newline_and_indent
(
	Editor* editor,
	Position cursor_remove
)
{	
	int use_spaces = editor->actual_file->file.use_spaces;

	size_t indent;
	size_t tab_size = (use_spaces)
		? editor->actual_file->file.tab_size
		: 1;

	uint32_t c = (use_spaces) ? U' ' : U'\t';

	int prev_has_open_delimiter = 0;

	{
		u32string* prev_text = file_get_line_text(
			&editor->actual_file->file,
			editor->cursor.pos.y - 1
		);

		indent = u32string_get_indent(prev_text, use_spaces);

		prev_has_open_delimiter = has_open_delimiter_in_line(
			editor,
			u32string_size(prev_text),
			prev_text);

		if (indent > 0 && u32string_is_empty(prev_text)) 
		{
			u32string_set_indent(prev_text, 0, use_spaces);
		}
	}

	size_t new_indent = (prev_has_open_delimiter)
		? indent + tab_size
		: indent;

	{
		u32string* text = file_get_line_text(
			&editor->actual_file->file,
			editor->cursor.pos.y);

		for (size_t i = 0; i < new_indent; i++) {
			u32string_insert(text, c, i);
		}
	}

	editor_cursor_move(
		editor,
		(Position) { new_indent, editor->cursor.pos.y } );

	Position cursor_insert = editor->cursor.pos;
	Position start = cursor_remove;
	Position end = editor->cursor.pos;

	u32string text = u32string_from((char[]) { '\n', '\0' });

	for (size_t i = 0; i < new_indent; i++) {
		u32string_push(&text, c);
	}

	Operation op = operation_create_insert(
		start, end,
		cursor_remove, cursor_insert,
		text
	);

	stack_clear(&editor->actual_file->redo);
	stack_push(&editor->actual_file->undo, &op);
}

// helper for editor_insert_newline
static int is_between_delimiter_with_auto_indent
(
	Editor* editor,
	char prev,
	char current
) 
{
	const struct language_rules* rules = (editor->actual_file->language) ?
		editor->actual_file->language->rules : NULL;

	if (!rules) {
		return 0;
	}

	for (size_t i = 0; i < rules->pair_count; i++) {
		if (prev == rules->pairs[i].open &&
			current == rules->pairs[i].close &&
			rules->pairs[i].auto_indent)
		{
			return 1;
		}
	}

	return 0;
}


int editor_insert_newline(Editor* editor) {
	if (editor->actual_file->readonly) {
		prompt_init(
			&editor->status_bar,
			PROMPT_READONLY,
			PT_INFO);

		return EIE_NOT_FATAL_ERROR;
	}

	const struct language_rules* rules = (editor->actual_file->language) ? 
		editor->actual_file->language->rules : NULL;

	int ret = EIE_OK;

	if (!rules || !rules->auto_indent) {
		ret = only_insert_a_newline(editor);

		if (editor->debug_mode) {
			log_write(&editor->log, "editor_insert_newline: SUCCESS");
		}

		return 0;
	}

	int is_delimiter_pair = 0;

	{	
		u32string* line_text = file_get_line_text(
			&editor->actual_file->file, 
			editor->cursor.pos.y);

		is_delimiter_pair = 
			(editor->cursor.pos.x > 0 && 
			 editor->cursor.pos.x < u32string_size(line_text)) &&
			is_between_delimiter_with_auto_indent(editor, 
				u32string_char(line_text, editor->cursor.pos.x - 1),
				u32string_char(line_text, editor->cursor.pos.x));
	}

	// file->dirty = 1
	ret = file_insert_newline(
		&editor->actual_file->file, 
		editor->cursor.pos, 
		&editor->result);

	if (ret < 0) {
		return ret;
	}

	Position cursor_remove = editor->cursor.pos;

	editor->cursor.pos.y++;
	editor->cursor.pos.x = 0;

	if (is_delimiter_pair) {
		ret = insert_delimiter_pair(
			editor, 
			cursor_remove);

		if (ret < 0) {
			return ret;
		}
	} 

	else {
		insert_newline_and_indent(
			editor,
			cursor_remove);
	}

	if (editor->debug_mode) {
		log_write(&editor->log, "editor_insert_newline: SUCCESS");
	}

	return ret;
}



/// --- DELETE ---

// helper for editor_delete_char_or_selection
static int delete_selection
(
	Editor* editor
)
{
	if (editor->selecting) {
		editor->selecting = 0;
	}

	Position a, b;
	selection_normalize(&editor->sel, &a, &b);

	Position cursor_insert = b;
	Position cursor_remove = a;

	Clipboard cb = clipboard_new();

	file_copy_selection(
		&editor->actual_file->file,
		&cb,
		&editor->sel,
		&editor->result
	);
	log_write(&editor->log, "delete_selection: cb.linewise = %d", cb.linewise);

	int ret = file_delete_selection(
		&editor->actual_file->file, 
		&editor->sel, 
		&editor->result);

	if (ret < 0) {
		clipboard_free(&cb);
		return ret;
	}

	if (ret == EIE_NOT_AN_ERROR) {
		clipboard_free(&cb);
		return EIE_OK;
	}

	editor->cursor.pos = a;
	editor_cursor_update(editor);

	Position start = a;
	Position end = b;

	u32string text = u32string_clone(&cb.text);

	Operation op = operation_create_delete(
		start, end,
		cursor_remove, cursor_insert,
		text
	);

	clipboard_free(&cb);

	stack_clear(&editor->actual_file->redo);
	stack_push(&editor->actual_file->undo, &op);

	return EIE_OK;
}

// helper for editor_delete_char_or_selection
static int delete_delimiter_pair
(
	Editor* editor
)
{
	int ret = EIE_OK;
	Position cursor_insert = editor->cursor.pos;

	uint32_t open;
	uint32_t close;

	{
		const u32string* text = file_get_line_text(
			&editor->actual_file->file,
			editor->cursor.pos.y);

		open = u32string_char(text, editor->cursor.pos.x - 1);
		close = u32string_char(text, editor->cursor.pos.x);
	}

	// the size of a delimiter is assumed to be equal
	// to 1
	editor_move_cursor_right(editor);

	for (size_t i = 0; i < 2; i++) {
		ret = file_delete_char(
			&editor->actual_file->file,
			editor->cursor.pos,
			&editor->result);

		editor_move_cursor_left(editor);

		if (ret < 0) {
			return EIE_FATAL_ERROR;
		}
	}

	Position cursor_remove = editor->cursor.pos;
	Position start = editor->cursor.pos;
	Position end = (Position) {
		editor->cursor.pos.x + 2,
		editor->cursor.pos.y
	};

	u32string text = u32string_from(
		(char []) { open, close, '\0' });

	Operation op = operation_create_delete(
		start, end,
		cursor_remove, cursor_insert,
		text
	);

	stack_clear(&editor->actual_file->redo);
	stack_push(&editor->actual_file->undo, &op);

	return EIE_OK;
}

// helper for editor_delete_char_or_selection
static int delete_single_char
(
	Editor* editor
)
{
	int ret = EIE_OK;
	Position cursor_insert = editor->cursor.pos;

	uint32_t c;

	{
		const u32string* text = file_get_line_text(
			&editor->actual_file->file,
			editor->cursor.pos.y);

		c = u32string_char(text, editor->cursor.pos.x - 1);
	}

	ret = file_delete_char(
		&editor->actual_file->file, 
		editor->cursor.pos,
		&editor->result);

	if (ret < 0) {
		return ret;
	}

	editor_move_cursor_left(editor);

	Position cursor_remove = editor->cursor.pos;
	Position start = editor->cursor.pos;
	Position end = (Position) { start.x + 1, start.y };

	u32string text = u32string_from_raw_copy(&c, 1);

	Operation op = operation_create_delete(
		start, end,
		cursor_remove, cursor_insert,
		text
	);

	Operation* last = stack_peek(&editor->actual_file->undo);

	const struct language_rules* rules = (editor->actual_file->language)
		? editor->actual_file->language->rules
		: NULL;

	stack_clear(&editor->actual_file->redo);

	if (operation_can_merge(last, &op, rules)) {
		operation_merge(last, &op);
	}

	else {
		stack_push(&editor->actual_file->undo, &op);
	}

	return ret;
}

// helper for editor_delete_char_or_selection
int delete_newline
(
	Editor* editor
)
{
	int ret = EIE_OK;
	Position cursor_insert = editor->cursor.pos;

	size_t prev_size = file_size_line(
		&editor->actual_file->file,
		editor->cursor.pos.y - 1);

	ret = file_merge_lines(
		&editor->actual_file->file, 
		editor->cursor.pos,
		&editor->result);

	if (ret < 0) {
		return ret;
	}

	editor_cursor_move(
		editor,
		(Position) {prev_size, editor->cursor.pos.y - 1} );

	Position cursor_remove = editor->cursor.pos;
	Position start = editor->cursor.pos;
	Position end = (Position) { 0, start.y + 1 };

	u32string text = u32string_from(
		(char []) { '\n', '\0' });

	Operation op = operation_create_delete(
		start, end,
		cursor_remove, cursor_insert,
		text
	);

	stack_clear(&editor->actual_file->redo);
	stack_push(&editor->actual_file->undo, &op);

	return EIE_OK;	
}

// helper for editor_delete_char_or_selection
static int is_between_delimiters_with_auto_complete
(
	Editor* editor,
	char prev,
	char current
) 
{
	const struct language_rules* rules = (editor->actual_file->language) ?
		editor->actual_file->language->rules : NULL;

	if (!rules) {
		return 0;
	}

	for (size_t i = 0; i < rules->pair_count; i++) {
		if (prev == rules->pairs[i].open &&
			current == rules->pairs[i].close &&
			rules->pairs[i].auto_complete)
		{
			return 1;
		}
	}

	return 0;
}

int editor_delete_char_or_selection
(
	Editor* editor,
	char prev, // used only if !editor->sel.active
	char current
) 
{
	if (editor->actual_file->readonly) {
		prompt_init(
			&editor->status_bar,
			PROMPT_READONLY,
			PT_INFO);

		return EIE_NOT_FATAL_ERROR;
	}

	int ret = EIE_OK;

	if (editor->sel.active) {
		ret = delete_selection(editor);
	} 

	else {
		if (editor->cursor.pos.x > 0) {
			if (editor->actual_file->language && 
				is_between_delimiters_with_auto_complete(editor, prev, 
					current)) 
			{
				ret = delete_delimiter_pair(editor);
			}

			else {
				ret = delete_single_char(editor);
			}
		} 

		// newline
		else if (editor->cursor.pos.y > 0 && 
			editor->cursor.pos.y < file_num_lines(&editor->actual_file->file)) 
		{
			ret = delete_newline(editor);
		}
	}

	if (ret < 0) {
		return ret;
	}

	if (editor->debug_mode) {
		log_write(
			&editor->log, 
			"editor_delete_char_or_selection: SUCCESS");
	}

	return ret;
}


void editor_del_from_cursor_left(Editor* editor) {
	if (editor->cursor.pos.x == 0) {
		return;
	}

	Selection editor_sel = editor->sel;
	int selection_active = editor_sel.active;
	int selecting = editor->selecting;

	editor_selection_clear(editor);
	selection_start(&editor->sel, POS_ZERO);

	editor->sel.start = (Position) { 
		0,
		editor->cursor.pos.y
	};

	editor->sel.end = (Position) {
		editor->cursor.pos.x,
		editor->cursor.pos.y
	};

	editor_delete_char_or_selection(editor, '\0', '\0');

	if (selection_active) {
		editor->sel = editor_sel;
		editor->selecting = selecting;
	}

	else {
		editor_selection_clear(editor);
	}
}

void editor_del_from_cursor_right(Editor* editor) {
	size_t size = file_size_line(
		&editor->actual_file->file,
		editor->cursor.pos.y
	);

	if (editor->cursor.pos.x == size) {
		return;
	}

	Selection editor_sel = editor->sel;
	int selection_active = editor_sel.active;
	int selecting = editor->selecting;

	editor_selection_clear(editor);
	selection_start(&editor->sel, POS_ZERO);

	editor->sel.start = (Position) { 
		editor->cursor.pos.x,
		editor->cursor.pos.y
	};

	editor->sel.end = (Position) {
		size,
		editor->cursor.pos.y
	};

	editor_delete_char_or_selection(editor, '\0', '\0');

	if (selection_active) {
		editor->sel = editor_sel;
		editor->selecting = selecting;
	}

	else {
		editor_selection_clear(editor);
	}	
}


/// --- INDENT ---

int editor_indent_line
(
	Editor* editor,
	size_t y
)
{
	if (y >= file_num_lines(&editor->actual_file->file) - 1) {
		return EIE_NOT_FATAL_ERROR;
	}

	int ret = EIE_OK;
	Position cursor_remove = editor->cursor.pos;

	ret = file_indent_a_line(
		&editor->actual_file->file, 
		y,
		&editor->result);

	if (ret < 0) {
		return ret;
	}

	size_t tab_size = (editor->actual_file->file.use_spaces)
		? editor->actual_file->file.tab_size
		: 1;

	if (editor->cursor.pos.y == y) {
		editor_cursor_move(
			editor,
			(Position) { editor->cursor.pos.x + 
							tab_size,
						 editor->cursor.pos.y } );
	}

	Position cursor_insert = editor->cursor.pos;
	Position start = (Position) { 0, y }; // only y coord is important
	Position end = (Position) { 0, y }; // only y coord is important

	Operation op = operation_create_indent(
		start, end,
		cursor_remove, cursor_insert
	);

	if (editor->sel.active) {
		if (editor->sel.start.y == y) {
			editor->sel.start.x += tab_size;
		}

		if (editor->sel.end.y == y) {
			editor->sel.end.x += tab_size;
		}
	}

	stack_clear(&editor->actual_file->redo);
	stack_push(&editor->actual_file->undo, &op);

	return ret;	
}

// helper for editor_indent_selection_or_line
static int indent_selection
(
	Editor* editor
)
{
	int ret = EIE_OK;
	Position cursor_remove = editor->cursor.pos;

	Position a, b;
	selection_normalize(&editor->sel, &a, &b);

	ret = file_indent_selection(
		&editor->actual_file->file,
		&editor->sel,
		&editor->result
	);

	size_t tab_size = (editor->actual_file->file.use_spaces)
		? editor->actual_file->file.tab_size
		: 1;

	editor_cursor_move(
		editor,
		(Position) { editor->cursor.pos.x + tab_size,
					 editor->cursor.pos.y } );

	Position cursor_insert = editor->cursor.pos;
	Position start = a; // only y coord is important
	Position end = b; // only y coord is important

	Operation op = operation_create_indent(
		start, end,
		cursor_remove, cursor_insert
	);

	stack_clear(&editor->actual_file->redo);
	stack_push(&editor->actual_file->undo, &op);

	return ret;
}

int editor_indent_selection_or_line(Editor* editor) {
	if (editor->actual_file->readonly) {
		prompt_init(
			&editor->status_bar,
			PROMPT_READONLY,
			PT_INFO);

		return EIE_NOT_FATAL_ERROR;
	}

	int ret;

	if (!editor->sel.active) {
		ret = editor_indent_line(editor, editor->cursor.pos.y);
	} 

	else {
		ret = indent_selection(editor);
	}

	if (ret < 0) {
		return ret;
	}

	if (editor->debug_mode) {
		log_write(&editor->log, "editor_indent_selection_or_line: SUCCESS");
	}

	return 0;
}



/// --- UNINDENT ---

int editor_unindent_line
(
	Editor* editor,
	size_t y
)
{
	if (y >= file_num_lines(&editor->actual_file->file) - 1) {
		return EIE_NOT_FATAL_ERROR;
	}

	int ret = EIE_OK;
	Position cursor_insert = editor->cursor.pos;

	int use_spaces = editor->actual_file->file.use_spaces;
	size_t tab_size = (use_spaces)
		? editor->actual_file->file.tab_size
		: 1;

	size_t move_cursor = 0;

	{
		u32string* line = file_get_line_text(
			&editor->actual_file->file, 
			editor->cursor.pos.y);

		size_t indent = u32string_get_indent(line, use_spaces);

		move_cursor = (indent >= tab_size)
			? tab_size
			: indent;
	}

	ret = file_unindent_a_line(
		&editor->actual_file->file, 
		y,
		&editor->result);

	if (ret < 0) {
		return ret;
	}

	if (ret == EIE_NOT_AN_ERROR) {
		return 0;
	}

	if (editor->cursor.pos.y == y) {
		editor_cursor_move(
			editor,
			(Position) { editor->cursor.pos.x - move_cursor,
						 editor->cursor.pos.y } );
	}

	Position cursor_remove = editor->cursor.pos;
	Position start = (Position) { 0, y }; // only y coord is important
	Position end = (Position) { 0, y }; // only y coord is important

	Operation op = operation_create_unindent(
		start, end,
		cursor_remove, cursor_insert
	);

	if (editor->sel.active) {
		if (editor->sel.start.y == y) {
			editor->sel.start.x -= move_cursor;
		}

		if (editor->sel.end.y == y) {
			editor->sel.end.x -= move_cursor;
		}
	}

	stack_clear(&editor->actual_file->redo);
	stack_push(&editor->actual_file->undo, &op);

	return ret;
}

static int unindent_selection
(
	Editor* editor
)
{
	int ret = EIE_OK;
	Position cursor_insert = editor->cursor.pos;

	Position a, b;
	selection_normalize(&editor->sel, &a, &b);

	// the start and end of the selection, as well
	// as the cursor, can have arbitrary indentation levels.
	// therefore, it is necessary to know how far each one
	// needs to move
	size_t move_cursor = 0;

	int use_spaces = editor->actual_file->file.use_spaces;
	size_t tab_size = (use_spaces)
		? editor->actual_file->file.tab_size
		: 1;

	{
		u32string* line_text = file_get_line_text(
			&editor->actual_file->file, 
			editor->cursor.pos.y);

		size_t indent = u32string_get_indent(line_text, use_spaces);

		move_cursor = (indent >= tab_size)
			? tab_size
			: indent;
	}

	file_unindent_selection(
		&editor->actual_file->file,
		&editor->sel,
		&editor->result
	);

	editor_cursor_move(
		editor,
		(Position) { editor->cursor.pos.x - move_cursor,
					  editor->cursor.pos.y } );

	Position cursor_remove = editor->cursor.pos;
	Position start = a; // only y coord is important
	Position end = b; // only y coord is important

	Operation op = operation_create_unindent(
		start, end,
		cursor_remove, cursor_insert
	);

	stack_clear(&editor->actual_file->redo);
	stack_push(&editor->actual_file->undo, &op);

	return ret;
}

int editor_unindent_selection_or_line(Editor* editor) {
	if (editor->actual_file->readonly) {
		prompt_init(
			&editor->status_bar,
			PROMPT_READONLY,
			PT_INFO);

		return EIE_NOT_FATAL_ERROR;
	}

	int ret;

	editor->selecting = 0;

	if (!editor->sel.active) {
		ret = editor_unindent_line(editor, editor->cursor.pos.y);
	} 

	else {
		ret = unindent_selection(editor);
	}

	if (ret < 0) {
		return ret;
	}

	if (editor->debug_mode) {
		log_write(&editor->log, "editor_unindent_selection_or_line: SUCCESS");
	}

	return 0;
}



/// --- COMMENT ---

int editor_comment_line_or_selection(Editor* editor) {
	if (editor->actual_file->readonly) {
		prompt_init(
			&editor->status_bar,
			PROMPT_READONLY,
			PT_INFO);

		return EIE_NOT_FATAL_ERROR;
	}

	int ret = EIE_OK;

	if (!editor->actual_file->language) {
		if (editor->debug_mode) {
			log_write(&editor->log,
				"editor_comment_line_or_selection: lang plugin is NULL");
		}

		prompt_init(
			&editor->status_bar,
			PROMPT_LANG_PLUGIN_NULL,
			PT_INFO
		);

		return EIE_NOT_FATAL_ERROR;
	}

	const struct language_rules* rules = editor->actual_file->language->rules;

	if (!rules) {
		if (editor->debug_mode) {
			log_write(&editor->log,
				"editor_comment_line_or_selection: lang_rules is NULL");
		}

		prompt_init(
			&editor->status_bar,
			PROMPT_LANG_RULES_NULL,
			PT_INFO
		);

		return EIE_NOT_FATAL_ERROR;
	}

	const char* comment_fmt = rules->comment_fmt;

	if (!comment_fmt) {
		if (editor->debug_mode) {
			log_write(&editor->log,
				"editor_comment_line_or_selection: comment_fmt is NULL");
		}

		prompt_init(
			&editor->status_bar,
			PROMPT_LANG_COMMENT_NULL,
			PT_INFO
		);

		return EIE_NOT_FATAL_ERROR;		
	}

	Position cursor_remove = editor->cursor.pos;
	Position cursor_insert = editor->cursor.pos;

	Position start, end;

	ssize_t move_cursor;

	if (!editor->sel.active) {
		ret = file_comment_line(
			&editor->actual_file->file, 
			editor->cursor.pos.y,
			comment_fmt,
			&move_cursor,
			&editor->result);

		start = end = editor->cursor.pos;
	}

	else {
		ret = file_comment_selection(
			&editor->actual_file->file,
			&editor->sel,
			comment_fmt,
			editor->cursor.pos.y,
			&move_cursor);

		Position a, b;
		selection_normalize(&editor->sel, &a, &b);

		start = a;
		end = b;
	}

	if (ret < 0) {
		return ret;
	}

	editor->cursor.pos.x += move_cursor;
	editor_cursor_update(editor);

	if (move_cursor < 0) {
		cursor_remove.x += move_cursor;
	}

	else {
		cursor_insert.x += move_cursor;
	}

	Operation op = operation_create_comment(
		start, end,
		cursor_remove, cursor_insert);

	stack_clear(&editor->actual_file->redo);
	stack_push(&editor->actual_file->undo, &op);

	if (editor->debug_mode) {
		log_write(&editor->log,
			"editor_comment_line_or_selection: SUCCESS");
	}

	return ret;
}



/// --- REPLACE ---

// helper for editor_replace_pattern
static void range_delete
(
	Editor* editor,
	const u32string* pattern,
	Vector* replacements
)
{
	size_t pattern_size = u32string_size(pattern);

	for (size_t i = 0; i < file_num_lines(&editor->actual_file->file); i++) {
		Line* line = vector_get(&editor->actual_file->file.lines, i);

		ssize_t index;
		int first = 1;

		while ((index = u32string_find(
				&line->text, 
				0,
				u32string_size(&line->text),
				pattern)) >= 0)
		{
			editor->sel.active = 0;

			if (first) {
				line->dirty = 1;
				first = 0;
			}

			if (replacements) {
				Position pos = (Position) { index, i };

				vector_push(replacements, &pos);
			}

			u32string_remove_range(
				&line->text,
				index,
				index + pattern_size
			);
		}
	}	
}

// helper for editor_replace_pattern
static void range_replace
(
	Editor* editor,
	const u32string* pattern,
	const u32string* text,
	Vector* replacements
)
{
	size_t pattern_size = u32string_size(pattern);
	size_t text_size = u32string_size(text);

	log_write(&editor->log, "vai tomar nocu");

	for (size_t i = 0; i < file_num_lines(&editor->actual_file->file); i++) {
		Line* line = vector_get(&editor->actual_file->file.lines, i);

		ssize_t index;
		size_t pos = 0;
		int first = 1;

		while ((index = u32string_find(
				&line->text, 
				pos,
				u32string_size(&line->text),
				pattern)) >= 0)
		{
			if (first) {
				line->dirty = 1;
				first = 0;
			}

			if (replacements) {
				Position pos = (Position) { index, i };

				log_write(&editor->log, "(%zu, %zu)", index, i);

				vector_push(replacements, &pos);
			}

			u32string_remove_range(
				&line->text,
				index,
				index + pattern_size
			);

			u32string_insert_range_raw(
				&line->text,
				index,
				u32string_into_ptr_const(text),
				text_size
			);

			pos = index + text_size;
		}
	}
}

// helper for editor_handle_replace_pattern
void editor_replace_pattern
(
	Editor* editor,
	const u32string* pattern,
	const u32string* text
)
{
	if (editor->actual_file->readonly) {
		prompt_init(
			&editor->status_bar,
			PROMPT_READONLY,
			PT_INFO);

		return;
	}

	if (!pattern || !text) {
		return;
	}

	if (u32string_is_empty(pattern)) {
		return;
	}

	Vector replacements;
	vector_init(&replacements, sizeof(Position), NULL);

	if (u32string_is_empty(text)) {
		range_delete(editor, pattern, &replacements);
	}

	else {
		range_replace(editor, pattern, text, &replacements);
	}

	editor->actual_file->file.dirty = 1;

	Position cursor_remove = editor->cursor.pos;
	Position cursor_insert = editor->cursor.pos;

	size_t pattern_size = u32string_size(pattern);
	size_t text_size = u32string_size(text);

	ssize_t offset = text_size - pattern_size;

	if (editor->sel.active) {
		editor->sel.end.x += offset;
	}

	editor_cursor_move(
		editor,
		(Position) { editor->cursor.pos.x + offset,
					 editor->cursor.pos.y } );

	Operation op = operation_create_replace(
		cursor_remove, cursor_insert,
		u32string_clone(pattern), u32string_clone(text),
		replacements
	);

	stack_clear(&editor->actual_file->redo);
	stack_push(&editor->actual_file->undo, &op);
}


static void range_selection_delete
(
	Editor* editor,
	const u32string* pattern,
	Vector* replacements
)
{
	Position a, b;
	selection_normalize(&editor->sel, &a, &b);

	size_t pattern_size = u32string_size(pattern);

	for (size_t i = a.y; i <= b.y; i++) {
		Line* line = vector_get(&editor->actual_file->file.lines, i);

		ssize_t index;
		int first = 1;

		size_t find_from = 0;
		size_t find_until = u32string_size(&line->text);

		if (find_until == 0) {
			continue;
		}

		if (i == a.y) {
			if (a.x >= u32string_size(&line->text) - 1) {
				continue;
			}


			find_from = a.x;
		}

		else if (i == b.y) {
			if (b.x == 0) {
				continue;
			}

			find_until = b.x;
		}


		while ((index = u32string_find(
				&line->text, 
				find_from,
				find_until,
				pattern)) >= 0)
		{
			editor->sel.active = 0;

			if (first) {
				line->dirty = 1;
				first = 0;
			}

			if (replacements) {
				Position pos = (Position) { index, i };

				vector_push(replacements, &pos);
			}

			u32string_remove_range(
				&line->text,
				index,
				index + pattern_size
			);
		}
	}
}

static void range_selection_replace
(
	Editor* editor,
	const u32string* pattern,
	const u32string* text,
	Vector* replacements
)
{
	Position a, b;
	selection_normalize(&editor->sel, &a, &b);

	size_t pattern_size = u32string_size(pattern);
	size_t text_size = u32string_size(text);

	for (size_t i = a.y; i <= b.y; i++) {
		Line* line = vector_get(&editor->actual_file->file.lines, i);

		ssize_t index;
		size_t pos = 0;
		int first = 1;

		size_t find_until = u32string_size(&line->text);

		if (find_until == 0) {
			continue;
		}

		if (i == a.y) {
			if (a.y == u32string_size(&line->text) - 1) {
				continue;
			}

			pos = a.x;
		}

		else if (i == b.y) {
			if (b.y == 0) {
				continue;
			}

			find_until = b.x;
		}

		while ((index = u32string_find(
				&line->text, 
				pos,
				find_until,
				pattern)) >= 0)
		{
			if (first) {
				line->dirty = 1;
				first = 0;
			}

			if (replacements) {
				Position pos = (Position) { index, i };

				log_write(&editor->log, "(%zu, %zu)", index, i);

				vector_push(replacements, &pos);
			}

			u32string_remove_range(
				&line->text,
				index,
				index + pattern_size
			);

			u32string_insert_range_raw(
				&line->text,
				index,
				u32string_into_ptr_const(text),
				text_size
			);

			pos = index + text_size;
		}
	}

}

void editor_replace_within_selection
(
	Editor* editor,
	const u32string* pattern,
	const u32string* text
)
{
	if (editor->actual_file->readonly) {
		prompt_init(
			&editor->status_bar,
			PROMPT_READONLY,
			PT_INFO);

		return;
	}

	if (!editor->sel.active) {
		return;
	}

	if (u32string_is_empty(pattern)) {
		return;
	}

	Vector replacements;
	vector_init(&replacements, sizeof(Position), NULL);

	if (u32string_is_empty(text)) {
		range_selection_delete(editor, pattern, &replacements);
	}

	else {
		range_selection_replace(editor, pattern, text, &replacements);
	}

	editor->actual_file->file.dirty = 1;

	Position cursor_remove = editor->cursor.pos;
	Position cursor_insert = editor->cursor.pos;

	size_t pattern_size = u32string_size(pattern);
	size_t text_size = u32string_size(text);

	ssize_t offset = text_size - pattern_size;

	if (editor->sel.active) {
		if (selection_start_before_end(&editor->sel)) {
			editor->sel.end.x += offset;
		}

		else {
			editor->sel.start.x += offset;
		}
	}

	editor_cursor_move(
		editor,
		(Position) { editor->cursor.pos.x + offset,
					 editor->cursor.pos.y } );

	Operation op = operation_create_replace(
		cursor_remove, cursor_insert,
		u32string_clone(pattern), u32string_clone(text),
		replacements
	);

	stack_clear(&editor->actual_file->redo);
	stack_push(&editor->actual_file->undo, &op);
}




/// --- LINEMOVE_UP ---

// helper for editor_move_line_or_selection_up
static int move_line_up
(
	Editor* editor
)
{
	if (editor->cursor.pos.y == 0) {
		return EIE_NOT_AN_ERROR;
	}

	int ret = EIE_OK;

	Position cursor_insert = editor->cursor.pos;

	ret = file_move_line_up(
		&editor->actual_file->file, 
		editor->cursor.pos.y,
		&editor->result);

	editor->cursor.pos.y--;
	editor_cursor_update(editor);

	Position cursor_remove = editor->cursor.pos;
	Position start = editor->cursor.pos;
	Position end = start;


	MoveDirection direction = LINEMOVE_UP;

	Operation op = operation_create_linemove(
		start, end,
		cursor_remove, cursor_insert,
		direction
	);

	stack_clear(&editor->actual_file->redo);
	stack_push(&editor->actual_file->undo, &op);

	return ret;
}

// helper for editor_move_line_or_selection_up
static int move_selection_up
(
	Editor* editor
)
{
	int ret = EIE_OK;

	Position a, b;
	selection_normalize(&editor->sel, &a, &b);

	if (a.y == 0) {
		return EIE_NOT_AN_ERROR;
	}

	for (size_t i = a.y; i < b.y + 1; i++) {
		ret = file_move_line_up(
			&editor->actual_file->file,
			i,
			&editor->result
		);

		if (ret < 0) {
			return ret;
		}
	}

	a.y--;
	b.y--;

	Position cursor_insert = editor->cursor.pos;

	if (selection_start_before_end(&editor->sel)) {
		editor->sel.start = a;
		editor->sel.end = b;
	}

	else {
		editor->sel.start = b;
		editor->sel.end = a;
	}

	editor->cursor.pos.y--;
	editor_cursor_update(editor);

	Position cursor_remove = editor->cursor.pos;
	Position start = a;
	Position end = b;

	log_write(&editor->log, "%zu, %zu", start.y, end.y);

	MoveDirection direction = LINEMOVE_UP;

	Operation op = operation_create_linemove(
		start, end,
		cursor_remove, cursor_insert,
		direction
	);

	stack_clear(&editor->actual_file->redo);
	stack_push(&editor->actual_file->undo, &op);

	return ret;
}

int editor_move_line_or_selection_up(Editor* editor) {
	if (editor->actual_file->readonly) {
		prompt_init(
			&editor->status_bar,
			PROMPT_READONLY,
			PT_INFO);

		return EIE_NOT_FATAL_ERROR;
	}

	int ret = EIE_OK;

	if (!editor->sel.active) {
		ret = move_line_up(editor);
	} 

	else {
		ret = move_selection_up(editor);
	}

	if (ret < 0) {
		return ret;
	}

	if (editor->debug_mode) {
		log_write(&editor->log,
			"editor_move_line_or_selection_up: SUCCESS");
	}	

	return ret;
}



/// --- LINEMOVE_DOWN ---

// helper for editor_move_line_or_selection_down
static int move_line_down
(
	Editor* editor
)
{
	if (editor->cursor.pos.y >= file_num_lines(&editor->actual_file->file) - 1) {
		return EIE_NOT_AN_ERROR;
	}

	int ret = EIE_OK;

	Position cursor_remove = editor->cursor.pos;

	ret = file_move_line_down(
		&editor->actual_file->file, 
		editor->cursor.pos.y,
		&editor->result);

	editor->cursor.pos.y++;
	editor_cursor_update(editor);

	Position cursor_insert = editor->cursor.pos;
	Position start = cursor_insert;
	Position end = start;

	MoveDirection direction = LINEMOVE_DOWN;

	Operation op = operation_create_linemove(
		start, end,
		cursor_remove, cursor_insert,
		direction
	);

	stack_clear(&editor->actual_file->redo);
	stack_push(&editor->actual_file->undo, &op);

	return ret;
}

// helper for editor_move_line_or_selection_down
static int move_selection_down
(
	Editor* editor
)
{
	int ret = EIE_OK;

	Position a, b;
	selection_normalize(&editor->sel, &a, &b);

	if (b.y >= file_num_lines(&editor->actual_file->file) - 1) {
		return EIE_NOT_AN_ERROR;
	}

	size_t index = b.y;

	while (1) {
		ret = file_move_line_down(
			&editor->actual_file->file,
			index,
			&editor->result
		);

		if (ret < 0) {
			return ret;
		}

		if (index == a.y) {
			break;
		}

		index--;
	}

	a.y++;
	b.y++;

	Position cursor_remove = editor->cursor.pos;

	if (selection_start_before_end(&editor->sel)) {
		editor->sel.start = a;
		editor->sel.end = b;
	}

	else {
		editor->sel.start = b;
		editor->sel.end = a;
	}

	editor->cursor.pos.y++;
	editor_cursor_update(editor);

	Position cursor_insert = editor->cursor.pos;
	Position start = a;
	Position end = b;

	MoveDirection direction = LINEMOVE_DOWN;

	Operation op = operation_create_linemove(
		start, end,
		cursor_remove, cursor_insert,
		direction
	);

	stack_clear(&editor->actual_file->redo);
	stack_push(&editor->actual_file->undo, &op);

	return ret;
}

int editor_move_line_or_selection_down(Editor* editor) {
	if (editor->actual_file->readonly) {
		prompt_init(
			&editor->status_bar,
			PROMPT_READONLY,
			PT_INFO);

		return EIE_NOT_FATAL_ERROR;
	}

	int ret = EIE_OK;

	if (!editor->sel.active) {
		ret = move_line_down(editor);
	} 

	else {
		ret = move_selection_down(editor);
	}

	if (ret < 0) {
		return ret;
	}

	if (editor->debug_mode) {
		log_write(&editor->log, 
			"editor_move_line_or_selection_down: SUCCESS");
	}

	return ret;
}



/// --- MATCH AND FIND ---


// helper for editor_find_next_pattern_match
static int find_from_range
(
	Editor* editor,
	Position start,
	Position end,
	const u32string* pattern,
	int select_if_find
)
{
	size_t pattern_size = u32string_size(pattern);

	for (size_t i = start.y; i <= end.y; i++) {
		u32string* line_text = file_get_line_text(
			&editor->actual_file->file, 
			i);

		size_t pos = (i == start.y) ? start.x : 0;
		size_t until = u32string_size(line_text);

		if (i == end.y) {
			if (end.x == 0) {
				continue;
			}

			until = end.x;
		}

		ssize_t index;

		while ((index = u32string_find(
					line_text, 
					pos,
					until,
					pattern)) >= 0)
		{
			size_t start = index;
			size_t end = start + pattern_size;
			size_t y = i;

			editor_cursor_move(
				editor,
				(Position) { end, y } );

			if (select_if_find) {
				editor->selecting = 0;
				editor->sel.active = 1;

				editor->sel.start = (Position) {
					start, y
				};

				editor->sel.end = (Position) {
					end, y
				};
			}

			pos = index + pattern_size;

			return 1;
		}
	}

	return 0;
}

// helper for editor_match and editor_handle_find_pattern
int editor_find_next_pattern_match
(
	Editor* editor,
	Position start,
	const u32string* pattern,
	int select_if_find
) 
{
	if (u32string_is_empty(pattern)) {
		return 0;
	}

	size_t lines = file_num_lines(&editor->actual_file->file);

	Position end = { 
		file_size_line(&editor->actual_file->file, lines - 1),
		lines - 1
	};

	if (find_from_range(editor, start, end, pattern, select_if_find)) {
		return 1;
	}

	if (start.y == 0) {
		return 0;
	}

	end = (Position) {
		file_size_line(&editor->actual_file->file, start.y - 1),
		start.y - 1
	};

	if (find_from_range(editor, POS_ZERO, end, pattern, select_if_find)) {
		return 1;
	}

	return 0;		
}

int editor_find_next_pattern_within_selection
(
	Editor* editor,
	const u32string* pattern,
	int select_if_find
)
{
	if (!editor->sel.active) {
		return 0;
	}

	if (u32string_is_empty(pattern)) {
		return 0;
	}

	Position a, b;
	selection_normalize(&editor->sel, &a, &b);

	b = (Position) { b.x, b.y + 1 };

	if (find_from_range(editor, a, b, pattern, select_if_find)) {
		return 1;
	}	

	return 0;
}

static int is_open_delimiter
(
	const struct pair* pairs,
	size_t pair_count,
	uint32_t c,
	char* close
)
{
	for (size_t i = 0; i < pair_count; i++) {
		if (c == (unsigned char) pairs[i].open &&
			pairs[i].open != pairs[i].close)
		{
			if (close) { *close = pairs[i].close; }

			return 1;
		}
	}

	return 0;
}

static int is_close_delimiter
(
	const struct pair* pairs,
	size_t pair_count,
	uint32_t c,
	char* open
)
{
	for (size_t i = 0; i < pair_count; i++) {
		if (c == (unsigned char) pairs[i].close &&
			pairs[i].open != pairs[i].close)
		{
			if (open) { *open = pairs[i].open; }

			return 1;
		}
	}

	return 0;
}

static int must_ignore_token
(
	const Vector* tokens,
	size_t x,
	size_t* token_begin,
	size_t* token_end
)
{
	for (size_t i = 0; i < tokens->size; i++) {
		const struct token* token = vector_get_const(
			tokens,
			i
		);

		if ((token->highlight == HL_STRING ||
			 token->highlight == HL_CHAR) &&
			x >= token->begin && x <= token->end) {
			if (token_begin) {
				*token_begin = token->begin;
			}

			if (token_end) {
				*token_end = token->end;
			}

			return 1;
		}
	}

	return 0;
}

static int find_for_open_delimiter
(
	Editor* editor,
	Position start,
	char open,
	char close
)
{
	size_t depth = 1;
	int has_lexer = (editor->actual_file->lexer != NULL);

	for (size_t y = start.y + 1; y-- > 0;) {
		const u32string* line = file_get_line_text(
			&editor->actual_file->file,
			y
		);

		size_t size = u32string_size(line);

		if (size == 0) {
			continue;
		}

		const Vector* tokens = NULL;

		if (has_lexer) {
			tokens = file_get_line_tokens(
				&editor->actual_file->file,
				y
			);			
		}

		size_t x = (y == start.y) 
			? (start.x == size) ? start.x : start.x + 1
			: size;

		for (; x-- > 0;) {
			size_t token_begin;

			if (has_lexer &&
				must_ignore_token(tokens, x, &token_begin, NULL)) {
				x = token_begin;
				continue;
			}

			uint32_t c = u32string_char(line, x);

			if (c == (unsigned char) open) {
				depth--;
			}

			else if (c == (unsigned char) close) {
				depth++;
			}

			if (depth == 0) {
				editor_cursor_move(
					editor,
					(Position) { x + 1, y }
				);

				return 1;
			}
		}
	}

	return 0;	
}

static int find_for_close_delimiter
(
	Editor* editor,
	Position start,
	char open,
	char close
)
{
	size_t depth = 1;
	size_t lines = file_num_lines(&editor->actual_file->file);
	int has_lexer = (editor->actual_file->lexer != NULL);

	for (size_t y = start.y; y < lines; y++) {
		const Vector* tokens = NULL;

		if (has_lexer) {
			tokens = file_get_line_tokens(
				&editor->actual_file->file,
				y
			);			
		}

		const u32string* line = file_get_line_text(
			&editor->actual_file->file,
			y
		);

		size_t size = u32string_size(line);
		size_t x = (y == start.y) ? start.x: 0;

		for (; x < size; x++) {
			size_t token_end;

			if (has_lexer &&
				must_ignore_token(tokens, x, NULL, &token_end)) {
				x = token_end;
				continue;
			}

			uint32_t c = u32string_char(line, x);


			if (c == (unsigned char) close) {
				depth--;
			}

			else if (c == (unsigned char) open) {
				depth++;
			}

			if (depth == 0) {
				editor_cursor_move(
					editor,
					(Position) { x, y }
				);

				return 1;
			}
		}
	}

	return 0;
}

static int find_actual_block
(
	Editor* editor,
	Position start,
	const struct pair* pairs,
	size_t pair_count,
	int pos_x_equals_size
) 
{
	size_t lines = file_num_lines(&editor->actual_file->file);

	char open, close;
	int has_lexer = (editor->actual_file->lexer != NULL);

	size_t depth = 1;

	for (size_t y = start.y + 1; y-- > 0;) {
		const Vector* tokens = NULL;

		if (has_lexer) {
			tokens = file_get_line_tokens(
				&editor->actual_file->file,
				y
			);
		}

		const u32string* line = file_get_line_text(
			&editor->actual_file->file,
			y
		);

		size_t size = u32string_size(line);
		size_t x = (y == start.y) ? start.x + 1 : size;

		for (; x-- > 0;) {
			size_t token_begin;

			if (has_lexer &&
				must_ignore_token(tokens, x, &token_begin, NULL)) 
			{
				x = token_begin;
				continue;
			}

			uint32_t c = u32string_char(line, x);

			if (is_open_delimiter(pairs, pair_count, c, &close)) {
				depth--;
			}

			else if (is_close_delimiter(pairs, pair_count,  c, &open)) {
				depth++;
			}

			if (depth == 0) {
				if (x == start.x && 
					!pos_x_equals_size &&
					y == start.y) 
				{
					depth++;
					continue;
				}

				open = c;

				size_t new_y = y;
				size_t new_x = x;

				if (x == size - 1 && y == lines - 1) {
					return 0;
				}

				if (x == size - 1 && y < lines - 1) {
					new_y = y + 1;
					new_x = 0;
				}

				else if (x < size - 1) {
					new_x = x + 1;
				}

				return find_for_close_delimiter(
					editor,
					(Position) { new_x, new_y },
					open,
					close
				);				
			}
		}
	}

	return 0;
}

/// --- FIND BLOCK ---



/*

Will find, if it exists, the delimiters enclosing*
the cursor and move the cursor to:

1) The closing delimiter if the actual char is an
opening delimiter itself
2) The opening delimiter if the actual char is an
closing delimiter itself
3) The first closing delimiter found starting
from the cursor position

The cursor must not move if not inside a block.

* ENCLOSING is important because the cursor
must be inside the block for the block to be detected.
For example, assume the following text and that
'|' represents the current cursor position:

" main(|hello) "

after the function, the cursor must move to:

" main(hello|) "

but the cursor will not move to the previous position if
it is in this position:

"main|(hello)"

because the program will consider that the cursor is
outside the (hello) block.

-- EXPECTED BEHAVIOR -- :

Assume that '|' represents the current cursor position and the
following text inside a file:

"
int main(void) {|
	printf("it was really fun implementing this function :) \n");

	return 69;
}
"

the expected behavior is for the cursor to move to:

"
int main(void) {
	printf("it was really fun implementing this function :) \n");

	return 69;
|}
"

of if the cursor is at:

"
int main(void) {
	|printf("it was really fun implementing this function :) \n");

	return 69;
}
"

the behavior should also be:

"
int main(void) {
	printf("it was really fun implementing this function :) \n");

	return 69;
|}
"

however, if the cursor is at:

"
int main(void) {
	printf("it was really |fun implementing this function :) \n");

	return 69;
}
"

the behavior should be:

"
int main(void) {
	printf("it was really fun implementing this function :) \n"|);

	return 69;
}
"

and not:

"
int main(void) {
	printf("it was really fun implementing this function :|) \n");

	return 69;
}
"

Therefore, if a lexer exists, the function must skip
the search if the control indices fall within an
HL_STRING token, or similar token whose internal
delimiter should be ignored.

*/
int editor_find_actual_block(Editor* editor) {
	if (!editor->actual_file->language ||
		!editor->actual_file->language->rules ||
		!editor->actual_file->language->rules->pairs) 
	{
		return 0;
	}

	Position pos = editor->cursor.pos;

	uint32_t actual_char;

	int pos_x_equals_size = 0;

	{
		const u32string* line = file_get_line_text(
			&editor->actual_file->file,
			pos.y
		);

		size_t size = u32string_size(line);

		if (pos.x == size && size > 0) {
			pos_x_equals_size = 1;
			pos.x = size - 1;
		}

		actual_char = u32string_char(line, pos.x);
	}

	char complement = '\0';

	const struct pair* pairs = 
		editor->actual_file->language->rules->pairs;

	size_t pair_count = 
		editor->actual_file->language->rules->pair_count;

	if (is_close_delimiter(pairs, pair_count, actual_char, &complement) &&
		!pos_x_equals_size) 
	{		
		if (pos.x == 0) {
			if (pos.y == 0) {
				return 0;
			}

			else {
				pos.y--;
				pos.x = file_size_line(
					&editor->actual_file->file,
					pos.y
				);
			}
		}

		else {
			pos.x--;
		}

		return find_for_open_delimiter(
			editor,
			pos,
			complement,
			actual_char
		);
	}

	// will find the next closing delimiter
	else {
		return find_actual_block(
			editor, 
			pos, 
			pairs, 
			pair_count,
			pos_x_equals_size
		);
	}
}



// -- MATCH --

void editor_match(Editor* editor) {
	if (editor->sel.active) {
		if (editor->sel.start.y != editor->sel.end.y) {
			prompt_init(
				&editor->status_bar,
				PROMPT_MULTILINE_ON_MATCH,
				PT_INFO);

			return;
		}

		// start.y == end.y
		if (editor->sel.start.x == editor->sel.end.x) {
			prompt_init(
				&editor->status_bar,
				PROMPT_EMPTY_ON_MATCH,
				PT_INFO);

			return;
		}

		editor->selecting = 0;
		size_t start = (editor->sel.start.x < editor->sel.end.x) ?
			editor->sel.start.x : editor->sel.end.x;

		size_t size = (start == editor->sel.start.x) ?
			(editor->sel.end.x - editor->sel.start.x) :
			(editor->sel.start.x - editor->sel.end.x);

		if (size == 0) {
			return;
		}

		u32string* line = file_get_line_text(&editor->actual_file->file,
			editor->sel.start.y);

		u32string pattern = u32string_slice(
			line, 
			start, 
			start + size);

		int found = editor_find_next_pattern_match(
			editor,
			editor->sel.end,
			&pattern,
			1);

		if (!found) {
			prompt_init(&editor->status_bar, PROMPT_NOT_FOUND, PT_INFO);
		}

		u32string_free(&pattern);
	} else {
		editor_select_word(editor);
	}	
}



/// --- UNDO AND REDO

void editor_undo(Editor* editor) {
	if (editor->actual_file->readonly) {
		prompt_init(
			&editor->status_bar,
			PROMPT_READONLY,
			PT_INFO);

		return;
	}

	if (stack_empty(&editor->actual_file->undo)) {
		return;
	}

	editor->actual_file->file.dirty = 1;

	Operation op;
	stack_pop(&editor->actual_file->undo, &op);

	switch (op.type) {
	case OP_INSERT: {
		Operation redo_op = operation_inverse(&op);

		editor_operation_remove(editor, &redo_op);

		stack_push(&editor->actual_file->redo, &redo_op);

		break;
	}

	case OP_DELETE: {
		Operation redo_op = operation_inverse(&op);

		editor_operation_insert(editor, &redo_op);

		stack_push(&editor->actual_file->redo, &redo_op);

		break;
	}

	case OP_INDENT: {
		Operation redo_op = operation_inverse(&op);

		editor_operation_unindent(editor, &redo_op);

		stack_push(&editor->actual_file->redo, &redo_op);

		break;
	}

	case OP_UNINDENT: {
		Operation redo_op = operation_inverse(&op);

		editor_operation_indent(editor, &redo_op);

		stack_push(&editor->actual_file->redo, &redo_op);

		break;
	}

	case OP_COMMENT: {
		Operation redo_op = operation_inverse(&op);

		editor_operation_comment(editor, &redo_op);

		stack_push(&editor->actual_file->redo, &redo_op);

		break;
	}

	case OP_REPLACE: {
		Operation redo_op = operation_inverse(&op);

		editor_operation_replace(editor, &redo_op);

		stack_push(&editor->actual_file->redo, &redo_op);

		break;
	}

	case OP_LINEMOVE: {
		Operation redo_op = operation_inverse(&op);

		editor_operation_linemove(editor, &redo_op);

		stack_push(&editor->actual_file->redo, &redo_op);

		break;
	}

	default:
		break;
	}
}

void editor_redo(Editor* editor) {
	if (editor->actual_file->readonly) {
		prompt_init(
			&editor->status_bar,
			PROMPT_READONLY,
			PT_INFO);

		return;
	}

	if (stack_empty(&editor->actual_file->redo)) {
		return;
	}

	editor->actual_file->file.dirty = 1;

	Operation op;
	stack_pop(&editor->actual_file->redo, &op);

	switch (op.type) {
	case OP_INSERT: {
		Operation undo_op = operation_inverse(&op);

		editor_operation_remove(editor, &undo_op);

		stack_push(&editor->actual_file->undo, &undo_op);

		break;
	}

	case OP_DELETE: {
		Operation undo_op = operation_inverse(&op);

		editor_operation_insert(editor, &undo_op);

		stack_push(&editor->actual_file->undo, &undo_op);

		break;
	}

	case OP_INDENT: {
		Operation undo_op = operation_inverse(&op);

		editor_operation_unindent(editor, &undo_op);

		stack_push(&editor->actual_file->undo, &undo_op);

		break;
	}

	case OP_UNINDENT: {
		Operation undo_op = operation_inverse(&op);

		editor_operation_indent(editor, &undo_op);

		stack_push(&editor->actual_file->undo, &undo_op);

		break;
	}

	case OP_COMMENT: {
		Operation undo_op = operation_inverse(&op);

		editor_operation_comment(editor, &undo_op);

		stack_push(&editor->actual_file->undo, &undo_op);

		break;
	}

	case OP_REPLACE: {
		Operation undo_op = operation_inverse(&op);

		editor_operation_replace(editor, &undo_op);

		stack_push(&editor->actual_file->undo, &undo_op);

		break;
	}

	case OP_LINEMOVE: {
		Operation undo_op = operation_inverse(&op);

		editor_operation_linemove(editor, &undo_op);

		stack_push(&editor->actual_file->undo, &undo_op);

		break;
	}

	default:
		break;
	}
}