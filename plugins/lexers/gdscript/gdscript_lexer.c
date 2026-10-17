#include <ctype.h>
#include <stddef.h>
#include <stdint.h>
#include <stdlib.h>

#include "plugins/plugin.h"

struct gdscript_lexer {
	struct lexer base;
};

static const char* const gdscript_extensions[] = {
	".gd"
};

static const char gdscript_comment[] = "#";

static const struct pair gdscript_pairs[] = {
	{'(', ')', 1, 1},
	{'[', ']', 1, 1},
	{'{', '}', 1, 1},
	{'"', '"', 0, 1},
	{'\'', '\'', 0, 1}
};

static const char* const keywords[] = {
	"if", "elif", "else", "for", "while", "match", "when",
	"break", "continue", "pass", "return", "class", "class_name",
	"extends", "is", "in", "as", "self", "super", "signal", "func",
	"static", "const", "enum", "var", "breakpoint", "preload", "await",
	"assert", "not", "and", "or"
};

static const char* const builtin_types[] = {
	"void", "bool", "int", "float", "String", "StringName", "NodePath",
	"Array", "Dictionary", "Callable", "Signal", "Variant", "Object",
	"RefCounted", "Resource", "Node", "Node2D", "Node3D",
	"Control", "CanvasItem", "ObjectID", "RID", "Vector2", "Vector2i",
	"Vector3", "Vector3i", "Vector4", "Vector4i", "Rect2", "Rect2i",
	"Transform2D", "Transform3D", "Basis", "Quaternion", "Projection",
	"Color", "Plane", "AABB", "PackedByteArray", "PackedInt32Array",
	"PackedInt64Array", "PackedFloat32Array", "PackedFloat64Array",
	"PackedStringArray", "PackedVector2Array", "PackedVector3Array",
	"PackedColorArray"
};

static const char* const constants[] = {
	"true", "false", "null", "PI", "TAU", "INF", "NAN"
};

static int is_space(uint32_t c) {
	return c <= 0xff && isspace((unsigned char) c) != 0;
}

static int is_identifier_start(uint32_t c) {
	if (c == U'_') {
		return 1;
	}

	if (c < 0x80) {
		return isalpha((unsigned char) c) != 0;
	}

	// GDScript permits most Unicode letters in identifiers. This is a
	// highlighter heuristic; the lexer API does not expose Unicode categories.
	return !is_space(c);
}

static int is_identifier_continue(uint32_t c) {
	return is_identifier_start(c) ||
		(c < 0x80 && isdigit((unsigned char) c));
}

static int gdscript_is_word_char(uint32_t c) {
	return is_identifier_continue(c);
}

static int word_equals
(
	const uint32_t* line,
	size_t begin,
	size_t end,
	const char* word
)
{
	size_t i = 0;

	while (word[i] != '\0') {
		if (begin + i >= end || line[begin + i] != (unsigned char) word[i]) {
			return 0;
		}
		i++;
	}

	return begin + i == end;
}

static int word_in_list
(
	const uint32_t* line,
	size_t begin,
	size_t end,
	const char* const* words,
	size_t count
)
{
	for (size_t i = 0; i < count; i++) {
		if (word_equals(line, begin, end, words[i])) {
			return 1;
		}
	}

	return 0;
}

static int emit
(
	struct token* tokens,
	size_t* count,
	size_t capacity,
	enum highlight highlight,
	size_t begin,
	size_t end
)
{
	if (begin == end) {
		return 1;
	}

	if (*count >= capacity) {
		return 0;
	}

	tokens[(*count)++] = (struct token) { highlight, begin, end };
	return 1;
}

static int is_hex(uint32_t c) {
	return (c >= U'0' && c <= U'9') ||
		(c >= U'a' && c <= U'f') ||
		(c >= U'A' && c <= U'F');
}

static size_t scan_number(const uint32_t* line, size_t len, size_t i) {
	size_t p = i;

	if (p + 1 < len && line[p] == U'0' &&
		(line[p + 1] == U'x' || line[p + 1] == U'X'))
	{
		p += 2;
		while (p < len && (is_hex(line[p]) || line[p] == U'_')) p++;
		return p;
	}

	if (p + 1 < len && line[p] == U'0' &&
		(line[p + 1] == U'b' || line[p + 1] == U'B'))
	{
		p += 2;
		while (p < len &&
			((line[p] >= U'0' && line[p] <= U'1') || line[p] == U'_')) p++;
		return p;
	}

	if (p + 1 < len && line[p] == U'0' &&
		(line[p + 1] == U'o' || line[p + 1] == U'O'))
	{
		p += 2;
		while (p < len &&
			((line[p] >= U'0' && line[p] <= U'7') || line[p] == U'_')) p++;
		return p;
	}

	while (p < len &&
		((line[p] >= U'0' && line[p] <= U'9') || line[p] == U'_')) p++;

	if (p < len && line[p] == U'.' && p + 1 < len &&
		line[p + 1] >= U'0' && line[p + 1] <= U'9')
	{
		p++;
		while (p < len &&
			((line[p] >= U'0' && line[p] <= U'9') || line[p] == U'_')) p++;
	}

	if (p < len && (line[p] == U'e' || line[p] == U'E')) {
		p++;
		if (p < len && (line[p] == U'+' || line[p] == U'-')) p++;
		while (p < len &&
			((line[p] >= U'0' && line[p] <= U'9') || line[p] == U'_')) p++;
	}

	return p;
}

static size_t find_quote_end
(
	const uint32_t* line,
	size_t len,
	size_t from,
	uint32_t quote,
	int triple,
	int raw
)
{
	for (size_t i = from; i < len; i++) {
		if (line[i] == U'\\' && i + 1 < len) {
			// In raw strings, only a backslash before the matching quote or
			// another backslash has special meaning.
			if (!raw || line[i + 1] == quote || line[i + 1] == U'\\') {
				i++;
				continue;
			}
		}

		if (line[i] != quote) continue;

		if (!triple) return i;
		if (i + 2 < len && line[i + 1] == quote && line[i + 2] == quote) {
			return i;
		}
	}

	return len;
}

static size_t gdscript_tokenize_line
(
	struct lexer* lexer,
	const uint32_t* line,
	size_t len,
	struct token* tokens,
	size_t max_tokens,
	enum lex_state state_in,
	enum lex_state* state_out
)
{
	(void) lexer;
	size_t count = 0;
	size_t i = 0;
	int continued_string = state_in == LEX_STATE_STRING ||
		state_in == LEX_STATE_CHAR || state_in == LEX_STATE_OTHER ||
		state_in == LEX_STATE_BLOCK_COMMENT;
	uint32_t continued_quote =
		(state_in == LEX_STATE_CHAR || state_in == LEX_STATE_BLOCK_COMMENT)
			? U'\'' : U'"';
	int continued_triple = state_in == LEX_STATE_STRING ||
		state_in == LEX_STATE_CHAR;

	if (state_out) *state_out = LEX_STATE_NORMAL;
	if (len == 0 || !tokens || max_tokens == 0) {
		if (state_out && continued_string) *state_out = state_in;
		return 0;
	}

	if (continued_string) {
		size_t close = find_quote_end(line, len, 0, continued_quote,
			continued_triple, 0);
		size_t content_end = close;
		if (!emit(tokens, &count, max_tokens, HL_STRING, 0, content_end)) {
			if (state_out) *state_out = state_in;
			return count;
		}
		if (close < len) {
			size_t closing_size = continued_triple ? 3 : 1;
			emit(tokens, &count, max_tokens, HL_PUNCTUATION, close,
				close + closing_size);
			i = close + closing_size;
		} else {
			if (state_out) *state_out = state_in;
			return count;
		}
	}

	while (i < len && count < max_tokens) {
		if (is_space(line[i])) {
			i++;
			continue;
		}

		if (line[i] == U'#') {
			emit(tokens, &count, max_tokens, HL_COMMENT, i, len);
			break;
		}

		// GDScript annotations, such as @export and @onready.
		if (line[i] == U'@' && i + 1 < len && is_identifier_start(line[i + 1])) {
			size_t begin = i++;
			while (i < len && is_identifier_continue(line[i])) i++;
			emit(tokens, &count, max_tokens, HL_PREPROCESSOR, begin, i);
			continue;
		}

		// Node shorthand paths ($Node and %UniqueName).
		if ((line[i] == U'$' || line[i] == U'%') && i + 1 < len &&
			is_identifier_start(line[i + 1]))
		{
			size_t begin = i++;
			while (i < len && (is_identifier_continue(line[i]) ||
				line[i] == U'/' || line[i] == U'.')) i++;
			emit(tokens, &count, max_tokens, HL_METHOD_OR_ATTRIB, begin, i);
			continue;
		}

		// Raw strings use an r/R prefix. Triple quotes may span lines.
		int raw = (line[i] == U'r' || line[i] == U'R') && i + 1 < len &&
			(line[i + 1] == U'"' || line[i + 1] == U'\'');
		size_t quote_pos = raw ? i + 1 : i;
		if (line[quote_pos] == U'"' || line[quote_pos] == U'\'') {
			uint32_t quote = line[quote_pos];
			int triple = quote_pos + 2 < len && line[quote_pos + 1] == quote &&
				line[quote_pos + 2] == quote;
			size_t opening = triple ? 3 : 1;
			size_t body = quote_pos + opening;
			if (raw) emit(tokens, &count, max_tokens, HL_SPECIFIER, i, quote_pos);
			emit(tokens, &count, max_tokens, HL_PUNCTUATION, quote_pos, body);

			size_t close = find_quote_end(line, len, body, quote, triple, raw);
			emit(tokens, &count, max_tokens, HL_STRING, body, close);
			if (close < len) {
				emit(tokens, &count, max_tokens, HL_PUNCTUATION, close,
					close + opening);
				i = close + opening;
			} else {
				if (triple || (len > body && line[len - 1] == U'\\')) {
					if (state_out) {
						if (triple) {
							*state_out = quote == U'"'
								? LEX_STATE_STRING : LEX_STATE_CHAR;
						} else {
							*state_out = quote == U'"'
								? LEX_STATE_OTHER : LEX_STATE_BLOCK_COMMENT;
						}
					}
				}
				i = len;
			}
			continue;
		}

		if ((line[i] >= U'0' && line[i] <= U'9') ||
			(line[i] == U'.' && i + 1 < len &&
			 line[i + 1] >= U'0' && line[i + 1] <= U'9'))
		{
			size_t begin = i;
			i = scan_number(line, len, i);
			emit(tokens, &count, max_tokens, HL_NUMBER, begin, i);
			continue;
		}

		if (is_identifier_start(line[i])) {
			size_t begin = i++;
			while (i < len && is_identifier_continue(line[i])) i++;

			enum highlight hl = HL_NORMAL;
			if (word_in_list(line, begin, i, keywords,
				sizeof(keywords) / sizeof(keywords[0])))
			{
				hl = HL_KEYWORD;
			} else if (word_in_list(line, begin, i, builtin_types,
				sizeof(builtin_types) / sizeof(builtin_types[0])))
			{
				hl = HL_TYPE;
			} else if (word_in_list(line, begin, i, constants,
				sizeof(constants) / sizeof(constants[0])))
			{
				hl = HL_CONSTANT;
			} else {
				size_t next = i;
				while (next < len && is_space(line[next])) next++;
				if (next < len && line[next] == U'(') hl = HL_FUNCTION;
			}

			emit(tokens, &count, max_tokens, hl, begin, i);
			continue;
		}

		// Prefer longer operators before their one-character prefixes.
		static const char* const operators[] = {
			"**=", "<<=", ">>=", "==", "!=", "<=", ">=", "->", ":=",
			"+=", "-=", "*=", "/=", "%=", "&=", "|=", "^=", "<<",
			">>", "**", "//", "+", "-", "*", "/", "%", "=", "<",
			">", "&", "|", "^", "~", "!"
		};
		int matched = 0;
		for (size_t op = 0; op < sizeof(operators) / sizeof(operators[0]); op++) {
			size_t op_len = 0;
			while (operators[op][op_len] != '\0') op_len++;
			if (i + op_len > len) continue;

			size_t j = 0;
			while (j < op_len && line[i + j] == (unsigned char) operators[op][j]) j++;
			if (j == op_len) {
				emit(tokens, &count, max_tokens, HL_OPERATOR, i, i + op_len);
				i += op_len;
				matched = 1;
				break;
			}
		}
		if (matched) continue;

		if (line[i] == U'(' || line[i] == U')' || line[i] == U'[' ||
			line[i] == U']' || line[i] == U'{' || line[i] == U'}' ||
			line[i] == U',' || line[i] == U'.' || line[i] == U';' ||
			line[i] == U':')
		{
			emit(tokens, &count, max_tokens, HL_PUNCTUATION, i, i + 1);
		} else {
			emit(tokens, &count, max_tokens, HL_NORMAL, i, i + 1);
		}
		i++;
	}

	return count;
}

static void gdscript_reset(struct lexer* lexer) {
	(void) lexer;
}

static void gdscript_destroy(struct lexer* lexer) {
	free(lexer);
}

static struct lexer* gdscript_create_lexer(void) {
	struct gdscript_lexer* gd = calloc(1, sizeof(*gd));
	if (!gd) return NULL;

	gd->base.tokenize_line = gdscript_tokenize_line;
	gd->base.destroy = gdscript_destroy;
	gd->base.reset = gdscript_reset;
	return &gd->base;
}

static const struct language_rules gdscript_rules = {
	.pairs = gdscript_pairs,
	.pair_count = sizeof(gdscript_pairs) / sizeof(gdscript_pairs[0]),
	.comment_fmt = gdscript_comment,
	.auto_indent = 1,
	.is_word_char = gdscript_is_word_char
};

static const struct language_plugin gdscript_plugin = {
	.name = "gdscript",
	.extensions = gdscript_extensions,
	.extension_count = sizeof(gdscript_extensions) / sizeof(gdscript_extensions[0]),
	.reserved_filenames = NULL,
	.reserved_filename_count = 0,
	.create_lexer = gdscript_create_lexer,
	.rules = &gdscript_rules
};

const struct language_plugin* plugin_init(void) {
	return &gdscript_plugin;
}
