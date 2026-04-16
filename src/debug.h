#ifndef DEBUG_H
#define DEBUG_H

#include <stddef.h>
#include <stdarg.h>

typedef struct {
	const char* filename;
	int fd;
} Log;

void log_init(Log* log, const char* filename);
void log_write(const Log* log, const char* fmt, ...);

#endif