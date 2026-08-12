#ifndef EDITOR_AUTO_COMPLETE_H
#define EDITOR_AUTO_COMPLETE_H

#include "util/types/vector.h"
#include "util/types/u32string.h"

typedef enum {
	AUTOCOMP_CMD,
	AUTOCOMP_FILE,
	AUTOCOMP_SHELL,
	AUTOCOMP_LANG
} AutoCompType;

typedef enum {
	AUTOCOMP_RESULT_SHOW_CANDIDATES,
	AUTOCOMP_RESULT_SUCCESS,
	AUTOCOMP_RESULT_NO_CANDIDATES
} AutoCompResultType;

typedef struct {
	unsigned char _;
} AutoCompDummy;

typedef struct {
	AutoCompResultType type;

	union {
		Vector candidates;
		AutoCompDummy dummy;
	};

} AutoCompResult;

AutoCompResult autocomp_cmd
(
	const u32string* string, 
	Prompt* pt
);

AutoCompResult autocomp_file
(
	const u32string* string,
	Prompt* pt,
	size_t start
);

#endif