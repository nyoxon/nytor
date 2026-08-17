// definition and implementation of the CMD commands

#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <assert.h>
#include <errno.h>
#include <dirent.h>

#include "editor/core/cmd.h"
#include "editor/core/action.h"
#include "util/types/u32string.h"
#include "util/files.h"
#include "window/render.h"

/*
In order to go to a specific implementation/definition, use the
find feature of your editor for the pattern: "--- CMD_NAME ---"

For example, to goto the 'find' implementation: "--- FIND"
*/


// Window.on_select callback
static WindowResult on_select_goto_file(void* userdata) {
	assert(userdata != NULL);

	Editor* editor = userdata;
	editor_change_actual_file(editor,
		editor->window.cursor.pos.y);

	return WINDOW_CLOSE;
}

// Window.on_select callback
static WindowResult on_select_change_theme(void* userdata) {
	assert(userdata != NULL);

	Editor* editor = userdata;

	u32string* theme = vector_get(
		&editor->window.content,
		editor->window.cursor.pos.y
	);

	char* themeu8 = u32string_into_u8(theme);

	int ret = config_load_specific_theme(
		&editor->config,
		themeu8
	);

	if (ret < -1) {
		prompt_init(
			&editor->status_bar,
			"failed to load theme",
			PT_INFO
		);
	}

	log_write(&editor->log, "theme: %s",
		editor->config.theme_path);

	free(themeu8);

	return WINDOW_CLOSE;
}

// Window.on_select callback
WindowResult show_help
(
	void* userdata
)
{
	assert(userdata != NULL && "userdata ptr must be a non-null pointer");

	Editor* editor = userdata;
	size_t selected_index = editor->window.cursor.pos.y;

	cmd cmd = COMMANDS[selected_index];

	Clipboard cb;
	cb.linewise = 0;

	cb.text = u32string_from(cmd.description);

	char buf[128];
	sprintf(buf, "%s man", cmd.name);

	editor_create_internal_file(
		editor,
		buf,
		&cb
	);

	clipboard_free(&cb);

	return WINDOW_CLOSE;
}


/// --- CMD_COMMANDS DEFINITIONS ---


// --- SAVE ---
const cmd save = { 
	.name = "save", 
	.description = 
		"¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨\n"
		"Name: save\n"
		"------------------------------------------------------------------\n"
		"Action: saves the actual file into memory.\n"
		"------------------------------------------------------------------\n"
		"More:\n\n"
		" ---- BEHAVIOR ----\n\n"
		"(1) The file must have a name. To name a file, use 'saveas'\n"
		"------------------------------------------------------------------\n"
		"Errors: any possible error returned by the 'open' or 'write' syscall\n"
		"¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨"
};


// --- SAVEAS ---
const cmd saveas = {
	.name = "saveas",
	.description = 
		"¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨\n"
		"Name: saveas\n"
		"------------------------------------------------------------------\n"
		"Args (required):\n"
		" -	filename\n"
		"------------------------------------------------------------------\n"
		"Action: saves the actual file into memory with the passed \n"
		"filename.\n"
		"------------------------------------------------------------------\n"
		"More:\n\n"
		" ---- FILENAME FORMAT ---- \n\n"
		"If there is a space character inside the filename, use quotation marks \n"
		"\" \" to delimit the filename itself. If there isn't, the use of quotation\n"
		"marks is optional. In order to pass a filename with spaces and that contains \n"
		"a \", you must escape that \" using \\ .\n\n"
		" ---- BEHAVIOR ----\n\n"
		"(1) If the actual file already exists, this command will rename it.\n\n"
		"(2) Otherwise, will create/overwrite a file with the given filename.\n"
		"------------------------------------------------------------------\n"
		"Errors: any possible error returned by the 'open' or 'write'\n"
		"syscalls while creating/opening a non-existing file. It is \n"
		"also an error to pass a filename of a file that is already in the editor.\n"
		"¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨",
};


// --- QUIT ---
const cmd quit = {
	.name = "quit",
	.description = 
		"¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨\n"
		"Name: quit\n"
		"------------------------------------------------------------------\n"
		"Action: exits the program, trying to close all open files.\n"
		"------------------------------------------------------------------\n"
		"More:\n\n"
		" ---- BEHAVIOR ----\n\n"
		"(1) If there are any dirty files, the program "
		"will display\ntheir names and ask how to proceed:\n\n"
		"    (1) quit anyway and the changes will be lost\n"
		"    (2) save all, but untitled files, and quit\n"
		"    (3) manually save them, which means that the user "
		"will need to\nsave each of them manually\n\n"
		"There is a single exception to this behavior: specifically,\nwhen "
		"there is an empty, untitled (\"untitled\") file with nothing "
		"written on it.\n"
		"------------------------------------------------------------------\n"
		"Errors: none\n"
		"¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨",
};


// --- QUITS ---
const cmd quits = {
	.name = "quits",
	.description = 
		"¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨\n"
		"Name: quits\n"
		"------------------------------------------------------------------\n"
		"Action: exits the program saving all but untitled files.\n"
		"------------------------------------------------------------------\n"
		"More:\n\n"
		" ---- BEHAVIOR ----\n\n"
		"(1) The idea is to bypass the intermediate process of deciding \n"
		"what to do if there is a single dirty file in the program that the \n"
		"quit command does.\n"
		"------------------------------------------------------------------\n"
		"Errors: none\n"
		"¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨",
};


const cmd quit_forced = {
	.name = "quit!",
	.description = 
		"¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨\n"
		"Name: quit!\n"
		"------------------------------------------------------------------\n"
		"Action: exits the program forcibly.\n"
		"------------------------------------------------------------------\n"
		"Errors: none\n"
		"¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨",
};


// --- NEXT ---
const cmd next = {
	.name = "next",
	.description = 
		"¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨\n"
		"Name: next\n"
		"------------------------------------------------------------------\n"
		"Args: none\n"
		"------------------------------------------------------------------\n"
		"Action: moves to the next file loaded.\n"
		"------------------------------------------------------------------\n"
		"More: for this command, the files in memory are treated \n"
		"as if they were in a circular buffer.\n"
		"------------------------------------------------------------------\n"
		"Errors: none\n"
		"¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨",
};


// --- PREV ---
const cmd prev = {
	.name = "prev",
	.description = 
		"¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨\n"
		"Name: prev\n"
		"------------------------------------------------------------------\n"
		"Args: none\n"
		"------------------------------------------------------------------\n"
		"Action: moves to the prev file loaded.\n"
		"------------------------------------------------------------------\n"
		"More: for this command, the files in memory are treated \n"
		"as if they were in a circular buffer.\n"
		"------------------------------------------------------------------\n"
		"Errors: none\n"
		"¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨",
};


// --- NEW ---
const cmd new = {
	.name = "new",
	.description =
		"¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨\n"
		"Name: new\n"
		"------------------------------------------------------------------\n"
		"Action: creates a new, empty and untitled file.\n\n"
		"------------------------------------------------------------------\n"
		"More: to name the file, simply save it.\n"
		"------------------------------------------------------------------\n"
		"Errors: none\n"
		"¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨",
};


// --- NEWAS ---
const cmd newas = {
	.name = "newas",
	.description =
		"¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨\n"
		"Name: newas\n"
		"------------------------------------------------------------------\n"
		"Args (required):\n"
		" -	filename\n"
		"------------------------------------------------------------------\n"
		"Action: tries to create a new and empty file with the \n"
		"passed filename.\n"
		"------------------------------------------------------------------\n"
		"More:\n\n"
		" ---- FILENAME FORMAT ---- \n\n"
		"If there is a space character inside the filename, use quotation marks \n"
		"\" \" to delimit the filename itself. If there isn't, the use of quotation\n"
		"marks is optional. In order to pass a filename with spaces and that contains \n"
		"a \", you must escape that \" using \\ .\n\n"		
		" ---- BEHAVIOR ----\n\n"
		"(1) The created file must still be saved to actually exist on the\n"
		"user's system.\n\n"
		"(2) If a different file has the same name as the created file, you will receive\n"
		"a warning.\n"
		"------------------------------------------------------------------\n"
		"Errors: it is an error to attempt to create a file with the \n"
		"same name as a file that is already in the editor.\n"
		"¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨",
};


// --- CLOSE ---
const cmd close_cmd = { // man 2 close
	.name = "close",
	.description = 
		"¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨\n"
		"Name: close\n"
		"------------------------------------------------------------------\n"
		"Args (optional):\n"
		" -	filename\n"
		"------------------------------------------------------------------\n"
		"Action: tries to close the file with the specified filename.\n"
		"------------------------------------------------------------------\n"
		"More:\n\n"
		" ---- FILENAME FORMAT ---- \n\n"
		"If there is a space character inside the filename, use quotation marks \n"
		"\" \" to delimit the filename itself. If there isn't, the use of quotation\n"
		"marks is optional. In order to pass a filename with spaces and that contains \n"
		"a \", you must escape that \" using \\ .\n\n"
		" ---- BEHAVIOR ----\n\n"
		"(1) The file must not be dirty.\n\n"
		"(1) If no argument is passed, the current file will be closed.\n\n"
		"(2) If the current file is closed and there is no file left in the editor, \n"
		"an empty and untitled file will be created\n\n"
		" ---- EXAMPLES ----\n\n"
		"See 'open' manual\n"
		"------------------------------------------------------------------\n"
		"Errors: it is an error to attempt to close a file that does not \n"
		"exist inside the editor.\n"
		"¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨",
};

// --- CLOSE! ---
const cmd close_forced = {
	.name = "close!",
	.description = 
		"¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨\n"
		"Name: close!\n"
		"------------------------------------------------------------------\n"
		"Args (optional):\n"
		" -	filename\n"
		"------------------------------------------------------------------\n"
		"Action: same as 'close' but forcibly\n"
		"------------------------------------------------------------------\n"
		"Errors: it is an error to attempt to close a file that does not \n"
		"exist inside the editor.\n"
		"¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨",
};


// --- OPEN ---
const cmd open_cmd = { // man 2 open
	.name = "open",
	.description =
		"¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨\n"
		"Name: open\n"
		"------------------------------------------------------------------\n"
		"Args (required):\n"
		" -	filename\n"
		"------------------------------------------------------------------\n"
		"Action: opens the file with the specified filename.\n"
		"------------------------------------------------------------------\n"
		"More:\n\n"
		" ---- FILENAME FORMAT ---- \n\n"
		"If there is a space character inside the filename, use quotation marks \n"
		"\" \" to delimit the filename itself. If there isn't, the use of quotation\n"
		"marks is optional. In order to pass a filename with spaces and that contains \n"
		"a \", you must escape that \" using \\ .\n\n"
		" ---- BEHAVIOR ----\n\n"
		"(1) If the file does not exist, the command's behavior \n"
		"is the same as that of 'newas', aside from the part that \n"
		"passing a filename that already exits is an error.\n\n"
		" ---- EXAMPLES ----\n\n"
		"In order to open a file called \"cool_file.php\":\n\n"
		"> open cool_file.php\n\n"
		"In order to open a file called \"not a cool file\":\n\n"
		"> open \"not a cool file\"\n\n"
		"In order to open a file called \"why name me like \"this\":\n\n"
		"> open \"why name me like \\\"this\"\n"
		"------------------------------------------------------------------\n"
		"Errors: any possible returned by the 'open' syscall.\n"
		"If the third argument is passed, it must be exactly \n"
		"equal to 'read'\n"
		"¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨",
};


// --- SELECT ---
const cmd select_cmd = { // man 2 select
	.name = "select",
	.description =
		"¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨\n"
		"Name: select\n"
		"------------------------------------------------------------------\n"
		"Args (optional):\n"
		" -	all \n"
		" -	line line_number (optional)\n"
		" -	lines line_numb1 (required) line_numb2 (required)\n"
		" -	word\n"
		" -	complete\n"
		"------------------------------------------------------------------\n"
		"Action: starts and creates a selection.\n"
		"------------------------------------------------------------------\n"
		"More:\n\n"
		" ---- BEHAVIOR AND LIMITATIONS ----\n\n"
		"(1) If 'all' is passed, the entire file will be selected.\n\n"
		"(2) Lines are numbered from 1 to the size of the file. For this reason, \n"
		"'line_number' = 0 indicates the last line of the file.\n\n"
		"(3) If 'line' is passed, the line with 'line_number' will be selected.\n"
		"If 'line_number' is omitted, the actual line will be selected.\n\n"
		"(4) If 'lines' is passed, a selection will be created selecting the \n"
		"lines 'line_numb1' and 'line_numb2' and all the lines between them.\n\n"
		"The order of the lines is important in the following sense: if numb1 \n"
		"is less than numb2, the screen cursor will move to the end of line \n"
		"numb2; otherwise, it will move to the beginning of the line numb2. \n"
		"In that sense, 0 is greater than any other valid line number.\n"
		"For example, the following commands are equivalent:\n\n"
		"- select all\n"
		"- select lines 1 0\n\n"
		"Nonetheless, 'select lines 0 1' is not equivalent to 'select all', \n"
		"because they move the cursor to different places, even though they both \n"
		"select the entire file.\n\n"
		"(5) If 'word' is passed, the word where the cursor is positioned will be \n"
		"selected. A word is not necessarily a word in the strict sense, but \n"
		"rather a set of characters \"of the same type\".\n"
		"For example, consider the following text, where '|' represents the cursor \n"
		"position and \"\" delimitate a string: \n\n"
		"- \"Hello, wo|rld\"\n\n"
		"the 'select word' command will select \"world\". Now, if the cursor is in a \n"
		"position like this:\n\n"
		"- \"Hello,|world\"\n\n"
		"the 'select word' command will select \", \"\n\n"
		"(6) If 'complete' is passed, the selection limits will be adjusted so that \n"
		"the selection is complete.\n\n"
		"(7) If there are no arguments, an empty selection will be created at the \n"
		"cursor position.\n\n"
		"(8) In order to use the selection behavior that other commands have, \n"
		"you can use first the 'select' command to select the desired lines and\n"
		"then call the other command.\n"
		"------------------------------------------------------------------\n"
		"Errors: it is an error to pass a line number that does not correspond \n"
		"to an actual line in the file.\n"
		"¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨",
};


// --- COPY ---
const cmd copy = {
	.name = "copy",
	.description = 
		"¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨\n"
		"Name: copy\n"
		"------------------------------------------------------------------\n"
		"Action: copies the active selection to clipboard\n"
		"------------------------------------------------------------------\n"
		"Errors: none\n"
		"¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨",
};


// --- PASTE ---
const cmd paste = {
	.name = "paste",
	.description =
		"¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨\n"
		"Name: paste\n"
		"------------------------------------------------------------------\n"
		"Action: pastes the content of the clipboard into the file \n"
		"from the cursor position\n"
		"------------------------------------------------------------------\n"
		"Errors: none\n"
		"¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨",
};


// --- INDENT ---
const cmd indent = {
	.name = "indent",
	.description =
		"¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨\n"
		"Name: indent\n"
		"------------------------------------------------------------------\n"
		"Action: indents lines.\n"
		"------------------------------------------------------------------\n"
		"More:\n\n"
		" ---- SELECTION, BEHAVIOR AND LIMITATIONS ----\n\n"
		"If there is an active selection, all lines within it will be indented, \n"
		"otherwise, just the current line.\n"
		"------------------------------------------------------------------\n"
		"Errors: none\n"
		"¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨",
};


// --- UNINDENT ---
const cmd unindent = {
	.name = "unindent",
	.description =	
		"¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨\n"
		"Name: unindent\n"
		"------------------------------------------------------------------\n"
		"Action: unindent lines.\n"
		"------------------------------------------------------------------\n"
		"More:\n\n"
		" ---- SELECTION, BEHAVIOR AND LIMITATIONS ----\n\n"
		"If there is an active selection, all lines within it will be unindented, \n"
		"otherwise, just the current line.\n"
		"------------------------------------------------------------------\n"
		"Errors: none\n"
		"¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨",
};


// --- COMMENT ---
const cmd comment = {
	.name = "comment",
	.description =
		"¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨\n"
		"Name: comment\n"
		"------------------------------------------------------------------\n"
		"Action: comments on uncomments lines\n"
		"------------------------------------------------------------------\n"
		"More:\n\n"
		" ---- SELECTION, BEHAVIOR AND LIMITATIONS ----\n\n"
		"(1) This command only works if the current file has a language plugin, \n"
		"which is the responsible for telling the program what constitutes a \n"
		"comment.\n\n"
		"(2) If the command operates on a single line, that line will \n"
		"be commented out if it is not already commented, and uncommented \n"
		"if it is.\n"
		"(3) If the command operates on a set of lines, the program will \n"
		"uncomment the lines only if all of them are commented; otherwise, \n"
		"it will comment all of them, even if some are already commented.\n\n"
		"(4) If there is an active selection, the command will operate on the lines \n"
		"within it; otherwise, just on the actual line.\n"
		"------------------------------------------------------------------\n"
		"Errors: none\n"
		"¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨",
};


// --- REPLACE ---
const cmd replace = {
	.name = "replace",
	.description =
		"¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨\n"
		"Name: replace\n"
		"------------------------------------------------------------------\n"
		"Args: pattern (required) text (optional)\n"
		"------------------------------------------------------------------\n"
		"Action: replaces occurrences of 'pattern' with 'text'\n"
		"------------------------------------------------------------------\n"
		"More:\n\n"
		" ---- PARSING THE ARGS ---- \n\n"
		"This command accepts only two argument; therefore, if the internal \n"
		"function identifies a valid pattern and a valid text, any additional\n"
		"arguments will be ignored.\n\n"
		" ---- PATTERN FORMAT ---- \n\n"
		"If there is a space character inside the pattern/text, use quotation marks \n"
		"\" \" to delimit the pattern/text itself. If there isn't, the use of quotation\n"
		"marks is optional. In order to pass a pattern/text with spaces and that contains \n"
		"a \", you must escape that \" using \\ .\n\n"
		" ---- SELECTION, BEHAVIOR AND LIMITATIONS ----\n\n"
		"(1) If 'text' is omitted, occurrences of 'pattern' will be \n"
		"excluded.\n\n"
		"(2) If there is an active selection, the command will only operate \n"
		"within it; otherwise, it will operate on the entire file.\n\n"
		" ---- EXAMPLES ----\n\n"
		"Assume the following text inside a file:\n\n"
		"\" \n"
		"int main(void) {\n"
		"	printf(\"goodbye world\\n\");\n"
		"}\n"
		"\" \n\n"
		"In order to remove all occurrences of \"main\":\n\n"
		"> replace main\n\n"
		"In order to replace all occurrences of \"main\" with \"mono\":\n\n"
		"> replace main mono\n\n"
		"In order to replace all occurrences of \"main\" with \"mono mono\":\n\n"
		"> replace main \"mono mono\"\n\n"
		"In order to replace all occurrences of \"main\" with \"mono \"mono\":\n\n"
		"> replace main \"mono \\\"mono\"\n\n"
		"In order to replace all occurrences of \"main main\" with \"mono\":\n\n"
		"> replace \"main main\" mono\n"
		"------------------------------------------------------------------\n"
		"Errors: it is an error not to pass a 'pattern'.\n"
		"¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨",
};


// --- UNDO ---
const cmd undo = {
	.name = "undo",
	.description =
		"¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨\n"
		"Name: undo\n"
		"------------------------------------------------------------------\n"
		"Args (optional):\n"
		" -	clear (optional)\n"
		"------------------------------------------------------------------\n"
		"Action: undo the last operation.\n"
		"------------------------------------------------------------------\n"
		"More: \n\n"
		" ---- BEHAVIOR ----\n\n"
		"If 'clear' is passed, all operations in the undo and redo stack will \n"
		"be removed.\n\n"
		" ---- UNDOABLE OPERATIONS ----\n\n"
		"These are the operations that the undo command can undo:\n\n"
		"- write text\n"
		"- delete text\n"
		"- comment a line/selection\n"
		"- uncomment a line/selection\n"
		"- indent a line/selection\n"
		"- unindent a line/selection\n"
		"- move lines/selection up\n"
		"- move lines/selection down\n"
		"- replace a pattern\n"
		"------------------------------------------------------------------\n"
		"Errors: none\n"
		"¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨",
};


// --- REDO ---
const cmd redo = {
	.name = "redo",
	.description =
		"¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨\n"
		"Name: redo\n"
		"------------------------------------------------------------------\n"
		"Args (optional):\n"
		" -	clear\n"
		"------------------------------------------------------------------\n"
		"Action: redo the last operation.\n"
		"------------------------------------------------------------------\n"
		"More:\n\n"
		" ---- BEHAVIOR ----\n\n"
		"If 'clear' is passed, all operations in the redo stack will be removed.\n\n"
		" ---- REDOABLE OPERATIONS ----\n\n"
		"See 'undo' manual\n"
		"------------------------------------------------------------------\n"
		"Errors: none\n"
		"¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨",
};


// --- FIND ---
const cmd find = {
	.name = "find",
	.description = 
		"¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨\n"
		"Name: find\n"
		"------------------------------------------------------------------\n"
		"Args (required):\n"
		" -	pattern\n"
		"------------------------------------------------------------------\n"
		"Action: finds and selects the first 'pattern' in the file.\n"
		"------------------------------------------------------------------\n"
		"More: \n\n"
		" ---- PARSING THE ARGS ---- \n\n"
		"This command accepts only one argument; therefore, if the internal \n"
		"function identifies a valid pattern, any additional arguments will be \n"
		"ignored.\n\n"
		" ---- PATTERN FORMAT ---- \n\n"
		"If there is a space character inside the pattern, use quotation marks \n"
		"\" \" to delimit the pattern itself. If there isn't, the use of quotation\n"
		"marks is optional. In order to pass a pattern with spaces and that contains \n"
		"a \", you must escape this \" using \\ .\n\n"
		" ---- SELECTION, BEHAVIOR AND LIMITATIONS ---- \n\n"
		"(1) If there is an active selection, the search will take \n"
		"place within the selection; otherwise, in the entire file.\n\n"
		"(2) The provided pattern must be something that would be contained \n"
		"within a single line.\n\n"
		"(3) The pattern selection it will always be on the first occurrence\n"
		"of 'pattern' in the file/selection. In order to search for consecutive \n"
		"matches, consider the 'match' command.\n\n"
		" ---- EXAMPLES ---- \n\n"
		"Assume the following text inside a file: \n\n"
		"\" \n"
		"int main(void) {\n"
		"	printf(\"goodbye world\\n\");\n"
		"}\n"
		"\" \n\n"
		"In order to find, for example, the pattern \"main\": \n\n"
		"> find main \n\n"
		"or \n\n"
		"> find \"main\" \n\n"
		"If you use the quotation marks in this case, even though it's optional, \n"
		"ensure that you write the last \", because the following is an error:\n\n"
		"> find \"main \n\n"
		"Notice that, based on the error above, in order to really find the pattern \n"
		"\"main, you need to enclose it with quotation marks and escape the \":\n\n"
		"> find \"\\\"main\" \n\n"
		"In order to find, for example, \"goodbye world\", you need to use quotation \n"
		"marks: \n\n"
		"> find \"goodbye world\"\n"
		"------------------------------------------------------------------\n"
		"Errors: it is an error to include escape characters in the pattern \n"
		"other than \\\" or \\\\ . \n"
		"¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨",
};


// --- MATCH ---
const cmd match = {
	.name = "match",
	.description = 
		"¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨\n"
		"Name: match\n"
		"------------------------------------------------------------------\n"
		"Action: selects a word or finds the next pattern match in the file.\n"
		"------------------------------------------------------------------\n"
		"More:\n\n"
		" ---- SELECTION, BEHAVIOR AND LIMITATIONS ---- \n\n"
		"(1) If there is no active selection, the word where the cursor is \n"
		"located will be selected. A word is not necessarily a word in the \n"
		"strict sense, but rather a set of characters \"of the same type\".\n\n"
		"(2) If there is an active selection, the program will find the next match \n"
		"where the pattern is the content inside the selection. The selection \n"
		"must be within a single line.\n\n"
		"(3) Therefore, while the first behavior of the command complements the second \n"
		"in a strict way, the 'find' command complements it in a general way.\n\n"
		"(4) If the pattern is not found by the end of the file, the search will restart \n"
		"from the beginning of the file.\n\n"
		"(5) The search is case sensitive.\n\n"
		" ---- EXAMPLES ---- \n\n"
		"Assume the following text inside a file and that '|' represents the actual \n"
		"cursor position and that there is no active selection:\n\n"
		"\"\n"
		"hel|lo goodbye\n"
		"\n"
		"hello\n"
		"\n"
		"goodbye .hello\n"
		"\"\n\n"
		"If the command is called, the word \"hello\" will be selected. If it is \n"
		"called again, the cursor will move to the end of the \"hello\" of the 3º \n"
		"line and the word will be selected.\n\n"
		"However, if the cursor is at: \n\n"
		"\"\n"
		"hello goodbye\n"
		"\n"
		"hello\n"
		"\n"
		"goodbye |.hello\n"
		"\"\n\n"
		"The word \" .\" will be selected\n"
		"------------------------------------------------------------------\n"
		"Errors: it is an error to attempt to call this command while a multiline \n"
		"selection is active. \n"
		"¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨",
};


// --- GOTO ---
const cmd goto_cmd = {
	.name = "goto",
	.description =
		"¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨\n"
		"Name: goto\n"
		"------------------------------------------------------------------\n"
		"Args (optional):\n"
		" -	line_number\n"
		" -	filename\n"
		"------------------------------------------------------------------\n"
		"Action: go to line 'line_number' or to file with name 'filename'.\n"
		"------------------------------------------------------------------\n"
		"More:\n\n"
		" ---- PATTERN FORMAT ---- \n\n"
		"If there is a space character inside the arg, use quotation marks \n"
		"\" \" to delimit the name itself. If there isn't, the use of quotation\n"
		"marks is optional. In order to pass a arg with spaces and that contains \n"
		"a \", you must escape this \" using \\ .\n\n"
		" ---- BEHAVIOR AND LIMITATIONS ----\n\n"
		"(1) Lines are numbered from 1 to size_of_file; for this reason, \n"
		"'line_number' == 0 or 'line_number' > size_of_file points to the \n"
		"last line of the file.\n\n"
		"(2) If there is no argument, the program will go to the first line of \n"
		"the file.\n\n"
		"(3) The program firt attempts to convert the passed argument into a \n"
		"number; if it fails, the argument is treated as a name.\n\n"
		" ---- EXAMPLES ----\n\n"
		"Suppose the command is called like this: \n\n"
		"> goto 19\n\n"
		"If the file has a line 19, the cursor will move to it; otherwise, it will \n"
		"move to the last line of the file.\n\n"
		"In order to go to the last line, without exception:\n\n"
		"> goto 0\n\n"
		"In order to go to the first line: \n\n"
		"> goto 1\n\n"
		"or\n\n"
		"> goto\n\n"
		"Suppose now that the command is called like this: \n\n"
		"> goto cool_text.txt\n\n"
		"If the file cool_text.txt exists inside the editor, it will become the \n"
		"current file.\n\n"
		"In order to goto a file named \"unix file with spaces in the name '-'\": \n\n"
		"> goto \"unix file with spaces in the name '-'\"\n\n"
		"Ilustrating the behavior of first attempting to convert the passed arg \n"
		"to a number, suppose that the command is called like this:\n\n"
		"> goto 123weird_text_name.txt\n\n"
		"Since the program will not be able to convert the passed arg into a number, \n"
		"it will attempt to change the actual file to \"123weird_text_name.txt\"\n"
		"------------------------------------------------------------------\n"
		"Errors: it is an error to pass a negative line_number \n"
		"or a filename that does not correspond to an actual file \n"
		"in the program. \n"
		"¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨",
};


// --- SET ---
const cmd set = {
	.name = "set",
	.description = 
		"¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨\n"
		"Name: set\n"
		"------------------------------------------------------------------\n"
		"Args (required):\n"
		" -	indent spaces/tabs \n"
		" -	spaces \n"
		" -	perm writeable/readonly \n"
		" -	tabsize value \n"
		" -	language name \n"
		"------------------------------------------------------------------\n"
		"Action: set internal file configurations at runtime.\n"
		"------------------------------------------------------------------\n"
		"More:\n\n"
		" ---- BEHAVIOR AND LIMITATIONS ----\n\n"
		"(1) If 'indent' is passed, the indentation of the file will be \n"
		"converted from spaces/tabs to tabs/spaces. If tabs is passed, only the \n"
		"indentation spaces will be converted to tabs.\n\n"
		"(2) If 'spaces' is passed, the inner value of use_spaces (as indentation) will be \n"
		"changed based on what it is now.\n\n"
		"(3) If 'perm' and 'writeable' are passed, the program will try to make the \n"
		"file writable. If 'readonly' is passed, the program will make the file \n"
		"readonly. In this case, it's important to say that the commands history \n"
		"will be lost.\n\n"
		"(4) If 'tabsize' is passed, the internal tab_size of the actual file will be \n"
		"changed.\n\n"
		"(5) If 'language', the actual language plugin will change to the one \n"
		"for the 'name' language.\n\n"
		" ---- EXAMPLES ----\n\n"
		"In order to convert the indentation spaces to tabs:\n\n"
		"> set indent tabs\n\n"
		"In order to use tabs as indentation (if you are using spaces):\n\n"
		"> set spaces\n\n"
		"In order to set a file as readonly:\n\n"
		"> set perm readonly\n\n"
		"In order to use a certain language, for example, C:\n\n"
		"> set language c\n"
		"------------------------------------------------------------------\n"
		"Errors: it is an error to pass a language name that does not have \n"
		"a language plugin associated or a non-positive tabsize. \n"
		"¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨",
};


// --- TERMINAL ---
const cmd terminal = {
	.name = "terminal",
	.description =
		"¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨\n"
		"Name: terminal\n"
		"------------------------------------------------------------------\n"
		"Action: creates a new pty and runs bash on it.\n"
		"------------------------------------------------------------------\n"
		"More:\n\n"
		" ---- BEHAVIOR AND LIMITATIONS ---- \n\n"
		"(1) Don't forget to close that pty using the 'exit' command\n\n"
		"(2) The new pty will occupy the entire usable area of the older terminal\n"
		"------------------------------------------------------------------\n"
		"Errors: error during the creation of the pty.\n"
		"¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨",
};


// --- SYSTEM ---
const cmd system_cmd = { // man 3 system
	.name = "system",
	.description = 
		"¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨\n"
		"Name: system\n"
		"------------------------------------------------------------------\n"
		"Args (required):\n"
		" -	command args \n"
		"------------------------------------------------------------------\n"
		"Action: calls a shell command using the current section.\n"
		"------------------------------------------------------------------\n"
		"More: \n\n"
		" --- BEHAVIOR AND LIMITATIONS ---\n\n"
		"(1) You don't need to pass the command or arguments in quotes\n\n"
		"(2) The output of the called program will be completely ignored, and\n"
		"the only thing displayed will be the exit code. If you need to see\n"
		"command output, consider the 'terminal' command.\n\n"
		" --- TERMS OF USE ---\n\n"
		"By using this command, you agree not to call an interactive or\n"
		"blocking program. Violating this term will result in great sadness\n"
		"and annoyance.\n\n"
		" --- EXAMPLES ---\n\n"
		" > system gcc -Wall main.c\n"
		"------------------------------------------------------------------\n"
		"Errors: any error returned or possible from the called program. \n"
		"¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨",
};


// --- SHOW ---
const cmd show = {
	.name = "show",
	.description =
		"¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨\n"
		"Name: show\n"
		"------------------------------------------------------------------\n"
		"Args (required): \n"
		" -	file\n"
		" -	lines\n"
		" -	tabs\n"
		" -	cursor\n"
		" -	language\n"
		"------------------------------------------------------------------\n"
		"Action: shows something inside the file or the editor.\n"
		"------------------------------------------------------------------\n"
		"More:\n\n"
		" ---- BEHAVIOR ----\n\n"
		"(1) If 'file' is passed, the name of the current file will be shown at \n"
		"the status bar.\n\n"
		"(2) If 'lines' is passed, the line numbering will be disabled or enabled\n"
		"depending on the current value.\n\n"
		"(3) If 'tabs' is passed, tabs inside the file will start being displayed \n"
		"or will be hidden.\n\n"
		"(4) If 'cursor' is passed, the screen cursor will start begin displayed. \n"
		"This option is simply a way to fix a potential bug hidden within the \n"
		"program.\n\n"
		"(5) If 'language' is passed, the name of the language associated with the \n"
		"current plugin will be displayed in the status bar.\n"
		"------------------------------------------------------------------\n"
		"Errors: none \n"
		"¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨\n",
};


// --- FILES ---
const cmd files = {
	.name = "files",
	.description = 
		"¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨\n"
		"Name: files\n"
		"------------------------------------------------------------------\n"
		"Action: creates a window showing all the files that the \n"
		"editor is handling.\n"
		"------------------------------------------------------------------\n"
		"More:\n\n"
		" ---- BEHAVIOR ----\n\n"
		"It is possible to move the cursor between the lines \n"
		"displayed in the window and select one of them by pressing \n"
		"ENTER, SPACE or TAB. Once a line is selected, the current file \n"
		"will change to the file named in the selected line.\n"
		"------------------------------------------------------------------\n"
		"Errors: none\n"
		"¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨"
};


// --- SUSPEND ---
const cmd suspend = {
	.name = "suspend",
	.description = 
		"¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨\n"
		"Name: suspend\n"
		"------------------------------------------------------------------\n"
		"Action: temporarily suspends the program.\n"
		"------------------------------------------------------------------\n"
		"More: simply raises the SIGTSTP signal. To return to the program, \n"
		"write the command: \n\n"
		"$ fg\n\n"
		"the command is \"fg\", the $ symbol is just a shell prompt representation.\n"
		"------------------------------------------------------------------\n"
		"Errors: none\n"
		"¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨",
};


// --- BLOCK ---
const cmd block = {
	.name = "block",
	.description = 
		"¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨\n"
		"Name: block\n"
		"------------------------------------------------------------------\n"
		"Action: moves the cursor to the edges of the block of code that \n"
		"encloses it.\n"
		"------------------------------------------------------------------\n"
		"More:\n\n"
		" ---- BEHAVIOR AND LIMITATIONS ---\n\n"
		"(1) This command only works if there is a language plugin that dictates\n"
		"what constitutes a delimiter pair.\n\n"
		"(2) The program will ignore delimiter pairs whose opening and closing \n"
		"characters are identical.\n\n"
		"(3) The cursor must be inside the block in order to that block \n"
		"being detected.\n\n"
		"(4) If the cursor is over a closing delimiter, the program will find \n"
		"the corresponding opening delimiter.\n\n"
		"(5) If the cursor is not over a closing delimiter, the program will find\n"
		"the closing delimiter of the block.\n\n"
		"(6) If there is a lexer plugin, the program will ignore delimiter pairs\n"
		"that are inside of tokens of the types HL_STRING and HL_CHAR.\n"
		"------------------------------------------------------------------\n"
		"Errors: none \n"
		"¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨"
};


// --- THEMES ---
const cmd themes = {
	.name = "themes",
	.description =
		"¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨\n"
		"Name: themes\n"
		"------------------------------------------------------------------\n"
		"Actions: creates a window showing all the availables themes.\n"
		"------------------------------------------------------------------\n"
		"More: \n\n"
		"(1) You can select one of them to use temporarily, pressing SPACE,\n"
		"ENTER or TAB\n\n"
		"(2) In order to make the change permanent, change the theme in the\n"
		"configuration file.\n"
		"------------------------------------------------------------------\n"
		"Errors: any error in reading the theme file will result in the use\n"
		"of the default theme from the config file or the editor's default\n"
		"theme.\n"
		"¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨"
};


// --- SYNC ---
const cmd sync_cmd = {
	.name = "sync",
	.description = 
		"¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨\n"
		"Name: sync\n"
		"------------------------------------------------------------------\n"
		"Action: synchronizes the current editor file with the filesystem one.\n"
		"------------------------------------------------------------------\n"
		"More: \n\n"
		" -- BEHAVIOR AND LIMITATIONS --\n\n"
		"(1) The synchronization will only occur if there is a corresponding\n"
		"filesystem file to the editor's current one.\n"
		"------------------------------------------------------------------\n"
		"Errors: any error returned by the 'open' syscall\n"
		"¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨"
};


// --- HELP ---
const cmd help = {
	.name = "help",
	.description = 
		"This is the manual page for the nytor prompt commands.\n\n"
		"To see what a command does, run \"help command\" at the prompt\n"
		"or \"help\" at prompt and select the desired command.\n\n"
		"A new file will open containing information about the command.\n\n"
		"Here are all the possible commands:\n\n"
		"-    help\n"
		"-    save\n"
		"-    saveas\n"
		"-    quit\n"
		"-    quits\n"
		"-    quit!\n"
		"-    find\n"
		"-    block\n"
		"-    match\n"
		"-    goto\n"
		"-    next\n"
		"-    prev\n"
		"-    new\n"
		"-    newas\n"
		"-    close\n"
		"-    close!\n"
		"-    open\n"
		"-    select\n"
		"-    copy\n"
		"-    paste\n"
		"-    comment\n"
		"-    indent\n"
		"-    unindent\n"
		"-    replace\n"
		"-    undo\n"
		"-    redo\n"
		"-    themes\n"
		"-    terminal\n"
		"-    system\n"
		"-    show\n"
		"-    suspend\n"
		"-    sync\n"
		"\nDon't forget to close this window.",
};

const cmd COMMANDS[] = {
	help,
	quit,
	quits,
	quit_forced,
	save,
	saveas,
	new,
	newas,
	open_cmd,
	close_cmd,
	close_forced,
	copy,
	paste,
	find,
	themes,
	match,
	block,
	goto_cmd,
	prev,
	next,
	select_cmd,
	indent,
	unindent,
	comment,
	replace,
	undo,
	redo,
	terminal,
	system_cmd,
	show,
	files,
	set,
	suspend,
	sync_cmd,
};

const size_t COMMANDS_COUNT = sizeof(COMMANDS) / sizeof(COMMANDS[0]);


// creates an internal file containing the description of a command
static void handle_help(Editor* editor, char* words[], size_t n) {
	if (n == 0) {
		return;
	}

	// > help
	if (n == 1) {
		WindowOptions options = {
			.pos_type = WINDOWPOS_CENTRALIZED,
			.sw = 0.2,
			.sh = 0.5,
			.tsize = editor->tsize,
			.tab_size = editor->config.tab_size,
			.on_select = show_help
		};

		editor->window = window_new(&options);
		editor->has_window = 1;

		for (size_t i = 0; i < COMMANDS_COUNT; i++) {
			u32string cmd_name = u32string_from(COMMANDS[i].name);

			vector_push(&editor->window.content, &cmd_name);
		}
	}

	// > help cmd
	else if (n == 2) {
		Clipboard cb;
		cb.linewise = 0;

		char buf[128];

		if (strcmp(words[1], help.name) == 0) {
			cb.text = u32string_from(help.description);
			sprintf(buf, "manual page");
		}

		else if (strcmp(words[1], save.name) == 0) {
			cb.text = u32string_from(save.description);
			sprintf(buf, "%s man", save.name);
		}

		else if (strcmp(words[1], quit.name) == 0) {
			cb.text = u32string_from(quit.description);
			sprintf(buf, "%s man", quit.name);
		}

		else if (strcmp(words[1], quits.name) == 0) {
			cb.text = u32string_from(quits.description);
			sprintf(buf, "%s man", quits.name);
		}


		else if (strcmp(words[1], quit_forced.name) == 0) {
			cb.text = u32string_from(quit_forced.description);
			sprintf(buf, "%s man", quit_forced.name);
		}

		else if (strcmp(words[1], saveas.name) == 0) {
			cb.text = u32string_from(saveas.description);
			sprintf(buf, "%s man", saveas.name);
		}

		else if (strcmp(words[1], find.name) == 0) {
			cb.text = u32string_from(find.description);
			sprintf(buf, "%s man", find.name);
		}

		else if (strcmp(words[1], themes.name) == 0) {
			cb.text = u32string_from(themes.description);
			sprintf(buf, "%s man", themes.name);
		}

		else if (strcmp(words[1], files.name) == 0) {
			cb.text = u32string_from(files.description);
			sprintf(buf, "%s man", files.name);
		}

		else if (strcmp(words[1], match.name) == 0) {
			cb.text = u32string_from(match.description);
			sprintf(buf, "%s man", match.name);
		}

		else if (strcmp(words[1], goto_cmd.name) == 0) {
			cb.text = u32string_from(goto_cmd.description);
			sprintf(buf, "%s man", goto_cmd.name);
		}

		else if (strcmp(words[1], next.name) == 0) {
			cb.text = u32string_from(next.description);
			sprintf(buf, "%s man", next.name);
		}

		else if (strcmp(words[1], prev.name) == 0) {
			cb.text = u32string_from(prev.description);
			sprintf(buf, "%s man", prev.name);
		}

		else if (strcmp(words[1], new.name) == 0) {
			cb.text = u32string_from(new.description);
			sprintf(buf, "%s man", new.name);
		}

		else if (strcmp(words[1], newas.name) == 0) {
			cb.text = u32string_from(newas.description);
			sprintf(buf, "%s man", newas.name);
		}

		else if (strcmp(words[1], close_cmd.name) == 0) {
			cb.text = u32string_from(close_cmd.description);
			sprintf(buf, "%s man", close_cmd.name);
		}

		else if (strcmp(words[1], close_forced.name) == 0) {
			cb.text = u32string_from(close_forced.description);
			sprintf(buf, "%s man", close_forced.name);
		}

		else if (strcmp(words[1], open_cmd.name) == 0) {
			cb.text = u32string_from(open_cmd.description);
			sprintf(buf, "%s man", open_cmd.name);
		}

		else if (strcmp(words[1], select_cmd.name) == 0) {
			cb.text = u32string_from(select_cmd.description);
			sprintf(buf, "%s man", select_cmd.name);
		}

		else if (strcmp(words[1], copy.name) == 0) {
			cb.text = u32string_from(copy.description);
			sprintf(buf, "%s man", copy.name);
		}

		else if (strcmp(words[1], paste.name) == 0) {
			cb.text = u32string_from(paste.description);
			sprintf(buf, "%s man", paste.name);
		}

		else if (strcmp(words[1], indent.name) == 0) {
			cb.text = u32string_from(indent.description);
			sprintf(buf, "%s man", indent.name);
		}

		else if (strcmp(words[1], unindent.name) == 0) {
			cb.text = u32string_from(unindent.description);
			sprintf(buf, "%s man", unindent.name);
		}

		else if (strcmp(words[1], comment.name) == 0) {
			cb.text = u32string_from(comment.description);
			sprintf(buf, "%s man", comment.name);
		}

		else if (strcmp(words[1], replace.name) == 0) {
			cb.text = u32string_from(replace.description);
			sprintf(buf, "%s man", replace.name);
		}

		else if (strcmp(words[1], undo.name) == 0) {
			cb.text = u32string_from(undo.description);
			sprintf(buf, "%s man", undo.name);
		}

		else if (strcmp(words[1], redo.name) == 0) {
			cb.text = u32string_from(redo.description);
			sprintf(buf, "%s man", redo.name);
		}

		else if (strcmp(words[1], set.name) == 0) {
			cb.text = u32string_from(set.description);
			sprintf(buf, "%s man", set.name);
		}

		else if (strcmp(words[1], terminal.name) == 0) {
			cb.text = u32string_from(terminal.description);
			sprintf(buf, "%s man", terminal.name);
		}

		else if (strcmp(words[1], system_cmd.name) == 0) {
			cb.text = u32string_from(system_cmd.description);
			sprintf(buf, "%s man", system_cmd.name);
		}

		else if (strcmp(words[1], show.name) == 0) {
			cb.text = u32string_from(show.description);
			sprintf(buf, "%s man", show.name);
		}

		else if (strcmp(words[1], suspend.name) == 0) {
			cb.text = u32string_from(suspend.description);
			sprintf(buf, "%s man", suspend.name);
		}

		else if (strcmp(words[1], block.name) == 0) {
			cb.text = u32string_from(block.description);
			sprintf(buf, "%s man", block.name);
		}

		else if (strcmp(words[1], sync_cmd.name) == 0) {
			cb.text = u32string_from(sync_cmd.description);
			sprintf(buf, "%s man", sync_cmd.name);
		}

		else {
			prompt_init(
				&editor->status_bar,
				PROMPT_INVALID_ARG,
				PT_INFO);

			return;
		}

		editor_create_internal_file(editor, buf, &cb);

		clipboard_free(&cb);		
	}
}


// editor_handle_cmd splits the string passed
// to the prompt; however, there are some
// commands where it is necessary to join
// those words back into a single string
// considering the PATTERN FORMAT
static int words_to_u8string
(
	Editor* editor,
	char** words,
	size_t count,
	char** string 		// must be freed
)
{
	if (!words || count == 0 || !string) {
		return -1;
	}

	if (*words[0] != '"') {
		size_t size = strlen(words[0]);

		*string = malloc((size + 1) * sizeof(char));

		if (!(*string)) {
			prompt_init(&editor->status_bar,
				"internal error",
				PT_INFO);

			*string = NULL;
			return -1;
		}

		memcpy(
			*string,
			words[0],
			size * sizeof(char)
		);

		(*string)[size] = '\0';
		return 0;
	}

	else {
		char* joined_args;

		if (strjoin(words, count, &joined_args, " ") < 0) {
			prompt_init(
				&editor->status_bar,
				"internal error",
				PT_INFO
			);

			*string = NULL;
			return -1;
		}

		char tmp[512];

		int valid = parse_quoted(
			joined_args, 
			tmp, 
			sizeof(tmp));

		free(joined_args);

		if (!valid) {
			prompt_init(&editor->status_bar,
				PROMPT_INVALID_ARG,
				PT_INFO);

			*string = NULL;
			return -1;
		}

		if (strlen(tmp) == 0) {
			prompt_init(&editor->status_bar,
				PROMPT_EMPTY_STRING,
				PT_INFO);

			*string = NULL;
			return -1;
		}

		*string = strdup(tmp);
		return 0;
	}	
}


// SYSTEM helper
static void run_extern_command
(
	Editor* editor,
	const char* cmd,
	Clipboard* cb
) 
{
	FILE* fp = popen(cmd, "r");
	char buffer[4096];

	while (fgets(buffer, sizeof(buffer), fp) != NULL) {
		if (cb) {
			u32string tmp = u32string_from(buffer);

			u32string_append_raw(
				&cb->text,
				u32string_into_ptr_const(&tmp),
				u32string_size(&tmp)
			);

			u32string_free(&tmp);
		}
	}

	int status = pclose(fp);

	int code = (WIFEXITED(status))
		? WEXITSTATUS(status)
		: 0; // check WIFEXITED

	if (!cb) {
		char buf[64];
		sprintf(buf, "exit code: %d", code);

		prompt_init(
			&editor->status_bar,
			buf,
			PT_INFO
		);

		return;
	}

	if (cb &&
		!u32string_is_empty(&cb->text) &&
		u32string_char(&cb->text, u32string_size(&cb->text) - 1) == 
		U'\n') 
	{
		u32string_remove(
			&cb->text, 
			u32string_size(&cb->text) - 1,
			NULL);
	}
}


#define MAX_WORDS_LENGTH 4

// --- EDITOR_HANDLE_CMD --- aka THE MONSTER
int editor_handle_cmd
(
	Editor* editor, 
	const u32string* u32_cmd
) 
{
	char* cmd = u32string_into_u8(u32_cmd);

	char* words[MAX_WORDS_LENGTH];
	size_t n = 0;

	// split
	for (char* tok = strtok(cmd, " ");
		 tok;
		 tok = strtok(NULL, " "))
	{
		if (n > MAX_WORDS_LENGTH) {
			break;
		}

		words[n++] = tok;
	}


	/// --- SAVE ---
	if (strcmp(words[0], save.name) == 0) {
		editor_save_file(editor, editor->actual_file_index);

		goto cleanup;
	}


	/// --- SAVEAS ---
	if (strcmp(words[0], saveas.name) == 0) {
		if (n == 1) {
			prompt_init(
				&editor->status_bar,
				PROMPT_NO_ARGS,
				PT_INFO);

			goto cleanup;
		}

		// > saveas atumalaka.laka
		else {
			const char* new_name = words[1];

			ssize_t index = editor_has_filename(editor, new_name);

			if (index >= 0 &&
				(size_t) index != editor->actual_file_index) 
			{
				// MAYBE: it may also be possible to close the
				// existing file instead returning an error
				prompt_init(
					&editor->status_bar,
					PROMPT_FILE_EXISTS,
					PT_INFO);

				goto cleanup;
			}

			editor_file_set_lang_plugin(
				editor->actual_file,
				new_name,
				&editor->lang_plugins_data);

			// rename
			if (editor->actual_file->file.filename &&
				file_exists(editor->actual_file->file.filename)) {
				// otherwise, the user would receive a noti notification
				editor_file_del_watcher(
					editor->actual_file,
					editor->inotify_fd
				);

				int ret = rename(
					editor->actual_file->file.filename,
					new_name
				);

				editor_file_set_watcher(
					editor->actual_file,
					new_name,
					editor->inotify_fd
				);

				if (ret != 0) {
					prompt_init(
						&editor->status_bar,
						strerror(errno),
						PT_INFO
					);

					goto cleanup;
				}


				free(editor->actual_file->file.filename);
				editor->actual_file->file.filename = strdup(new_name);

				editor_save_file(editor, editor->actual_file_index);

				editor_prompt_init(editor);

				file_set_all_lines_dirty(&editor->actual_file->file);

				if (editor->config.use_autocomplete &&
					!editor->actual_file->readonly) 
				{
					editor_file_sync(
						editor->actual_file,
						editor->config.use_autocomplete,
						&editor->result
					);				
				}

				// editor_file_update_metadata(editor->actual_file);

				goto cleanup;		
			}

			// create a new file
			free(editor->actual_file->file.filename);
			editor->actual_file->file.filename = strdup(new_name);

			editor_save_file(editor, editor->actual_file_index);
			editor_prompt_init(editor);

			file_set_has_dirty_line(&editor->actual_file->file);

			goto cleanup;
		}
	}


	/// --- QUIT ---
	if (strcmp(words[0], quit.name) == 0) {
		free(cmd);
		int ret = editor_quit(editor);

		if (ret == 0) {
			return -1; // -1 == quit the program
		}

		else {
			return 0;
		}
	}


	/// --- QUITS ---
	if (strcmp(words[0], quits.name) == 0) {
		free(cmd);

		for (size_t i = 0; i < editor->files.size; i++) {
			EditorFile* ef = vector_get(&editor->files, i);

			if (ef->type == EDITOR_FILE_PROTOTYPE) {
				continue;
			}

			if (ef->file.dirty && ef->file.filename) {
				file_save(&ef->file, &editor->result);
			}
		}

		return -1; // -1 == quit the program
	}


	/// --- QUIT! ---
	if (strcmp(words[0], quit_forced.name) == 0) {
		free(cmd);
		return -1;
	}


	/// --- FIND ---
	if (strcmp(words[0], find.name) == 0) {
		if (n == 1) {
			prompt_init(&editor->status_bar,
				PROMPT_NO_ARGS,
				PT_INFO);

			goto cleanup;
		}

		char* pattern;
		int ret = words_to_u8string(
			editor,
			words + 1,
			n - 1,
			&pattern
		);

		if (ret < 0) {
			goto cleanup;
		}

		u32string patt = u32string_from(pattern);
		free(pattern);

		if (editor->sel.active) {
			int found = editor_find_next_pattern_within_selection(
				editor,
				&patt,
				1
			);

			if (!found) {
				prompt_init(
					&editor->status_bar,
					"not found",
					PT_INFO
				);
			}

			u32string_free(&patt);

			goto cleanup;
		}

		else {
			int found = editor_find_next_pattern_match(
				editor,
				POS_ZERO,
				&patt,
				1
			);

			if (!found) {
				prompt_init(
					&editor->status_bar,
					"not found",
					PT_INFO
				);
			}

			u32string_free(&patt);

			goto cleanup;
		}
	}


	/// --- MATCH ---
	if (strcmp(words[0], match.name) == 0) {
		editor_match(editor);

		goto cleanup;
	}


	/// --- GOTO ---
	if (strcmp(words[0], goto_cmd.name) == 0) {
		if (n == 1) {
			editor_cursor_move(editor,
				(Position) { 0, 0 });

			selection_update(&editor->sel, editor->cursor.pos);

			goto cleanup;
		}

		char* end;
		long numb = strtol(words[1], &end, 10);

		// goto a file
		if (end == words[1] || *end != '\0') {
			char* filename;

			int ret = words_to_u8string(
				editor,
				words + 1,
				n - 1,
				&filename
			);

			if (ret < 0) {
				goto cleanup;
			}

			ssize_t index = editor_has_filename(editor, filename);
			free(filename);

			if (index < 0) {
				prompt_init(
					&editor->status_bar,
					PROMPT_INVALID_FILENAME,
					PT_INFO);

				goto cleanup;
			}

			editor_change_actual_file(editor, index);

			goto cleanup;
		}

		// goto a line

		size_t lines = file_num_lines(&editor->actual_file->file);

		if (numb < 0) {
			prompt_init(
				&editor->status_bar,
				PROMPT_OUT_OF_BOUNDS,
				PT_INFO);

			goto cleanup;
		}

		size_t y = (numb == 0 || (size_t) numb > lines) 
			? lines - 1 
			: (size_t) numb - 1;

		editor_cursor_move(editor,
			(Position)  { 0, y });

		selection_update(&editor->sel, editor->cursor.pos);

		goto cleanup;
	}


	/// --- NEXT ---
	if (strcmp(words[0], next.name) == 0) {
		editor_change_next_file(editor);

		goto cleanup;
	}


	/// --- PREV ---
	if (strcmp(words[0], prev.name) == 0) {
		editor_change_prev_file(editor);

		goto cleanup;
	}


	/// --- NEW ---
	if (strcmp(words[0], new.name) == 0) {
		editor_create_new_file(editor, 0);

		goto cleanup;
	}


	/// --- NEWAS ---
	if (strcmp(words[0], newas.name) == 0) {
		if (n == 1) {
			prompt_init(
				&editor->status_bar,
				PROMPT_NO_ARGS,
				PT_INFO);

			goto cleanup;
		}

		char* filename;
		int ret = words_to_u8string(
			editor,
			words + 1,
			n - 1,
			&filename
		);

		if (ret < 0) {
			goto cleanup;
		}


		// it's an error to try to create
		// an existing file
		if (editor_has_filename(editor, filename) >= 0) {
			prompt_init(
				&editor->status_bar,
				PROMPT_FILE_EXISTS_INTERNALLY,
				PT_INFO
			);

			free(filename);
			goto cleanup;
		}

		editor_create_new_file(editor, 0);
		editor->actual_file->internal = 0;
		editor->actual_file->file.filename = strdup(filename);

		if (file_exists(filename)) {
			prompt_init(
				&editor->status_bar,
				PROMPT_HAS_EQUAL_FILE,
				PT_INFO
			);

			editor->status_bar.invert_color = 1;
		}

		else {
			editor_prompt_init(editor);
		}

		free(filename);

		goto cleanup;
	}


	/// --- CLOSE ---
	if (strcmp(words[0], close_cmd.name) == 0) {
		if (n == 1) {
			editor_close_file(editor, editor->actual_file_index);
			goto cleanup;
		}

		char* filename;
		int ret = words_to_u8string(
			editor,
			words + 1,
			n - 1,
			&filename
		);

		if (ret < 0) {
			goto cleanup;
		}

		ssize_t index = editor_has_filename(editor, filename);

		free(filename);

		if (index < 0) {
			prompt_init(
				&editor->status_bar,
				PROMPT_FILE_DNT_EXIST,
				PT_INFO
			);

			goto cleanup;
		}

		editor_close_file(editor, index);

		goto cleanup;
	}



	/// --- CLOSE! ---
	if (strcmp(words[0], close_forced.name) == 0) {
		if (n == 1) {
			editor_close_file_forced(editor, editor->actual_file_index);
			goto cleanup;
		}

		char* filename;
		int ret = words_to_u8string(
			editor,
			words + 1,
			n - 1,
			&filename
		);

		if (ret < 0) {
			goto cleanup;
		}

		ssize_t index = editor_has_filename(editor, filename);

		free(filename);

		if (index < 0) {
			prompt_init(
				&editor->status_bar,
				PROMPT_FILE_DNT_EXIST,
				PT_INFO
			);

			goto cleanup;
		}

		editor_close_file_forced(editor, index);

		goto cleanup;
	}


	/// --- OPEN ---
	if (strcmp(words[0], open_cmd.name) == 0) {
		if (n == 1) {
			prompt_init(
				&editor->status_bar,
				PROMPT_NO_ARGS,
				PT_INFO
			);

			goto cleanup;
		}

		char* filename;
		int ret = words_to_u8string(
			editor,
			words + 1,
			n - 1,
			&filename
		);

		if (ret < 0) {
			goto cleanup;
		}

		ssize_t index = editor_has_filename(editor, filename);

		if (index >= 0) {
			editor_change_actual_file(editor, index);
		}

		else {
			editor_open_file(editor, filename, 0);
		}

		free(filename);

		goto cleanup;
	}


	/// --- SELECT ---
	if (strcmp(words[0], select_cmd.name) == 0) {
		if (n == 1) {
			editor_start_and_create_selection(editor);

			goto cleanup;
		}

		const char* arg = words[1];

		if (strcmp(arg, "all") == 0) {
			editor_select_all_file(editor);

			goto cleanup;
		}

		if (strcmp(arg, "word") == 0) {
			editor_select_word(editor);

			goto cleanup;
		}

		if (strcmp(arg, "complete") == 0) {
			editor_selection_complete(editor);

			goto cleanup;
		}

		if (strcmp(arg, "line") == 0) {
			if (n == 2) {
				editor_select_line(editor);

				goto cleanup;
			}

			char* end;
			long numb = strtol(words[2], &end, 10);

			if (end == words[2] || *end != '\0') {
				prompt_init(
					&editor->status_bar,
					PROMPT_INVALID_ARG,
					PT_INFO
				);

				goto cleanup;
			}

			if (numb < 0 || 
				(size_t) numb > file_num_lines(&editor->actual_file->file)) 
			{
				prompt_init(
					&editor->status_bar,
					PROMPT_OUT_OF_BOUNDS,
					PT_INFO
				);

				goto cleanup;
			}

			size_t index = (numb == 0)
				? file_num_lines(&editor->actual_file->file) - 1
				: (size_t) numb - 1;

			editor_cursor_move(editor,
				(Position) { 0, index });

			editor_select_line(editor);

			goto cleanup;
		}

		if (strcmp(arg, "lines") == 0) {
			if (n < 4) {
				prompt_init(
					&editor->status_bar,
					PROMPT_INSUFFICIENT_ARGS,
					PT_INFO
				);

				goto cleanup;
			}

			char* end;
			long y1 = strtol(words[2], &end, 10);

			if (end == words[2] || *end != '\0') {
				prompt_init(
					&editor->status_bar,
					PROMPT_INVALID_ARG,
					PT_INFO
				);

				goto cleanup;			
			}

			long y2 = strtol(words[3], &end, 10);

			if (end == words[3] || *end != '\0') {
				prompt_init(
					&editor->status_bar,
					PROMPT_INVALID_ARG,
					PT_INFO
				);

				goto cleanup;			
			}

			if (y1 < 0 || y2 < 0 ||
				(size_t) y1 > file_num_lines(&editor->actual_file->file) ||
				(size_t) y2 > file_num_lines(&editor->actual_file->file))
			{
				prompt_init(
					&editor->status_bar,
					PROMPT_OUT_OF_BOUNDS,
					PT_INFO
				);

				goto cleanup;			
			}

			editor->sel.active = 1;
			editor->selecting = 0;

			size_t index1 = (y1 == 0)
				? file_num_lines(&editor->actual_file->file) - 1
				: (size_t) y1 - 1;

			size_t index2 = (y2 == 0)
				? file_num_lines(&editor->actual_file->file) - 1
				: (size_t) y2 - 1;				

			if (index1 < index2) {
				size_t y2_size = file_size_line(
					&editor->actual_file->file, index2);

				editor->sel.start.x = 0;
				editor->sel.end.x = y2_size;
				editor->sel.start.y = index1;
				editor->sel.end.y = index2;

				editor_cursor_move(editor,
					(Position) { y2_size, index2 } );
			}

			else {
				size_t y1_size = file_size_line(
					&editor->actual_file->file, index1);

				editor->sel.start.x = 0;
				editor->sel.end.x = y1_size;
				editor->sel.start.y = index2;
				editor->sel.end.y = index1;

				editor_cursor_move(editor,
					(Position) { 0, index2 } );
			}

			goto cleanup;
		}

		prompt_init(
			&editor->status_bar,
			PROMPT_INVALID_ARG,
			PT_INFO
		);

		goto cleanup;
	}


	/// --- COPY ---
	if (strcmp(words[0], copy.name) == 0) {
		editor_copy_selection(editor);

		goto cleanup;
	}


	/// --- PASTE ---
	if (strcmp(words[0], paste.name) == 0) {
		editor_paste_clipboard(editor);

		goto cleanup;
	}


	/// --- INDENT ---
	if (strcmp(words[0], indent.name) == 0) {
		editor_indent_selection_or_line(editor);

		goto cleanup;
	}


	/// --- UNINDENT ---
	if (strcmp(words[0], unindent.name) == 0) {
		editor_unindent_selection_or_line(editor);

		goto cleanup;
	}


	/// --- COMMENT ---
	if (strcmp(words[0], comment.name) == 0) {
		editor_comment_line_or_selection(editor);

		goto cleanup;
	}


	/// --- REPLACE ---
	if (strcmp(words[0], replace.name) == 0) {
		if (n == 1) {
			prompt_init(
				&editor->status_bar,
				PROMPT_INSUFFICIENT_ARGS,
				PT_INFO
			);

			goto cleanup;
		}

		u32string pattern;
		u32string text;

		if (*words[1] != '"') {
			pattern = u32string_from(words[1]);

			if (n == 2) {
				text = u32string_new();
			}

			else {
				if (*words[2] != '"') {
					text = u32string_from(words[2]);
					goto replace;
				}

				char* joined_text;

				// n >= 3
				if (strjoin(words + 2, n - 2, &joined_text, " ") < 0) {
					prompt_init(
						&editor->status_bar,
						"internal error",
						PT_INFO
					);

					u32string_free(&pattern);
					goto cleanup;
				}

				char tmp[512];
				int valid = parse_quoted(joined_text, tmp, sizeof(tmp));

				if (!valid) {
					prompt_init(
						&editor->status_bar,
						PROMPT_INVALID_ARG,
						PT_INFO
					);

					free(cmd);
					free(joined_text);
					u32string_free(&pattern);
					return 0;						
				}

				text = u32string_from(tmp);
				free(joined_text);
			}
		}

		else {
			char* joined_args;

			if (strjoin(words + 1, n - 1, &joined_args, " ") < 0) {
				prompt_init(
					&editor->status_bar,
					"internal error",
					PT_INFO
				);

				goto cleanup;
			}

			size_t pattern_size;

			{
				char tmp[512];
				int valid = parse_quoted(joined_args, tmp, sizeof(tmp));

				if (!valid ) {
					prompt_init(
						&editor->status_bar,
						PROMPT_INVALID_ARG,
						PT_INFO
					);

					free(joined_args);
					goto cleanup;
				}

				pattern = u32string_from(tmp);
				pattern_size = strlen(tmp);

				if (pattern_size == 0) {
					prompt_init(
						&editor->status_bar,
						PROMPT_INVALID_ARG,
						PT_INFO
					);

					free(joined_args);
					u32string_free(&pattern);

					goto cleanup;
				}
			} // drop tmp

			// Assume 'joined_args = "hello, world"', where the
			// quotation marks are part of the string.
			// Since joined_args is a valid string, pattern will
			// contain 'hello, world' without the quotes.
			// Therefore, if strlen(joined_args) ==
			// pattern_size + 2, the text must be an empty string.
			if (strlen(joined_args) == pattern_size + 2) {
				text = u32string_new();
				free(joined_args);
				goto replace;
			}

			//								  '"' + '"' + ' ' = 3;
			char* joined_text = joined_args + pattern_size + 3;

			if (*joined_text != '"') {
				char* space = strchr(joined_text, ' ');

				if (space) {
					char tmp[512];
					size_t len = space - joined_text;

					strncpy(tmp, joined_text, len);
					tmp[len] = '\0';

					text = u32string_from(tmp);
					free(joined_args);
					goto replace;
				}
			}

			else {
				char tmp[512];
				int valid = parse_quoted(joined_text, tmp, sizeof(tmp));

				if (!valid) {
					prompt_init(
						&editor->status_bar,
						PROMPT_INVALID_ARG,
						PT_INFO
					);

					free(joined_args);
					u32string_free(&pattern);

					goto cleanup;
				}

				text = u32string_from(tmp);
				free(joined_args);
			}
		}

	replace:
		if (!editor->sel.active) {
			if (!editor_find_next_pattern_match(
					editor,
					POS_ZERO,
					&pattern,
					1))
			{
				prompt_init(
					&editor->status_bar,
					PROMPT_NOT_FOUND,
					PT_INFO);

				u32string_free(&pattern);
				u32string_free(&text);

				goto cleanup;
			}

			editor_replace_pattern(
				editor,
				&pattern,
				&text);
		
			u32string_free(&pattern); u32string_free(&text);
			goto cleanup;
		}

		else {
			if (!editor_find_next_pattern_within_selection(
					editor,
					&pattern,
					1))
			{
				prompt_init(
					&editor->status_bar,
					PROMPT_NOT_FOUND,
					PT_INFO);

				u32string_free(&pattern);
				u32string_free(&text);

				goto cleanup;
			}

			editor_replace_within_selection(
				editor,
				&pattern,
				&text);
		
			u32string_free(&pattern); u32string_free(&text);

			goto cleanup;
		}
	}


	/// --- UNDO ---
	if (strcmp(words[0], undo.name) == 0) {
		if (n == 1) {
			editor_undo(editor);
			goto cleanup;
		}

		const char* arg = words[1];

		if (strcmp(arg, "clear") == 0) {
			stack_clear(&editor->actual_file->undo);
			stack_clear(&editor->actual_file->redo);

			goto cleanup;
		}

		prompt_init(
			&editor->status_bar,
			PROMPT_INVALID_ARG,
			PT_INFO
		);

		goto cleanup;
	}


	/// --- REDO ---
	if (strcmp(words[0], redo.name) == 0) {
		if (n == 1) {
			editor_redo(editor);
			goto cleanup;
		}

		const char* arg = words[1];

		if (strcmp(arg, "clear") == 0) {
			stack_clear(&editor->actual_file->redo);
			goto cleanup;
		}

		prompt_init(
			&editor->status_bar,
			PROMPT_INVALID_ARG,
			PT_INFO
		);

		goto cleanup;
	}


	/// --- SET ---
	if (strcmp(words[0], set.name) == 0) {
		if (n == 1) {
			prompt_init(
				&editor->status_bar,
				PROMPT_INSUFFICIENT_ARGS,
				PT_INFO
			);

			goto cleanup;			
		}

		const char* arg = words[1];

		if (strcmp(arg, "spaces") == 0) {
			editor->actual_file->file.use_spaces = 
				!editor->actual_file->file.use_spaces;

			goto cleanup;
		}

		if (strcmp(arg, "tabsize") == 0) {
			char* end;
			long numb = strtol(words[2], &end, 10);

			if (end == words[2] || *end != '\0') {
				prompt_init(
					&editor->status_bar,
					PROMPT_INVALID_ARG,
					PT_INFO
				);

				goto cleanup;
			}

			if (numb <= 0) {
				prompt_init(
					&editor->status_bar,
					PROMPT_INVALID_ARG,
					PT_INFO
				);

				goto cleanup;
			}

			editor->actual_file->file.tab_size = numb;

			goto cleanup;
		}

		if (strcmp(arg, "perm") == 0) {
			if (n == 2) {
				prompt_init(
					&editor->status_bar,
					PROMPT_INSUFFICIENT_ARGS,
					PT_INFO
				);

				goto cleanup;			
			}

			const char* value = words[2];

			if (strcmp(value, "readonly") == 0 &&
				!editor->actual_file->readonly) 
			{
				editor_file_set_readonly(
					editor->actual_file);

				if (editor->config.use_autocomplete) {
					trie_free(&editor->actual_file->words);
				}

				goto cleanup;
			}

			if (strcmp(value, "writeable") == 0 &&
				editor->actual_file->readonly) 
			{
				if (editor_file_set_writeable(
					editor->actual_file) < 0)
				{
					prompt_init(
						&editor->status_bar,
						PROMPT_PERM_DENIED,
						PT_INFO
					);
				}

				else {
					if (editor->config.use_autocomplete) 
					{
						editor_file_sync(
							editor->actual_file,
							editor->config.use_autocomplete,
							&editor->result
						);				
					}
				}

				goto cleanup;
			}

			prompt_init(
				&editor->status_bar,
				PROMPT_INVALID_ARG,
				PT_INFO
			);

			goto cleanup;			
		}

		if (strcmp(arg, "indent") == 0) {
			if (n == 2) {
				prompt_init(
					&editor->status_bar,
					PROMPT_INSUFFICIENT_ARGS,
					PT_INFO
				);

				goto cleanup;
			}

			const char* value = words[2];

			if (strcmp(value, "spaces") == 0) {
				file_convert_tabs_to_spaces(
					&editor->actual_file->file);

				goto cleanup;
			}

			if (strcmp(value, "tabs") == 0) {
				file_convert_spaces_to_tabs(
					&editor->actual_file->file);

				goto cleanup;
			}

			prompt_init(
				&editor->status_bar,
				PROMPT_INVALID_ARG,
				PT_INFO
			);

			goto cleanup;
		}

		if (strcmp(arg, "language") == 0) {
			if (n == 2) {
				prompt_init(
					&editor->status_bar,
					PROMPT_INSUFFICIENT_ARGS,
					PT_INFO
				);

				goto cleanup;
			}

			const char* value = words[2];

			int ret = editor_file_change_lang(
					editor->actual_file,
					value,
					&editor->lang_plugins_data);

			if (ret < 0) {
				prompt_init(
					&editor->status_bar,
					PROMPT_INVALID_LANGUAGE,
					PT_INFO
				);

				goto cleanup;
			}

			if (editor->config.use_autocomplete &&
				!editor->actual_file->readonly) 
			{
				editor_file_sync(
					editor->actual_file,
					editor->config.use_autocomplete,
					&editor->result
				);				
			}

			goto cleanup;
		}

		prompt_init(
			&editor->status_bar,
			PROMPT_INVALID_ARG,
			PT_INFO
		);

		goto cleanup;
	}


	/// --- TERMINAL ---
	if (strcmp(words[0], terminal.name) == 0) {
		editor_open_terminal(editor);

		goto cleanup;
	}


	/// --- SYSTEM ---
	if (strcmp(words[0], system_cmd.name) == 0) {
		if (n == 1) {
			prompt_init(
				&editor->status_bar,
				PROMPT_INSUFFICIENT_ARGS,
				PT_INFO
			);

			goto cleanup;
		}

		char* command;

		if (strjoin(words + 1, n - 1, &command, " ") < 0) {
			prompt_init(
				&editor->status_bar,
				"internal error",
				PT_INFO
			);

			goto cleanup;			
		}

		run_extern_command(
			editor,
			command,
			NULL
		);

		free(command);

		goto cleanup;
	}


	/// --- SHOW ----
	if (strcmp(words[0], "show") == 0) {
		if (n == 1) {
			prompt_init(
				&editor->status_bar,
				PROMPT_INSUFFICIENT_ARGS,
				PT_INFO
			);

			goto cleanup;	
		}

		const char* arg = words[1];

		if (strcmp(arg, "cursor") == 0) {
			show_cursor();

			goto cleanup;
		}

		if (strcmp(arg, "file") == 0) {
			editor_prompt_init(editor);

			goto cleanup;
		}

		if (strcmp(arg, "lines") == 0) {
			editor->config.line_numbers = !editor->config.line_numbers;

			goto cleanup;
		}

		if (strcmp(arg, "language") == 0) {
			if (editor->actual_file->language) {
				prompt_init(
					&editor->status_bar,
					editor->actual_file->language->name,
					PT_INFO
				);
			}

			else {
				prompt_init(
					&editor->status_bar,
					"no language",
					PT_INFO
				);
			}

			goto cleanup;
		}

		if (strcmp(arg, "tabs") == 0) {
			editor->config.show_tabs = !editor->config.show_tabs;

			goto cleanup;
		}

		prompt_init(
			&editor->status_bar,
			PROMPT_INVALID_ARG,
			PT_INFO
		);

		goto cleanup;
	}


	/// --- FILES ---
	if (strcmp(words[0], files.name) == 0) {
		WindowOptions options = {
			.pos_type = WINDOWPOS_CENTRALIZED,
			.sw = 0.2,
			.sh = 0.5,
			.tsize = editor->tsize,
			.tab_size = editor->config.tab_size,
			.on_select = on_select_goto_file
		};

		editor->window = window_new(&options);
		editor->has_window = 1;

		for (size_t i = 0; i < editor->files.size; i++) {
			const EditorFile* ef = vector_get_const(
				&editor->files,
				i
			);

			u32string name;

			if (ef->type == EDITOR_FILE_PROTOTYPE) {
				assert(ef->proto_data.options.filename != NULL);

				name = u32string_from(ef->proto_data.options.filename);
			}

			else {
				if (ef->file.filename) {
					name = u32string_from(ef->file.filename);
				}

				else {
					name = u32string_from("untitled");
				}
			}

			vector_push(&editor->window.content, &name);
		}

		goto cleanup;		
	}


	/// --- SUSPEND ---
	if (strcmp(words[0], suspend.name) == 0) {
		editor->suspend = 1;

		goto cleanup;
	}


	/// --- BLOCK ---
	if (strcmp(words[0], block.name) == 0) {
		int found = editor_find_actual_block(editor);

		if (!found) {
			const char not_found[] = "not inside a block";
			const char* message = not_found;

			if (!editor->actual_file->language) {
				message = PROMPT_LANG_PLUGIN_NULL;
			}

			else if (!editor->actual_file->language->rules) {
				message = PROMPT_LANG_RULES_NULL;
			}

			else if (!editor->actual_file->language->rules->pairs) {
				message = PROMPT_LANG_PAIRS_NULL;
			}

			prompt_init(
				&editor->status_bar,
				message,
				PT_INFO
			);
		}

		goto cleanup;
	}


	/// --- THEMES ---
	if (strcmp(words[0], themes.name) == 0) {
		WindowOptions options = {
			.pos_type = WINDOWPOS_CENTRALIZED,
			.sw = 0.2,
			.sh = 0.5,
			.tsize = editor->tsize,
			.tab_size = editor->config.tab_size,
			.on_select = on_select_change_theme
		};

		editor->window = window_new(&options);
		editor->has_window = 1;

		const char* home = getenv("HOME");

		if (!home) {
			prompt_init(
				&editor->status_bar,
				strerror(errno),
				PT_INFO
			);

			window_close(&editor->window);
			editor->has_window = 0;

			goto cleanup;
		}

		char path[PATH_MAX_LENGTH];

		sprintf(path, "%s/.config/nytor/themes/", home);

		DIR* dir = opendir(path);

		if (!dir) {
			prompt_init(
				&editor->status_bar,
				strerror(errno),
				PT_INFO
			);

			window_close(&editor->window);
			editor->has_window = 0;

			goto cleanup;			
		}

		struct dirent* entry;

		while ((entry = readdir(dir)) != NULL) {
			if (strcmp(entry->d_name, ".") == 0 ||
				strcmp(entry->d_name, "..") == 0)
			{
				continue;
			}

			u32string theme = u32string_from(entry->d_name);

			vector_push(&editor->window.content, &theme);
		}

		closedir(dir);

		goto cleanup;
	}


	/// --- SYNC ---
	if (strcmp(words[0], sync_cmd.name) == 0) {
		if (!editor->actual_file->new_file) {
			editor_file_sync(
				editor->actual_file,
				editor->config.use_autocomplete,
				&editor->result
			);
		}

		goto cleanup;
	}

	
	/// --- HELP ---
	if (strcmp(words[0], help.name) == 0) {
		handle_help(editor, words, n);

		goto cleanup;
	}
	

	prompt_init(
		&editor->status_bar,
		PROMPT_INVALID_COMMAND,
		PT_INFO);

cleanup:
	free(cmd);
	return 0;
}
