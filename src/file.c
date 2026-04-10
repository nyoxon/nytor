#include "file.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <fcntl.h>		// open
#include <unistd.h>		// read and write
#include <assert.h>

static void destroy_line(void* ptr) {
	Line* line = (Line*) ptr;
	free(line->text);
}

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

		size_t len = end - start + (data[end] == '\n' ? 1 : 0);

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

File* file_open(const char* filename) {
	size_t size;
	char* data = read_file(filename, &size);

	if (!data) {
		int fd = open(filename, O_WRONLY | O_CREAT, 0644);

		if (fd < 0) {
			return NULL;
		}

		close(fd);
		data = strdup("");
		size = 0;
	}

	File* file = malloc(sizeof(File));
	file->filename = strdup(filename);
	vector_init(&file->lines, sizeof(Line), destroy_line);
	file->dirty = 0;

	if (size == 0) {
		Line line = {0};
		line.text = strdup("");
		line.len = 0;
		vector_push(&file->lines, &line);
	} else {
		split_lines(file, data, size);
	}

	free(data);
	return file;
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

	for (size_t i = 0; i < file->lines.size; i++) {
		Line* line = vector_get(&file->lines, i);
		write(fd, line->text, line->len);
	}

	close(fd);

	file->dirty = 0;
	return 0;
}

void file_free(File* file) {
	if (!file) { return; }

	free(file->filename);
	vector_free(&file->lines);

	free(file);
}

// int file_insert_line(File* file, size_t index, const char* text) {
// 	if (index >= file->lines.size) {
// 		return -1;
// 	}

// 	Line line;
// 	line.len = strlen(text);
// 	line.text = malloc(line.len + 2); // +1 for '\n' and +1 for '\0'
// 	strcpy(line.text, text);

// 	if (line.text[line.len - 1] != '\n') {
// 		line.text[line.len] = '\n';
// 		line.len++;
// 		line.text[line.len] = '\0';
// 	}

// 	// line.dirty = 1;
// 	vector_insert(&file->lines, index, &line);

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


int file_insert_char(File* file, Cursor* cursor, char c) {
	if (cursor->y >= file->lines.size) {
		return -1;
	}

	Line* line = vector_get(&file->lines, cursor->y);
	char* new_text = malloc(line->len + 2); // + char + '\0'

	// copy until you reach the character
	memcpy(new_text, line->text, cursor->x);
	new_text[cursor->x] = c;

	// copy everything after the character
	memcpy(new_text + cursor->x + 1, line->text + cursor->x, line->len - cursor->x);
	new_text[line->len + 1] = '\0';

	free(line->text);
	line->text = new_text;
	line->len++;
	// line->dirty = 1;

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

	free(line->text);
	line->text = new_text;
	line->len--;
	// line->dirty = 1;

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
	char* tail_text = malloc(tail_len + 2);

	memcpy(tail_text, line->text + cursor->x, tail_len);

	if (tail_len == 0 || tail_text[tail_len - 1] != '\n') {
		tail_text[tail_len] = '\n';
		tail_len++;
	}

	tail_text[tail_len] = '\0';

	new_line.text = tail_text;
	new_line.len = tail_len;
	// new_line.dirty = 1;

	// head = original line
	size_t head_len = cursor->x;

	if (head_len == 0 || line->text[head_len - 1] != '\n') {
		head_len++;
	}

	char* head_text = malloc(head_len + 1);

	memcpy(head_text, line->text, cursor->x);
	head_text[cursor->x] = '\n';
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
	Line* curr = vector_get(&file->lines, cursor->y);

	size_t prev_len = prev->len;

	if (prev_len > 0 && prev->text[prev_len - 1] == '\n') {
		prev_len--;
	}

	size_t new_len = prev_len + curr->len;
	char* new_text = malloc(new_len + 1);

	memcpy(new_text, prev->text, prev_len);
	memcpy(new_text + prev_len, curr->text, curr->len);

	new_text[new_len] = '\0';

	free(prev->text);
	prev->text = new_text;
	prev->len = new_len;
	// prev->dirty = 1;

	vector_remove_and_destroy(&file->lines, cursor->y);

	file->dirty = 1;
	return 0;
}

void file_copy_line(File* file, Line* clipboard, size_t y) {
	if (clipboard) {
		free(clipboard->text);
		free(clipboard);
		clipboard = NULL;
	}

	if (y >= file->lines.size) {
		return;
	}

	Line* line = vector_get(&file->lines, y);
	clipboard = malloc(sizeof(Line));
	clipboard->len = line->len;
	clipboard->text = malloc(line->len + 1);
	memcpy(clipboard->text, line->text, line->len);
	clipboard->text[line->len] = '\0';
}

void file_paste_line(File* file, Line* clipboard, size_t y) {
	if (!clipboard) {
		return;
	}

	Line new_line;
	new_line.len = clipboard->len;
	new_line.text = malloc(clipboard->len + 1);
	memcpy(new_line.text, clipboard->text, clipboard->len);
	new_line.text[clipboard->len] = '\0';

	vector_insert(&file->lines, y, &new_line);
	file->dirty = 1;
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

void file_copy_selection(File* file, Line** clipboard, Selection* sel) {
	if (!sel->active) {
		return;
	}

	if (sel->start_x == sel->end_x && sel->start_y == sel->end_y) {
		return;
	}

	size_t x1, y1, x2, y2;
	selection_normalize(sel, &x1, &y1, &x2, &y2);

	if (*clipboard) {
		free((*clipboard)->text);
		free((*clipboard));
		*clipboard = NULL;
	}

	size_t total_len = 0;

	for (size_t y = y1; y <= y2; y++) {
		Line* line = vector_get(&file->lines, y);

		size_t start = (y == y1) ? x1 : 0;
		size_t end   = (y == y2) ? x2 : line->len;

		total_len += (end - start);
	}

	*clipboard = malloc(sizeof(Line));
	(*clipboard)->text = malloc(total_len + 1);
	(*clipboard)->len = total_len;

	size_t pos = 0;

	for (size_t y = y1; y <= y2; y++) {
		Line* line = vector_get(&file->lines, y);

		size_t start = (y == y1) ? x1 : 0;
		size_t end   = (y == y2) ? x2 : line->len;

		memcpy((*clipboard)->text + pos, line->text + start, end - start);
		pos += (end - start);
	}

	(*clipboard)->text[pos] = '\0';
}

void file_paste_clipboard(File* file, Line* clipboard, Cursor* c) {
	if (!clipboard || clipboard->len == 0) {
		return;
	}

	Line* start_line = vector_get(&file->lines, c->y);
	Line backup_line;
	backup_line.text = malloc(start_line->len + 1);

	if (!backup_line.text) {
		return;
	}

	memcpy(backup_line.text, start_line->text, start_line->len);
	backup_line.len = start_line->len;
	backup_line.text[start_line->len] = '\0';

	size_t start = 0;
	int new_lines = 0;
	int start_cx = c->x;
	int first = 1;
	size_t insert_y = c->y;

	vector_remove_and_destroy(&file->lines, c->y);
	printf("c->y: %ld, size: %ld\n", c->y, file->lines.size);

	for (size_t i = 0; i < clipboard->len; i++) {
		if (clipboard->text[i] == '\n') {
			new_lines++;
			size_t len = i - start + 1;

			Line line;

			if (first) {
				size_t prefix_size = start_cx;
				size_t total_len = len + prefix_size;
				line.text = malloc(total_len + 1);

				memcpy(line.text, backup_line.text, prefix_size);
				memcpy(line.text + prefix_size,
					   clipboard->text + start, len);

				line.len = total_len;
				line.text[total_len] = '\0';

				first = 0;
			} else {
				line.text = malloc(len + 1);
				memcpy(line.text, clipboard->text + start, len);
				line.text[len] = '\0';
				line.len = len;
			}

			printf("INSERT at y=%ld size(before)=%zu\n", c->y, file->lines.size);
			vector_insert(&file->lines, insert_y, &line);
			printf("INSERT at y=%ld size(before)=%zu\n", c->y, file->lines.size);
			insert_y++;

			start = i + 1;
		}
	}

	if (clipboard->len - start > 0) {
		if (new_lines == 0) {
			size_t prefix_size = start_cx;
			size_t suffix_size = backup_line.len - start_cx;
			size_t len = prefix_size + clipboard->len + suffix_size;

			Line line;
			line.text = malloc(len + 2);

			if (!line.text) {
				free(backup_line.text);
				return;
			}

			memcpy(line.text, backup_line.text, prefix_size);
			memcpy(line.text + prefix_size,
				   clipboard->text, clipboard->len);

			memcpy(line.text + prefix_size + clipboard->len, 
				backup_line.text + start_cx, suffix_size);
			line.text[len - 1] = '\n';
			line.text[len] = '\0';
			line.len = len;

			vector_insert(&file->lines, c->y, &line);

			c->x = prefix_size + clipboard->len;
			insert_y++;
		} else {
			size_t clipboard_remainder = (clipboard->len - start);
			size_t start_line_remainder = (backup_line.len - start_cx);
			size_t len = clipboard_remainder + start_line_remainder;

			Line line;
			line.text = malloc(len + 1);

			if (!line.text) {
				free(backup_line.text);
				return;
			}

			memcpy(line.text, clipboard->text + start, clipboard_remainder);

			memcpy(line.text + clipboard_remainder,
				backup_line.text + start_cx,
				start_line_remainder);

			line.text[len] = '\0';
			line.len = len;

			vector_insert(&file->lines, insert_y, &line);
			insert_y++;

			c->x = clipboard_remainder;
		}
	}

	if (new_lines > 0) {
		c->y = insert_y - 1;
	}

	free(backup_line.text);
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
	Line* last_line = vector_get(&file->lines, file->lines.size - 1);

	sel->active = 1;
	sel->start_x = 0;
	sel->end_x = last_line->len;
	sel->start_y = 0;
	sel->end_y = file->lines.size - 1;

	c->x = sel->end_x;
	c->y = sel->end_y;
}

int line_get_indent(Line* line) {
	int indent = 0;

	while ((size_t) indent < line->len && line->text[indent] == ' ') {
		indent++;
	}

	return indent;
}

int file_compute_indent_level(File* file, int up_to_line) {
	int depth = 0;

	for (int y = 0; y < up_to_line; y++) {
		Line* line = vector_get(&file->lines, (size_t) y);

		for (size_t i = 0; i < line->len; i++) {
			if (line->text[i] == '{') {
				depth++;
			}

			if (line->text[i] == '}') {
				depth--;
			}

			if (depth < 0) {
				depth = 0;
			}
		}
	}

	return depth;
}

void line_set_indent(Line* line, size_t indent) {
	size_t i = 0;

	while (i < line->len++ && line->text[i] == ' ') {
		i++;
	}

	size_t new_len = indent + (line->len - i);
	char* new_text = malloc(new_len + 1);

	memset(new_text, ' ', indent);
	memcpy(new_text + indent, line->text + i, line->len - i);

	new_text[new_len] = '\0';
	free(line->text);
	line->text = new_text;
	line->len = new_len;
}