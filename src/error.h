#ifndef ERROR_H
#define ERROR_H

/*
The value returned by a function with int return-value
indicates whether or not an error has occurred in the function.
To know the nature of the error, pass a non-null pointer to a Result
as argument.

The reason attribute of the Result struct is a non-terminated
string or a NULL pointer, its function is to determine the reason
of the error. 
The type attribute is a enum that specify whether or not the result
is an error and, if the former is true, specify the nature of it. If
the result is not an error, then result type must be set to ERROR_OK and
reason set to NULL (result_ok() does this job and cleans the last result).

The Editor struct contains a result attribute and its function is to
contains the last error that has occurred inside the program. Hence,
to start handling errors, a pointer to editor.result must be passed to
the function.

If an error occurs inside a function, then it checks whether or not 
the pointer to the result is NULL. If the latter, result_set_reason() 
(which is the responsible for cleaning the result) must be called 
and result->type set. Hence, once a new result is created, the last 
one is cleaned.
*/

#define EIE_FATAL_ERROR -1 // EIE = error in error
#define EIE_NOT_FATAL_ERROR -2
#define EIE_NOT_AN_ERROR 1

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
	ERROR_STATIC
};

typedef struct {
	enum ErrorType type;
	char* reason;
} Result;

int result_set_reason(Result* result, const char* reason);
void result_free(Result* result);
void result_ok(Result* result);

#endif