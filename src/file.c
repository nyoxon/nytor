#include "file.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <fcntl.h>		// open
#include <unistd.h>		// read and write
#include <assert.h>
#include <errno.h>

static char* read_file
(
	const char* filename, 
	size_t* size, 
	Result* result
) 
{
	int fd = open(filename, O_RDONLY);

	if (fd < 0) {
		if (errno == ENOENT) {
			result_set_reason(result,
				"read_file: file does not exist");
			result->type = ERROR_FILE_DOES_NOT_EXIST;

		} else if (errno == EACCES) {
			result_set_reason(result,
				"read_file: access denied");
			result->type = ERROR_FILE_HANDLE;

		} else if (errno == EPERM) {
			result_set_reason(result,
				"read_file: operation denied");
			result->type = ERROR_FILE_HANDLE;
		}

		return NULL;
	}

	off_t fsize = lseek(fd, 0, SEEK_END);
	lseek(fd, 0, SEEK_SET);

	char* data = malloc(fsize + 1);

	if (!data) {
		close(fd);

		result_set_reason(result,
			"read_file: data is invalid");
		result->type = ERROR_MALLOC;

		return NULL;
	}

	ssize_t read_bytes = read(fd, data, fsize);
	close(fd);

	if (read_bytes < 0) {
		free(data);

		result_set_reason(result,
			"read_file: read_bytes < 0");
		result->type = ERROR_SYS_READ;

		return NULL;
	}

	data[read_bytes] = '\0';
	*size = read_bytes;

	return data;
}

static int split_lines
(
	File* file, 
	const char* data, 
	size_t size
) 
{
	size_t start = 0;

	while (start < size) {
		size_t end = start;

		while (end < size && data[end] != '\n') {
			end++;
		}

		size_t len = end - start;

		Line line;
		line.text = malloc(len + 1);

		if (!line.text) {
			return -1;
		}

		memcpy(line.text, data + start, len);
		line.text[len] = '\0';
		line.len = len;
		// line.dirty = 0;

		vector_push(&file->lines, &line);

		start = end + 1;
	}

	return 0;
}

int file_open
(
	File* file, 
	const char* filename, 
	int* new_file,
	Result* result
) 
{	
	if (!file) {
		result_set_reason(result,
			"file_open: file is a null pointer");
		result->type = ERROR_NULL_POINTER;

		return -1;
	}

	if (!filename) {
		result_set_reason(result,
			"file_open: filename is null");
		result->type = ERROR_NULL_POINTER;

		return -1;
	}

	if (*new_file) {
		if (result) {
			result_set_reason(result,
				"file_open: value of passed 'new_file' must be zero");

			result->type = ERROR_INVALID_ARGUMENT;
		}

		return EIE_FATAL_ERROR;
	}

	size_t size;
	char* data = read_file(filename, &size, result);

	if (!data) {
		if (!result) {
			return EIE_FATAL_ERROR;
		} else {
			if (result->type == ERROR_FILE_DOES_NOT_EXIST) {
				int fd = open(filename, O_WRONLY | O_CREAT, 0644);

				if (fd < 0) {
					if (result) {
						result_set_reason(result,
							"file_open: failed to create a new file");
					}

					result->type = ERROR_FILE_HANDLE;
					return -1;
				}

				*new_file = 1;
				close(fd);
				data = strdup("");
				size = 0;
			} else {
				return EIE_FATAL_ERROR;
			}
		}
	}

	file->filename = strdup(filename);
	vector_init(&file->lines, sizeof(Line), vector_line_destroy);
	file->dirty = (*new_file) ? 1 : 0;

	if (size == 0) {
		Line line = {0};
		line.text = strdup("");
		line.len = 0;
		vector_push(&file->lines, &line);
	} else {
		if (split_lines(file, data, size) < 0) {
			free(data);

			result_set_reason(result,
				"file_open: error returned by a static function");
			result->type = ERROR_STATIC;

			return EIE_FATAL_ERROR;
		}
	}

	free(data);

	result_ok(result);

	return 0;
}

int file_save(File* file, Result* result) {
	if (!file) { 
		result_set_reason(result,
			"file_save: file is a null pointer");
		result->type = ERROR_NULL_POINTER;

		return EIE_FATAL_ERROR; 
	}

	if (!file->dirty) {
		return EIE_NOT_AN_ERROR; // neither error or success
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
		return 0;
	}

	for (size_t i = 0; i < file->lines.size; i++) {
		Line* line = vector_get(&file->lines, i);

		if (!line) {
			result_set_reason(result,
				"file_save: line is invalid");
			result->type = ERROR_NULL_POINTER;

			return EIE_NOT_FATAL_ERROR;			
		}

		write(fd, line->text, line->len);

		if (i < file->lines.size -1) {
			write(fd, "\n", 1);
		}
	}

	close(fd);

	file->dirty = 0;

	result_ok(result);

	return 0;
}

void file_free(File* file) {
	if (!file) { return; }

	free(file->filename);
	vector_free(&file->lines);
}

// int file_insert_line(File* file, size_t y, const char* text) {
// 	if (y >= file->lines.size) {
// 		return -1;
// 	}

// 	size_t len = strlen(text);
// 	Line line;

// 	line.text = malloc(len + 1); // '\0'
// 	memcpy(line.text, text, len);
// 	line.len = len;
// 	line.text[len] = '\0';

// 	// line.dirty = 1;
// 	vector_insert(&file->lines, y, &line);

// 	file->dirty = 1;
// 	return 0;
// }

// int file_delete_line(File* file, size_t index) {
// 	if (index >= file->lines.size) {
// 		return -1;
// 	}

// 	vector_remove_and_destroy(&file->lines, index);

// 	file->dirty = 1;
// 	return 0;
// }


int file_insert_char
(
	File* file, 
	Cursor* cursor, 
	char c,
	Result* result
) {
	if (!file) { 
		result_set_reason(result,
			"file_insert_char: file is a null pointer");
		result->type = ERROR_NULL_POINTER;

		return EIE_FATAL_ERROR; 
	}

	if (!cursor) { 
		result_set_reason(result,
			"file_insert_char: cursor is a null pointer");
		result->type = ERROR_NULL_POINTER;

		return EIE_FATAL_ERROR; 
	}

	if (cursor->y >= file->lines.size) {
		result_set_reason(result,
			"file_insert_char: cursor->y >= file->lines.size");
		result->type = ERROR_INDEX_OUT_OF_BOUNDS;

		return EIE_NOT_FATAL_ERROR;
	}

	Line* line = vector_get(&file->lines, cursor->y);

	if (!line) {
		result_set_reason(result,
			"file_insert_char: line is invalid");
		result->type = ERROR_NULL_POINTER;

		return EIE_NOT_FATAL_ERROR;
	}

	char* new_text = malloc(line->len + 2); // + sizeof(char) + '\0'

	if (!new_text) {
		result_set_reason(result,
			"file_insert_char: new_text is invalid");
		result->type = ERROR_MALLOC;

		return EIE_NOT_FATAL_ERROR;
	}

	// copy until you reach the character
	memcpy(new_text, line->text, cursor->x);
	new_text[cursor->x] = c;

	// copy everything after the character
	memcpy(new_text + cursor->x + 1, line->text + cursor->x, line->len - cursor->x);
	new_text[line->len + 1] = '\0';

	line_replace(line, new_text, line->len + 1);
	// line->dirty = 1;
	cursor->x++;

	file->dirty = 1;

	result_ok(result);

	return 0;
}

int file_delete_char(File* file, Cursor* cursor, Result* result) {
	if (!file) { 
		result_set_reason(result,
			"file_delete_char: file is a null pointer");
		result->type = ERROR_NULL_POINTER;

		return EIE_FATAL_ERROR; 
	}

	if (!cursor) { 
		result_set_reason(result,
			"file_delete_char: cursor is a null pointer");
		result->type = ERROR_NULL_POINTER;

		return EIE_FATAL_ERROR; 
	}

	if (cursor->y >= file->lines.size) {
		result_set_reason(result,
			"file_delete_char: cursor->y >= file->lines.size");
		result->type = ERROR_INDEX_OUT_OF_BOUNDS;

		return EIE_NOT_FATAL_ERROR;
	}

	if (cursor->x == 0) {
		result_set_reason(result,
			"file_delete_char: cursor->x == 0");
		result->type = ERROR_INDEX_OUT_OF_BOUNDS;

		return EIE_NOT_FATAL_ERROR;
	}

	Line* line = vector_get(&file->lines, cursor->y);

	if (!line) {
		result_set_reason(result,
			"file_delete_char: line is invalid");
		result->type = ERROR_NULL_POINTER;

		return EIE_NOT_FATAL_ERROR;
	}

	char* new_text = malloc(line->len);

	if (!new_text) {
		result_set_reason(result,
			"file_delete_char: new_text is invalid");
		result->type = ERROR_MALLOC;

		return EIE_NOT_FATAL_ERROR;
	}

	// rewrites the offset containing character by the offset that comes right after it
	memcpy(new_text, line->text, cursor->x - 1);
	memcpy(new_text + cursor->x - 1, line->text + cursor->x, line->len - cursor->x);

	new_text[line->len - 1] = '\0';

	line_replace(line, new_text, line->len - 1);
	// line->dirty = 1;
	cursor->x--;

	file->dirty = 1;

	result_ok(result);

	return 0;
}

int file_insert_newline(File* file, Cursor* cursor, Result* result) {
	if (!file) {
		result_set_reason(result,
			"file_insert_newline: file is a null pointer");
		result->type = ERROR_NULL_POINTER;

		return EIE_FATAL_ERROR; 
	}

	if (!cursor) { 
		result_set_reason(result,
			"file_insert_newline: cursor is a null pointer");
		result->type = ERROR_NULL_POINTER;

		return EIE_FATAL_ERROR; 
	}

	if (cursor->y >= file->lines.size) {
		result_set_reason(result,
			"file_insert_newline: cursor->y >= file->lines.size");
		result->type = ERROR_INDEX_OUT_OF_BOUNDS;

		return EIE_NOT_FATAL_ERROR;
	}

	Line* line = vector_get(&file->lines, cursor->y);

	if (!line) {
		result_set_reason(result,
			"file_insert_newline: line is invalid");
		result->type = ERROR_NULL_POINTER;

		return EIE_NOT_FATAL_ERROR;
	}

	Line new_line;

	// tail = new line
	size_t tail_len = line->len - cursor->x;
	char* tail_text = malloc(tail_len + 1);

	if (!tail_text) {
		result_set_reason(result,
			"file_insert_newline: tail_text is invalid");
		result->type = ERROR_MALLOC;

		return EIE_NOT_FATAL_ERROR;
	}

	memcpy(tail_text, line->text + cursor->x, tail_len);

	tail_text[tail_len] = '\0';

	new_line.text = tail_text;
	new_line.len = tail_len;
	// new_line.dirty = 1;

	// head = original line
	size_t head_len = cursor->x;
	char* head_text = malloc(head_len + 1);

	if (!head_text) {
		result_set_reason(result,
			"file_insert_newline: head_text is invalid");
		result->type = ERROR_MALLOC;

		free(tail_text);

		return EIE_NOT_FATAL_ERROR;
	}	

	memcpy(head_text, line->text, cursor->x);
	head_text[head_len] = '\0';

	free(line->text);
	line->text = head_text;
	line->len = head_len;
	// line->dirty = 1;

	vector_insert(&file->lines, cursor->y + 1, &new_line);

	file->dirty = 1;

	result_ok(result);

	return 0;
}

int file_merge_lines(File* file, Cursor* cursor, Result* result) {
	if (!file) {
		result_set_reason(result,
			"file_merge_lines: file is a null pointer");
		result->type = ERROR_NULL_POINTER;

		return EIE_FATAL_ERROR; 
	}

	if (!cursor) { 
		result_set_reason(result,
			"file_merge_lines: cursor is a null pointer");
		result->type = ERROR_NULL_POINTER;

		return EIE_FATAL_ERROR; 
	}

	if (cursor->y == 0 || cursor->y >= file->lines.size) {
		result_set_reason(result,
			"file_merge_lines: cursor->y == 0 || cursor->y >= file->lines.size");
		result->type = ERROR_INDEX_OUT_OF_BOUNDS;

		return EIE_NOT_FATAL_ERROR;
	}

	Line* prev = vector_get(&file->lines, cursor->y - 1);

	if (!prev) {
		result_set_reason(result,
			"file_merge_lines: prev is invalid");
		result->type = ERROR_NULL_POINTER;

		return EIE_NOT_FATAL_ERROR;
	}

	size_t old_prev_len = prev->len;
	Line* curr = vector_get(&file->lines, cursor->y);

	if (!curr) {
		result_set_reason(result,
			"file_merge_lines: prev is invalid");
		result->type = ERROR_NULL_POINTER;

		return EIE_NOT_FATAL_ERROR;
	}

	size_t new_len = prev->len + curr->len;
	char* new_text = malloc(new_len + 1);

	if (!new_text) {
		result_set_reason(result,
			"file_merge_lines: new_text is invalid");
		result->type = ERROR_MALLOC;

		return EIE_NOT_FATAL_ERROR;
	}

	memcpy(new_text, prev->text, prev->len);
	memcpy(new_text + prev->len, curr->text, curr->len);

	new_text[new_len] = '\0';

	free(prev->text);
	prev->text = new_text;
	prev->len = new_len;
	// prev->dirty = 1;

	vector_remove_and_destroy(&file->lines, cursor->y);
	cursor->y--;
	cursor->x = old_prev_len;

	file->dirty = 1;

	result_ok(result);

	return 0;
}

int file_move_line_up(File* file, size_t* y, Result* result) {
	if (!file) {
		result_set_reason(result,
			"file_move_line_up: file is a null pointer");
		result->type = ERROR_NULL_POINTER;

		return EIE_FATAL_ERROR; 
	}

	if (*y == 0) {
		result_set_reason(result,
			"file_move_line_up: *y == 0");
		result->type = ERROR_INDEX_OUT_OF_BOUNDS;

		return EIE_NOT_FATAL_ERROR;
	}

	int ret = vector_swap(&file->lines, *y, *y - 1);

	if (ret < 0) {
		result_set_reason(result,
			"file_move_line_up: error returned by vector_swap");
		result->type = ERROR_VECTOR;

		return EIE_NOT_FATAL_ERROR;
	}

	(*y)--;

	file->dirty = 1;

	result_ok(result);

	return 0;
}

int file_move_line_down(File* file, size_t* y, Result* result) {
	if (!file) {
		result_set_reason(result,
			"file_move_line_down: file is a null pointer");
		result->type = ERROR_NULL_POINTER;

		return EIE_FATAL_ERROR; 
	}

	if (*y >= file->lines.size - 1) {
		result_set_reason(result,
			"file_move_line_down: *y >= file->lines.size - 1");
		result->type = ERROR_INDEX_OUT_OF_BOUNDS;

		return EIE_NOT_FATAL_ERROR;
	}

	int ret = vector_swap(&file->lines, *y, *y + 1);

	if (ret < 0) {
		result_set_reason(result,
			"file_move_line_down: error returned by vector_swap");
		result->type = ERROR_VECTOR;

		return EIE_NOT_FATAL_ERROR;
	}

	(*y)++;

	file->dirty = 1;

	result_ok(result);

	return 0;
}

static int delete_within_line
(
	File* file,
	size_t y,
	size_t x1, size_t x2,
	Cursor* c
)
{
	Line* line = vector_get(&file->lines, y);

	if (!line) {
		return -1;
	}

	size_t new_len = line->len - (x2 - x1);
	char* new_text = malloc(new_len + 1);

	if (!new_text) {
		return -1;
	}

	memcpy(new_text, line->text, x1);
	memcpy(new_text + x1,
		   line->text + x2,
		   line->len - x2);

	new_text[new_len] = '\0';

	line_replace(line, new_text, new_len);

	c->x = x1;
	c->y = y;

	return 0;
}

static int delete_multiline
(
	File* file,
	size_t y1, size_t x1,
	size_t y2, size_t x2,
	Cursor* c
)
{
	Line* first = vector_get(&file->lines, y1);
	Line* last  = vector_get(&file->lines, y2);

	if (!first || !last) {
		return -1;
	}

	size_t prefix_len = x1;
	size_t suffix_len = last->len - x2;

	size_t new_len = prefix_len + suffix_len;
	char* new_text = malloc(new_len + 1);

	if (!new_text) {
		return -1;
	}

	memcpy(new_text, first->text, prefix_len);
	memcpy(new_text + prefix_len,
	       last->text + x2,
	       suffix_len);

	new_text[new_len] = '\0';

	free(first->text);
	first->text = new_text;
	first->len = new_len;

	for (size_t y = y1 + 1; y <= y2; y++) {
		vector_remove_and_destroy(&file->lines, y1 + 1);
	}

	c->y = y1;
	c->x = x1;

	return 0;
}

int file_delete_selection
(
	File* file, 
	Cursor* c, 
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

	if (!c) { 
		result_set_reason(result,
			"file_delete_selection: cursor is a null pointer");
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

	size_t x1, y1, x2, y2;
	selection_normalize(sel, &x1, &y1, &x2, &y2);

	if (y1 == y2) {
		if (delete_within_line(file, y1, x1, x2, c) < 0) {
			result_set_reason(result,
				"file_delete_selection: error returned by a static function");
			result->type = ERROR_STATIC;

			return EIE_NOT_FATAL_ERROR;
		}
	} else {
		if (delete_multiline(file, y1, x1, y2, x2, c) < 0) {
			result_set_reason(result,
				"file_delete_selection: error returned by a static function");
			result->type = ERROR_STATIC;

			return EIE_NOT_FATAL_ERROR;
		}
	}

	selection_clear(sel);
	file->dirty = 1;

	result_ok(result);

	return 0;
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

	size_t x1, y1, x2, y2;
	selection_normalize(sel, &x1, &y1, &x2, &y2);

	if (y1 == y2 && x1 == x2) {
		return EIE_NOT_AN_ERROR;
	}

	Line* last_line = vector_get(&file->lines, y2);

	if (!last_line) {
		result_set_reason(result,
			"file_copy_selection: last_line is a null pointer");
		result->type = ERROR_NULL_POINTER;

		return EIE_NOT_FATAL_ERROR;
	}

	cb->linewise = (x1 == 0 && x2 == last_line->len);

	free(cb->text);
	cb->text = NULL;
	cb->len = 0;

	size_t total_len = 0;

	for (size_t y = y1; y <= y2; y++) {
		Line* line = vector_get(&file->lines, y);

		if (!line) {
			result_set_reason(result,
				"file_copy_selection: line is a null pointer");
			result->type = ERROR_NULL_POINTER;

			return EIE_NOT_FATAL_ERROR;
		}

		size_t start = (y == y1) ? x1 : 0;
		size_t end   = (y == y2) ? x2 : line->len;

		total_len += (end - start);

		if (y < y2) total_len += 1;
	}

	if (cb->linewise) total_len += 1;

	cb->text = malloc(total_len + 1);

	if (!cb->text) {
		result_set_reason(result,
			"file_copy_selection: cb->text is a null pointer");
		result->type = ERROR_NULL_POINTER;

		return EIE_NOT_FATAL_ERROR;
	}

	cb->len = total_len;

	size_t pos = 0;

	for (size_t y = y1; y <= y2; y++) {
		Line* line = vector_get(&file->lines, y);

		if (!line) {
			result_set_reason(result,
				"file_copy_selection: line is a null pointer");
			result->type = ERROR_NULL_POINTER;

			free(cb->text);

			return EIE_NOT_FATAL_ERROR;
		}

		size_t start = (y == y1) ? x1 : 0;
		size_t end   = (y == y2) ? x2 : line->len;

		size_t len = end - start;

		memcpy(cb->text + pos, line->text + start, len);
		pos += len;

		if (y < y2) {
			cb->text[pos++] = '\n';
		}
	}

	if (cb->linewise) {
		cb->text[pos++] = '\n';
	}

	cb->text[pos] = '\0';

	result_ok(result);

	return 0;
}

static int paste_inline(File* file, Clipboard* cb, Cursor* c) {
	Line* line = vector_get(&file->lines, c->y);

	if (!line) {
		return -1;
	}

	size_t new_len = line->len + cb->len;
	char* new_text = malloc(new_len + 1);

	if (!new_text) {
		return -1;
	}

	memcpy(new_text, line->text, c->x);
	memcpy(new_text + c->x, cb->text, cb->len);
	memcpy(new_text + c->x + cb->len,
	       line->text + c->x,
	       line->len - c->x);

	new_text[new_len] = '\0';

	free(line->text);
	line->text = new_text;
	line->len = new_len;

	c->x += cb->len;

	return 0;
}

int file_paste_clipboard
(
	File* file, 
	Clipboard* cb, 
	Cursor* c,
	Result* result
) 
{
	if (!file) {
		result_set_reason(result,
			"file_paste_clipboard: file is a null pointer");
		result->type = ERROR_NULL_POINTER;

		return EIE_FATAL_ERROR; 
	}

	if (!cb) { 
		result_set_reason(result,
			"file_paste_clipboard: cb is a null pointer");
		result->type = ERROR_NULL_POINTER;

		return EIE_FATAL_ERROR; 
	}

	if (!c) { 
		result_set_reason(result,
			"file_paste_clipboard: cursor is a null pointer");
		result->type = ERROR_NULL_POINTER;

		return EIE_FATAL_ERROR; 
	}

	if (cb->len == 0) {
		return EIE_NOT_AN_ERROR;
	}

	int has_nl = strchr(cb->text, '\n') != NULL;

	if (!has_nl) {
		if (paste_inline(file, cb, c) < 0) {
			result_set_reason(result,
				"file_paste_clipboard: error returned by a static function");
			result->type = ERROR_STATIC;

			return EIE_NOT_FATAL_ERROR;
		}

		file->dirty = 1;
		result_ok(result);

		return 0;
	}

	if (cb->linewise) {
		size_t insert_y = c->y;
		size_t start = 0;

		for (size_t i = 0; i < cb->len; i++) {
			if (cb->text[i] == '\n') {
				size_t len = i - start;

				Line line;
				line.text = malloc(len + 1);

				if (!line.text) {
					result_set_reason(result,
						"file_paste_clipboard: line.text is a null pointer");
					result->type = ERROR_NULL_POINTER;

					return EIE_FATAL_ERROR;
				}

				memcpy(line.text, cb->text + start, len);
				line.text[len] = '\0';
				line.len = len;

				vector_insert(&file->lines, insert_y++, &line);

				start = i + 1;
			}
		}

		c->y = insert_y;
		c->x = 0;

		result_ok(result);

		file->dirty = 1;
		return 0;
	}

	// multiline
	Line* line = vector_get(&file->lines, c->y);

	if (!line) {
		result_set_reason(result,
			"file_paste_clipboard: line is a null pointer");
		result->type = ERROR_NULL_POINTER;

		return EIE_NOT_FATAL_ERROR;
	}

	size_t prefix_len = c->x;
	size_t suffix_len = line->len - c->x;

	char* prefix = malloc(prefix_len + 1);

	if (!prefix) {
		result_set_reason(result,
			"file_paste_clipboard: prefix is a null pointer");
		result->type = ERROR_NULL_POINTER;

		return EIE_NOT_FATAL_ERROR;
	}

	memcpy(prefix, line->text, prefix_len);
	prefix[prefix_len] = '\0';

	char* suffix = malloc(suffix_len + 1);

	if (!suffix) {
		result_set_reason(result,
			"file_paste_clipboard: suffix is a null pointer");
		result->type = ERROR_NULL_POINTER;

		free(prefix);

		return EIE_NOT_FATAL_ERROR;
	}

	memcpy(suffix, line->text + c->x, suffix_len);
	suffix[suffix_len] = '\0';

	size_t start = 0;
	size_t insert_y = c->y;

	int first = 1;

	for (size_t i = 0; i <= cb->len; i++) {
		if (i == cb->len || cb->text[i] == '\n') {
			size_t len = i - start;

			if (first) {
				size_t new_len = len + prefix_len;
				char* new_text = malloc(new_len + 1);

				if (!new_text) {
					result_set_reason(result,
						"file_paste_clipboard: new_text is a null pointer");
					result->type = ERROR_NULL_POINTER;

					return EIE_FATAL_ERROR;
				}			

				memcpy(new_text, prefix, prefix_len);
				memcpy(new_text + prefix_len, cb->text + start, len);
				new_text[new_len] = '\0';

				line_replace(line, new_text, new_len);

				first = 0;
			} else {
				Line new_line;

				new_line.text = malloc(len + 1);

				if (!new_line.text) {
					result_set_reason(result,
						"file_paste_clipboard: new_line.text is a null pointer");
					result->type = ERROR_NULL_POINTER;

					return EIE_FATAL_ERROR;
				}

				memcpy(new_line.text, cb->text + start, len);
				new_line.text[len] = '\0';
				new_line.len = len;	

				vector_insert(&file->lines, insert_y + 1, &new_line);

				insert_y++;
			}

			start = i + 1;
		}
	}

	Line* last = vector_get(&file->lines, insert_y);

	if (!last) {
		result_set_reason(result,
			"file_paste_clipboard: last is a null pointer");
		result->type = ERROR_NULL_POINTER;

		return EIE_NOT_FATAL_ERROR;
	}

	size_t new_len = last->len + suffix_len;
	char* new_text = malloc(new_len + 1);

	if (!new_text) {
		result_set_reason(result,
			"file_paste_clipboard: new_text is a null pointer");
		result->type = ERROR_NULL_POINTER;

		return EIE_NOT_FATAL_ERROR;
	}

	memcpy(new_text, last->text, last->len);
	memcpy(new_text + last->len, suffix, suffix_len);
	new_text[new_len] = '\0';

	line_replace(last, new_text, new_len);

	free(suffix);
	free(prefix);

	c->y = insert_y;
	c->x = last->len - suffix_len;

	result_ok(result);

	file->dirty = 1;

	return 0;
}

int file_select_line
(
	File* file, 
	size_t y, 
	Selection* sel,
	Result* result
) 
{
	if (!file) {
		result_set_reason(result,
			"file_select_line: file is a null pointer");
		result->type = ERROR_NULL_POINTER;

		return EIE_FATAL_ERROR; 
	}

	if (!sel) { 
		result_set_reason(result,
			"file_select_line: sel is a null pointer");
		result->type = ERROR_NULL_POINTER;

		return EIE_FATAL_ERROR; 
	}

	if (y >= file->lines.size) {
		result_set_reason(result,
			"file_select_line: y >= file->lines.size");
		result->type = ERROR_INDEX_OUT_OF_BOUNDS;

		return EIE_NOT_FATAL_ERROR;
	}

	Line* line = vector_get(&file->lines, y);

	if (!line) {
		result_set_reason(result,
			"file_select_line: line is a null pointer");
		result->type = ERROR_NULL_POINTER;

		return EIE_NOT_FATAL_ERROR;
	}

	sel->active = 1;
	sel->start_y = y;
	sel->end_y = y;

	sel->start_x = 0;
	sel->end_x = line->len;

	result_ok(result);

	return 0;
}

int file_select_all_file
(
	File* file, 
	Selection* sel, 
	Cursor* c,
	Result* result
) 
{
	if (!file) {
		result_set_reason(result,
			"file_select_all_file: file is a null pointer");
		result->type = ERROR_NULL_POINTER;

		return EIE_FATAL_ERROR; 
	}

	if (!sel) { 
		result_set_reason(result,
			"file_select_all_file: sel is a null pointer");
		result->type = ERROR_NULL_POINTER;

		return EIE_FATAL_ERROR; 
	}

	if (!c) { 
		result_set_reason(result,
			"file_select_all_file: cursor is a null pointer");
		result->type = ERROR_NULL_POINTER;

		return EIE_FATAL_ERROR; 
	}

	if (file->lines.size == 0) {
		sel->active = 0;

		return EIE_NOT_AN_ERROR;
	}

	Line* last_line = vector_get(&file->lines, file->lines.size - 1);

	if (!last_line) {
		result_set_reason(result,
			"file_select_all_file: last_line is a null pointer");
		result->type = ERROR_NULL_POINTER;

		return EIE_NOT_FATAL_ERROR;
	}

	sel->active = 1;
	sel->start_x = 0;
	sel->end_x = last_line->len;
	sel->start_y = 0;
	sel->end_y = file->lines.size - 1;

	c->x = last_line->len;
	c->y = sel->end_y;

	result_ok(result);

	return 0;
}