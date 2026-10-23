#include <stdlib.h>
#include <stddef.h>
#include <string.h>

#include "util/debug/error.h"

void result_init(Result* result) {
	if (!result) {
		return;
	}

	result->type = ERROR_OK;
	result->reason = NULL;
}

int result_set_reason(Result* result, const char* reason) {
	if (!result) {
		return EIE_NOT_FATAL_ERROR;
	}

	if (!reason) {
		free(result->reason);
		result->reason = NULL;
		return EIE_OK;
	}

	size_t len = strlen(reason);
	char* new_reason = malloc(len + 1);

	if (!new_reason) {
		free(result->reason);
		result->reason = NULL;
		return EIE_NOT_FATAL_ERROR;
	}

	memcpy(new_reason, reason, len + 1);
	free(result->reason);

	result->reason = new_reason;

	return EIE_OK;
}

int result_set_error(Result* result, enum ErrorType type, const char* reason) {
	if (!result) {
		return EIE_OK;
	}

	result->type = type;
	return result_set_reason(result, reason);
}

void result_free(Result* result) {
	if (!result) {
		return;
	}

	free(result->reason);
	result->reason = NULL;
	result->type = ERROR_OK;
}

void result_ok(Result* result) {
	if (!result) {
		return;
	}
	
	free(result->reason);
	result->reason = NULL;

	result->type = ERROR_OK;
}

const char* result_reason(const Result* result) {
	if (!result || !result->reason) {
		return "no error message available";
	}

	return result->reason;
}
