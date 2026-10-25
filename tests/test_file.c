#define _POSIX_C_SOURCE 200809L

#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include "file/file.h"
#include "test.h"

static int write_bytes(
	const char* path, 
	const char* data, 
	size_t size
) 
{
	FILE* stream = fopen(path, "wb");

	if (!stream) {
		return -1;
	}

	int failed = fwrite(data, 1, size, stream) != size;

	if (fclose(stream) != 0) {
		failed = 1;
	}

	return failed ? -1 : 0;
}

static int file_contains(
	const char* path, 
	const char* expected
) 
{
	FILE* stream = fopen(path, "rb");

	if (!stream) {
		return 0;
	}

	char actual[256];
	size_t size = fread(actual, 1, sizeof(actual) - 1, stream);
	int failed = ferror(stream);

	if (fclose(stream) != 0) {
		failed = 1;
	}

	actual[size] = '\0';

	return !failed && strcmp(actual, expected) == 0;
}

static int line_equals(
	File* file, 
	size_t y, 
	const char* expected
) 
{
	u32string* line = file_get_line_text(file, y);
	if (!line) {
		return 0;
	}

	char* actual = u32string_into_u8(line);
	if (!actual) {
		return 0;
	}

	int equal = strcmp(actual, expected) == 0;
	free(actual);
	return equal;
}

int test_file_io(void) {
	char directory[] = "/tmp/nytor-tests-XXXXXX";
	CHECK(mkdtemp(directory) != NULL);

	char existing_path[256];
	char empty_path[256];
	char missing_path[256];
	char invalid_path[256];
	CHECK(snprintf(existing_path, sizeof(existing_path), "%s/existing.txt", directory) > 0);
	CHECK(snprintf(empty_path, sizeof(empty_path), "%s/empty.txt", directory) > 0);
	CHECK(snprintf(missing_path, sizeof(missing_path), "%s/missing.txt", directory) > 0);
	CHECK(snprintf(invalid_path, sizeof(invalid_path), "%s/invalid.txt", directory) > 0);

	const char initial[] = "alpha\n" "\xce\xb2" "eta";
	CHECK(write_bytes(existing_path, initial, sizeof(initial) - 1) == 0);
	CHECK(write_bytes(empty_path, "", 0) == 0);
	const char invalid_utf8[] = { (char) 0xc3, '(' };
	CHECK(write_bytes(invalid_path, invalid_utf8, sizeof(invalid_utf8)) == 0);

	FileOptions options = {
		.path = existing_path,
		.tab_size = 4,
		.use_spaces = 1
	};
	File file = {0};
	Result result;
	result_init(&result);

	CHECK(file_open(&file, &options, &result) == EIE_OK);
	CHECK(result.type == ERROR_OK && result.reason == NULL);
	CHECK(file_num_lines(&file) == 2);
	CHECK(line_equals(&file, 0, "alpha"));
	CHECK(line_equals(&file, 1, "\xce\xb2" "eta"));


	// insert char
	CHECK(file_insert_char(&file, (Position) { 5, 0 }, U'!') == EIE_OK);
	CHECK(file_save(&file, &result) == EIE_OK);
	CHECK(result.type == ERROR_OK && result.reason == NULL);
	CHECK(file_contains(existing_path, "alpha!\n" "\xce\xb2" "eta"));


	// save not dirty
	CHECK(file_save(&file, &result) == EIE_NOT_AN_ERROR);
	CHECK(result.type == ERROR_OK && result.reason == NULL);


	// delete char
	CHECK(file_delete_char(&file, (Position) { 6, 0 }) == EIE_OK);
	CHECK(file_save(&file, &result) == EIE_OK);
	CHECK(result.type == ERROR_OK && result.reason == NULL);
	CHECK(file_contains(existing_path, "alpha\n" "\xce\xb2" "eta"));


	// insert newline at the end
	CHECK(file_insert_newline(&file, (Position) { 5, 0 }, &result) == EIE_OK);
	CHECK(result.type == ERROR_OK && result.reason == NULL);
	CHECK(file_save(&file, &result) == EIE_OK);
	CHECK(result.type == ERROR_OK && result.reason == NULL);
	CHECK(file_num_lines(&file) == 3);
	CHECK(file_contains(existing_path, "alpha\n" "\n" "\xce\xb2" "eta"));


	// merge
	CHECK(file_merge_lines(&file, 1, &result) == EIE_OK);
	CHECK(result.type == ERROR_OK && result.reason == NULL);
	CHECK(file_save(&file, &result) == EIE_OK);
	CHECK(result.type == ERROR_OK && result.reason == NULL);
	CHECK(file_num_lines(&file) == 2);
	CHECK(file_contains(existing_path, "alpha\n" "\xce\xb2" "eta"));


	// insert newline in the middle
	CHECK(file_insert_newline(&file, (Position) { 2, 0 }, &result) == EIE_OK);
	CHECK(result.type == ERROR_OK && result.reason == NULL);
	CHECK(file_save(&file, &result) == EIE_OK);
	CHECK(result.type == ERROR_OK && result.reason == NULL);
	CHECK(file_num_lines(&file) == 3);
	CHECK(file_contains(existing_path, "al\n" "pha\n" "\xce\xb2" "eta"));


	// merge again
	CHECK(file_merge_lines(&file, 1, &result) == EIE_OK);
	CHECK(result.type == ERROR_OK && result.reason == NULL);
	CHECK(file_save(&file, &result) == EIE_OK);
	CHECK(result.type == ERROR_OK && result.reason == NULL);
	CHECK(file_num_lines(&file) == 2);
	CHECK(file_contains(existing_path, "alpha\n" "\xce\xb2" "eta"));


	// merge at the beginning
	CHECK(file_merge_lines(&file, 0, &result) == EIE_NOT_FATAL_ERROR);
	CHECK(result.type == ERROR_INDEX_OUT_OF_BOUNDS);


	// merge at the end
	CHECK(file_merge_lines(&file, 2, &result) == EIE_NOT_FATAL_ERROR);
	CHECK(result.type == ERROR_INDEX_OUT_OF_BOUNDS);



	// move line down
	CHECK(file_move_line_down(&file, 0, &result) == EIE_OK);
	CHECK(result.type == ERROR_OK && result.reason == NULL);
	CHECK(file_save(&file, &result) == EIE_OK);
	CHECK(result.type == ERROR_OK && result.reason == NULL);
	CHECK(file_num_lines(&file) == 2);
	CHECK(file_contains(existing_path, "\xce\xb2" "eta\n" "alpha"));


	// move line up
	CHECK(file_move_line_up(&file, 1, &result) == EIE_OK);
	CHECK(result.type == ERROR_OK && result.reason == NULL);
	CHECK(file_save(&file, &result) == EIE_OK);
	CHECK(result.type == ERROR_OK && result.reason == NULL);
	CHECK(file_num_lines(&file) == 2);
	CHECK(file_contains(existing_path, "alpha\n" "\xce\xb2" "eta"));


	// try to move the first line up
	CHECK(file_move_line_up(&file, 0, &result) == EIE_NOT_AN_ERROR);
	CHECK(!file.dirty);
	CHECK(file_contains(existing_path, "alpha\n" "\xce\xb2" "eta"));

	// try to move the last line down
	CHECK(file_move_line_down(&file, 1, &result) == EIE_NOT_AN_ERROR);
	CHECK(!file.dirty);
	CHECK(file_contains(existing_path, "alpha\n" "\xce\xb2" "eta"));


	// indent a line using spaces
	CHECK(file_indent_a_line(&file, 0, &result) == EIE_OK);
	CHECK(result.type == ERROR_OK && result.reason == NULL);
	CHECK(file_save(&file, &result) == EIE_OK);
	CHECK(result.type == ERROR_OK && result.reason == NULL);
	CHECK(file_num_lines(&file) == 2);
	CHECK(file_contains(existing_path, "    alpha\n" "\xce\xb2" "eta"));


	// unindent a line using spaces
	CHECK(file_unindent_a_line(&file, 0, &result) == EIE_OK);
	CHECK(result.type == ERROR_OK && result.reason == NULL);
	CHECK(file_save(&file, &result) == EIE_OK);
	CHECK(result.type == ERROR_OK && result.reason == NULL);
	CHECK(file_num_lines(&file) == 2);
	CHECK(file_contains(existing_path, "alpha\n" "\xce\xb2" "eta"));


	// insert n > tab_size
	CHECK(file_insert_char(&file, (Position) { 0, 0 }, U' ') == EIE_OK);
	CHECK(file_insert_char(&file, (Position) { 0, 0 }, U' ') == EIE_OK);
	CHECK(file_insert_char(&file, (Position) { 0, 0 }, U' ') == EIE_OK);
	CHECK(file_insert_char(&file, (Position) { 0, 0 }, U' ') == EIE_OK);
	CHECK(file_insert_char(&file, (Position) { 0, 0 }, U' ') == EIE_OK);
	CHECK(file_save(&file, &result) == EIE_OK);
	CHECK(result.type == ERROR_OK && result.reason == NULL);
	CHECK(file_contains(existing_path, "     alpha\n" "\xce\xb2" "eta"));


	// unindent a line using spaces
	CHECK(file_unindent_a_line(&file, 0, &result) == EIE_OK);
	CHECK(result.type == ERROR_OK && result.reason == NULL);
	CHECK(file_save(&file, &result) == EIE_OK);
	CHECK(result.type == ERROR_OK && result.reason == NULL);
	CHECK(file_num_lines(&file) == 2);
	CHECK(file_contains(existing_path, " alpha\n" "\xce\xb2" "eta"));


	// unindent a line using spaces
	CHECK(file_unindent_a_line(&file, 0, &result) == EIE_OK);
	CHECK(result.type == ERROR_OK && result.reason == NULL);
	CHECK(file_save(&file, &result) == EIE_OK);
	CHECK(result.type == ERROR_OK && result.reason == NULL);
	CHECK(file_num_lines(&file) == 2);
	CHECK(file_contains(existing_path, "alpha\n" "\xce\xb2" "eta"));


	file.use_spaces = 0;


	// indent a line tabs
	CHECK(file_indent_a_line(&file, 0, &result) == EIE_OK);
	CHECK(result.type == ERROR_OK && result.reason == NULL);
	CHECK(file_save(&file, &result) == EIE_OK);
	CHECK(result.type == ERROR_OK && result.reason == NULL);
	CHECK(file_num_lines(&file) == 2);
	CHECK(file_contains(existing_path, "\talpha\n" "\xce\xb2" "eta"));


	// unindent a line tabs
	CHECK(file_unindent_a_line(&file, 0, &result) == EIE_OK);
	CHECK(result.type == ERROR_OK && result.reason == NULL);
	CHECK(file_save(&file, &result) == EIE_OK);
	CHECK(result.type == ERROR_OK && result.reason == NULL);
	CHECK(file_num_lines(&file) == 2);
	CHECK(file_contains(existing_path, "alpha\n" "\xce\xb2" "eta"));


	Selection sel;
	selection_start(&sel, POS_ZERO);
	sel.start = (Position) { 5, 0 };
	sel.end = (Position) { 0, 1 };


	// indent a selection using tabs
	CHECK(file_indent_selection(&file, &sel, &result) == EIE_OK);
	CHECK(result.type == ERROR_OK && result.reason == NULL);
	CHECK(file_save(&file, &result) == EIE_OK);
	CHECK(result.type == ERROR_OK && result.reason == NULL);
	CHECK(file_num_lines(&file) == 2);
	CHECK(file_contains(existing_path, "\talpha\n" "\t\xce\xb2" "eta"));


	CHECK(position_equal(&sel.end, &(Position) { 1, 1 }));


	// unindent a selection using tabs
	CHECK(file_unindent_selection(&file, &sel, &result) == EIE_OK);
	CHECK(result.type == ERROR_OK && result.reason == NULL);
	CHECK(file_save(&file, &result) == EIE_OK);
	CHECK(result.type == ERROR_OK && result.reason == NULL);
	CHECK(file_num_lines(&file) == 2);
	CHECK(file_contains(existing_path, "alpha\n" "\xce\xb2" "eta"));


	CHECK(position_equal(&sel.end, &(Position) { 0, 1 }));


	file.use_spaces = 1;


	// indent a selection using spaces
	CHECK(file_indent_selection(&file, &sel, &result) == EIE_OK);
	CHECK(result.type == ERROR_OK && result.reason == NULL);
	CHECK(file_save(&file, &result) == EIE_OK);
	CHECK(result.type == ERROR_OK && result.reason == NULL);
	CHECK(file_num_lines(&file) == 2);
	CHECK(file_contains(existing_path, "    alpha\n" "    \xce\xb2" "eta"));


	CHECK(position_equal(&sel.end, &(Position) { 4, 1 }));


	// unindent a selection using spaces
	CHECK(file_unindent_selection(&file, &sel, &result) == EIE_OK);
	CHECK(result.type == ERROR_OK && result.reason == NULL);
	CHECK(file_save(&file, &result) == EIE_OK);
	CHECK(result.type == ERROR_OK && result.reason == NULL);
	CHECK(file_num_lines(&file) == 2);
	CHECK(file_contains(existing_path, "alpha\n" "\xce\xb2" "eta"));


	CHECK(position_equal(&sel.end, &(Position) { 0, 1 }));


	const char comment_fmt[] = "//";


	// comment a line
	CHECK(file_comment_line(&file, 0, comment_fmt, NULL) == EIE_OK);
	CHECK(result.type == ERROR_OK && result.reason == NULL);
	CHECK(file_save(&file, &result) == EIE_OK);
	CHECK(result.type == ERROR_OK && result.reason == NULL);
	CHECK(file_num_lines(&file) == 2);
	CHECK(file_contains(existing_path, "// alpha\n" "\xce\xb2" "eta"));	


	// uncomment a line
	CHECK(file_comment_line(&file, 0, comment_fmt, NULL) == EIE_OK);
	CHECK(result.type == ERROR_OK && result.reason == NULL);
	CHECK(file_save(&file, &result) == EIE_OK);
	CHECK(result.type == ERROR_OK && result.reason == NULL);
	CHECK(file_num_lines(&file) == 2);
	CHECK(file_contains(existing_path, "alpha\n" "\xce\xb2" "eta"));



	// comment without space
	CHECK(file_insert_char(&file, (Position) { 0, 0 }, U'/') == EIE_OK);
	CHECK(file_insert_char(&file, (Position) { 0, 0 }, U'/') == EIE_OK);
	CHECK(file_save(&file, &result) == EIE_OK);
	CHECK(result.type == ERROR_OK && result.reason == NULL);
	CHECK(file_contains(existing_path, "//alpha\n" "\xce\xb2" "eta"));


	// uncomment a line
	CHECK(file_comment_line(&file, 0, comment_fmt, NULL) == EIE_OK);
	CHECK(result.type == ERROR_OK && result.reason == NULL);
	CHECK(file_save(&file, &result) == EIE_OK);
	CHECK(result.type == ERROR_OK && result.reason == NULL);
	CHECK(file_num_lines(&file) == 2);
	CHECK(file_contains(existing_path, "alpha\n" "\xce\xb2" "eta"));



	// comment a selection
	CHECK(file_comment_selection(&file, &sel, comment_fmt, 67, NULL) == EIE_OK);
	CHECK(result.type == ERROR_OK && result.reason == NULL);
	CHECK(file_save(&file, &result) == EIE_OK);
	CHECK(result.type == ERROR_OK && result.reason == NULL);
	CHECK(file_num_lines(&file) == 2);
	CHECK(file_contains(existing_path, "// alpha\n" "// \xce\xb2" "eta"));


	CHECK(position_equal(&sel.start, &(Position) { 8, 0 }));
	CHECK(position_equal(&sel.end, &(Position) { 3, 1 }));



	// uncomment a selection
	CHECK(file_comment_selection(&file, &sel, comment_fmt, 67, NULL) == EIE_OK);
	CHECK(result.type == ERROR_OK && result.reason == NULL);
	CHECK(file_save(&file, &result) == EIE_OK);
	CHECK(result.type == ERROR_OK && result.reason == NULL);
	CHECK(file_num_lines(&file) == 2);
	CHECK(file_contains(existing_path, "alpha\n" "\xce\xb2" "eta"));


	CHECK(position_equal(&sel.start, &(Position) { 5, 0 }));
	CHECK(position_equal(&sel.end, &(Position) { 0, 1 }));


	// one line already commented
	CHECK(file_insert_char(&file, (Position) { 0, 0 }, U'/') == EIE_OK);
	CHECK(file_insert_char(&file, (Position) { 0, 0 }, U'/') == EIE_OK);
	CHECK(file_save(&file, &result) == EIE_OK);
	CHECK(result.type == ERROR_OK && result.reason == NULL);
	CHECK(file_contains(existing_path, "//alpha\n" "\xce\xb2" "eta"));


	// comment a selection with one line already commented
	// this line will be commented anyway
	CHECK(file_comment_selection(&file, &sel, comment_fmt, 67, NULL) == EIE_OK);
	CHECK(result.type == ERROR_OK && result.reason == NULL);
	CHECK(file_save(&file, &result) == EIE_OK);
	CHECK(result.type == ERROR_OK && result.reason == NULL);
	CHECK(file_num_lines(&file) == 2);
	CHECK(file_contains(existing_path, "// //alpha\n" "// \xce\xb2" "eta"));


	CHECK(position_equal(&sel.start, &(Position) { 8, 0 }));
	CHECK(position_equal(&sel.end, &(Position) { 3, 1 }));


	// comment a commented selection
	CHECK(file_comment_selection(&file, &sel, comment_fmt, 67, NULL) == EIE_OK);
	CHECK(result.type == ERROR_OK && result.reason == NULL);
	CHECK(file_save(&file, &result) == EIE_OK);
	CHECK(result.type == ERROR_OK && result.reason == NULL);
	CHECK(file_num_lines(&file) == 2);
	CHECK(file_contains(existing_path, "//alpha\n" "\xce\xb2" "eta"));


	CHECK(position_equal(&sel.start, &(Position) { 5, 0 }));
	CHECK(position_equal(&sel.end, &(Position) { 0, 1 }));


	// uncomment a line
	CHECK(file_comment_line(&file, 0, comment_fmt, NULL) == EIE_OK);
	CHECK(result.type == ERROR_OK && result.reason == NULL);
	CHECK(file_save(&file, &result) == EIE_OK);
	CHECK(result.type == ERROR_OK && result.reason == NULL);
	CHECK(file_num_lines(&file) == 2);
	CHECK(file_contains(existing_path, "alpha\n" "\xce\xb2" "eta"));



	// select a line, without the next
	CHECK(file_select_line(&file, 0, &sel, 0) == EIE_OK);
	CHECK(position_equal(&sel.start, &(Position) { 0, 0 }));
	CHECK(position_equal(&sel.end, &(Position) { 5, 0 }));


	// select a line, with the next
	CHECK(file_select_line(&file, 0, &sel, 1) == EIE_OK);
	CHECK(position_equal(&sel.start, &(Position) { 0, 0 }));
	CHECK(position_equal(&sel.end, &(Position) { 0, 1 }));


	// select the last line, with the next
	CHECK(file_select_line(&file, 1, &sel, 1) == EIE_OK);
	CHECK(position_equal(&sel.start, &(Position) { 0, 1 }));
	CHECK(position_equal(&sel.end, &(Position) { 4, 1 }));



	// select the entire file
	CHECK(file_select_all_file(&file, &sel) == EIE_OK);
	CHECK(position_equal(&sel.start, &(Position) { 0, 0 }));
	CHECK(position_equal(&sel.end, &(Position) { 4, 1 }));



	// copy selection
	Clipboard cb = clipboard_new();

	CHECK(file_copy_selection(&file, &cb, &sel, &result) == EIE_OK);
	CHECK(result.type == ERROR_OK && result.reason == NULL);

	char* cb_text = u32string_into_u8(&cb.text);
	CHECK(strcmp(cb_text, "alpha\n" "\xce\xb2" "eta") == 0);

	free(cb_text);


	// paste clipboard

	Cursor c;
	c.pos = (Position) {2, 0};

	CHECK(file_paste_clipboard(&file, &cb, &c, &result) == EIE_OK);
	CHECK(result.type == ERROR_OK && result.reason == NULL);
	CHECK(file_save(&file, &result) == EIE_OK);
	CHECK(result.type == ERROR_OK && result.reason == NULL);
	CHECK(file_contains(
		existing_path, 
		"alalpha\n" "\xce\xb2" "etapha\n" "\xce\xb2" "eta")
	);

	cb_text = u32string_into_u8(&cb.text);
	CHECK(strcmp(cb_text, "alpha\n" "\xce\xb2" "eta") == 0);

	free(cb_text);

	clipboard_free(&cb);


	// delete selection
	sel.active = 1;
	sel.start = (Position) { 2, 0 };
	sel.end = (Position) { 4, 1 };

	CHECK(file_delete_selection(&file, &sel, &result) == EIE_OK);
	CHECK(result.type == ERROR_OK && result.reason == NULL);
	CHECK(file_save(&file, &result) == EIE_OK);
	CHECK(result.type == ERROR_OK && result.reason == NULL);
	CHECK(file_contains(existing_path, "alpha\n" "\xce\xb2" "eta"));


	CHECK(unlink(existing_path) == 0);
	CHECK(file_sync(&file, &result) < 0);
	CHECK(result.type == ERROR_FILE_DOES_NOT_EXIST);
	CHECK(line_equals(&file, 0, "alpha"));
	file_free(&file);

	options.path = empty_path;
	File empty_file = {0};
	CHECK(file_open(&empty_file, &options, &result) == EIE_OK);
	CHECK(file_num_lines(&empty_file) == 1);
	CHECK(line_equals(&empty_file, 0, ""));
	file_free(&empty_file);

	options.path = missing_path;
	File missing_file = {0};
	CHECK(file_open(&missing_file, &options, NULL) == EIE_OK);
	CHECK(missing_file.dirty);
	CHECK(access(missing_path, F_OK) < 0 && errno == ENOENT);
	CHECK(file_save(&missing_file, NULL) == EIE_OK);
	CHECK(access(missing_path, F_OK) == 0);
	file_free(&missing_file);

	options.path = invalid_path;
	File invalid_file = {0};
	CHECK(file_open(&invalid_file, &options, &result) == EIE_FATAL_ERROR);
	CHECK(result.type == ERROR_INVALID_FILE);
	result_free(&result);

	CHECK(unlink(empty_path) == 0);
	CHECK(unlink(missing_path) == 0);
	CHECK(unlink(invalid_path) == 0);
	CHECK(rmdir(directory) == 0);

	return 0;
}
