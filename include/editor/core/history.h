#ifndef HISTORY_H
#define HISTORY_H

#include "util/types/u32string.h"
#include "util/types/position.h"
#include "util/types/vector.h"
#include "plugins/plugin.h"

enum operation_type {
	OP_INSERT,
	OP_DELETE,
	OP_INDENT,
	OP_UNINDENT,
	OP_COMMENT,
	OP_REPLACE,
	OP_LINEMOVE
};

typedef struct {
	Position start;
	Position end;
	u32string text;
} InsertDeleteData;

typedef struct {
	Position start;
	Position end;
} RangeData;

typedef struct {
	Vector replacements; // vector of Position
	u32string old_text;
	u32string new_text;
} ReplaceData;

typedef enum {
	LINEMOVE_UP,
	LINEMOVE_DOWN
} MoveDirection;

typedef struct {
	Position start;
	Position end;
	MoveDirection direction;
} LineMoveData;


typedef struct {
	enum operation_type type;

	// position where the cursor would go in the event of a remove
	Position cursor_remove;

	// position where the cursor would go in the event of a insert
	Position cursor_insert;

	// that logics works for all types except for replace and comment,
	// given that they are a simultaneous insert and remove operations,
	// and linemove

	// for replace type, cursor_remove represents where the
	// cursor should go when switching from the longer string
	// to the shorter one, whereas cursor_insert applies when
	// switching from the shorter to the longer one

	// for comment type, cursor_remove should be where the cursor
	// would go if the selection were uncommented, whereas the
	// cursor_insert should be where the cursor would go
	// if it were commented

	// for linemove type, cursor_remove should be where the cursor
	// would go if moving the selection up, whereas the cursor_insert
	// should be where the cursor would go if it were moving down

	union {
		InsertDeleteData insert;
		InsertDeleteData delete;
		RangeData indent;
		RangeData unindent;
		RangeData comment;
		ReplaceData replace;
		LineMoveData linemove;
	};

} Operation;


// all functions that use 'text' take ownership of the value
Operation operation_create_insert
(
	Position start, Position end,
	Position cursor_before, Position cursor_insert,
	u32string text
);

Operation operation_create_delete
(
	Position start, Position end,
	Position cursor_remove, Position cursor_insert,
	u32string text
);

Operation operation_create_indent
(
	Position start, Position end,
	Position cursor_remove, Position cursor_insert
);

Operation operation_create_unindent
(
	Position start, Position end,
	Position cursor_remove, Position cursor_insert
);

Operation operation_create_comment
(
	Position start, Position end,
	Position cursor_remove, Position cursor_insert
);

Operation operation_create_replace
(
	Position cursor_remove, Position cursor_insert,
	u32string old_text, u32string new_text,
	Vector replacements
);

Operation operation_create_linemove(
	Position start, Position end,
	Position cursor_remove, Position cursor_insert,
	MoveDirection direction
);

Operation operation_inverse(const Operation* op);

int belongs_to_language
(
	uint32_t c,
	const struct language_rules* rules
);

int operation_can_merge
(
	const Operation* a, 
	const Operation* b,
	IsWordChar is_word_char
);

void operation_merge(Operation* a, Operation* b);

void vector_operation_destroy(void* ptr);

#endif