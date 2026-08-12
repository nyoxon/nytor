#include "editor/core/history.h"

Operation operation_create_insert
(
	Position start, Position end,
	Position cursor_remove, Position cursor_insert,
	u32string text
)
{
	return (Operation) {
		.type = OP_INSERT,
		.cursor_remove = cursor_remove, 
		.cursor_insert = cursor_insert,
		.insert = { start, end, text }
	};
}

Operation operation_create_delete
(
	Position start, Position end,
	Position cursor_remove, Position cursor_insert,
	u32string text
)
{
	return (Operation) {
		.type = OP_DELETE,
		.cursor_remove = cursor_remove, 
		.cursor_insert = cursor_insert,
		.delete = { start, end, text }
	};
}

Operation operation_create_indent
(
	Position start, Position end,
	Position cursor_remove, Position cursor_insert
)
{
	return (Operation) {
		.type = OP_INDENT,
		.cursor_remove = cursor_remove, 
		.cursor_insert = cursor_insert,
		.indent = { start, end }
	};
}

Operation operation_create_unindent
(
	Position start, Position end,
	Position cursor_remove, Position cursor_insert
)
{
	return (Operation) {
		.type = OP_UNINDENT,
		.cursor_remove = cursor_remove, 
		.cursor_insert = cursor_insert,
		.unindent = { start, end }
	};
}

Operation operation_create_comment
(
	Position start, Position end,
	Position cursor_remove, Position cursor_insert
)
{
	return (Operation) {
		.type = OP_COMMENT,
		.cursor_remove = cursor_remove, 
		.cursor_insert = cursor_insert,
		.comment = { start, end }
	};
}

Operation operation_create_replace
(
	Position cursor_remove, Position cursor_insert,
	u32string old_text, u32string new_text,
	Vector replacements
)
{
	return (Operation) {
		.type = OP_REPLACE,
		.cursor_remove = cursor_remove, 
		.cursor_insert = cursor_insert,
		.replace = { replacements, old_text, new_text }
	};
}

Operation operation_create_linemove(
	Position start, Position end,
	Position cursor_remove, Position cursor_insert,
	MoveDirection direction
)
{
	return (Operation) {
		.type = OP_LINEMOVE,
		.cursor_remove = cursor_remove,
		.cursor_insert = cursor_insert,
		.linemove = { start, end, direction }
	};
}

Operation operation_inverse(const Operation* op) {
	switch (op->type) {
	case OP_INSERT:
		return (Operation) {
			.type = OP_DELETE,
			.cursor_remove = op->cursor_remove,
			.cursor_insert = op->cursor_insert,
			.delete = { op->insert.start, op->insert.end, op->insert.text }
		};

	case OP_DELETE:
		return (Operation) {
			.type = OP_INSERT,
			.cursor_remove = op->cursor_remove,
			.cursor_insert = op->cursor_insert,
			.insert = { op->delete.start, op->delete.end, op->delete.text }
		};

	case OP_INDENT:
		return (Operation) {
			.type = OP_UNINDENT,
			.cursor_remove = op->cursor_remove,
			.cursor_insert = op->cursor_insert,
			.unindent = { op->indent.start, op->indent.end }
		};

	case OP_UNINDENT:
		return (Operation) {
			.type = OP_INDENT,
			.cursor_remove = op->cursor_remove,
			.cursor_insert = op->cursor_insert,
			.unindent = { op->unindent.start, op->unindent.end }
		};

	case OP_COMMENT:
		return (Operation) {
			.type = OP_COMMENT,
			.cursor_remove = op->cursor_remove,
			.cursor_insert = op->cursor_insert,
			.comment = { op->comment.start, op->comment.end }
		};

	case OP_REPLACE:
		return (Operation) {
			.type = OP_REPLACE,
			.cursor_remove = op->cursor_remove,
			.cursor_insert = op->cursor_insert,
			.replace = {
				op->replace.replacements,
				op->replace.new_text, 
				op->replace.old_text }
		};

	case OP_LINEMOVE:
		MoveDirection direction = (op->linemove.direction == LINEMOVE_UP)
			? LINEMOVE_DOWN
			: LINEMOVE_UP;

		Position start = op->linemove.start;
		Position end = op->linemove.end;

		return (Operation) {
			.type = OP_LINEMOVE,
			.cursor_remove = op->cursor_remove,
			.cursor_insert = op->cursor_insert,
			.linemove = {
				start,
				end,
				direction
			}
		};

	default:
		return *op;
	}
}

int belongs_to_language
(
	uint32_t c,
	const struct language_rules* rules
)
{
	if (!rules) {
		return 0;
	}

	for (size_t i = 0; i < rules->pair_count; i++) {
		if (c == (unsigned char) rules->pairs[i].open || 
			c == (unsigned char) rules->pairs[i].close) 
		{
			return 1;
		}
	}

	return 0;
}

int operation_can_merge
(
	const Operation* a, 
	const Operation* b,
	const struct language_rules* rules
) 
{
	if (!a || !b) {
		return 0;
	}

	if (a->type != b->type) {
		return 0;
	}

	switch (a->type){
	case OP_INSERT: {
		if (u32string_is_empty(&a->insert.text) ||
			u32string_is_empty(&b->insert.text))
		{
			return 0;
		}

		if (!position_equal(&a->insert.end, &b->insert.start)) {
			return 0;
		}

		uint32_t last = u32string_char(
			&a->insert.text,
			u32string_size(&a->insert.text) - 1);

		uint32_t first = u32string_char(
			&b->insert.text,
			0);

		if (belongs_to_language(first, rules) ||
			belongs_to_language(last, rules))
		{
			return 0;
		}

		if (u32_isspace(first) && !u32_isspace(last)) {
			return 0;
		}

		return 1;
	}

	case OP_DELETE: {
		if (u32string_is_empty(&a->delete.text) ||
			u32string_is_empty(&b->delete.text))
		{
			return 0;
		}

		if (!position_equal(&a->delete.start, &b->delete.end)) {
			return 0;
		}

		uint32_t last = u32string_char(
			&b->delete.text,
			u32string_size(&b->delete.text) - 1);

		uint32_t first = u32string_char(
			&a->delete.text,
			0);

		if (belongs_to_language(first, rules) ||
			belongs_to_language(last, rules))
		{
			return 0;
		}

		if (u32_isspace(first) && !u32_isspace(last)) {
			return 0;
		}

		return 1;
	}

	default:
		return 0;
	}
}

void operation_merge(Operation* a, Operation* b) {
	if (a->type != b->type) {
		return;
	}

	switch (a->type) {
	case OP_INSERT:
		a->insert.end = b->insert.end;
		a->cursor_insert = b->cursor_insert;

		u32string_append_raw(
			&a->insert.text,
			u32string_into_ptr_const(&b->insert.text),
			u32string_size(&b->insert.text));

		u32string_free(&b->insert.text);

		break;

	case OP_DELETE:
		a->delete.start = b->delete.start;
		a->cursor_remove = b->cursor_remove;

		u32string_prepend_raw(
			&a->delete.text,
			u32string_into_ptr_const(&b->delete.text),
			u32string_size(&b->delete.text));

		u32string_free(&b->delete.text);

		break;

	default:
		return;
	}
}

void vector_operation_destroy(void* ptr) {
	Operation* op = (Operation*) ptr;

	switch (op->type) {
	case OP_INSERT:
		u32string_free(&op->insert.text);
		break;

	case OP_DELETE:
		u32string_free(&op->delete.text);
		break;

	case OP_REPLACE:
		vector_free(&op->replace.replacements);
		u32string_free(&op->replace.old_text);
		u32string_free(&op->replace.new_text);
		break;

	default:
		break;
	}
}