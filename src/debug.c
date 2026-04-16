#include "debug.h"
#include <fcntl.h>
#include <unistd.h>
#include <string.h>
#include <stdio.h>

void log_init(Log* log, const char* filename) {
	log->fd = open(filename, O_WRONLY | O_TRUNC | O_CREAT, 0644);
	log->filename = filename;
}

void log_write(const Log* log, const char* fmt, ...) {
	if (log->fd < 0) {
		return;
	}

	char buffer[1024];

	va_list args;
	va_start(args, fmt);

	int len = vsnprintf(buffer, sizeof(buffer), fmt, args);

	va_end(args);

	if (len > 0) {
		write(log->fd, buffer, len);
	}

	write(log->fd, "\n", 1);
}