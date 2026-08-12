#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <fcntl.h>
#include <unistd.h>
#include <assert.h>
#include <errno.h>
#include <assert.h>

#include "file/file.h"
#include "terminal/input.h"

static void vector_line_destroy(void* ptr) {
	Line* line = (Line*) ptr;
	line_free(line);
}

void file_init(File* file) {
	if (!file) {
		return;
	}

	vector_init(&file->lines, sizeof(Line), vector_line_destroy);
}

void file_free(File* file) {
	if (!file) { return; }

	free(file->filename);
	vector_free(&file->lines);
}

void file_set_line_dirty(File* file, size_t y) {
	Line* line = vector_get(&file->lines, y);
	line->dirty = 1;
}

u32string* file_get_line_text(File* file, size_t y) {
	Line* line = vector_get(&file->lines, y);
	return &line->text;
}

Vector* file_get_line_tokens(File* file, size_t y) {
	Line* line = vector_get(&file->lines, y);
	return &line->tokens;
}

Line* file_get_line
(
	File* file,
	size_t y
)
{
	return vector_get(&file->lines, y);
}


size_t file_num_lines
(
	const File* file
)
{
	return file->lines.size;
}

size_t file_size_line
(
	File* file,
	size_t y
)
{
	const u32string* line = file_get_line_text(file, y);

	if (!line) {
		return 0;
	}

	return u32string_size(line);
}

void file_push(File* file, Line* line) {
	if (!file || !line) {
		return;
	}

	vector_push(&file->lines, line);
}

static uint32_t* read_file
(
	const char* filename, // a non-null pointer
	ssize_t* size, // a non null pointer
	Result* result // a non null pointer
) 
{
	int fd = open(filename, O_RDONLY);

	if (fd < 0) {
		if (result) {
			if (errno == ENOENT) {
				result_set_reason(result,
					"read_file: file does not exist");
				result->type = ERROR_FILE_DOES_NOT_EXIST;
			} 

			else {
				char reason[512];
				sprintf(reason, 
					"read_file (%s): access denied", 
					 filename);

				result_set_reason(result, reason);
				result->type = ERROR_FILE_HANDLE;           
			}
		}

		*size = -1;

		return NULL;
	}

	off_t fsize = lseek(fd, 0, SEEK_END);

	if (fsize == 0) {
		close(fd);

		*size = 0;

		if (result) {
			result_ok(result);
		}

		return NULL;    
	}

	lseek(fd, 0, SEEK_SET);

	char* bytes = malloc(fsize);

	if (!bytes) {
		close(fd);

		if (result) {
			result_set_reason(result,
				"read_file: bytes is invalid");
			result->type = ERROR_MALLOC;
		}

		return NULL;
	}

	ssize_t read_bytes = read(fd, bytes, fsize);
	close(fd);

	if (read_bytes < 0) {
		free(bytes);

		if (result) {
			result_set_reason(result,
				"read_file: read_bytes < 0");
			result->type = ERROR_SYS_READ;
		}

		return NULL;
	}

	uint32_t* data = malloc(fsize * sizeof(*data));

	if (!data) {
		free(bytes);

		if (result) {
			result_set_reason(result,
				"read_file: data is invalid");
			result->type = ERROR_MALLOC;
		}

		return NULL;
	}

	size_t i = 0;
	size_t j = 0;

	while (i < (size_t) read_bytes) {
		uint32_t cp;

		int n = u32_decode(bytes + i, read_bytes - i, &cp);

		if (n < 0) { // invalid UTF-8
			break;
		}

		data[j++] = cp;

		i += n;
	}

	*size = j;
	data = realloc(data, j * sizeof(*data));

	if (result) {
		result_ok(result);
	}

	free(bytes);
	return data;
}

static int split_lines
(
	File* file, 
	const uint32_t* data, 
	size_t size
) 
{
	size_t start = 0;

	while (start < size) {
		size_t end = start;

		while (end < size && data[end] != U'\n') {
			end++;
		}

		u32string text = u32string_new();

		for (size_t i = start; i < end; i++) {
			u32string_push(&text, data[i]);
		}

		Line line = line_with_text(text);
		file_push(file, &line);

		start = end + 1;
	}

	return EIE_OK;
}

static void file_create_empty
(
	File* file, 
	size_t tab_size,
	int use_spaces
) 
{
	file_init(file);
	file->filename = NULL;
	file->dirty = 1;
	file->tab_size = tab_size;
	file->use_spaces = use_spaces;

	Line line = line_new();

	file_push(file, &line);
}

int file_open
(
	File* file, 
	const char* filename, 
	size_t tab_size,
	int use_spaces,
	Result* result
) 
{   
	if (!file) {
		result_set_reason(result,
			"file_open: file is a null pointer");
		result->type = ERROR_NULL_POINTER;

		return EIE_FATAL_ERROR;
	}

	if (!filename) {
		file_create_empty(file, tab_size, use_spaces);

		result_ok(result);

		return EIE_OK;
	}

	ssize_t size = -1;

	uint32_t* data = read_file(
		filename, 
		&size, 
		result);

	if (!data) {
		if (result->type == ERROR_FILE_DOES_NOT_EXIST) {
			int fd = open(filename, O_CREAT | O_EXCL | O_RDWR, 0644);

			if (fd < 0) {
				perror("sys_open");
				result_set_reason(result,
					"file_open: invalid file");
				result->type = ERROR_FILE_HANDLE;

				return EIE_FATAL_ERROR;
			} 

			else {
				close(fd);
				unlink(filename);
			}

			file_create_empty(file, tab_size, use_spaces);
			file->filename = strdup(filename);

			result_ok(result);

			return EIE_OK;          
		}

		if (size == 0) {
			file_create_empty(file, tab_size, use_spaces);
			file->filename = strdup(filename);
			file->dirty = 0;

			return EIE_OK;
		}

		return EIE_FATAL_ERROR;
	}

	file_init(file);
	file->filename = strdup(filename);
	file->dirty = 0;
	file->tab_size = tab_size;
	file->use_spaces = use_spaces;

	if (split_lines(file, data, size) < 0) {
		free(data);

		result_set_reason(result,
			"file_open: error returned by a static function");
		result->type = ERROR_STATIC;

		return EIE_FATAL_ERROR;
	}

	free(data);
	result_ok(result);

	return EIE_OK;
}

void file_convert_spaces_to_tabs
(
	File* file
)
{
	size_t tab_size = file->tab_size;

	u32string tab = u32string_from("\t");

	for (size_t i = 0; i < file->lines.size; i++) {
		u32string* text = vector_get(&file->lines, i);

		int first = 1;
		size_t run_start = 0;
		size_t run_len = 0;

		for (size_t j = 0; j < u32string_size(text); j++) {
			uint32_t c = u32string_char(text, j);

			if (run_len == 0) {
				run_start =  j;
			}

			if (c == U' ') {
				run_len++;
			}

			else {
				break;
			}

			if (run_len % tab_size == 0) {
				if (first) {
					file_set_line_dirty(file, i);
					first = 0;
				}

				u32string_remove_range(
					text,
					run_start,
					run_start + run_len
				);

				u32string_insert_range_raw(
					text,
					run_start,
					u32string_into_ptr_const(&tab),
					1
				);

				j = run_start;

				run_len = 0;        
			}
		}
	}

	u32string_free(&tab);

	file->dirty = 1;
	file->use_spaces = 0;
}

void file_convert_tabs_to_spaces(File* file) {
	size_t tab_size = file->tab_size;

	u32string spaces = u32string_new();

	for (size_t i = 0; i < tab_size; i++) {
		u32string_push(&spaces, U' ');
	}

	for (size_t i = 0; i < file->lines.size; i++) {
		u32string* text = vector_get(&file->lines, i);

		int first = 1;
		size_t screen_x = 0;

		for (size_t j = 0; j < u32string_size(text); j++) {
			uint32_t c = u32string_char(text, j);

			if (c == U'\t') {
				if (first) {
					file_set_line_dirty(file, i);
					first = 0;
				}

				size_t quant = tab_size - (screen_x % tab_size);

				u32string_remove_range(
					text,
					j,
					j + 1
				);

				u32string_insert_range_raw(
					text,
					j,
					u32string_into_ptr_const(&spaces),
					quant
				);

				screen_x += quant;
			}

			else {
				size_t width = char_screen_width(
					c,
					j,
					tab_size
				);

				screen_x += width;
			}
		}
	}

	u32string_free(&spaces);

	file->dirty = 1;
	file->use_spaces = 1;
}

void file_sync(File* file) {
	vector_free(&file->lines);

	ssize_t size;
	uint32_t* data = read_file(
		file->filename, 
		&size, 
		NULL);

	file->dirty = 0;

	// file with empty lines
	if (size == 0) {
		Line line = line_new();

		file_push(file, &line);
	} 

	else {
		split_lines(file, data, size);
	}

	free(data);
}

int file_save(File* file, Result* result) {
	if (!file) { 
		result_set_reason(result,
			"file_save: file is a null pointer");
		result->type = ERROR_NULL_POINTER;

		return EIE_FATAL_ERROR; 
	}

	if (!file->dirty) {
		return EIE_NOT_AN_ERROR;
	}

	int fd = open(file->filename, O_WRONLY | O_TRUNC | O_CREAT, 0644);

	if (fd < 0) {
		if (result) {
			if (errno == ENOENT) {
				result_set_reason(result,
					"file_save: file does not exist");
				result->type = ERROR_FILE_DOES_NOT_EXIST;

			} else if (errno == EACCES) {
				result_set_reason(result,
					"file_save: access denied");
				result->type = ERROR_FILE_HANDLE;

			} else if (errno == EPERM) {
				result_set_reason(result,
					"file_save: operation denied");
				result->type = ERROR_FILE_HANDLE;
			}
		}

		return EIE_FATAL_ERROR;
	}

	if (file->lines.size == 0) {
		write(fd, "", 1);
		result_ok(result);

		file->dirty = 0;

		return EIE_OK;
	}

	for (size_t i = 0; i < file->lines.size; i++) {
		u32string* line_text = file_get_line_text(file, i);

		if (!line_text) {
			result_set_reason(result,
				"file_save: line_text is invalid");
			result->type = ERROR_NULL_POINTER;

			return EIE_NOT_FATAL_ERROR;         
		}

		char* text = u32string_into_u8(line_text);

		write(fd, text, strlen(text));

		free(text);

		// for (size_t j = 0; j < line_size(line); j++) {
		//  char utf8[4];
		//  int len = utf8_encode(u32string_char(line, j), utf8);

		//  write(fd, utf8, len);
		// }

		if (i < file->lines.size -1) {
			write(fd, "\n", 1);
		}
	}

	file->dirty = 0;

	close(fd);
	result_ok(result);

	return EIE_OK;
}

void file_create_tokens(File* file, struct lexer* lexer) {
	if (!file || !lexer) {
		return;
	}

	enum lex_state state;

	for (size_t i = 0; i < file->lines.size; i++) {
		Line* line = vector_get(&file->lines, i);

		if (i == 0) {
			line_tokenize(line, lexer);
			state = line->state_out;
		}

		else {
			line->state_in = state;

			line_tokenize(line, lexer);

			state = line->state_out;
		}
	}
}

static enum lex_state get_state_before
(
	File* file, 
	size_t first_dirty
) 
{
	if (first_dirty == 0) {
		return LEX_STATE_NORMAL;
	}

	const Line* line = file_get_line(file, first_dirty - 1);

	return line->state_out;
}

void file_recalculate_tokens
(
	File* file,
	struct lexer* lexer
) 
{
	if (!file || !lexer) {
		return;
	}

	/*
	The 'dirtiness' of a line should imply the
	'dirtiness' of the file as a whole.
	Therefore, regarding any potential bug
	in the token calculation, the first thing
	to consider is whether there is indeed
	strong synchronization between these
	two concepts.

	Even though the 'dirtiness' of a file
	dictates whether is possible to do
	a token recalculation operation, this
	function must not unset the file->dirty
	flag as line_tokenize does to a line.
	*/

	if (!file->dirty) {
		return;
	}

	ssize_t first_dirty = -1;

	for (size_t i = 0; i < file->lines.size; i++) {
		Line* line = file_get_line(file, i);

		if (line->dirty) {
			first_dirty = i;

			break;
		}
	}

	/*
	There was a 
	version of this function that recalculated tokens
	only for dirty lines; however, with the addition
	of states, an additional step was needed.

	The reason for it is simply the result of calling
	lexer->reset() at the end of the function. The reset
	function resets the lexer's state, meaning the lexer
	plugin no longer knows whether it is inside a block
	of comment, a string, etc. In my lexer implementation,
	for the lexer to determine the current block context,
	it needs to tokenize the line where that block begins.
	The problem is: if you modify a line that is not the
	start line of the block context, only that line will
	be set as dirty, which means that the lexer will not
	know that he is inside a context block.

	For example, consider the following code:

	  "
	1:
	2: (comment block begin)
	3:
	4: hello
	5: goodbye
	6:
	7: (comment block end)
	8:
	9: hello world
	10: goodbye world
	  "
	
	The first time the lexer tokenizes the file, it will
	necessarily tokenize the line number 2, which is the
	line where the comment block begins. During this process,
	it will detect that it's inside a comment block and will
	maintain this context state until it encounters the end
	of the block, while also marking every line it encounters
	along the way as a "comment".
	At the end of the process, the states of each line are
	as follows:

	1: in = normal, out = normal
	2: in = normal, out = comment
	3: in = comment, out = comment,
	4: in = comment, out = comment,
	5: in = comment, out = comment,
	6: in = comment, out = comment,
	7: in = comment, out = normal,
	8: in = normal, out = normal
	9: in = normal, out = normal,
	10: in = normal, out = normal

	Now, if you only modify the line number 4, for example,
	it will be set as dirty. However, since line 2 will not
	be dirty, the comment state will be lost and line 4
	will not be marked as a comment (HL_COMMENT).
	
	The use of states resolves this problem as follows:
	when setting line->state_in to state, which is the
	state_out of the line immediately above the current
	one and sending this information to the lexer via
	tokenize_line, the lexer can use the value of
	state_in to determine whether to treat that line
	as part of the previous line context.

	But there is still another problem. Suppose you change
	the boundaries of the comment block somehow, for example,
	by deleting the string that marks the end of the block.
	By doing that, the expected result is that the rest of
	the entire file becomes a comment. The problem is:
	you only modified the line containing the end of the
	comment block, which means that is the only line in
	the file that will be marked as dirty; therefore,
	if line.dirty is the only condition for tokenizing a line,
	the rest of the file won't be retokenized as it should be.

	How to solve that problem? After removing the end of
	the comment block, the states will be as follow:

	1: in = normal, out = normal
	2: in = normal, out = comment
	3: in = comment, out = comment,
	4: in = comment, out = comment,
	5: in = comment, out = comment,
	6: in = comment, out = comment,
	7: in = comment, out = comment,
	8: in = normal, out = normal 	// not dirty
	9: in = normal, out = normal, 	// not dirty
	10: in = normal, out = normal 	// not dirty

	Note that line8.state_in != line7.state_out, and that is
	exactly the new condition that must be taken into consideration, ie,

	actual_line.state_in != prev_line.state_out

	Why that works? Note that the fundamental relationship
	within retokenization is the fact that line->state_in becomes
	state (prev_line.state_out); that occurs to propagate the
	context from the previous line to the current one as
	demonstrated in the discussion above. Therefore, the fact
	that those values differ necessarily tells us that the
	context from the prev line needs to be passed to the current
	one, even though the current line is not marked as dirty.

	Notice that this completely solves our problems; since
	line8.in != line7.out, line 8 will be retokenized and marked
	as comment:

	7: in = comment, out = comment,
	8: in = comment, out = comment,
	9: in = normal, out = normal,

	Since, line9.in != line8.out, line 9 will be retokenized and
	marked as comment:

	7: in = comment, out = comment,
	8: in = comment, out = comment,
	9: in = comment, out = comment,	

	and so on...
	
	In words, that new condition asks: "the state this line should
	receive is different from the state in which it was originally
	analyzed?" If so, the line must be retokenized.

	However, how does the function now know when to stop tokenizing?
	The condition is: !dirty && old_in == line->state_in && 
								old_out == line->state_out

	That condition, in words, means: "this line has not
	changed, the context in which it was analyzed has
	not changed, and the context it produces has not
	changed."

	If the condition is true, we know that the context of the
	previous line is exactly the state of the current line before.


				 same state_out			   same state_in
	previous_line	---->		actual_line  ---->		 no tokenization

	For example, remember the first example where we modified the
	line number 4. That line will be retokenized, but what about
	the following lines? The states will be:

	1: in = normal, out = normal
	2: in = normal, out = comment
	3: in = comment, out = comment,
	4: in = comment, out = comment, // dirty
	5: in = comment, out = comment, // not dirty
	6: in = comment, out = comment, // not dirty
	7: in = comment, out = normal, 	// not dirty
	8: in = normal, out = normal 	// ..
	9: in = normal, out = normal,
	10: in = normal, out = normal

	At the end of the tokenization of the line 4, the program will
	check the stop condition:
	
	line 4:
		!dirty ? false -> continues to line 5

	line 5:
		!dirty ? true
		old_in == line->state_in ? true
		old_out == line->state_out ? true
		-> stop at line 5

	That means that the context of the line 4 don't need be
	passed to the line 5, because the line 5 already has it.

	Now, imagine a more complicated example which ilustrates
	the reason (!dirty) is necessary:

	  "
	1:
	2:
	3:
	4: hello
	5: goodbye
	6:
	7:
	8:
	9: hello world
	10: goodbye world
	  "

	Suppose that you enter a newline as follows:

	  "
	1:
	2:
	3:
	4: he
	5: llo
	6: goodbye
	7:
	8:
	9:
	10: hello world
	11: goodbye world
	  "	

	Both lines 4 and 5 are dirty now. After line 4's tokenization
	we'll have: old_in (NORMAL) == line->state_in (NORMAL) and
	old_out (NORMAL) == line->state_out (NORMAL). Therefore, if
	(!dirty) were not taken into account, the loop would terminate
	at line 4, and line 5 would not be tokenized at all.

	*/

	if (first_dirty >= 0) {
		enum lex_state state = get_state_before(file, first_dirty);

		for (size_t i = first_dirty; i < file->lines.size; i++) {
			Line* line = file_get_line(file, i);

			enum lex_state old_in = line->state_in;
			enum lex_state old_out = line->state_out;

			int dirty = line->dirty;

			int needs_tokenize = dirty || state != line->state_in;

			if (needs_tokenize) {
				line->state_in = state;

				// I thought of this behavior without
				// giving it much thought, but it
				// seems correct to me
				if (u32string_is_empty(&line->text)) {
					line->state_out = state;
					line->dirty = 0;
				} 

				else {
					line_tokenize(line, lexer);
				}
			}

			state = line->state_out;

			// stop condition
			if (!dirty &&
				old_in == line->state_in &&
				old_out == line->state_out) 
			{
				break;
			}
		}

		lexer->reset(lexer);
	}
}

int file_insert_char
(
	File* file, 
	const Position pos, 
	uint32_t c,
	Result* result
) 
{
	if (!file) { 
		result_set_reason(result,
			"file_insert_char: file is a null pointer");
		result->type = ERROR_NULL_POINTER;

		return EIE_FATAL_ERROR; 
	}

	if (pos.y >= file->lines.size) {
		result_set_reason(result,
			"file_insert_char: pos.y >= file->lines.size");
		result->type = ERROR_INDEX_OUT_OF_BOUNDS;

		return EIE_NOT_FATAL_ERROR;
	}

	Line* line = vector_get(&file->lines, pos.y);

	if (!line) {
		result_set_reason(result,
			"file_insert_char: line is invalid");
		result->type = ERROR_NULL_POINTER;

		return EIE_NOT_FATAL_ERROR;
	}

	u32string* line_text = &line->text;

	if (!line_text) {
		result_set_reason(result,
			"file_insert_char: line_text is invalid");
		result->type = ERROR_NULL_POINTER;

		return EIE_NOT_FATAL_ERROR;
	}

	u32string_insert(line_text, c, pos.x);

	file->dirty = 1;
	line->dirty = 1;

	result_ok(result);

	return EIE_OK;
}

int file_delete_char
(
	File* file, 
	const Position pos, 
	Result* result
) 
{
	if (!file) { 
		result_set_reason(result,
			"file_delete_char: file is a null pointer");
		result->type = ERROR_NULL_POINTER;

		return EIE_FATAL_ERROR; 
	}

	if (pos.y >= file->lines.size) {
		result_set_reason(result,
			"file_delete_char: pos.y >= file->lines.size");
		result->type = ERROR_INDEX_OUT_OF_BOUNDS;

		return EIE_NOT_FATAL_ERROR;
	}

	if (pos.x == 0) {
		result_set_reason(result,
			"file_delete_char: pos.x == 0");
		result->type = ERROR_INDEX_OUT_OF_BOUNDS;

		return EIE_NOT_FATAL_ERROR;
	}

	Line* line = vector_get(&file->lines, pos.y);

	if (!line) {
		result_set_reason(result,
			"file_delete_char: line is invalid");
		result->type = ERROR_NULL_POINTER;

		return EIE_NOT_FATAL_ERROR;
	}

	u32string* line_text = &line->text;

	if (!line_text) {
		result_set_reason(result,
			"file_delete_char: line_text is invalid");
		result->type = ERROR_NULL_POINTER;

		return EIE_NOT_FATAL_ERROR;
	}

	u32string_remove(line_text, pos.x - 1, NULL);

	file->dirty = 1;
	line->dirty = 1;

	result_ok(result);

	return EIE_OK;
}

int file_insert_newline
(
	File* file, 
	const Position pos, 
	Result* result
) 
{
	if (!file) {
		result_set_reason(result,
			"file_insert_newline: file is a null pointer");
		result->type = ERROR_NULL_POINTER;

		return EIE_FATAL_ERROR; 
	}

	if (pos.y >= file->lines.size) {
		result_set_reason(result,
			"file_insert_newline: pos.y >= file->lines.size");
		result->type = ERROR_INDEX_OUT_OF_BOUNDS;

		return EIE_NOT_FATAL_ERROR;
	}

	Line* line = vector_get(&file->lines, pos.y);

	if (!line) {
		result_set_reason(result,
			"file_insert_newline: line is invalid");
		result->type = ERROR_NULL_POINTER;

		return EIE_NOT_FATAL_ERROR;
	}

	u32string* line_text = &line->text;

	if (!line_text) {
		result_set_reason(result,
			"file_insert_newline: line_text is invalid");
		result->type = ERROR_NULL_POINTER;

		return EIE_NOT_FATAL_ERROR;
	}

	if (pos.x > u32string_size(line_text)) {
		result_set_reason(result,
			"file_insert_newline: pos.x out of bounds");
		result->type = ERROR_INDEX_OUT_OF_BOUNDS;

		return EIE_NOT_FATAL_ERROR;
	}

	size_t tail_size = u32string_size(line_text) - pos.x;

	// tail = new line
	uint32_t* tail_text = malloc(tail_size * sizeof(*tail_text));

	if (!tail_text) {
		result_set_reason(result,
			"file_insert_newline: tail_text is invalid");
		result->type = ERROR_MALLOC;

		return EIE_NOT_FATAL_ERROR;
	}

	memcpy(
		tail_text, 
		u32string_into_ptr_const(line_text) + pos.x, 
		tail_size * sizeof(*tail_text));

	u32string new_text = u32string_from_raw(tail_text, tail_size);
	Line new_line = line_with_text(new_text);

	// head = original line
	size_t head_size = pos.x;
	uint32_t* head_text = malloc(head_size * sizeof(*head_text));

	if (!head_text) {
		result_set_reason(result,
			"file_insert_newline: head_text is invalid");
		result->type = ERROR_MALLOC;

		line_free(&new_line);

		return EIE_NOT_FATAL_ERROR;
	}   

	memcpy(
		head_text, 
		u32string_into_ptr_const(line_text), 
		pos.x * sizeof(*head_text));

	u32string_replace_text_raw(line_text, head_text, head_size);

	file->dirty = 1;
	line->dirty = 1;
	new_line.dirty = 1;

	vector_insert(&file->lines, pos.y + 1, &new_line);

	result_ok(result);

	return EIE_OK;
}

int file_merge_lines
(
	File* file, 
	const Position pos, 
	Result* result
) 
{
	if (!file) {
		result_set_reason(result,
			"file_merge_lines: file is a null pointer");
		result->type = ERROR_NULL_POINTER;

		return EIE_FATAL_ERROR; 
	}

	if (pos.y == 0 || pos.y >= file->lines.size) {
		result_set_reason(result,
			"file_merge_lines: pos.y == 0 || pos.y >= file->lines.size");
		result->type = ERROR_INDEX_OUT_OF_BOUNDS;

		return EIE_NOT_FATAL_ERROR;
	}

	Line* prev_line = vector_get(&file->lines, pos.y - 1);

	if (!prev_line) {
		result_set_reason(result,
			"file_merge_lines: prev_line is invalid");
		result->type = ERROR_NULL_POINTER;

		return EIE_NOT_FATAL_ERROR;
	}

	u32string* prev_text = &prev_line->text;

	if (!prev_text) {
		result_set_reason(result,
			"file_merge_lines: prev_text is invalid");
		result->type = ERROR_NULL_POINTER;

		return EIE_NOT_FATAL_ERROR;
	}

	size_t old_prev_size = u32string_size(prev_text);
	u32string* curr = file_get_line_text(file, pos.y);

	if (!curr) {
		result_set_reason(result,
			"file_merge_lines: curr is invalid");
		result->type = ERROR_NULL_POINTER;

		return EIE_NOT_FATAL_ERROR;
	}

	size_t curr_size = u32string_size(curr);
	size_t new_size = old_prev_size + curr_size;
	uint32_t* new_text = malloc(new_size * sizeof(*new_text));

	if (!new_text) {
		result_set_reason(result,
			"file_merge_lines: new_text is invalid");
		result->type = ERROR_MALLOC;

		return EIE_NOT_FATAL_ERROR;
	}

	memcpy(
		new_text, 
		u32string_into_ptr_const(prev_text), 
		old_prev_size * sizeof(*new_text));

	memcpy(
		new_text + old_prev_size, 
		u32string_into_ptr_const(curr), 
		curr_size * sizeof(*new_text));

	u32string_replace_text_raw(prev_text, new_text, new_size);

	vector_remove_and_destroy(&file->lines, pos.y);

	file->dirty = 1;
	prev_line->dirty = 1;

	result_ok(result);

	return EIE_OK;
}

int file_indent_a_line
(
	File* file, 
	size_t y,
	Result* result
) 
{
	if (!file) {
		result_set_reason(result,
			"file_indent_a_line: file is a null pointer");
		result->type = ERROR_NULL_POINTER;

		return EIE_FATAL_ERROR; 
	}

	Line* line = vector_get(&file->lines, y);

	if (!line) {
		result_set_reason(result,
			"file_indent_a_line: line is invalid");
		result->type = ERROR_NULL_POINTER;

		return EIE_NOT_FATAL_ERROR;
	}

	u32string* line_text = &line->text;

	if (!line_text) {
		result_set_reason(result,
			"file_indent_a_line: line_text is a null pointer");
		result->type = ERROR_NULL_POINTER;

		return EIE_NOT_FATAL_ERROR;
	}

	size_t tab_size = (file->use_spaces) ? file->tab_size : 1;

	size_t size = u32string_size(line_text);
	size_t new_size = tab_size + size;
	uint32_t* new_text = malloc(new_size * sizeof(*new_text));

	if (!new_text) {
		result_set_reason(result,
			"file_indent_a_line: new_text is a null pointer");
		result->type = ERROR_MALLOC;

		return EIE_NOT_FATAL_ERROR;
	}

	uint32_t c = (file->use_spaces) ? U' ' : U'\t';

	for (size_t i = 0; i < tab_size; i++) {
		new_text[i] = c;
	}

	memcpy(
		new_text + tab_size, 
		u32string_into_ptr_const(line_text), 
		size * sizeof(*new_text));

	u32string_replace_text_raw(line_text, new_text, new_size);

	file->dirty = 1;
	line->dirty = 1;

	result_ok(result);

	return EIE_OK;
}


int file_indent_selection
(
	File* file,
	Selection* sel,
	Result* result
)
{
	int ret = EIE_OK;

	size_t tab_size = (file->use_spaces) ? file->tab_size : 1;

	Position a, b;
	selection_normalize(sel, &a, &b);

	for (size_t i = a.y; i < b.y + 1; i++) {
		ret = file_indent_a_line(
			file, 
			i,
			result);

		if (ret < 0) {
			return EIE_FATAL_ERROR;
		}
	}

	a.x += tab_size;
	b.x += tab_size;

	if (selection_start_before_end(sel)) 
	{
		sel->start.x = (sel->start.x == 0) 
			? 0 
			: a.x;

		sel->start.y = a.y;
		sel->end = b;
	} 

	else {
		sel->start = b;
		sel->end.x = (sel->end.x == 0) ?
			0: a.x;
		sel->end.y = a.y;
	}

	return ret;
}

int file_unindent_a_line
(
	File* file, 
	size_t y, 
	Result* result
) 
{
	if (!file) {
		result_set_reason(result,
			"file_unindent_a_line: file is a null pointer");
		result->type = ERROR_NULL_POINTER;

		return EIE_FATAL_ERROR; 
	}

	Line* line = vector_get(&file->lines, y);

	if (!line) {
		result_set_reason(result,
			"file_unindent_a_line: line is a null pointer");
		result->type = ERROR_NULL_POINTER;

		return EIE_NOT_FATAL_ERROR;
	}

	u32string* line_text = &line->text;

	if (!line_text) {
		result_set_reason(result,
			"file_unindent_a_line: line_text is a null pointer");
		result->type = ERROR_NULL_POINTER;

		return EIE_NOT_FATAL_ERROR;
	}

	size_t indent = u32string_get_indent(line_text, file->use_spaces);
	size_t tab_size = (file->use_spaces) ? file->tab_size : 1;
	uint32_t c = (file->use_spaces) ? U' ' : U'\t';

	if (indent == 0) {
		return EIE_NOT_AN_ERROR;
	}

	size_t remaining_indent = (indent >= tab_size) ?
		(indent - tab_size) : 0;

	size_t size = u32string_size(line_text);
	size_t new_size = size - indent + remaining_indent;
	uint32_t* new_text = malloc(new_size * sizeof(*new_text));

	if (!new_text) {
		result_set_reason(result,
			"file_unindent_a_line: new_text is a null pointer");
		result->type = ERROR_MALLOC;

		return EIE_NOT_FATAL_ERROR;
	}

	for (size_t i = 0; i < remaining_indent; i++) {
		new_text[i] = c;
	}

	memcpy(
		new_text + remaining_indent, 
		u32string_into_ptr_const(line_text) + indent,
		(new_size - remaining_indent) * sizeof(*new_text));

	u32string_replace_text_raw(line_text, new_text, new_size);

	file->dirty = 1;
	line->dirty = 1;

	result_ok(result);

	return EIE_OK;  
}

int file_unindent_selection
(
	File* file,
	Selection* sel,
	Result* result
)
{
	int ret = EIE_OK;
	size_t tab_size = (file->use_spaces) ? file->tab_size : 1;

	Position a, b;
	selection_normalize(sel, &a, &b);

	// the start and end of the selection can have
	// carbitrary indentation levels. therefore, it is
	// necessary to know how far each one needs to move
	size_t move_start_sel = 0;
	size_t move_end_sel = 0;

	{
		u32string* line_text = file_get_line_text(
			file,
			a.y);

		size_t indent = u32string_get_indent(
			line_text,
			file->use_spaces);

		move_start_sel = (indent >= tab_size)
			? tab_size
			: indent;

		line_text = file_get_line_text(
			file,
			b.y);

		indent = u32string_get_indent(
			line_text,
			file->use_spaces);

		move_end_sel = (indent >= tab_size)
			? tab_size
			: indent;
	}

	for (size_t i = a.y; i < b.y + 1; i++) {
		ret = file_unindent_a_line(
			file, 
			i,
			result);

		if (ret < 0) {
			return EIE_FATAL_ERROR;
		}
	}

	a.x -= move_start_sel;
	b.x -= move_end_sel;

	if (selection_start_before_end(sel)) 
	{
		sel->start.x = (sel->start.x == 0) ?
			0 : a.x;
		sel->start.y = a.y;

		sel->end = b;
	} 

	else {
		sel->start = b;

		sel->end.x = (sel->end.x == 0) ?
			0: a.x;
		sel->end.y = a.y;
	}

	return ret;
}


int file_move_line_up(File* file, size_t y, Result* result) {
	if (!file) {
		result_set_reason(result,
			"file_move_line_up: file is a null pointer");
		result->type = ERROR_NULL_POINTER;

		return EIE_FATAL_ERROR; 
	}

	if (y == 0) {
		return EIE_NOT_AN_ERROR;
	}

	int ret = vector_swap(&file->lines, y, y - 1);

	if (ret < 0) {
		result_set_reason(result,
			"file_move_line_up: error returned by vector_swap");
		result->type = ERROR_VECTOR;

		return EIE_NOT_FATAL_ERROR;
	}

	Line* line = vector_get(&file->lines, y);
	line->dirty = 1;

	line = vector_get(&file->lines, y - 1);
	line->dirty = 1;

	result_ok(result);

	return EIE_OK;
}

int file_move_line_down(File* file, size_t y, Result* result) {
	if (!file) {
		result_set_reason(result,
			"file_move_line_down: file is a null pointer");
		result->type = ERROR_NULL_POINTER;

		return EIE_FATAL_ERROR; 
	}

	if (y >= file->lines.size - 1) {
		return EIE_NOT_AN_ERROR;
	}

	int ret = vector_swap(&file->lines, y, y + 1);

	if (ret < 0) {
		result_set_reason(result,
			"file_move_line_down: error returned by vector_swap");
		result->type = ERROR_VECTOR;

		return EIE_NOT_FATAL_ERROR;
	}

	Line* line = vector_get(&file->lines, y);
	line->dirty = 1;

	line = vector_get(&file->lines, y + 1);
	line->dirty = 1;

	file->dirty = 1;

	result_ok(result);

	return EIE_OK;
}


/// --- COMMENT ---

int file_comment_line
(
	File* file,
	size_t y,
	const char* comment_fmt,
	ssize_t* move_cursor,
	Result* result
)
{
	if (!file) {
		result_set_reason(result,
			"file_comment_line: file is a null pointer");
		result->type = ERROR_NULL_POINTER;

		return EIE_FATAL_ERROR; 
	}

	int ret = EIE_OK;

	u32string* line_text = file_get_line_text(file, y);

	if (!line_text) {
		result_set_reason(result,
			"file_comment_line: line_text is a null pointer");
		result->type = ERROR_NULL_POINTER;

		return EIE_NOT_FATAL_ERROR;
	}

	if (u32string_is_empty(line_text)) {
		return EIE_NOT_AN_ERROR;
	}

	file_set_line_dirty(file, y);

	size_t comment_fmt_size = strlen(comment_fmt);
	size_t indent = u32string_get_indent(
		line_text,
		file->use_spaces);

	int is_comment = u32string_is_comment(
		line_text, 
		indent,
		comment_fmt,
		comment_fmt_size);

	u32string u32_comment_fmt = u32string_from(comment_fmt);

	if (!is_comment) {
		u32string_push(&u32_comment_fmt, ' ');

		u32string_insert_range_raw(
			line_text,
			indent,
			u32string_into_ptr_const(&u32_comment_fmt),
			comment_fmt_size + 1);

		if (move_cursor) {
			*move_cursor = comment_fmt_size + 1;
		}
	}

	else if (is_comment) {
		ssize_t index = u32string_find(
			line_text, 
			indent,
			u32string_size(line_text),
			&u32_comment_fmt);

		int has_space = u32string_char(
			line_text, 
			index + comment_fmt_size) ==
			U' ';

		u32string_remove_range(
			line_text,
			index,
			index + comment_fmt_size + has_space);

		if (move_cursor) {
			*move_cursor = - (comment_fmt_size + has_space);
		}
	}

	u32string_free(&u32_comment_fmt);

	file->dirty = 1;

	return ret;
}


// helper for file_comment_selection
static int all_lines_comment
(
	File* file,
	size_t start, size_t end,
	const char* comment_fmt,
	size_t* smallest_indent
)
{
	size_t comment_fmt_size = strlen(comment_fmt);
	size_t small;
	int all_lines = 1;

	for (size_t i = start; i < end; i++) {
		u32string* line_text = file_get_line_text(file, i);

		if (u32string_is_empty(line_text)) {
			continue;
		}

		size_t indent = u32string_get_indent(
			line_text,
			file->use_spaces);

		if (i == start) {
			small = indent;
		}

		else {
			if (indent < small) {
				small = indent;
			}
		}

		int is_comment = u32string_is_comment(
			line_text,
			indent,
			comment_fmt,
			comment_fmt_size);

		if (!is_comment) {
			all_lines = 0;
		}
	}

	if (smallest_indent) {
		*smallest_indent = small;
	}

	return all_lines;
}

// helper for file_comment_selection
// here, line_text is supposed to be a commented line
static int space_after_comment
(
	File* file,
	size_t y,
	size_t comment_fmt_size
)
{
	u32string* line_text = file_get_line_text(file, y);

	if (u32string_is_empty(line_text)) {
		return 0;
	}

	size_t indent = u32string_get_indent(
		line_text,
		file->use_spaces);

	return u32string_char(line_text, indent + comment_fmt_size) == U' ';
}

int file_comment_selection
(
	File* file,
	Selection* sel,
	const char* comment_fmt,
	size_t cursor_y,
	ssize_t* move_cursor
)
{
	if (!comment_fmt || strlen(comment_fmt) == 0) {
		return EIE_NOT_FATAL_ERROR;
	}

	int ret = EIE_OK;

	size_t smallest_indent;

	Position a, b;
	selection_normalize(sel, &a, &b);

	int comment = !all_lines_comment(
		file, 
		a.y, 
		b.y + 1,
		comment_fmt,
		&smallest_indent);

	size_t comment_fmt_size = strlen(comment_fmt);

	if (move_cursor && cursor_y >= a.y && cursor_y <= b.y) {
		*move_cursor = (comment)
		? comment_fmt_size + 1
		: space_after_comment(file, cursor_y, comment_fmt_size)
			?  -(comment_fmt_size + 1) : -comment_fmt_size;
	}

	ssize_t move_start_sel = (comment)
		? comment_fmt_size + 1
		: space_after_comment(file, a.y, comment_fmt_size)
			?  -(comment_fmt_size + 1) : -comment_fmt_size;

	ssize_t move_end_sel = (comment)
		? comment_fmt_size + 1
		: space_after_comment(file, b.y, comment_fmt_size)
			?  -(comment_fmt_size + 1) : -comment_fmt_size;


	u32string u32_comment_fmt = u32string_from(comment_fmt);

	for (size_t i = a.y; i < b.y + 1; i++) {
		u32string* line_text = file_get_line_text(file, i);

		if (u32string_is_empty(line_text)) {
			continue;
		}

		if (comment) {
			u32string_push(&u32_comment_fmt, ' ');

			u32string_insert_range_raw(
				line_text,
				smallest_indent,
				u32string_into_ptr_const(&u32_comment_fmt),
				comment_fmt_size + 1);
		}

		else {
			size_t indent = u32string_get_indent(
				line_text,
				file->use_spaces);

			ssize_t index = u32string_find(
				line_text, 
				indent,
				u32string_size(line_text),
				&u32_comment_fmt);

			int has_space = u32string_char(
				line_text, 
				index + comment_fmt_size) ==
				U' ';

			u32string_remove_range(
				line_text,
				index,
				comment_fmt_size + has_space);      
		}

		file_set_line_dirty(file, i);
	}

	u32string_free(&u32_comment_fmt);

	if (selection_start_before_end(sel)) {
		sel->start.x += move_start_sel;
		sel->end.x += move_end_sel;
	}

	else {
		sel->end.x += move_start_sel;
		sel->start.x += move_end_sel;
	}

	return ret;
}

static int delete_within_line
(
	File* file,
	Position start,
	Position end
)
{
	Line* line = vector_get(&file->lines, start.y);

	if (!line) {
		return -1;
	}

	u32string* line_text = &line->text;

	if (!line_text) {
		return -1;
	}

	size_t size = u32string_size(line_text);
	size_t new_size = size - (end.x - start.x);
	uint32_t* new_text = malloc(new_size * sizeof(*new_text));

	if (!new_text) {
		return -1;
	}

	memcpy(
		new_text, 
		u32string_into_ptr_const(line_text), 
		start.x * sizeof(*new_text));

	memcpy(
		new_text + start.x,
		u32string_into_ptr_const(line_text) + end.x,
		(size - end.x) * sizeof(*new_text));

	u32string_replace_text_raw(line_text, new_text, new_size);

	line->dirty = 1;

	return 0;
}

static int delete_multiline
(
	File* file,
	Position start,
	Position end
)
{
	Line* first_line = vector_get(&file->lines, start.y);

	if (!first_line) {
		return -1;
	}

	u32string* first = &first_line->text;
	u32string* last  = file_get_line_text(file, end.y);

	if (!first || !last) {
		return -1;
	}

	size_t last_size = u32string_size(last);

	size_t prefix_size = start.x;
	size_t suffix_size = last_size - end.x;

	size_t new_size = prefix_size + suffix_size;
	uint32_t* new_text = malloc(new_size * sizeof(*new_text));

	if (!new_text) {
		return -1;
	}

	memcpy(
		new_text, 
		u32string_into_ptr_const(first), 
		prefix_size * sizeof(*new_text));

	memcpy(
		new_text + prefix_size,
		u32string_into_ptr_const(last) + end.x,
		suffix_size * sizeof(*new_text));

	u32string_replace_text_raw(first, new_text, new_size);

	for (size_t y = start.y + 1; y <= end.y; y++) {
		vector_remove_and_destroy(&file->lines, start.y + 1);
	}

	first_line->dirty = 1;

	return 0;
}

int file_delete_selection
(
	File* file, 
	Selection* sel,
	Result* result
) 
{
	if (!file) {
		result_set_reason(result,
			"file_delete_selection: file is a null pointer");
		result->type = ERROR_NULL_POINTER;

		return EIE_FATAL_ERROR; 
	}

	if (!sel) {
		result_set_reason(result,
			"file_delete_selection: sel is a null pointer");
		result->type = ERROR_NULL_POINTER;

		return EIE_FATAL_ERROR;
	}

	if (!sel->active) {     
		return EIE_NOT_AN_ERROR; // not an error
	}

	Position a, b;
	selection_normalize(sel, &a, &b);

	if (a.y == b.y) {
		if (delete_within_line(file, a, b) < 0) {
			result_set_reason(result,
				"file_delete_selection: error returned by a static function");
			result->type = ERROR_STATIC;

			return EIE_NOT_FATAL_ERROR;
		}
	} 

	else {
		if (delete_multiline(file, a, b) < 0) {
			result_set_reason(result,
				"file_delete_selection: error returned by a static function");
			result->type = ERROR_STATIC;

			return EIE_NOT_FATAL_ERROR;
		}
	}

	file->dirty = 1;

	selection_clear(sel);

	result_ok(result);

	return EIE_OK;
}

int file_copy_selection
(
	File* file, 
	Clipboard* cb, 
	Selection* sel,
	Result* result
) 
{
	if (!file) {
		result_set_reason(result,
			"file_copy_selection: file is a null pointer");
		result->type = ERROR_NULL_POINTER;

		return EIE_FATAL_ERROR; 
	}

	if (!cb) { 
		result_set_reason(result,
			"file_copy_selection: cb is a null pointer");
		result->type = ERROR_NULL_POINTER;

		return EIE_FATAL_ERROR; 
	}

	if (!sel) {
		result_set_reason(result,
			"file_copy_selection: sel is a null pointer");
		result->type = ERROR_NULL_POINTER;

		return EIE_FATAL_ERROR;
	}

	if (!sel->active) {
		return EIE_NOT_AN_ERROR;
	}

	Position a, b;
	selection_normalize(sel, &a, &b);

	if (position_equal(&a, &b)) {
		return EIE_NOT_AN_ERROR;
	}

	u32string* last_text = file_get_line_text(file, b.y);

	if (!last_text) {
		result_set_reason(result,
			"file_copy_selection: last_text is a null pointer");
		result->type = ERROR_NULL_POINTER;

		return EIE_NOT_FATAL_ERROR;
	}

	cb->linewise = sel->linewise;

	size_t total_size = 0;

	for (size_t y = a.y; y <= b.y; y++) {
		u32string* line_text = file_get_line_text(file, y);

		if (!line_text) {
			result_set_reason(result,
				"file_copy_selection: line_text is a null pointer");
			result->type = ERROR_NULL_POINTER;

			return EIE_NOT_FATAL_ERROR;
		}

		size_t start = (y == a.y) ? a.x : 0;
		size_t end   = (y == b.y) ? b.x : u32string_size(line_text);

		total_size += (end - start);

		if (y < b.y) total_size += 1;
	}

	if (cb->linewise) total_size += 1;

	uint32_t* cb_text = malloc(total_size * sizeof(*cb_text));

	if (!cb_text) {
		result_set_reason(result,
			"file_copy_selection: cb->text is a null pointer");
		result->type = ERROR_NULL_POINTER;

		return EIE_NOT_FATAL_ERROR;
	}

	size_t pos = 0;

	for (size_t y = a.y; y <= b.y; y++) {
		u32string* line_text = file_get_line_text(file, y);

		if (!line_text) {
			result_set_reason(result,
				"file_copy_selection: line_text is a null pointer");
			result->type = ERROR_NULL_POINTER;

			free(cb_text);

			return EIE_NOT_FATAL_ERROR;
		}

		size_t start = (y == a.y) ? a.x : 0;
		size_t end   = (y == b.y) ? b.x : u32string_size(line_text);

		size_t size = end - start;

		memcpy(
			cb_text + pos, 
			u32string_into_ptr_const(line_text) + start, 
			size * sizeof(*cb_text));

		pos += size;

		if (y < b.y) {
			cb_text[pos++] = U'\n';
		}
	}

	if (cb->linewise) {
		cb_text[pos++] = U'\n';
	}

	u32string_replace_text_raw(&cb->text, cb_text, total_size);

	result_ok(result);

	return EIE_OK;
}

static int paste_inline(File* file, Clipboard* cb, Cursor* c) {
	Line* line = vector_get(&file->lines, c->pos.y);

	if (!line) {
		return EIE_NOT_FATAL_ERROR;
	}

	u32string* line_text = &line->text;

	u32string_insert_range_raw(
		line_text,
		c->pos.x,
		u32string_into_ptr_const(&cb->text),
		u32string_size(&cb->text));

	c->pos.x += u32string_size(&cb->text);
	line->dirty = 1;

	return EIE_OK;
}

static void line_append_or_replace
(
	u32string* line,
	const uint32_t* text,
	size_t size
)
{
	if (u32string_is_empty(line)) {
		u32string_replace_text_raw_copy(line, text, size);
	}

	else {
		u32string_append_raw(line, text, size);
	}
}

static void insert_first_segment
(
	File* file, 
	size_t first_index, 
	size_t size,
	Clipboard* cb
)
{
	Line* prefix = vector_get(&file->lines, first_index);

	u32string* prefix_text = &prefix->text;

	line_append_or_replace(
		prefix_text,
		u32string_into_ptr_const(&cb->text),
		size);

	prefix->dirty = 1;  
}

static void insert_middle_segment
(
	File* file,
	size_t start,
	size_t index,
	size_t size,
	Clipboard* cb
)
{
	u32string new_text = u32string_from_raw_copy(
		u32string_into_ptr_const(&cb->text) + start, 
		size);

	Line new_line = line_with_text(new_text);
	new_line.dirty = 1;

	vector_insert(
		&file->lines, 
		index, 
		&new_line);
}

// helper to append a suffix
static void append_suffix
(
	File* file,
	u32string* dst,
	size_t suffix_index
)
{
	u32string* suffix = file_get_line_text(file, suffix_index);

	if (!u32string_is_empty(suffix)) {
		u32string_append_raw(
			dst,
			u32string_into_ptr_const(suffix),
			u32string_size(suffix));
	}   
}

static void finish_last_line
(
	File* file,
	size_t start,
	size_t last_index,
	Clipboard* cb,
	Cursor* c
)
{
	size_t clipboard_size = u32string_size(&cb->text);

	// there is a last segment to handle
	if (start < clipboard_size) {
		size_t size = clipboard_size - start;

		u32string last_text = u32string_from_raw_copy(
			u32string_into_ptr_const(&cb->text) + start, 
			size);

		c->pos.y = last_index + 1;
		c->pos.x = u32string_size(&last_text);

		Line last_line = line_with_text(last_text);
		last_line.dirty = 1;

		append_suffix(
			file,
			&last_line.text,
			last_index + 1);

		vector_insert(
			&file->lines, 
			last_index + 1,
			&last_line);

		vector_remove_and_destroy(&file->lines, last_index + 2);
	}

	// clipboard ends with '\n', so there is no last segment
	// but we still need to deal with the last line inserted by
	// file_insert_newline

	else {
		Line* last_line = vector_get(&file->lines, last_index);

		c->pos.y = last_index + 1;
		c->pos.x = 0;
		last_line->dirty = 1;
	}
}


static int paste_multiline
(
	File* file, 
	Clipboard* cb, 
	Cursor* c,
	Result* result
) 
{
	size_t clipboard_size = u32string_size(&cb->text);
	size_t first_line_y = c->pos.y;
	size_t lines_added = 0;
	size_t start = 0;

	if (file_insert_newline(file, c->pos, result) < 0) {
		result_set_reason(result,
			"paste_multiline: file_insert_newline failed");
		result->type = ERROR_STATIC;

		return EIE_NOT_FATAL_ERROR;
	}

	int first = 1;

	for (size_t i = 0; i < clipboard_size; i++) {
		if (u32string_char(&cb->text, i) == U'\n') {
			size_t size = i - start;

			if (first) {
				insert_first_segment(
					file,
					first_line_y,
					size,
					cb);

				first = 0;
			} 

			// middle lines
			else {
				insert_middle_segment(
					file,
					start,
					first_line_y + lines_added + 1,
					size,
					cb);

				lines_added++;
			}

			start = i + 1;
		}
	}

	finish_last_line(
		file,
		start,
		first_line_y + lines_added,
		cb,
		c);

	return EIE_OK;
}

int file_paste_clipboard
(
	File* file, 
	Clipboard* cb, 
	Cursor* c,
	Result* result
) 
{
	if (u32string_is_empty(&cb->text)) {
		return EIE_NOT_AN_ERROR;
	}

	size_t nl = clipboard_count_newlines(cb);

	int ret;

	if (nl == 0) {
		ret = paste_inline(file, cb, c);

		if (ret < 0) {
			return ret;
		}
	}

	else {
		ret = paste_multiline(file, cb, c, result);

		if (ret < 0) {
			return ret;
		}
	}


	file->dirty = 1;
	result_ok(result);

	return ret;
}

int file_select_line
(
	File* file, 
	size_t y, 
	Selection* sel,
	Result* result
) 
{
	if (y >= file->lines.size) {
		return EIE_NOT_FATAL_ERROR;
	}

	u32string* line = file_get_line_text(file, y);

	if (!line) {
		return EIE_NOT_FATAL_ERROR;
	}

	sel->active = 1;
	sel->start.y = y;
	sel->start.x = 0;

	size_t lines = file->lines.size;
	int select_after = (lines > 1 && y < lines - 1);

	sel->end = (select_after)
		? (Position) { 0, y + 1 }
		: (Position) { u32string_size(line), y };

	result_ok(result);

	return EIE_OK;
}

int file_select_all_file
(
	File* file, 
	Selection* sel, 
	Result* result
) 
{
	if (file->lines.size == 0) {
		sel->active = 0;

		return EIE_NOT_AN_ERROR;
	}

	u32string* last_line_text = file_get_line_text(
		file, 
		file->lines.size - 1);

	if (!last_line_text) {
		return EIE_NOT_FATAL_ERROR;
	}

	size_t last_size = u32string_size(last_line_text);

	sel->active = 1;
	sel->start.x = 0;
	sel->end.x = last_size;
	sel->start.y = 0;
	sel->end.y = file->lines.size - 1;

	result_ok(result);

	return EIE_OK;
}