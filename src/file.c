#include "file.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <fcntl.h>		// open
#include <unistd.h>		// read and write
#include <assert.h>

static char* read_file(const char* filename, size_t* size) {
	int fd = open(filename, O_RDONLY);

	if (fd < 0) {
		return NULL;
	}

	off_t fsize = lseek(fd, 0, SEEK_END);
	lseek(fd, 0, SEEK_SET);

	char* data = malloc(fsize + 1);

	if (!data) {
		close(fd);
		return NULL;
	}

	ssize_t read_bytes = read(fd, data, fsize);
	close(fd);

	if (read_bytes < 0) {
		free(data);
		return NULL;
	}

	data[read_bytes] = '\0';
	*size = read_bytes;

	return data;
}

static void split_lines(File* file, const char* data, size_t size) {
	size_t start = 0;

	while (start < size) {
		size_t end = start;

		while (end < size && data[end] != '\n') {
			end++;
		}

		size_t len = end - start;

		Line line;
		line.text = malloc(len + 1);
		memcpy(line.text, data + start, len);
		line.text[len] = '\0';
		line.len = len;
		// line.dirty = 0;

		vector_push(&file->lines, &line);

		start = end + 1;
	}
}

int file_open(File* file, const char* filename, int* new_file) {
	if (*new_file) { // passed *new_file must be 0
		return -1;
	}

	size_t size;
	char* data = read_file(filename, &size);

	if (!data) {
		int fd = open(filename, O_WRONLY | O_CREAT, 0644);

		if (fd < 0) {
			return -1;
		}

		*new_file = 1;
		close(fd);
		data = strdup("");
		size = 0;
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
		split_lines(file, data, size);
	}

	free(data);
	return 0;
}

int file_save(File* file) {
	if (!file) { return -1; }

	if (!file->dirty) {
		return 1;
	}

	int fd = open(file->filename, O_WRONLY | O_TRUNC | O_CREAT, 0644);

	if (fd < 0) {
		return -1;
	}

	if (file->lines.size == 0) {
		write(fd, "", 1);
		return 0;
	}

	for (size_t i = 0; i < file->lines.size; i++) {
		Line* line = vector_get(&file->lines, i);
		write(fd, line->text, line->len);

		if (i < file->lines.size -1) {
			write(fd, "\n", 1);
		}
	}

	// [FUTURE] considerar ultimo '\n' dentro do arquivo?

	close(fd);

	file->dirty = 0;
	return 0;
}

void file_free(File* file) {
	if (!file) { return; }

	free(file->filename);
	vector_free(&file->lines);
}

int file_insert_line(File* file, size_t y, const char* text) {
	if (y >= file->lines.size) {
		return -1;
	}

	size_t len = strlen(text);
	Line line;

	line.text = malloc(len + 1); // '\0'
	memcpy(line.text, text, len);
	line.len = len;
	line.text[len] = '\0';

	// line.dirty = 1;
	vector_insert(&file->lines, y, &line);

	file->dirty = 1;
	return 0;
}

// int file_delete_line(File* file, size_t index) {
// 	if (index >= file->lines.size) {
// 		return -1;
// 	}

// 	vector_remove_and_destroy(&file->lines, index);

// 	file->dirty = 1;
// 	return 0;
// }


int file_insert_char(File* file, Cursor* cursor, char c) {
	if (cursor->y >= file->lines.size) {
		return -1;
	}

	Line* line = vector_get(&file->lines, cursor->y);

	char* new_text = malloc(line->len + 2); // + sizeof(char) + '\0'

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
	return 0;
}

int file_delete_char(File* file, Cursor* cursor) {
	if (cursor->y >= file->lines.size) {
		return -1;
	}

	if (cursor->x == 0) {
		return -1;
	}

	Line* line = vector_get(&file->lines, cursor->y);
	char* new_text = malloc(line->len);

	// rewrites the offset containing character by the offset that comes right after it
	memcpy(new_text, line->text, cursor->x - 1);
	memcpy(new_text + cursor->x - 1, line->text + cursor->x, line->len - cursor->x);

	new_text[line->len - 1] = '\0';

	line_replace(line, new_text, line->len - 1);
	// line->dirty = 1;
	cursor->x--;

	file->dirty = 1;
	return 0;
}

int file_insert_newline(File* file, Cursor* cursor) {
	if (cursor->y >= file->lines.size) {
		return -1;
	}

	Line* line = vector_get(&file->lines, cursor->y);
	Line new_line;

	// tail = new line
	size_t tail_len = line->len - cursor->x;
	char* tail_text = malloc(tail_len + 1);

	memcpy(tail_text, line->text + cursor->x, tail_len);

	tail_text[tail_len] = '\0';

	new_line.text = tail_text;
	new_line.len = tail_len;
	// new_line.dirty = 1;

	// head = original line
	size_t head_len = cursor->x;
	char* head_text = malloc(head_len + 1);

	memcpy(head_text, line->text, cursor->x);
	head_text[head_len] = '\0';

	free(line->text);
	line->text = head_text;
	line->len = head_len;
	// line->dirty = 1;

	vector_insert(&file->lines, cursor->y + 1, &new_line);

	file->dirty = 1;
	return 0;
}

int file_merge_lines(File* file, Cursor* cursor) {
	if (cursor->y == 0 || cursor->y >= file->lines.size) {
		return -1;
	}

	Line* prev = vector_get(&file->lines, cursor->y - 1);
	size_t old_prev_len = prev->len;
	Line* curr = vector_get(&file->lines, cursor->y);

	size_t new_len = prev->len + curr->len;
	char* new_text = malloc(new_len + 1);

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
	return 0;
}

int file_move_line_up(File* file, size_t* y) {
	if (*y == 0) {
		return -1;
	}

	int ret = vector_swap(&file->lines, *y, *y - 1);

	if (ret < 0) {
		return -1;
	}

	(*y)--;

	file->dirty = 1;

	return 0;
}

int file_move_line_down(File* file, size_t* y) {
	if (*y >= file->lines.size - 1) {
		return -1;
	}

	int ret = vector_swap(&file->lines, *y, *y + 1);

	if (ret < 0) {
		return -1;
	}

	(*y)++;

	file->dirty = 1;

	return 0;
}

static void delete_within_line
(
	File* file,
	size_t y,
	size_t x1, size_t x2,
	Cursor* c
)
{
	Line* line = vector_get(&file->lines, y);

	size_t new_len = line->len - (x2 - x1);
	char* new_text = malloc(new_len + 1);

	memcpy(new_text, line->text, x1);
	memcpy(new_text + x1,
		   line->text + x2,
		   line->len - x2);

	new_text[new_len] = '\0';

	line_replace(line, new_text, new_len);

	c->x = x1;
	c->y = y;
}

static void delete_multiline
(
	File* file,
	size_t y1, size_t x1,
	size_t y2, size_t x2,
	Cursor* c
)
{
	Line* first = vector_get(&file->lines, y1);
	Line* last  = vector_get(&file->lines, y2);

	size_t prefix_len = x1;
	size_t suffix_len = last->len - x2;

	size_t new_len = prefix_len + suffix_len;
	char* new_text = malloc(new_len + 1);

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
}

void file_delete_selection(File* file, Cursor* c, Selection* sel) {
	if (!sel->active) {
		return;
	}

	size_t x1, y1, x2, y2;
	selection_normalize(sel, &x1, &y1, &x2, &y2);

	if (y1 == y2) {
		delete_within_line(file, y1, x1, x2, c);
	} else {
		delete_multiline(file, y1, x1, y2, x2, c);
	}

	selection_clear(sel);
	file->dirty = 1;
}

void file_copy_selection(File* file, Clipboard* cb, Selection* sel) {
	if (!sel->active) return;

	size_t x1, y1, x2, y2;
	selection_normalize(sel, &x1, &y1, &x2, &y2);

	if (y1 == y2 && x1 == x2) return;

	Line* last_line = vector_get(&file->lines, y2);
	cb->linewise = (x1 == 0 && x2 == last_line->len);

	free(cb->text);
	cb->text = NULL;
	cb->len = 0;

	size_t total_len = 0;

	for (size_t y = y1; y <= y2; y++) {
		Line* line = vector_get(&file->lines, y);

		size_t start = (y == y1) ? x1 : 0;
		size_t end   = (y == y2) ? x2 : line->len;

		total_len += (end - start);

		if (y < y2) total_len += 1;
	}

	if (cb->linewise) total_len += 1;

	cb->text = malloc(total_len + 1);
	cb->len = total_len;

	size_t pos = 0;

	for (size_t y = y1; y <= y2; y++) {
		Line* line = vector_get(&file->lines, y);

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
}

static void paste_inline(File* file, Clipboard* cb, Cursor* c) {
	Line* line = vector_get(&file->lines, c->y);

	size_t new_len = line->len + cb->len;
	char* new_text = malloc(new_len + 1);

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
}

void file_paste_clipboard(File* file, Clipboard* cb, Cursor* c) {
	if (!cb || cb->len == 0) return;

	int has_nl = strchr(cb->text, '\n') != NULL;

	if (!has_nl) {
		paste_inline(file, cb, c);
		file->dirty = 1;
		return;
	}

	if (cb->linewise) {
		size_t insert_y = c->y;
		size_t start = 0;

		for (size_t i = 0; i < cb->len; i++) {
			if (cb->text[i] == '\n') {
				size_t len = i - start;

				Line line;
				line.text = malloc(len + 1);
				memcpy(line.text, cb->text + start, len);
				line.text[len] = '\0';
				line.len = len;

				vector_insert(&file->lines, insert_y++, &line);

				start = i + 1;
			}
		}

		c->y = insert_y;
		c->x = 0;

		file->dirty = 1;
		return;
	}

	// multiline
	Line* line = vector_get(&file->lines, c->y);
	size_t prefix_len = c->x;
	size_t suffix_len = line->len - c->x;

	char* prefix = malloc(prefix_len + 1);
	memcpy(prefix, line->text, prefix_len);
	prefix[prefix_len] = '\0';

	char* suffix = malloc(suffix_len + 1);
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

				memcpy(new_text, prefix, prefix_len);
				memcpy(new_text + prefix_len, cb->text + start, len);
				new_text[new_len] = '\0';

				line_replace(line, new_text, new_len);

				first = 0;
			} else {
				Line new_line;

				new_line.text = malloc(len + 1);
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

	size_t new_len = last->len + suffix_len;
	char* new_text = malloc(new_len + 1);

	memcpy(new_text, last->text, last->len);
	memcpy(new_text + last->len, suffix, suffix_len);
	new_text[new_len] = '\0';

	line_replace(last, new_text, new_len);

	free(suffix);
	free(prefix);

	c->y = insert_y;
	c->x = last->len - suffix_len;

	file->dirty = 1;
}

void file_select_line(File* file, size_t y, Selection* sel) {
	if (y >= file->lines.size) {
		return;
	}

	Line* line = vector_get(&file->lines, y);

	sel->active = 1;
	sel->start_y = y;
	sel->end_y = y;

	sel->start_x = 0;
	sel->end_x = line->len;
}

void file_select_all_file(File* file, Selection* sel, Cursor* c) {
	if (!sel) {
		return;
	}

	if (file->lines.size == 0) {
		sel->active = 0;
		return;
	}

	Line* last_line = vector_get(&file->lines, file->lines.size - 1);

	sel->active = 1;
	sel->start_x = 0;
	sel->end_x = last_line->len;
	sel->start_y = 0;
	sel->end_y = file->lines.size - 1;

	c->x = last_line->len;
	c->y = sel->end_y;
}