#include "error.h"
#include <stdlib.h>
#include <stddef.h>
#include <string.h>

// PRE: result != NULL for all functions

int result_set_reason(Result* result, const char* reason) {
	if (result->reason) {
		free(result->reason);
	}

	size_t len = strlen(reason);
	result->reason = malloc(len + 1);

	if (!result->reason) {
		return EIE_NOT_FATAL_ERROR;
	}

	memcpy(result->reason, reason, len);
	result->reason[len] = '\0';

	return 0;
}

void result_free(Result* result) {
	if (result->reason) {
		free(result->reason);
		result->reason = NULL;
	}
}

void result_ok(Result* result) {
	if (result->reason) {
		free(result->reason);
		result->reason = NULL;
	}

	result->type = ERROR_OK;
}