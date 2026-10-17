// Plugin: Rust syntax highlighting lexer
// Based on: std Rust, Rust programming language book
// Author: nyoxon
// Created in version: 1.2
// Last revision in version: 1.2

#include <ctype.h>
#include <stdint.h>
#include <stdlib.h>

#include "plugins/plugin.h"

// sad comment:
// self and Self are not constants, but i don't like
// to see them using the keyword color.
// for the same reason, 'mut' will be in the
// library_constants array

struct rust_lexer {
	struct lexer base;
	size_t block_depth;
	size_t raw_hashes;
	uint32_t quote;
};

static const char* const extensions[] = { ".rs" };
static const char comment_fmt[] = "//";

static const struct pair pairs[] = {
	{'{', '}', 1, 1},
	{'(', ')', 1, 1},
	{'[', ']', 1, 1},
	{'"', '"', 0, 1},
	{'\'', '\'', 0, 1}
};

static const char* const keywords[] = {
	"as", "async", "await",
	"break", "const", "continue",
	"crate", "dyn", "else",
	"enum", "extern", "false",
	"fn", "for", "if",
	"impl", "in", "let",
	"loop", "match", "mod",
	"move", "pub",
	"ref", "return", "static", "struct",
	"super", "trait", "true",
	"type", "unsafe", "use",
	"where", "while", "yield",
	"try"
};

static const char* const types[] = {
	"bool", "char", "str",
	"i8", "i16", "i32",
	"i64", "i128", "isize",
	"u8", "u16", "u32",
	"u64", "u128", "usize",
	"f32", "f64"
};

// common types and traits from std/core/alloc.
static const char* const library_types[] = {
	"String", "Vec", "Option",
	"Result", "Box", "Rc",
	"Weak", "Arc", "Cell",
	"RefCell", "OnceCell", "OnceLock",
	"HashMap", "HashSet", "BTreeMap",
	"BTreeSet", "BinaryHeap", "VecDeque",
	"LinkedList", "Cow", "Path",
	"PathBuf", "OsStr", "OsString",
	"CString", "CStr", "Duration",
	"Instant", "SystemTime", "UNIX_EPOCH",
	"File", "OpenOptions", "BufReader",
	"BufWriter", "Cursor", "TcpStream",
	"TcpListener", "UdpSocket", "IpAddr",
	"Ipv4Addr", "Ipv6Addr", "SocketAddr",
	"Ordering", "Reverse", "Mutex",
	"RwLock", "MutexGuard", "RwLockReadGuard",
	"RwLockWriteGuard", "JoinHandle", "Builder",
	"Pin", "PhantomData", "Infallible",
	"NonZeroUsize", "Iterator", "IntoIterator",
	"DoubleEndedIterator", "ExactSizeIterator", "From",
	"Into", "TryFrom", "TryInto",
	"AsRef", "AsMut", "Borrow",
	"ToOwned", "Clone", "Copy",
	"Default", "Drop", "Eq",
	"PartialEq", "Ord", "PartialOrd",
	"Hash", "Debug", "Display",
	"Formatter", "Error", "Iterator",
	"Extend", "FromIterator", "Deref",
	"DerefMut", "Fn", "FnMut",
	"FnOnce", "Send", "Sync",
	"Future", "IntoFuture", "Read",
	"Write", "Seek", "BufRead",
};

static const char* const library_functions[] = {
	"print", "println", "eprint",
	"eprintln", "format", "format_args",
	"panic", "todo", "unimplemented",
	"unreachable", "assert", "assert_eq",
	"assert_ne", "debug_assert", "debug_assert_eq",
	"debug_assert_ne", "matches", "vec",
	"cfg", "include", "include_str",
	"include_bytes", "concat", "env",
	"option_env", "stringify", "write",
	"writeln", "drop", "clone",
	"clone_from", "default", "to_string",
	"to_owned", "from", "into",
	"try_from", "try_into", "as_ref",
	"as_mut", "borrow", "unwrap",
	"expect", "unwrap_or", "unwrap_or_else",
	"map", "map_err", "and_then",
	"or_else", "is_some", "is_none",
	"is_ok", "is_err", "iter",
	"iter_mut", "into_iter", "enumerate",
	"zip", "filter", "filter_map",
	"fold", "for_each", "collect",
	"count", "find", "any",
	"all", "take", "skip",
	"push", "pop", "insert",
	"remove", "contains", "get",
	"get_mut", "entry", "sort",
	"sort_by", "reverse", "len",
	"is_empty", "capacity", "reserve",
	"read", "read_to_string", "write_all",
	"flush", "open", "create",
	"spawn", "join", "lock",
	"try_lock", "parse", "split",
	"replace", "trim", "starts_with",
	"ends_with"
};

static const char* const library_constants[] = {
	"None", "Some", "Ok",
	"Err", "PI", "E",
	"INFINITY", "NEG_INFINITY", "NAN",
	"Ordering", "Less", "Equal",
	"Greater", "MAX", "MIN",
	"self", "Self", "mut"
};

static const char* const operators[] = {
	">>=", "<<=", "..=",
	"...", "->", "=>",
	"::", "..", "==",
	"!=", "<=", ">=",
	"&&", "||", "<<",
	">>", "+=", "-=",
	"*=", "/=", "%=", "^=", "&=", "|=",
	"!", "%", "^",
	"&", "*", "-",
	"+", "=", "|",
	"<", ">", "/",
	"?", "~",
};

static int start(uint32_t c) {
	return c == '_' || (c < 128 && isalpha((unsigned char) c));
}

static int cont(uint32_t c) {
	return c == '_' ||
		(c < 128 && isalnum((unsigned char) c)) || c >= 128;
}

static int word(uint32_t c) {
	return cont(c);
}

static void emit
(
	struct token* t,
	size_t* n,
	size_t cap,
	enum highlight h,
	size_t a,
	size_t b
)
{
	if (*n < cap) {
		t[*n] = (struct token) {h, a, b};
	}

	if (*n < cap) {
		++*n;
	}
}

// s[a, b) == v
static int eq
(
	const uint32_t* s,
	size_t a,
	size_t b,
	const char* v
)
{
	size_t i = 0;

	for (; v[i] && a + i < b; ++i) {
		if (s[a + i] != (unsigned char) v[i]) {
			return 0;
		}
	}

	return !v[i] && a + i == b;
}

static int listed
(
	const uint32_t* s,
	size_t a,
	size_t b,
	const char* const* xs,
	size_t n
)
{
	for (size_t i = 0; i < n; ++i) {
		if (eq(s, a, b, xs[i])) {
			return 1;
		}
	}

	return 0;
}


// MAIN FUNCTION

static size_t rust_tokenize
(
	struct lexer* base,
	const uint32_t* s,
	size_t len,
	struct token* out,
	size_t cap,
	enum lex_state in,
	enum lex_state* state_out
)
{
	struct rust_lexer* lx = (struct rust_lexer*) base;
	size_t i = 0, n = 0;
	enum lex_state state=in;

	if (state == LEX_STATE_BLOCK_COMMENT && !lx->block_depth) {
		lx->block_depth=1;
	}

	while (i < len) {
		if (state == LEX_STATE_BLOCK_COMMENT) {
			size_t a = i;

			while (i < len) {
				if (i + 1 < len &&
					s[i] == '/' &&
					s[i + 1] == '*')
				{
					++lx->block_depth;
					i+=2;
				}

				else if (i + 1 < len &&
						 s[i] == '*' &&
						 s[i+1] == '/')
				{
					i += 2;

					if (--lx->block_depth==0) {
						state = LEX_STATE_NORMAL;
						break;
					}

				}

				else {
					++i;
				}

			}

			emit(out, &n, cap, HL_COMMENT, a, i);
			continue;
		}

		if (state == LEX_STATE_STRING) {
			size_t a = i;

			while (i < len) {
				if (lx->raw_hashes) {
					if (s[i] == '"' &&
						i + lx->raw_hashes < len)
					{
						size_t j = i + 1, k = 0;

						while(k < lx->raw_hashes &&
							  s[j + k] == '#')
						{
							++k;
						}

						if (k == lx->raw_hashes) {
							emit(out, &n, cap, HL_STRING, a, i);

							emit(
								out,
								&n,
								cap,
								HL_PUNCTUATION,
								i,
								j + k
							);

							i = j + k;
							a = i;
							state = LEX_STATE_NORMAL;
							lx->raw_hashes = 0;

							break;
						}
					}

					++i;
				}

				else if (s[i] == '\\') {
					emit(out, &n, cap, HL_STRING, a, i);

					size_t e = i;
					i += i + 1 < len ? 2 : 1;

					emit(out, &n, cap, HL_NUMBER, e, i);

					a=i;
				}

				else if (s[i] == lx->quote) {
					emit(out, &n, cap, HL_STRING, a, i);
					emit(out, &n, cap, HL_PUNCTUATION, i, i+1);
					++i;
					a = i;
					state = LEX_STATE_NORMAL;
					break;
				}

				else {
					++i;
				}
			}

			if (a < i) {
				emit(out, &n, cap, HL_STRING, a, i);
			}

			continue;
		}

		if (s[i] == ' ' || s[i] == '\t') {
			size_t a = i++;

			while(i < len && (s[i] == ' ' || s[i] == '\t')) {
				++i;
			}

			emit(out, &n, cap, HL_WHITESPACE, a, i);
			continue;
		}

		if (i + 1 < len && s[i] == '/' && s[i + 1] == '/') {
			emit(out, &n, cap, HL_COMMENT, i, len);
			break;
		}

		if (i + 1 < len && s[i] == '/' && s[i + 1] == '*') {
			size_t a = i;

			i += 2;
			lx->block_depth=1;
			state = LEX_STATE_BLOCK_COMMENT;

			while (i < len && lx->block_depth) {
				if(i + 1 < len &&
				   s[i] == '/' &&
				   s[i + 1] == '*')
				{
					++lx->block_depth;
					i += 2;
				}

				else if (i + 1 < len &&
						 s[i] == '*' &&
						 s[i + 1] == '/')
				{
					--lx->block_depth;
					i += 2;
				}

				else {
					++i;
				}
			}

			if (!lx->block_depth) {
				state = LEX_STATE_NORMAL;
			}
			emit(out, &n, cap, HL_COMMENT, a, i);
			continue;
		}

		/* raw strings: r#"..."#, br#"..."# and byte strings. */
		size_t prefix = i, r = i;

		if (s[r] == 'b' && r + 1 < len && s[r + 1] == 'r') {
			++r;
		}

		if (s[r] == 'r') {
			size_t j = r + 1;

			while (j < len && s[j] == '#') {
				++j;
			}

			if (j < len && s[j] == '"') {
				lx->raw_hashes = j - r - 1;
				lx->quote = '"';

				if (prefix < r) {
					emit(out, &n, cap, HL_PUNCTUATION, prefix, r);
				}

				emit(out, &n, cap, HL_PUNCTUATION, r, j);
				emit(out, &n, cap, HL_PUNCTUATION, j, j + 1);

				state = LEX_STATE_STRING;
				i = j + 1;

				continue;
			}
		}

		if (s[i] == '"' ||
		   (s[i] == 'b' && i + 1 < len && s[i + 1] =='"'))
		{
			if (s[i] == 'b') {
				emit(
					out,
					&n,
					cap,
					HL_PUNCTUATION,
					i,
					i + 1
				);

				++i;
			}

			lx->quote = '"';
			lx->raw_hashes = 0;

			state = LEX_STATE_STRING;

			emit(
				out,
				&n,cap,
				HL_PUNCTUATION,
				i,
				i + 1
			);

			++i;
			continue;
		}

		if (s[i]=='\'') {
			// lifetime/label if no closing quote
			// follows a single scalar
			size_t j = i + 1;

			if (j < len && start(s[j])) {
				++j;

				while (j < len && cont(s[j])) {
					++j;
				}

				if (j == len || s[j] != '\'') {
					emit(
						out,
						&n,
						cap,
						HL_SPECIFIER,
						i,
						j
					);

					i = j;
					continue;
				}
			}

			size_t a = i++;

			emit(
				out,
				&n,
				cap,
				HL_PUNCTUATION,
				a,
				i
			);

			while (i < len && s[i] != '\'' && s[i] !='\\') {
				++i;
			}

			emit(
				out,
				&n,
				cap,
				HL_CHAR,
				a + 1,
				i
			);

			if (i < len && s[i] == '\\') {
				size_t e = i++;

				if (i < len) {
					++i;
				}

				emit(
					out,
					&n,
					cap,
					HL_NUMBER,
					e,
					i
				);
			}

			if (i < len && s[i] == '\'') {
				emit(
					out,
					&n,
					cap,
					HL_PUNCTUATION,
					i,
					i + 1
				);

				++i;
			}

			continue;
		}

		if (start(s[i])) {
			size_t a = i++;

			while (i < len && cont(s[i])) {
				++i;
			}

			enum highlight h = HL_NORMAL;

			if (listed(
				s,
				a,
				i,
				keywords,
				ARRAY_SIZE(keywords)))
			{
				h = (eq(s, a, i, "true") ||
					 eq(s, a, i, "false"))
					? HL_CONSTANT
					: HL_KEYWORD;
			}

			else if (listed(
					s,
					a,
					i,
					types,
					ARRAY_SIZE(types)))
			{
				h=HL_TYPE;
			}

			else if (listed(
					s,
					a,
					i,
					library_constants,
					ARRAY_SIZE(library_constants)))
			{
				h=HL_CONSTANT;
			}

			else if (listed(
					s,
					a,
					i,
					library_types,
					ARRAY_SIZE(library_types)))
			{
				h = HL_LIB_TYPE;
			}

			else if (i < len && s[i] == '!' &&
					listed(
						s,
						a,
						i,
						library_functions,
						ARRAY_SIZE(library_functions)))
			{
				h = HL_LIB_FUNCTION;
			}

			else if (i < len && s[i] == '!') {
				h = HL_SPECIFIER;
			}

			else if (i < len && s[i] == '(') {
				h = listed(
						s,
						a,
						i,
						library_functions,
						ARRAY_SIZE(library_functions))
					? HL_LIB_FUNCTION
					: HL_FUNCTION;
			}

			emit(out, &n, cap, h, a, i);
			continue;
		}

		if (s[i] >= '0' && s[i] <= '9') {
			size_t a = i++;

			while (i < len &&
				 (cont(s[i]) || s[i] == '.') &&
				 !(s[i] == '.' && i + 1 < len &&
				   		s[i + 1] == '.'))
			{
				++i;
			}

			emit(out, &n, cap, HL_NUMBER, a, i);

			continue;
		}

		int matched = 0;

		for (size_t k = 0;
			k < ARRAY_SIZE(operators);
			++k)
		{
			size_t j = 0;

			while (operators[k][j] &&
				  i + j < len &&
				  s[i + j] == (unsigned char) operators[k][j])
			{
				++j;
			}

			if(!operators[k][j]) {
				emit(out, &n, cap, HL_OPERATOR, i, i + j);

				i += j;
				matched = 1;

				break;
			}
		}

		if (matched) {
			continue;
		}

		if (s[i] == '(' ||
			s[i] == ')' ||
			s[i] == '[' ||
			s[i]==']'	||
			s[i]=='{'	||
			s[i]=='}'	||
			s[i]==','	||
			s[i]=='.'	||
			s[i]==';'	||
			s[i]==':')
		{
			emit(out, &n, cap, HL_PUNCTUATION, i, i + 1);
		}

		else {
			emit(out, &n, cap, HL_NORMAL, i, i + 1);
		}

		++i;
	}

	if (state_out) {
		*state_out = state;
	}

	return n;
}

static void reset(struct lexer* b) {
	struct rust_lexer* l= (struct rust_lexer*) b;

	l->block_depth = 0;
	l->raw_hashes = 0;
	l->quote = 0;
}

static void destroy(struct lexer* b) {
	free(b);
}

static struct lexer* create(void) {
	struct rust_lexer* l = calloc(1, sizeof(*l));

	if (!l) {
		return NULL;
	}

	l->base.tokenize_line = rust_tokenize;
	l->base.reset = reset;
	l->base.destroy = destroy;
	return &l->base;
}

static const struct language_rules rules = {
	pairs,
	ARRAY_SIZE(pairs),
	comment_fmt,
	1,
	word
};

static const struct language_plugin plugin = {
	"rust",
	extensions,
	ARRAY_SIZE(extensions),
	NULL,
	0,
	create,
	&rules
};

const struct language_plugin* plugin_init(void) {
	return &plugin;
}