#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <dirent.h>
#include <sys/stat.h>
#include <assert.h>

#include "editor/core/cmd.h"
#include "editor/core/auto_complete.h"
#include "util/files.h"

static const AutoCompDummy dummy = {
	._ = '_'
};

static const AutoCompResult ResultSuccess = {
	.type = AUTOCOMP_RESULT_SUCCESS,
	.dummy = dummy
};

static const AutoCompResult ResultNoCandidates = {
	.type = AUTOCOMP_RESULT_NO_CANDIDATES,
	.dummy = dummy
};

static void vector_u32string_destroy(void* ptr) {
	u32string* string = (u32string*) ptr;

	u32string_free(string);
}

static int u32string_cmp(const void* a, const void* b) {
	const u32string* sa = a;
	const u32string* sb = b;

	int ret = (int) u32string_size(sa) - (int) u32string_size(sb);

	return ret;
}

static void get_cmd_candidates(Vector* candidates, const u32string* string) {
	size_t string_size = u32string_size(string);

	for (size_t i = 0; i < COMMANDS_COUNT; i++) {
		const char* cmd_name = COMMANDS[i].name;

		if (strlen(cmd_name) < string_size) {
			continue;
		}

		if (u32string_equalnu8(string, cmd_name, string_size)) {
			u32string candidate = u32string_from(cmd_name);

			vector_push(candidates, &candidate);
		}
	}
}

static void get_file_and_dirname
(
	const u32string* string,
	char** filename,
	char** dirname
) 
{
	if (!filename || !dirname || u32string_is_empty(string)) {
		return;
	}

	size_t string_size = u32string_size(string);

	uint32_t* last_slash = u32string_rchr(string, U'/');

	*filename = u32_to_utf8(
		(last_slash == NULL)
			? u32string_into_ptr_const(string)
			: last_slash + 1,

		(last_slash == NULL)
			? string_size
			: string_size - (last_slash -
				u32string_into_ptr((u32string*) string)) - 1 
	);

	*dirname = u32_to_utf8(
		u32string_into_ptr_const(string),

		(last_slash == NULL)
			? 0
			: last_slash - u32string_into_ptr((u32string*) string)		
	);

	last_slash = NULL;

	if (strlen(*dirname) == 0) {
		char* tmp = realloc(*dirname, 2);

		if (!tmp) {
			free(*dirname);
			free(*filename);

			*dirname = NULL;
			*filename = NULL;

			return;
		}

		*dirname = tmp;

		(*dirname)[0] = '.';
		(*dirname)[1] = '\0';
	}
}

static void get_file_candidates
(
	Vector* candidates, 
	const u32string* string
) 
{
	char* filename = NULL; char* dirname = NULL;
	get_file_and_dirname(string, &filename, &dirname);

	if (!filename || !dirname) {
		return;
	}

	DIR* dir = opendir(dirname);

	if (!dir) {
		free(filename);
		free(dirname);

		return;
	}

	struct dirent* entry;
	size_t filename_size = strlen(filename);

	while ((entry = readdir(dir)) != NULL) {
		if (strcmp(entry->d_name, ".") == 0 ||
			strcmp(entry->d_name, "..") == 0)
		{
			continue;
		} 

		if (strncmp(filename, entry->d_name, filename_size) == 0) {
			u32string name = u32string_new();

			if (u32string_size(string) > strlen(filename)) {
				u32string_appendu8(&name, dirname);
				u32string_push(&name, U'/');
			}

			u32string_appendu8(&name, entry->d_name);

			vector_push(candidates, &name);
		}
	}

	free(filename);
	free(dirname);

	closedir(dir);	
}

static Vector get_autocomp_candidates
(
	AutoCompType type,
	const u32string* string
) 
{
	Vector candidates;
	vector_init(&candidates, sizeof(u32string), vector_u32string_destroy);

	switch (type) {
	case AUTOCOMP_CMD:
		get_cmd_candidates(&candidates, string);
		break;

	case AUTOCOMP_FILE:
		get_file_candidates(&candidates, string);
		break;

	default:
		break;
	}

	qsort(
		candidates.data, 
		candidates.size, 
		candidates.elem_size, 
		u32string_cmp);

	return candidates;
}

static AutoCompResult autocomp
(
	const u32string* string,
	const Vector* candidates,
	Prompt* pt,
	size_t start,
	AutoCompType type
)
{
	const u32string* perfect_candidate =
		(const u32string*) vector_get_const(candidates, 0);

	size_t len;

	{
		const u32string* last = vector_get_const(
			candidates,
			candidates->size - 1);

		len = u32string_common_prefix(perfect_candidate, last);
	}

	if (candidates->size > 1) {
		int can_autocomplete = 1;

		if (u32string_equal(string, perfect_candidate)) {
			can_autocomplete = 0;
		}

		if (u32string_size(string) >= len) {
			can_autocomplete = 0;
		}

		if (can_autocomplete) {
			goto autocomplete;
		}

		return (AutoCompResult) {
			.type = AUTOCOMP_RESULT_SHOW_CANDIDATES,
			.candidates = *candidates
		};
	}


autocomplete:
	size_t buf_size = u32string_size(&pt->buf);

	/// FIXME
	assert(len >= (buf_size - start));

	pt->cursor.pos.x += len - (buf_size - start);

	u32string_remove_range(
		&pt->buf,
		start,
		buf_size
	);

	u32string_insert_range_raw(
		&pt->buf,
		start,
		u32string_into_ptr_const(perfect_candidate),
		len
	);

	if (candidates->size == 1) {
		uint32_t additional_char;

		if (type == AUTOCOMP_FILE) {
			char* name = u32string_into_u8(perfect_candidate);

			if (isdir(name)) {
				additional_char = U'/';
			}

			else {
				additional_char = U' ';
			}

			free(name);
		}

		else {
			additional_char = U' ';
		}

		u32string_push(&pt->buf, additional_char);
		pt->cursor.pos.x++;
	}

	return ResultSuccess;
}

AutoCompResult autocomp_cmd
(
	const u32string* string,
	Prompt* pt
) 
{
	Vector candidates = get_autocomp_candidates(
		AUTOCOMP_CMD,
		string);

	if (candidates.size == 0) {
		vector_free(&candidates);

		return ResultNoCandidates;
	}

	AutoCompResult result =  autocomp(
		string, 
		&candidates, 
		pt, 
		0, 
		AUTOCOMP_CMD);

	if (result.type == AUTOCOMP_RESULT_SUCCESS) {
		vector_free(&candidates);
	}

	return result;
}

AutoCompResult autocomp_file
(
	const u32string* string,
	Prompt* pt,
	size_t start
)
{
	Vector candidates = get_autocomp_candidates(
		AUTOCOMP_FILE,
		string
	);

	if (candidates.size == 0) {
		vector_free(&candidates);

		return ResultNoCandidates;
	}

	AutoCompResult result = autocomp(
		string, 
		&candidates, 
		pt, 
		start, 
		AUTOCOMP_FILE);

	if (result.type == AUTOCOMP_RESULT_SUCCESS) {
		vector_free(&candidates);
	}

	return result;
}