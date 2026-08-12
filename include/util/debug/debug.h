#ifndef DEBUG_H
#define DEBUG_H

/*
DEBUGGING in this program occurs writing on a external file (a log file)

to active debug mode, run the program using --debug=[PATH_TO_DEBUG_FILE]
*/

#include <stddef.h>
#include <stdarg.h>

typedef struct {
	const char* filename;
	int fd;
} Log;

void log_init(Log* log, const char* filename);

// variadic function
void log_write(const Log* log, const char* fmt, ...);

#endif