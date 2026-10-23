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

	CHECK(file_insert_char(&file, (Position) { 5, 0 }, U'!') == EIE_OK);
	CHECK(file_save(&file, &result) == EIE_OK);
	CHECK(result.type == ERROR_OK && result.reason == NULL);
	CHECK(file_contains(existing_path, "alpha!\n" "\xce\xb2" "eta"));

	CHECK(file_save(&file, &result) == EIE_NOT_AN_ERROR);
	CHECK(result.type == ERROR_OK && result.reason == NULL);

	CHECK(unlink(existing_path) == 0);
	CHECK(file_sync(&file, &result) < 0);
	CHECK(result.type == ERROR_FILE_DOES_NOT_EXIST);
	CHECK(line_equals(&file, 0, "alpha!"));
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
