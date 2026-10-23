#include <string.h>

#include "test.h"
#include "util/debug/error.h"

int test_result(void) {
	Result result;
	result_init(&result);

	CHECK(result.type == ERROR_OK);
	CHECK(result.reason == NULL);

	CHECK(result_set_error(
		&result,
		ERROR_FILE_DOES_NOT_EXIST,
		"missing file"
	) == EIE_OK);
	
	CHECK(result.type == ERROR_FILE_DOES_NOT_EXIST);
	CHECK(strcmp(result_reason(&result), "missing file") == 0);

	CHECK(result_set_error(&result, ERROR_MALLOC, NULL) == EIE_OK);
	CHECK(result.type == ERROR_MALLOC);
	CHECK(result.reason == NULL);
	CHECK(strcmp(result_reason(&result), "no error message available") == 0);

	CHECK(result_set_error(NULL, ERROR_MALLOC, "ignored") == EIE_OK);
	CHECK(result_reason(NULL) != NULL);

	result_ok(&result);
	CHECK(result.type == ERROR_OK);
	CHECK(result.reason == NULL);

	CHECK(result_set_reason(&result, "temporary") == EIE_OK);
	CHECK(result.reason != NULL);
	CHECK(result_set_reason(&result, NULL) == EIE_OK);
	CHECK(result.reason == NULL);

	result_free(&result);
	result_free(&result);
	CHECK(result.type == ERROR_OK);
	CHECK(result.reason == NULL);

	return 0;
}
