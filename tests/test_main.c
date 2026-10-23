#include "test.h"

int main(void) {
	if (test_result() != 0) {
		return 1;
	}

	fprintf(stdout, "PASS Result helpers\n");

	if (test_utf8_decoder() != 0) {
		return 1;
	}

	fprintf(stdout, "PASS strict UTF-8 decoding\n");

	if (test_file_io() != 0) {
		return 1;
	}
	
	fprintf(stdout, "PASS file I/O and Result contract\n");

	return 0;
}
