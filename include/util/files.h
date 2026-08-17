#ifndef FILES_H
#define FILES_H

#include <time.h>

#define PATH_MAX_LENGTH 1024

char* trim(char* s);
int has_extension(const char* path, const char* ext);

int make_theme_path
(
	char* out, 
	size_t size, 
	const char* path
);

int make_plugin_language_path
(
	char* out, 
	size_t size, 
	const char* path
);

int make_config_path
(
	char* out,
	size_t size
);

void get_filename_after_last_slash(char* out, const char* filename);

int file_exists(const char* filename);

char* absolute_parent_directory(const char* filename);

int parse_quoted(const char* src, char* dst, size_t dst_size);

int strjoin
(
	char* strings[],
	size_t count,
	char** out,
	const char* separator
);

int isdir(const char* path);

double timespec_ms
(
	const struct timespec* start,
	const struct timespec* end
);

#endif