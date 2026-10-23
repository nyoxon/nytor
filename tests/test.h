#ifndef NYTOR_TEST_H
#define NYTOR_TEST_H

#include <stdio.h>

#define CHECK(condition) \
	do { \
		if (!(condition)) { \
			fprintf(stderr, "%s:%d: check failed: %s\n", \
				__FILE__, __LINE__, #condition); \
			return 1; \
		} \
	} while (0)

int test_result(void);
int test_utf8_decoder(void);
int test_file_io(void);

#endif
