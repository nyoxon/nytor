// Plugin: Lua syntax highlighting lexer
// Based on: Lua 5.5.1
// Author: nyoxon
// Created in version: 1.2
// Last revision in version: 1.2

#include <ctype.h>
#include <stdint.h>
#include <stdlib.h>

#include "plugins/plugin.h"

// In this plugin, a different approach was adopted
// compared to traditional C and bash plugins.
// More specifcally, i don't use a structure that
// stores the length of the keywords/punctuations etc.
// There's no divine reason for this (like efficiency);
// it's just to experiment with different ways of
// implementing the lexer using the plugin API
// Therefore, the length of those things is calculated
// "at runtime".


// COMMENT ABOUT STRINGS
// I used sublime text (free version) for a year or two
// to write source code.
// In subl editor, a good number of the lexers for
// the languages i used performed string lexing in
// a specific way, and that is the way i will try
// to lex them (because i'm used to see a string in this way
// and because is great looking).

// This form is:
// - the string delimiters must have a specific color
// (usually the punctuation color)
// - the string content must have the string color
// - any escaped character sequences (usually by '\') within
// the string must have a color different from the previous
// two (in this case, the HL_NUMBER color will be used)

// This process has already been used, for example, in
// the C and bash lexers i've implemented

struct lua_lexer {
	struct lexer base;
	size_t long_bracket_equals;
	uint32_t short_quote;
};

static const char* const lua_extensions[] = { ".lua" };
static const char lua_comment[] = "--";

static const struct pair lua_pairs[] = {
	{'(', ')', 1, 1}, 
	{'[', ']', 1, 1}, 
	{'{', '}', 1, 1},
	{'"', '"', 0, 1}, 
	{'\'', '\'', 0, 1}
};

static const char* const lua_keywords[] = {
	"and", 
	"break", 
	"do", 
	"else", 
	"elseif", 
	"end", 
	"for",
	"function", 
	"global", 
	"goto", 
	"if", 
	"in", 
	"local", 
	"not",
	"or", 
	"repeat", 
	"return", 
	"then", 
	"until", 
	"while"
};

static const char* const lua_operators[] = {
	"...",
	 
	"..", 
	"//", "<<", 
	">>", "==", 
	"~=", "<=", 
	">=", "::",
	
	"+", "-", 
	"*", "/", 
	"%", "^", 
	"#", "&", 
	"~", "|", 
	"<", ">", 
	"=", ":"
};

static int is_space(uint32_t c) {
	return c <= UINT8_MAX && isspace((unsigned char)c) != 0;
}

// c < 0x80 -> valid ascii character
static int is_word_start(uint32_t c) {
	return c == U'_' || (c < 0x80 && isalpha((unsigned char)c));
}

static int is_word_continue(uint32_t c) {
	return is_word_start(c) || (c < 0x80 && isdigit((unsigned char)c));
}

// IsWordChar
static int lua_is_word_char(uint32_t c) {
	return is_word_continue(c);
}


// compare `word` to `line[begin..end)` and returns
// if they are equal
static int word_is
(
	const uint32_t* line, 
	size_t begin, 
	size_t end,
	const char* word
)
{
	size_t i = 0;
	
	while (word[i] && 
		   begin + i < end && 
		   line[begin + i] == (unsigned char) word[i])
	{
		i++;
	}
	
	return word[i] == '\0' && begin + i == end;
}

static int is_keyword
(
	const uint32_t* line, 
	size_t begin, 
	size_t end
) 
{
	for (size_t i = 0; 
		 i < sizeof(lua_keywords) / sizeof(lua_keywords[0]); 
		 i++)
	{
		if (word_is(line, begin, end, lua_keywords[i])) return 1;
	}
	
	return 0;
}

// push/append a token to the token array, respecting
// a maximum capacity
static int emit
(
	struct token* tokens, 
	size_t* count, 
	size_t capacity, // max tokens
	enum highlight hl, 
	size_t begin, 
	size_t end
)
{
	if (begin == end) return 1;
	
	if (*count >= capacity) return 0;
	
	tokens[(*count)++] = (struct token) { hl, begin, end };
	
	return 1;
}

// detect long strings open delimiters [=[, [==[ and [[
static int long_open
(
	const uint32_t* line, 
	size_t len, 
	size_t pos,
	size_t* equals, // how many equals (=)
	size_t* width // how many characters
)
{
	if (pos >= len || line[pos] != U'[') return 0;
	
	size_t i = pos + 1;
	
	while (i < len && line[i] == U'=') i++;
	
	if (i >= len || line[i] != U'[') return 0;
	
	*equals = i - pos - 1;
	*width = i - pos + 1;

	return 1;
}

// find, inside a single line, a long string close delimiter
static size_t find_long_close
(
	const uint32_t* line, 
	size_t len, 
	size_t from,
	size_t equals
)
{
	for (size_t i = from; i < len; i++) {
		if (line[i] != U']') continue;
		
		size_t j = i + 1;
		
		while (j < len && line[j] == U'=' && j - i - 1 < equals) {
			j++;
		}
		
		if (j - i - 1 == equals && j < len && line[j] == U']') {
			return i;
		}
	}
	
	return len;
}


// ex of valid numbers in lua:
// 4
// 0.4
// 4.57e-3
// 0.3e12
// 5e+20
// 0x1abf
static size_t scan_number
(
	const uint32_t* line, 
	size_t len, 
	size_t pos
) 
{
	size_t i = pos;
	
	// 0x1234 or 0X1234
	if (i + 1 < len && 
		line[i] == U'0' &&
	   (line[i + 1] == U'x' || line[i + 1] == U'X')) 
	{
		i += 2;
		
		while (i < len && 
		      (isxdigit((unsigned char)line[i]) || line[i] == U'_')) 
		{
			i++;
		}
	} 
	
	else {
		// this while consumes a number and appears again
		// in this function
		while (i < len && 
			  ((line[i] >= U'0' && line[i] <= U'9') || 
			   line[i] == U'_'))
		{
			i++;
		}
		
		// float, 123.123
		if (i < len && 
			line[i] == U'.' && 
			!(i + 1 < len && line[i + 1] == U'.'))  // 12..12
		{
			i++;
			
			while (i < len && 
				  ((line[i] >= U'0' && 
				  line[i] <= U'9') || line[i] == U'_'))
			{
				i++;
			}
		}
		
		// exp, 123e123 or 123E123 or 123e+123 or 123e-123
		if (i < len && 
		   (line[i] == U'e' || line[i] == U'E')) 
		{
			i++;
			
			if (i < len && 
			   (line[i] == U'+' || line[i] == U'-'))
			{
				i++;
			}
			
			while (i < len && 
				  ((line[i] >= U'0' && line[i] <= U'9') || 
				  line[i] == U'_'))
			{
				i++;
			}
		}
	}
	
	return i;
}

// main function
static size_t lua_tokenize_line
(
	struct lexer* base, 
	const uint32_t* line,
	size_t len, 
	struct token* tokens, 
	size_t max_tokens,
	enum lex_state state_in, 
	enum lex_state* state_out
)
{
	struct lua_lexer* lexer = (struct lua_lexer*) base;
	
	size_t count = 0, i = 0;
	
	enum lex_state state = state_in;
	
	// change only if necessary
	if (state_out) *state_out = LEX_STATE_NORMAL;
	
	if (!tokens || max_tokens == 0) return 0;

	// a long comment or string can continue across lines
	// (see COMMENT ABOUT STRINGS)
	if (state_in == LEX_STATE_BLOCK_COMMENT || 
		state_in == LEX_STATE_STRING) 
	{
		size_t close = find_long_close(
			line, 
			len, 
			0, 
			lexer->long_bracket_equals
		);
		
		// entire line
		if (close == len) {
			emit(
				tokens, 
				&count, 
				max_tokens,
				(state_in == LEX_STATE_BLOCK_COMMENT) 
					? HL_COMMENT 
					: HL_STRING,
				0, 
				len
			);
			
			if (state_out) *state_out = state_in;
			
			return count;
		}
		
		size_t close_width = lexer->long_bracket_equals + 2;
		
		if (state_in == LEX_STATE_BLOCK_COMMENT) {
			emit(
				tokens, 
				&count, 
				max_tokens, 
				HL_COMMENT, 
				0, 
				close + close_width
			);
		} 
		
		else {
			emit(
				tokens, 
				&count, 
				max_tokens, 
				HL_STRING, 
				0, 
				close
			);
			
			emit(
				tokens, 
				&count, 
				max_tokens, 
				HL_PUNCTUATION,
				close, 
				close + close_width
			);
		}
		
		i = close + close_width;
		
		state = LEX_STATE_NORMAL;
	}
	
	// short strings can continuse across lines
	// (see COMMENT ABOUT STRINGS)
	if (state_in == LEX_STATE_OTHER) {
		size_t segment = i;
		int closed = 0;
		
		while (i < len) {
			// escape character
			if (line[i] == U'\\' && i + 1 < len) { 
				emit(
					tokens, 
					&count, 
					max_tokens, 
					HL_STRING, 
					segment, 
					i
				);
				
				emit(
					tokens, 
					&count, 
					max_tokens, 
					HL_NUMBER, 
					i, 
					i + 2
				);
				
				i += 2; 
				segment = i;
				
				continue; 
			}
			if (line[i] == U'\\') {
				emit(
					tokens, 
					&count, 
					max_tokens, 
					HL_STRING, 
					segment, 
					i
				);
				
				emit(
					tokens, 
					&count, 
					max_tokens, 
					HL_NUMBER, 
					i, 
					i + 1
				);
				
				i++;
				segment = i;
				
				continue;
			}
			
			// symbol that opened the string
			if (line[i] == lexer->short_quote) {
				emit(
					tokens, 
					&count, 
					max_tokens, 
					HL_STRING, 
					segment, 
					i
				);
				
				emit(
					tokens, 
					&count, 
					max_tokens, 
					HL_PUNCTUATION, 
					i, 
					i + 1
				);
				
				i++;
				closed = 1;
				
				break; 
			}
			
			i++;
		}
		
		if (!closed) {
			emit(
				tokens, 
				&count, 
				max_tokens, 
				HL_STRING, 
				segment, 
				i
			);
		}
		
		// propagates the state
		if (!closed) {
			if (state_out) *state_out = LEX_STATE_OTHER;
			
			return count;
		}
		
		// the continued string is closed;
		state = LEX_STATE_NORMAL;
	}

	while (i < len && count < max_tokens) {
		if (is_space(line[i])) { i++; continue; }
		
		// block comment
		if (line[i] == U'-' && 
			i + 1 < len && 
			line[i + 1] == U'-') 
		{
			size_t begin = i;
			size_t equals, width;
			
			if (long_open(line, len, i + 2, &equals, &width)) {
				lexer->long_bracket_equals = equals;
				
				size_t body = i + 2 + width;
				
				size_t close = find_long_close(
					line, 
					len, 
					body, 
					equals
				);
				
				// comment exceeds the actual line
				if (close == len) {
					emit(
						tokens, 
						&count, 
						max_tokens, 
						HL_COMMENT, 
						i, 
						len
					);
					
					state = LEX_STATE_BLOCK_COMMENT;
					break;
				}
				
				i = close + equals + 2;
				
				// line[begin, i] is a comment
				emit(
					tokens, 
					&count, 
					max_tokens, 
					HL_COMMENT, 
					begin, 
					i
				);
				
				continue;
			}
			
			// line[i, len) is a comment
			emit(
				tokens, 
				&count, 
				max_tokens, 
				HL_COMMENT, 
				i, 
				len
			);
			
			break;
		}

		// long bracket strings: [=[...]=], with any 
		// number of '=' signs. (see COMMENT ABOUT STRINGS)
		{
			size_t equals, width;
			
			if (long_open(line, len, i, &equals, &width)) {
				lexer->long_bracket_equals = equals;
				
				size_t begin = i;
				
				size_t close = find_long_close(
					line, 
					len, 
					i + width, 
					equals
				);
				
				if (close == len) {
					emit(
						tokens, 
						&count, 
						max_tokens, 
						HL_PUNCTUATION,
						begin, 
						begin + width
					);
					
					emit(
						tokens, 
						&count, 
						max_tokens, 
						HL_STRING,
						begin + width, 
						len
					);
					
					state = LEX_STATE_STRING;
					
					break;
				}
				
				emit(
					tokens, 
					&count, 
					max_tokens, 
					HL_PUNCTUATION,
					begin, 
					begin + width
				);
				
				emit(
					tokens, 
					&count, 
					max_tokens, 
					HL_STRING,
					begin + width, 
					close
				);
				
				i = close + equals + 2;
				
				emit(
					tokens, 
					&count, 
					max_tokens, 
					HL_PUNCTUATION, 
					close, 
					i
				);
				
				continue;
			}
		}
		
		// short strings (see COMMENT ABOUT STRINGS)
		if (line[i] == U'\'' || line[i] == U'"') {
			uint32_t quote = line[i];
			size_t begin = i++;
			size_t segment = i;
			int closed = 0;
			
			emit(
				tokens, 
				&count, 
				max_tokens, 
				HL_PUNCTUATION, 
				begin, 
				i
			);

			while (i < len) {
				if (line[i] == U'\\' && i + 1 < len) {
					emit(
						tokens, 
						&count, 
						max_tokens, 
						HL_STRING, 
						segment, 
						i
					);
					
					emit(
						tokens, 
						&count, 
						max_tokens, 
						HL_NUMBER, 
						i, 
						i + 2
					);
					
					i += 2;
					segment = i;
					
					continue;
				}

				if (line[i] == U'\\') {
					emit(
						tokens, 
						&count, 
						max_tokens, 
						HL_STRING, 
						segment, 
						i
					);
					
					emit(
						tokens, 
						&count, 
						max_tokens, 
						HL_NUMBER, 
						i, 
						i + 1
					);
					
					i++;
					state = LEX_STATE_OTHER;
					
					break;
				}
				
				// string completed
				if (line[i] == quote) {
					emit(
						tokens, 
						&count, 
						max_tokens, 
						HL_STRING, 
						segment, 
						i
					);
					
					emit(
						tokens, 
						&count, 
						max_tokens, 
						HL_PUNCTUATION, 
						i, 
						i + 1
					);
					
					i++;
					closed = 1;
					
					break;
				}
				
				i++;
			}

			if (!closed && state != LEX_STATE_OTHER) {
				emit(
					tokens, 
					&count, 
					max_tokens, 
					HL_STRING, 
					segment, 
					i
				);
			}
			
			if (state == LEX_STATE_OTHER) {
				lexer->short_quote = quote;
			}
			
			continue;
		}
		
		// number
		if ((line[i] >= U'0' && line[i] <= U'9') ||
			(line[i] == U'.' && 
				i + 1 < len && 
				line[i + 1] >= U'0' && 
				line[i + 1] <= U'9')) 
		{
			size_t begin = i;
			i = scan_number(line, len, i);
			
			emit(
				tokens, 
				&count, 
				max_tokens, 
				HL_NUMBER, 
				begin, 
				i
			);
			
			continue;
		}
		
		// identifier
		if (is_word_start(line[i])) {
			size_t begin = i++;
			
			while (i < len && is_word_continue(line[i])) i++;
			
			enum highlight hl = HL_NORMAL;
			
			if (is_keyword(line, begin, i)) {
				hl = HL_KEYWORD;
			}
			
			else if (word_is(line, begin, i, "nil") || 
					 word_is(line, begin, i, "true") || 
					 word_is(line, begin, i, "false")) 
			{
				hl = HL_CONSTANT;
			}
			
			else {
				size_t next = i;
				while (next < len && is_space(line[next])) next++;
				
				if (next < len && 
					line[next] == U'(') 
				{
					hl = HL_FUNCTION;
				}
				
				else 
				if (begin > 0 && 
				   (line[begin - 1] == U'.' ||
						(begin > 1 && 
							line[begin - 1] == U':' && 
							line[begin - 2] != U':')))
				{
					hl = HL_METHOD_OR_ATTRIB;
				}
			}
			
			emit(
				tokens, 
				&count, 
				max_tokens, 
				hl, 
				begin, 
				i
			);
			
			continue;
		}
		
		
		// operator
		int matched = 0;
		
		for (size_t op = 0; 
			 op < sizeof(lua_operators) / sizeof(lua_operators[0]); 
			 op++) 
		{
			size_t n = 0;
			
			while (lua_operators[op][n]) n++;
			if (i + n > len) continue;
			
			size_t j = 0;
			
			while (j < n && 
				   line[i + j] == 
				   		(unsigned char) lua_operators[op][j])
			{
				j++;
			}
			
			if (j == n) {
				emit(
					tokens, 
					&count, 
					max_tokens, 
					HL_OPERATOR, 
					i, 
					i + n
				);
				
				i += n; matched = 1; break;
			}
		}
		
		if (matched) continue;
		
		// punctuation
		if (line[i] == U'(' || 
			line[i] == U')' || 
			line[i] == U'[' ||
			line[i] == U']' || 
			line[i] == U'{' || 
			line[i] == U'}' ||
			line[i] == U',' || 
			line[i] == U'.' || 
			line[i] == U';')
		{
			emit(
				tokens, 
				&count, 
				max_tokens, 
				HL_PUNCTUATION, 
				i, 
				i + 1
			);
		}
		
		else {
			emit(
				tokens, 
				&count, 
				max_tokens, 
				HL_NORMAL, 
				i, 
				i + 1
			);
		}
		
		i++;
	}
	
	if (state_out) *state_out = state;
	
	return count;
}

static void lua_destroy(struct lexer* lexer) { 
	free(lexer); 
}

static void lua_reset(struct lexer* lexer) {
	((struct lua_lexer*) lexer)->long_bracket_equals = 0;
	((struct lua_lexer*) lexer)->short_quote = 0;
}

static struct lexer* lua_create_lexer(void) {
	struct lua_lexer* lexer = calloc(1, sizeof(*lexer));
	
	if (!lexer) return NULL;
	
	lexer->base.tokenize_line = lua_tokenize_line;
	lexer->base.destroy = lua_destroy;
	lexer->base.reset = lua_reset;
	
	return &lexer->base;
}

static const struct language_rules lua_rules = {
	.pairs = lua_pairs,
	.pair_count = sizeof(lua_pairs) / sizeof(lua_pairs[0]),
	.comment_fmt = lua_comment,
	.auto_indent = 1,
	.is_word_char = lua_is_word_char
};

static const struct language_plugin lua_plugin = {
	.name = "lua",
	.extensions = lua_extensions,
	.extension_count = sizeof(lua_extensions) / sizeof(lua_extensions[0]),
	.reserved_filenames = NULL,
	.reserved_filename_count = 0,
	.create_lexer = lua_create_lexer,
	.rules = &lua_rules
};

const struct language_plugin* plugin_init(void) { 
	return &lua_plugin; 
}