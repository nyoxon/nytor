#ifndef ERROR_H
#define ERROR_H

// Contract for functions that return an int and optionally accept Result *:

// - The int is the operation status and remains authoritative for control
//   flow. EIE_OK means success; EIE_NOT_AN_ERROR means no-op; negative values
//   mean failure. EIE_FATAL_ERROR must stop or propagate the operation, while
//   EIE_NOT_FATAL_ERROR marks a recoverable failure.
// - Result * is optional diagnostic output for this operation. When supplied,
//   it is ERROR_OK with reason == NULL for success or no-op, and has a
//   non-ERROR_OK type for failure. reason is owned and NUL-terminated when
//   present; its allocation is best-effort, so it may be NULL on failure.
// - The caller initializes Result with result_init() before first use.
// - A NULL Result * must not change operation status or behavior.
// - The caller owns Result.reason and releases it with result_free() or
//   replaces it through the Result helpers.
//
// Example:
//   Result result;
//   result_init(&result);
//   int status = file_save(file, &result);
//   if (status < 0) { /* handle result.type and result_reason(&result) */ }
//   result_free(&result);
// Pass NULL instead when the caller only needs the integer status.

// Some callers and operations have not been migrated to this contract yet.

#define EIE_FATAL_ERROR -1 // failure that must stop/propagate the operation
#define EIE_NOT_FATAL_ERROR -2 // recoverable failure
#define EIE_NOT_AN_ERROR 1 // operation was a no-op
#define EIE_OK 0 // operation succeeded

enum ErrorType {
	ERROR_OK = 2000, // not a error;
	ERROR_INVALID_ARGUMENT, // and invalid argument has been passed to a function
	ERROR_FILE_HANDLE, // reading, writing or creating a file
	ERROR_FILE_DOES_NOT_EXIST,
	ERROR_MALLOC,
	ERROR_SYS_READ,
	ERROR_NULL_POINTER,
	ERROR_INDEX_OUT_OF_BOUNDS,
	ERROR_VECTOR,
	ERROR_STATIC,
	ERROR_INVALID_FILE,
	ERROR_SYS_SEEK, // seeking while reading a file
	ERROR_SYS_WRITE, // writing a saved file
	ERROR_FILE_TOO_LARGE, // file cannot fit in the editor's data structures
	ERROR_FILE_READ_ONLY, // save was refused because the file is read-only
	ERROR_FILE_UNNAMED, // save was refused because the file has no path
	ERROR_CONFIG, // editor configuration could not be loaded or accepted
	ERROR_PLUGIN_LOAD, // configured language plugin could not be loaded
	ERROR_SYS_CLOSE // closing a file descriptor failed
};

typedef struct {
	enum ErrorType type;
	char* reason; // owned, NUL-terminated diagnostic message, or NULL
} Result;

// Initialize a Result before first use. Does not free existing storage.
void result_init(Result* result);

// Set only the diagnostic message. A NULL reason clears the message.
int result_set_reason(Result* result, const char* reason);

// Set the error type and diagnostic message together. If result is NULL,
// diagnostic output is ignored. A NULL reason is allowed.
int result_set_error(Result* result, enum ErrorType type, const char* reason);

// Release the message and leave the Result in the ERROR_OK state.
void result_free(Result* result);

// Clear the message and set the type to ERROR_OK.
void result_ok(Result* result);

// Return the message or a stable fallback when no message is available.
const char* result_reason(const Result* result);

#endif
