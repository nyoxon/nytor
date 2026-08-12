#include <stdio.h>
#include <ctype.h>
#include <string.h>
#include <stdlib.h>
#include <fcntl.h>
#include <unistd.h>
#include <libgen.h>
#include <limits.h>
#include <sys/stat.h>

#include "util/files.h"

char* trim(char* s) {
	while (isspace((unsigned char) *s)) {
		s++;
	}

	if (*s == '\0') {
		return s;
	}

	char* end = s + strlen(s) - 1;

	while (end > s && isspace((unsigned char) *end)) {
		end--;
	}

	end[1] = '\0';

	return s;
}

void get_filename_after_last_slash(char* out, const char* filename) {
	const char* base = strrchr(filename, '/');

	if (base) {
		base++;
	}

	else {
		base = filename;
	}

	strcpy(out, base);
}

int has_extension(const char* path, const char* ext) {
	char* dot = strrchr(path, '.');

	if (!dot) {
		return 0;
	}

	if (strcmp(dot, ext) == 0) {
		return 1;
	} 

	return 0;
}

int make_theme_path
(
	char* out, 
	size_t size, 
	const char* path
) 
{
	if (!out || !path || strcmp(path, "") == 0) {
		return -1;
	}

	const char* home = getenv("HOME");

	if (!home) {
		return -1;
	}

	int n = snprintf(
		out,
		size,
		"%s/.config/nytor/themes/%s",
		home,
		path
	);

	if (n < 0 || (size_t) n >= size) {
		return -1;
	}

	return 0;
}

int make_plugin_language_path
(
	char* out, 
	size_t size, 
	const char* path
) 
{
	if (!out || !path || strcmp(path, "") == 0) {
		return -1;
	}

	const char* home = getenv("HOME");

	if (!home) {
		return -1;
	}

	int n = snprintf(
		out,
		size,
		"%s/.config/nytor/plugins/languages/%s.so",
		home,
		path
	);

	if (n < 0 || (size_t) n >= size) {
		return -1;
	}

	return 0;
}

int file_exists(const char* filename) {
	int fd = open(filename, O_RDONLY);

	if (fd == -1) {
		return 0;
	}

	else {
		close(fd);
		return 1;
	}
}

char* absolute_parent_directory(const char* filename) {
	char* copy = strdup(filename);
	char* dir = dirname(copy);

	char absolute[PATH_MAX_LENGTH];

	if (realpath(dir, absolute) == NULL) {
		free(copy);
		return NULL;
	}

	free(copy);

	return strdup(absolute);
}

int parse_quoted(const char* src, char* dst, size_t dst_size) {
	if (*src != '"') {
		return 0;
	}

	src++;

	size_t i = 0;

	while (*src) {
		if (*src == '\\') {
			src++;

			if (*src == '\0') {
				return 0;
			}

			switch (*src) {
			case '"':
				dst[i++] = '"';
				break;

			case '\\':
				dst[i++] = '\\';
				break;

			default:
				return 0;
			}

			src++;
		}

		else if (*src == '"') {
			dst[i] = '\0';
			return 1;
		}

		else {
			dst[i++] = *src++;
		}

		if (i + 1 >= dst_size) {
			return 0;
		}
	}

	return 0;
}

int strjoin
(
	char* strings[],
	size_t count,
	char** out,
	const char* separator
)
{
	if (!out) {
		return -1;
	}

	size_t sep_len = strlen(separator);
	size_t total = 1; // '\0'

	for (size_t i = 0; i < count; i++) {
		total += strlen(strings[i]);

		if (i + 1 < count) {
			total += sep_len;
		}
	}

	*out = malloc(total);

	if (!(*out)) {
		*out = NULL;
		return -1;
	}

	char* p = *out;

	for (size_t i = 0; i < count; i++) {
		size_t len = strlen(strings[i]);

		memcpy(p, strings[i], len);
		p += len;

		if (i + 1 < count) {
			memcpy(p, separator, sep_len);
			p += sep_len;
		}
	}

	*p = '\0';

	return 0;
}


int isdir(const char* path) {
	struct stat st;

	if (stat(path, &st) == -1) {
		return 0;
	}

	if (S_ISDIR(st.st_mode)) {
		return 1;
	}

	return 0;
}

double timespec_ms
(
	const struct timespec* start,
	const struct timespec* end
)
{
	return (end->tv_sec - start->tv_sec) * 1000.0 +
		   (end->tv_nsec - start->tv_nsec) / 1e6;
}