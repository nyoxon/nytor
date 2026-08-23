#include <stdlib.h>
#include <stddef.h>
#include <ctype.h>
#include <string.h>
#include <assert.h>

#include "c_lexer.h"

#define ARRAY_SIZE(x) (sizeof(x) / sizeof((x)[0]))

static int posix = 1;

/// --- TYPE DEFINITIONS ---

int is_c_word_char(uint32_t c) {
	unsigned char k = (unsigned char) c;

	return (isalnum(k) || k == '_');
}


// RESERVED FOR INTERNAL KEYWORDS OF THE LANG
struct keyword {
	const char* name;
	uint8_t len; // AVOIDS EXTENSIVE USE OF STRLEN()

	enum highlight highlight;
};


// WHILE SEVERAL LANGUAGE-INTERNAL keyword MAY HAVE
// DIFFERENT HL TYPES, SOME specific_keyword WILL HAVE
// HL DEFINED BY THE SEMANTICS OF THE ARRAY THAT STORES
// THEM 
struct specific_keyword {
	const char* name;
	uint8_t len;
};


// LOGICALLY EQUAL, BUT SEMANTICALLY DIFFERENT FROM
// specific_keyword

// '+', '-', '>' etc 
struct operator {
	const char* text;
	uint8_t len;
};



// LOGICALLY EQUAL, BUT SEMANTICALLY DIFFERENT FROM
// specific_keyword with postulated len = 1

// '.', ',', '{', '[' etc
struct punctuation {
	unsigned char text;
};




/// --- C LANG SPECIFIC DEFINITIONS ---	


static const char comment_fmt[] = "//";


// -- USED THE FIRST TIME A FILE IS LOADED --
static const char* extensions[] = {
	".c",
	".h"
};


// -- pair DEFINED IN include/plugins/plugin.h
static const struct pair C_PAIRS[] = {
	{'{', '}', 1, 1},
	{'(', ')', 1, 1},
	{'[', ']', 1, 1},
	{'"', '"', 0, 1},
	{'\'', '\'', 0, 1}
};

static const struct keyword C_KEYWORDS[] = {
	{"int", 		3, HL_TYPE},
	{"bool",		4, HL_TYPE},
	{"void", 		4, HL_TYPE},
	{"float", 		5, HL_TYPE},
	{"double", 		6, HL_TYPE},
	{"short", 		5, HL_TYPE},
	{"long", 		4, HL_TYPE},
	{"char", 		4, HL_TYPE},
	{"unsigned", 	8, HL_TYPE},
	{"signed", 		6, HL_TYPE},
	{"struct", 		6, HL_TYPE},
	{"typedef", 	7, HL_TYPE},
	{"auto", 		4, HL_TYPE},
	{"enum", 		4, HL_TYPE},
	{"union", 		5, HL_TYPE},

	{"NULL", 		4, HL_CONSTANT},
	{"nullptr",		7, HL_CONSTANT},
	{"true",		4, HL_CONSTANT},
	{"false",		5, HL_CONSTANT},

	{"extern", 		6, HL_SPECIFIER},
	{"static", 		6, HL_SPECIFIER},
	{"const", 		5, HL_SPECIFIER},
	{"constexpr",	9, HL_SPECIFIER},
	{"sizeof", 		6, HL_SPECIFIER},
	{"alignof",		7, HL_SPECIFIER},
	{"volatile", 	8, HL_SPECIFIER},
	{"inline", 		6, HL_SPECIFIER},
	{"restrict", 	8, HL_SPECIFIER},

	{"return", 		6, HL_KEYWORD}, 
	{"if", 			2, HL_KEYWORD}, 
	{"else", 		4, HL_KEYWORD}, 
	{"for", 		3, HL_KEYWORD}, 
	{"case", 		4, HL_KEYWORD},
	{"while", 		5, HL_KEYWORD},
	{"continue", 	8, HL_KEYWORD},
	{"break", 		5, HL_KEYWORD},
	{"default", 	7, HL_KEYWORD},
	{"do", 			2, HL_KEYWORD},
	{"goto", 		4, HL_KEYWORD},
	{"register", 	8, HL_KEYWORD},
	{"switch", 		6, HL_KEYWORD}
};


// hl = HL_PREPROCESSOR
static const struct specific_keyword C_PREPROCESSOR[] = {
	{"include", 	7},
	{"define",		6},
	{"undef",		5},
	{"if",			2},
	{"ifndef", 		6},
	{"ifdef",		5},
	{"endif",		5},
	{"else",		4},
	{"error",		5},
	{"elifdef",		7},
	{"elif",		4},
	{"elifndef",	8}, // C23
	{"warning",		7}, // not padronized
	{"line",		4}, 
	{"pragma",		6}, // not padronized
	{"ident",		5} // not padronized
};


// IN EACH OF THE FOLLOWING LIBS THERE A LOT
// OF OTHER DECLARED FUNCTIONS

// hl = HL_LIB_FUNCTION
static const struct specific_keyword C_LIB[] = {
	// stdio.h
	{"printf",      6},
	{"dprintf",		7},
	{"fprintf",     7},
	{"sprintf",     7},
	{"snprintf",    8},
	{"vprintf",		7},
	{"vfprintf",	8},
	{"vdprintf",	8},
	{"vsprintf",	8},
	{"vsnprintf", 	9},
	{"vasprintf",	9},
	{"asprintf",	8},

	{"scanf",		5},
	{"fscanf",		6},
	{"sscanf", 		6},
	{"vscanf",		6},
	{"vfscanf",		7},
	{"vsscanf",		7},

	{"puts",		4},
	{"fputs",		5},
	{"putchar",		7},
	{"fputc",		5},

	{"gets",		4},
	{"fgets",		5},
	{"getchar",		7},
	{"fgetc",		5},
	{"getc",		4},
	{"ungetc",		6},

	{"fopen",		5},
	{"fdopen",		6},
	{"freopen",		7},
	{"fclose",		6},

	{"fread",		5},
	{"fwrite",		6},

	{"fflush",		6},
	{"fseek",		5},
	{"ftell",		5},
	{"rewind",		6},
	{"fgetpos",		7},
	{"fsetpos",		7},

	{"remove",		6},
	{"rename",		6},
	{"tmpfile",		7},
	{"tmpnam",		6},

	{"perror",		6},

	{"feof",		4},
	{"ferror",		6},
	{"clearerr",	8},


	// stdlib.h
	{"malloc",		6},
	{"calloc",		6},
	{"realloc",		7},
	{"free",		4},

	{"abort",		5},
	{"exit",		4},
	{"quick_exit",	10},
	{"_Exit",		5},
	{"atexit",		6},
	{"at_quick_exit", 13},

	{"system",		6},
	{"getenv",		6},

	{"bsearch",		7},
	{"qsort",		5},

	{"abs",			3},
	{"labs",		4},
	{"llabs",		5},
	{"div",			3},
	{"ldiv",		4},
	{"lldiv",		5},

	{"atoi", 		4},
	{"atol",		4},
	{"atoll",		5},
	{"atof",		4},

	{"strtol",		6},
	{"strtoll",		7},
	{"strtoul",		7},
	{"strtoull",	8},

	{"strtof",		6},
	{"strtod",		6},
	{"strtold",		7},

	{"rand",		4},
	{"srand",		5},

	{"aligned_alloc", 13},


	// string.h
	{"memcpy",		6},
	{"memmove",		7},
	{"memset",		6},
	{"memcmp",		6},
	{"memchr",		6},

	{"strcpy",		6},
	{"strncpy",		7},
	{"strcat",		6},
	{"strncat", 	7},

	{"strcmp",		6},
	{"strncmp", 	7},

	{"strlen",		6},

	{"strchr",		6},
	{"strrchr",		7},
	{"strchrnul",	9},

	{"strstr",		6},
	{"strcasestr",	1},
	{"strpbrk",		7},

	{"strtok",		6},
	{"strtok_r",	8},

	{"strspn",		6},
	{"strcspn",		7},

	{"strerror",	8},

	{"strcoll", 	7},
	{"strxfrm",		7},
	
	{"strdup",		6},
	

	// math.h (x, xf, xl 😔)
	{"sin",			3},
	{"sinf",		4},
	{"sinl",		4},
	{"sinh",		4},
	{"sinhf",		5},
	{"sinhl",		5},
	{"cos",			3},
	{"cosf",		4},
	{"cosl",		4},
	{"cosh",		4},
	{"coshf",		5},
	{"coshl",		5},
	{"tan",			3},
	{"tanf",		4},
	{"tanl",		4},
	{"tanh",		4},
	{"tanhf",		5},
	{"tanhl",		5},

	{"asin", 		4},
	{"asinf",		5},
	{"asinl",		5},
	{"asinh",		5},
	{"asinhf",		6},
	{"asinhl",		6},
	{"acos",		4},
	{"acosf",		5},
	{"acosl",		5},
	{"acosh",		5},
	{"acoshf",		6},
	{"acoshl",		6},
	{"atan",		4},
	{"atanf",		5},
	{"atanl",		5},
	{"atanh",		5},
	{"atanhf",		6},
	{"atanhl",		6},
	{"atan2",		5},
	{"atan2f",		6},
	{"atan2l",		6},

	{"erf",			3},
	{"erff",		4},
	{"erfl",		4},
	{"tgamma",		6},
	{"tgammaf",		7},
	{"tgammal",		7},

	{"exp",			3},
	{"expf",		4},
	{"expl",		4},
	{"exp2",		4},
	{"exp2f",		5},
	{"exp2l",		5},
	{"exp10",		5},
	{"exp10f",		6},
	{"exp10l",		6},
	{"log",			3},
	{"logf",		4},
	{"logl",		4},
	{"log2",		4},
	{"log2f",		5},
	{"log2l",		5},
	{"log10",		5},
	{"log10f",		6},
	{"log10l",		6},

	{"pow",			3},
	{"powf",		4},
	{"powl",		4},
	{"sqrt",		4},
	{"sqrtf",		5},
	{"sqrtl",		5},
	{"cbrt",		4},
	{"cbrtf",		5},
	{"cbrtl",		5},

	{"hypot",		5},
	{"hypotf",		6},
	{"hypotl",		6},

	{"ceil",		4},
	{"ceilf",		5},
	{"ceill",		5},
	{"floor",		5},
	{"floorf",		6},
	{"floorl",		6},
	{"round",		5},
	{"roundf",		6},
	{"roundl",		6},
	{"trunc",		5},
	{"truncf",		6},
	{"truncl",		6},
	{"nearbyint",	9},
	{"nearbyintf", 10},
	{"nearbyintl", 10},
	{"rint",		4},
	{"rintf",		5},
	{"rintl",		5},

	{"fabs",		4},
	{"fabsf",		5},
	{"fabsl",		5},
	{"fmod",		4},
	{"fmodf",		5},
	{"fmodl",		5},
	{"remainder",	9},
	{"remainderf", 10},
	{"remainderl", 10},
	{"modf",		4},
	{"modff",		5},
	{"modfl",		5},
	{"drem", 		4},
	{"dremf",		5},
	{"dreml",		5},

	{"copysign",	8},
	{"copysignf",	9},
	{"copysignl",	9},
	{"fdim",		4},
	{"fdimf",		5},
	{"fdiml",		5},
	{"fmax",		4},
	{"fmaxf",		5},
	{"fmaxl",		5},
	{"fmin",		4},
	{"fminf",		5},
	{"fminl",		5},

	{"frexp",		5},
	{"frexpf",		6},
	{"frexpl",		6},
	{"ldexp",		5},
	{"ldexpf",		6},
	{"ldexpl",		6},

	{"isnan",		5},
	{"isinf",		5},
	{"isnormal",	8},
	{"isfinite",	8},
	{"fpclassify", 10},



	// complex.h
	{"cabs",		4},
	{"casbf",		5},
	{"cabsl",		5},
	{"carg",		4},
	{"cargf",		5},
	{"cargl",		5},
	{"cexp",		4},
	{"cexpf",		5},
	{"cexpl",		5},
	{"clog",		4},
	{"clogf",		5},
	{"clogl",		5},
	{"cpow",		4},
	{"cpowf",		5},
	{"cpowl",		5},
	{"csqrt",		5},
	{"csqrtf",		6},
	{"csqrtl",		6},
	{"csin",		4},
	{"csinf",		5},
	{"csinl",		5},
	{"ccos",		4},
	{"ccosf",		5},
	{"ccosl",		5},
	{"ctan",		4},
	{"ctanf",		5},
	{"ctanl",		5},



	// ctype.h
	{"isalnum",		7},
	{"isalpha",		7},
	{"isblank",		7},
	{"iscntrl",		7},
	{"isdigit",		7},
	{"isgraph",		7},
	{"islower",		7},
	{"isprint",		7},
	{"inpunct",		7},
	{"isspace",		7},
	{"isupper",		7},
	{"isxdigit",	8},
	{"isascii",		7},

	{"tolower",		7},
	{"toupper",		7},



	// time.h
	{"time",		4},
	{"clock",		5},
	{"difftime",	8},
	{"mktime",		6},

	{"gmtime",		6},
	{"localtime",	9},

	{"strftime",	8},
	{"asctime",		7},
	{"ctime",		5},



	// wchar.h
	{"wprintf",		7},
	{"fwprintf",	8},
	{"swprintf",	8},

	{"wscanf",		6},
	{"fwscanf",		7},
	{"swscanf",		7},

	{"wcslen",		6},
	{"wcscpy",		6},
	{"wcscmp",		6},

	{"btowc",		5},
	{"wctob",		5},

	{"mbrtowc",		7},
	{"wcrtomb",		7}
};



static const struct specific_keyword C_LIB_KEY[] = {
	// stddef.h	
	{"size_t",			6},
	{"ptrdiff_t",		9},
	{"max_align_t",	   11},
	{"nullptr_t",		9},



	// stdint.h
	{"int8_t",			6},
	{"int16_t",			7},
	{"int32_t",			7},
	{"int64_t",			7},

	{"uint8_t",			7},
	{"uint16_t",		8},
	{"uint32_t",		8},
	{"uint64_t",		8},

	{"intptr_t",		8},
	{"uintptr_t",		9},

	{"intmax_t",		8},
	{"uintmax_t",		9},

	{"int_least8_t",	12},
	{"int_least16_t",	13},
	{"int_least32_t",	13},
	{"int_least64_t",	13},

	{"uint_least8_t",	13},
	{"uint_least16_t",	14},
	{"uint_least32_t",	14},
	{"uint_least64_t",	14},

	{"int_fast8_t",		11},
	{"int_fast16_t",	12},
	{"int_fast32_t",	12},
	{"int_fast64_t",	12},

	{"uint_fast8_t",	12},
	{"uint_fast16_t",	13},
	{"uint_fast32_t",	13},
	{"uint_fast64_t",	13},



	// stdbool.h
	{"_Bool",		5},



	// stdio.h
	{"FILE",		4},
	{"fpos_t",		6},



	// time.h
	{"time_t",		6},
	{"clock_t",		7},
	{"tm",			2},
	{"timespec",	8},



	// wchar.h
	{"wchar_t",		7},
	{"wint_t",		6},
	{"mbstate_t",	9},


	// complex.h
	{"complex",		7},
	
	
	// signal.h
	{"sig_atomic_t",	12}
};



static const struct operator C_OPERATORS[] = {
	{"<<=",			3},
	{">>=",			3},

	{"--", 			2},
	{"++", 			2},
	{"==",			2},
	{">=",			2},
	{"<=",			2},
	{"!=",			2},
	{"&&",			2},
	{"||",			2},
	{">>",			2},
	{"<<",			2},
	{"+=",			2},
	{"-=",			2},
	{"*=",			2},
	{"/=",			2},
	{"%=",			2},
	{"&=",			2},
	{"|=",			2},
	{"^=",			2},
	{"->",			2},

	{"+",			1},
	{"-",			1},
	{"/",			1},
	{"*",			1},
	{"%",			1},

	{"=",			1},
	{">",			1},
	{"<",			1},

	{"!",			1},

	{"&",			1},
	{"|",			1},
	{"^",			1},
	{"~",			1},

	{"?",			1}
};



static const struct punctuation C_PUNCTUATIONS[] = {
	{'('},
	{')'},
	{'{'},
	{'}'},
	{'['},
	{']'},
	{';'},
	{':'},
	{'.'},
	{','},
	{'\\'}
};



#define C_PUNCTUATION_COUNT ARRAY_SIZE(C_PUNCTUATIONS)
#define C_OPERATOR_COUNT ARRAY_SIZE(C_OPERATORS)
#define C_LIB_KEY_COUNT ARRAY_SIZE(C_LIB_KEY)
#define C_LIB_COUNT ARRAY_SIZE(C_LIB)
#define C_PREPROCESSOR_COUNT ARRAY_SIZE(C_PREPROCESSOR)
#define C_KEYWORD_COUNT ARRAY_SIZE(C_KEYWORDS)
#define C_PAIR_COUNT ARRAY_SIZE(C_PAIRS)
#define EXTENSION_COUNT ARRAY_SIZE(extensions)



static int cmp_utf8_to_ascii
(
	const uint32_t* line,
	size_t len,
	size_t pos,
	const char* word
)
{
	size_t i = 0;

	while (word[i]) {
		if (pos + i >= len) {
			return 0;
		}

		if (line[pos + i] != (unsigned char) word[i]) {
			return 0;
		}

		i++;
	}

	return 1;
}

static int is_keyword(const uint32_t* word, size_t len, int* index) {
	for (size_t i = 0; i < C_KEYWORD_COUNT; i++) {
		if (C_KEYWORDS[i].len != len) {
			continue;
		}

		if (cmp_utf8_to_ascii(word, len, 0, C_KEYWORDS[i].name)) {
			*index = i;
			return 1;
		}
	}

	return 0;
}

static int is_preprocessor(const uint32_t* word, size_t len) {
	for (size_t i = 0; i < C_PREPROCESSOR_COUNT; i++) {
		if (C_PREPROCESSOR[i].len != len) {
			continue;
		}

		if (cmp_utf8_to_ascii(word, len, 0, C_PREPROCESSOR[i].name)) {
			return 1;
		}
	}

	return 0;
}

static int is_lib(const uint32_t* word, size_t len) {
	for (size_t i = 0; i < C_LIB_COUNT; i++) {
		if (C_LIB[i].len != len) {
			continue;
		}

		if (cmp_utf8_to_ascii(word, len, 0, C_LIB[i].name)) {
			return 1;
		}
	}

	return 0;
}

static int is_lib_key(const uint32_t* word, size_t len) {
	for (size_t i = 0; i < C_LIB_KEY_COUNT; i++) {
		if (C_LIB_KEY[i].len != len) {
			continue;
		}

		if (cmp_utf8_to_ascii(word, len, 0, C_LIB_KEY[i].name)) {
			return 1;
		}
	}

	return 0;
}


static int match_operator
(
	const uint32_t* line,
	size_t len,
	size_t pos,
	size_t* op_len
)
{
	for (size_t i = 0; i < C_OPERATOR_COUNT; i++) {
		size_t n = C_OPERATORS[i].len;

		if (pos + n <= len &&
			cmp_utf8_to_ascii(line, len, pos, C_OPERATORS[i].text))
		{
			*op_len = n;
			return 1;
		}
	}

	return 0;
}

static int is_punctuation(uint32_t c) {
	for (size_t i = 0; i < C_PUNCTUATION_COUNT; i++) {
		if (c == C_PUNCTUATIONS[i].text) {
			return 1;
		}
	}

	return 0;	
}

static int is_number_start
(
	const uint32_t* line,
	size_t i,
	size_t len
)
{
	if (i >= len) {
		return 0;
	}

	if (isdigit((unsigned char) line[i])) {
		return 1;
	}

	// .5
	if (line[i] == '.' &&
		(i + 1 < len) &&
		isdigit((unsigned char) line[i + 1]))
	{
		return 1;
	}

	return 0;
}

static void consume_number
(
	const uint32_t* line,
	size_t len,
	size_t* i
)
{
	size_t pos = *i;

	// 0x or 0X
	if (line[pos] == '0' &&
		(pos + 1 < len) &&
		((line[pos + 1] == 'x') ||
		(line[pos + 1] == 'X')))
	{
		pos += 2;

		while (pos < len &&
			isxdigit((unsigned char) line[pos]))
		{
			pos++;
		}

		goto suffix;
	}


	// 0b or 0B
	if (line[pos] == '0' &&
		(pos + 1 < len) &&
		((line[pos + 1] == 'b') ||
		(line[pos + 1] == 'B')))
	{
		pos += 2;

		while (pos < len &&
			((line[pos] == '0') ||
			(line[pos] == '1')))
		{
			pos++;
		}

		goto suffix;
	}


	// whole part
	while (pos < len &&
		isdigit((unsigned char) line[pos]))
	{
		pos++;
	}


	// decimal part
	if (pos < len && line[pos] == '.') {
		pos++;

		while (pos < len &&
			isdigit((unsigned char) line[pos]))
		{
			pos++;
		}
	}


	// expoent
	if (pos < len &&
		((line[pos] == 'e') ||
		(line[pos] == 'E')))
	{
		pos++;

		if (pos < len &&
			((line[pos] == '+') ||
			(line[pos] == '-')))
		{
			pos++;
		}

		while (pos < len &&
			isdigit((unsigned char) line[pos]))
		{
			pos++;
		}
	}


suffix:
	while (pos < len &&
		((line[pos] == 'u') ||
		(line[pos] == 'U') ||
		(line[pos] == 'f') ||
		(line[pos] == 'F') ||
		(line[pos] == 'l') ||
		(line[pos] == 'L')))
	{
		pos++;
	}

	*i = pos;
}


// POSIX.1-2008
static const struct specific_keyword C_POSIX[] = {
	// unistd.h
	{"access",		6},
	{"alarm",		5},
	{"chdir",		5},
	{"chown",		5},
	{"close",		5},
	{"dup",			3},
	{"dup2",		4},
	{"execv",		5},
	{"execve",		6},
	{"execvp",		6},
	{"_execvpe",	8},
	{"fchdir",		6},
	{"fork",		4},
	{"fsync",		5},
	{"ftruncate",	9},

	{"getcwd",		6},
	{"getegid",		7},
	{"geteuid",		7},
	{"getgid",		6},
	{"getgroups",	9},
	{"gethostname",11},
	{"getlogin",	8},
	{"getpagesize",11},
	{"getpgid",		7},
	{"getpgrp",		7},
	{"getpid",		6},
	{"getppid",		7},
	{"getuid",		6},

	{"isatty",		6},
	{"link",		4},
	{"lseek",		5},
	{"pipe",		4},
	{"pread",		5},
	{"pwrite",		6},
	{"read",		4},
	{"readlink",	8},
	{"rmdir",		5},

	{"setgid",		6},
	{"setpgid",		7},
	{"setsid",		6},
	{"setuid",		6},

	{"sleep",		5},
	{"symlink",		7},
	{"sync",		4},
	{"sysconf",		7},
	{"tcgetpgrp",	9},
	{"tcsetpgrp",	9},
	{"truncate",	8},
	{"ttyname",		7},
	{"unlink",		6},
	{"usleep",		6},
	{"write",		5},



	// fcntl.h
	{"open",		4},
	{"openat",		6},
	{"creat",		5},
	{"fcntl",		5},



	// dirent.h
	{"opendir",		7},
	{"fdopendir",	9},
	{"readdir",		7},
	{"rewinddir",	9},
	{"seekdir",		7},
	{"telldir",		7},
	{"closedir",	8},
	{"dirfd",		5},



	// sys/stat.h
	{"chmod",		5},
	{"fchmod",		6},
	{"fstat",		5},
	{"fstatat",		7},
	{"lstat",		5},
	{"mkdir",		5},
	{"mkdirat",		7},
	{"mkfifo",		6},
	{"mknod",		5},
	{"stat",		4},
	{"umask",		5},
	{"utimensat",	9},



	// sys/mman.h
	{"mmap",		4},
	{"munmap",		6},
	{"mprotec",		7},
	{"msync",		5},
	{"mlock",		5},
	{"munlock",		7},
	{"shm_open",	8},
	{"shm_unlink", 10},



	// sys/wait.h
	{"wait",		4},
	{"waitpid",		7},
	{"waitid",		6},



	// signal.h
	{"kill",		4},
	{"killpg",		6},
	{"raise",		5},

	{"sigaction",	9},
	{"sigaddset",	9},
	{"sigdelset",	9},
	{"sigemptyset",11},
	{"sigfillset", 10},
	{"sigismember",11},
	{"sigpending", 10},
	{"sigprocmask",11},
	{"sigqueue",	8},
	{"sigsuspend", 10},
	{"sigtimedwait",12},
	{"sigwait",		7},
	{"sigwaitinfo",11},



	// pthread.h
	{"pthread_create",			14},
	{"pthread_join",			12},
	{"pthread_exit",			12},
	{"pthread_self",			12},

	{"pthread_mutex_init",		18},
	{"pthread_mutex_lock",		18},
	{"pthread_mutex_unlock",	20},
	{"pthread_mutex_destroy",	21},

	{"pthread_cond_init",		17},
	{"pthread_cond_wait",		17},
	{"pthread_cond_signal",		19},
	{"pthread_cond_broadcast",	22},

	{"pthread_rwlock_init",		19},
	{"pthread_rwlock_rdlock",	21},
	{"pthread_rwlock_wrlock",	21},
	{"pthread_rwlock_unlock",	21},

	{"pthread_once",			12},

	{"pthread_key_create",		18},
	{"pthread_setspecific",		19},
	{"pthread_getspecific",		19},

	{"pthread_cancel",			14},
	{"pthread_detach",			14},



	// semaphore.h
	{"sem_init",		8},
	{"sem_destroy",		11},
	{"sem_wait",		8},
	{"sem_trywat",		10},
	{"sem_post",		8},
	{"sem_open",		8},
	{"sem_close",		9},
	{"sem_unlink",		10},



	// poll.h
	{"poll",		4},
	{"ppoll",		5},



	// sys/select.h
	{"select",		6},
	{"pselect",		7},



	// sys/socket.h
	{"socket",			6},
	{"socketpair",	   10},
	{"accept",			6},
	{"accept4",			7},
	{"bind",			4},
	{"connect",			7},
	{"listen",			6},
	{"recv",			4},
	{"recvfrom",		8},
	{"recvmsg",			7},
	{"send",			4},
	{"sendto",			6},
	{"sendmsg",			7},
	{"shutdown",		8},
	{"getsockname",	   11},
	{"getpeername",	   11},
	{"setsockopt",	   10},
	{"getsockopt",	   10},



	// netdb.h
	{"getaddrinfo",		11},
	{"freeaddrinfo",	12},
	{"gai_sterror",		11},
	{"getnameinfo",		11},



	// time.h
	{"clock_gettime",	13},
	{"clock_settime",	13},
	{"clock_getres",	12},
	{"nanosleep",		 9},
	{"timer_create",	12},
	{"timer_delete",	12},
	{"timer_settime",	13},
	{"timer_gettime",	13},
};


static const struct specific_keyword C_POSIX_KEY[] = {
	{"ssize_t",			7},
	{"off_t",			5},
	{"pid_t",			5},
	{"uid_t",			5},
	{"gid_t",			5},
	{"mode_t",			6},
	{"dev_t",			5},
	{"ino_t",			5},
	{"nlink_t",			7},
	{"blkcnt_t",		8},
	{"blksize_t",		9},
	{"fsblkcnt_t",		10},
	{"fsfilcnt_t",		10},
	{"key_t",			5},
	{"id_t",			4},
	{"useconds_t",		10},
	{"suseconds_t",		11},
	{"socklen_t",		9},
	{"nfds_t",			6},
	{"pthread_t",		9},
	{"pthread_attr_t",	14},
	{"pthread_mutex_t",	15},
	{"pthread_cond_t",	14},
	{"pthread_rwlock_t",16},
	{"pthread_key_t",	13},
	{"pthread_once_t",	14},
	{"pthread_spinlock_t", 18},
	{"pthread_barrier_t",  17},
	{"sem_t",			5},
	{"DIR",				3},
	
	
	// signal.h
	{"SIGHUP",			6},
	{"SIGINT",			6},
	{"SIGQUIT",			7},
	{"SIGILL",			6},
	{"SIGTRAP",			7},
	{"SIGABRT",			7},
	{"SIGBUS",			6},
	{"SIGFPE",			6},
	{"SIGKILL",			7},
	{"SIGUSR1",			7},
	{"SIGSEGV",			7},
	{"SIGUSR2",			7},
	{"SIGPIPE",			7},
	{"SIGALARM",		8},
	{"SIGTERM",			7},
	{"SIGSTKFLT",		9},
	{"SIGCHLD",			7},
	{"SIGCONT",			7},
	{"SIGSTOP",			7},
	{"SIGTSTP",			7},
	{"SIGTTIN",			7},
	{"SIGTTOU",			7},
	{"SIGURG",			6},
	{"SIGXCPU",			7},
	{"SIGXFSZ",			7},
	{"SIGVTALRM",		9},
	{"SIGPROF",			7},
	{"SIGWINCH",		8},
	{"SIGIO",			5},
	{"SIGPWR",			6},
	{"SIGSYS",			6},
	
	
	// unistd.h
	{"STDIN_FILENO",	12},
	{"STDOUT_FILENO",	13},
	{"STDERR_FILENO",	13}
};

#define C_POSIX_COUNT ARRAY_SIZE(C_POSIX)
#define C_POSIX_KEY_COUNT ARRAY_SIZE(C_POSIX_KEY)

static int is_posix(const uint32_t* word, size_t len) {
	for (size_t i = 0; i < C_POSIX_COUNT; i++) {
		if (C_POSIX[i].len != len) {
			continue;
		}

		if (cmp_utf8_to_ascii(word, len, 0, C_POSIX[i].name)) {
			return 1;
		}
	}

	return 0;
}

static int is_posix_key(const uint32_t* word, size_t len) {
	for (size_t i = 0; i < C_POSIX_KEY_COUNT; i++) {
		if (C_POSIX_KEY[i].len != len) {
			continue;
		}

		if (cmp_utf8_to_ascii(word, len, 0, C_POSIX_KEY[i].name)) {
			return 1;
		}
	}

	return 0;
}

static int is_hex(uint32_t c) {
	return ('0' <= c && c <= '9') ||
		   ('a' <= c && c <= 'f') ||
		   ('A' <= c && c <= 'F');
}

static size_t c_escape_length(const uint32_t* s, size_t len) {
	if (len < 2 || *s != '\\') {
		return 0;
	}

	switch (s[1]) {
	case '\'':
	case '"':
	case '?':
	case '\\':
	case 'a':
	case 'b':
	case 'f':
	case 'n':
	case 't':
	case 'r':
	case 'v':
		return 2;
	}

	if (s[1] >= '0' && s[1] <= '7') {
		size_t i = 1;

		while (i < len && i <= 3 && s[i] >= '0' && s[i] <= '7') {
			i++;
		}

		return i;
	}

	if (s[1] == 'x') {
		size_t i = 2;

		while (i < len && is_hex(s[i])) {
			i++;
		}

		if (i == 2) {
			return 0; // only a "\x"
		}

		return i;
	}

	if (s[1] == 'u') {
		if (len < 6) {
			return 0;
		}

		for (size_t i = 2; i < 6; i++) {
			if (!is_hex(s[i])) {
				return 0;
			}
		}

		return 6;
	}

	if (s[1] == 'U') {
		if (len < 10) {
			return 0;
		}

		for (size_t i = 2; i < 10; i++) {
			if (!is_hex(s[i])) {
				return 0;
			}
		}

		return 10;
	}

	return 0;
}

enum c_lexer_state {
	C_LEX_NORMAL,
	C_LEX_BLOCK_COMMENT,
	C_LEX_STRING,
	C_LEX_CHAR,
	C_LEX_PREPROCESSOR,
	C_LEX_FUNC
};


struct c_lexer {
	struct lexer base;

	enum c_lexer_state state;
};

static void handle_multiline_lexer_state
(
	struct c_lexer* lexer,
	const uint32_t* line,
	size_t len,
	size_t* i,
	struct token* tokens,	
	size_t* ntokens,
	enum highlight hl,
	enum lex_state* state_out
)
{
	enum lex_state state = (hl == HL_STRING)
		? LEX_STATE_STRING
		: LEX_STATE_CHAR;

	uint32_t c = (hl == HL_STRING)
		? U'"'
		: U'\'';

	size_t begin = *i;

	while (*i < len) {
		if (line[*i] == '\\') {
			size_t n = c_escape_length(
				line + *i,
				len - *i
			);

			if (n != 0) {
				if (begin != *i) {
					tokens[(*ntokens)++] = (struct token) {
						hl,
						begin,
						*i
					};
				}

				tokens[(*ntokens)++] = (struct token) {
					HL_NUMBER,
					*i,
					*i + n
				};

				*i += n;
				begin = *i;
				continue;
			}
			
			(*i)++;
			continue;
		}

		if (line[*i] == c) {
			state = LEX_STATE_NORMAL;

			lexer->state = C_LEX_NORMAL;
			break;
		}

		(*i)++;
	}

	if (begin != *i) {
		tokens[(*ntokens)++] = (struct token) {
			hl,
			begin,
			*i
		};				
	}

	if (*i < len && line[*i] == c) {
		tokens[(*ntokens)++] = (struct token) {
			HL_PUNCTUATION,
			*i,
			*i + 1
		};

		(*i)++;		
	}

	if (state_out) {
		*state_out = state;
	}	
}

/// --- language_plugin CALLBACKS ---

static size_t c_tokenize_line
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
	struct c_lexer* c = (struct c_lexer*) lexer;

	size_t ntokens = 0;
	size_t i = 0;

	if (len == 0) {
		return ntokens;
	}

	int first_token = 1;
	int state_has_changed = 0;

	while (i < len && ntokens < max_tokens) {
		while (i < len && isspace((unsigned char) line[i])) {
			i++;
		}

		if (i >= len) {
			break;
		}

		if (c->state == C_LEX_BLOCK_COMMENT ||
			(!state_has_changed && state_in == LEX_STATE_BLOCK_COMMENT)) 
		{
			size_t begin = i;

			while (i + 1 < len && 
				!(line[i] == '*' && line[i + 1] == '/'))
			{
				i++;
			}

			if (i + 1 < len) {
				i += 2;
				c->state = C_LEX_NORMAL;

				if (state_out) {
					*state_out = LEX_STATE_NORMAL;
				}

				state_has_changed = 1;
			} 

			else {
				i = len;

				if (state_out) {
					*state_out = LEX_STATE_BLOCK_COMMENT;
				}

				state_has_changed = 1;
			}

			tokens[ntokens++] = (struct token) {
				HL_COMMENT,
				begin,
				i
			};

			first_token = 0;
			continue;
		}

		if (c->state == C_LEX_PREPROCESSOR && line[i] == '<') {
			tokens[ntokens++] = (struct token) {
				HL_PUNCTUATION,
				i,
				i + 1
			};

			i++;
			size_t begin = i;

			while (i < len) {
				if (line[i] == '>') {
					break;
				}

				i++;
			}

			if (begin != i) {
				tokens[ntokens++] = (struct token) {
					HL_STRING,
					begin,
					i
				};				
			}

			if (i < len && line[i] == '>') {
				tokens[ntokens++] = (struct token) {
					HL_PUNCTUATION,
					i,
					i + 1
				};

				i++;			
			}

			first_token = 0;
			continue;			
		}

		if (c->state == C_LEX_STRING ||
			(!state_has_changed && state_in == LEX_STATE_STRING))
		{
			state_has_changed = 1;

			handle_multiline_lexer_state(
				c,
				line,
				len,
				&i,
				tokens,
				&ntokens,
				HL_STRING,
				state_out
			);

			first_token = 0;
			continue;
		}

		if (c->state == C_LEX_CHAR ||
			(!state_has_changed && state_in == LEX_STATE_CHAR))
		{
			state_has_changed = 1;

			handle_multiline_lexer_state(
				c,
				line,
				len,
				&i,
				tokens,
				&ntokens,
				HL_CHAR,
				state_out
			);

			first_token = 0;
			continue;
		}		

		//   comment //
		if (i + 1 < len &&
			line[i] == '/' &&
			line[i + 1] == '/')
		{
			tokens[ntokens++] = (struct token) {
				HL_COMMENT,
				i,
				len
			};

			first_token = 0;
			break;
		}

		// 	comment /*
		if (c->state == C_LEX_NORMAL &&
			i + 1 < len &&
			line[i] == '/' &&
			line[i + 1] == '*')
		{
			size_t begin = i;

			i += 2;

			while (i + 1 < len &&
				!(line[i] == '*' && line[i + 1] == '/'))
			{
				i++;
			}

			if (i + 1 < len) {
				i += 2;
				c->state = C_LEX_NORMAL;

				if (state_out) {
					*state_out = LEX_STATE_NORMAL;

					state_has_changed = 1;
				}
			} 

			else {
				c->state = C_LEX_BLOCK_COMMENT;
				i = len;

				if (state_out) {
					*state_out = LEX_STATE_BLOCK_COMMENT;

					state_has_changed = 1;
				}
			}

			tokens[ntokens++] = (struct token) {
				HL_COMMENT,
				begin,
				i
			};

			first_token = 0;
			continue;
		}


		// preprocess
		if (first_token && i < len && line[i] == '#') {
			size_t begin_line = i;
			i++;
			
			while (i < len && 
				(isspace((unsigned char) line[i])))
			{
				i++;
			}
			
			size_t begin_word = i;

			while (i < len &&
				(isalnum((unsigned char) line[i]) ||
				line[i] == '_')) // TODO
			{
				i++;
			}

			enum highlight hl;

			if (is_preprocessor(line + begin_word, 
				i - begin_word)) 
			{
				hl = HL_PREPROCESSOR;
				
				if (line[begin_word] == U'p') {
					tokens[ntokens++] = (struct token) {
						hl,
						0,
						len
					};
					
					break;
				}
			} else {
				hl = HL_NORMAL;
			}

			tokens[ntokens++] = (struct token) {
				hl,
				begin_line,
				begin_line + 1
			};

			first_token = 0;

			if (hl == HL_PREPROCESSOR) {
				tokens[ntokens++] = (struct token) {
					hl,
					begin_word,
					i
				};

				c->state = C_LEX_PREPROCESSOR;
			}

			else {
				tokens[ntokens++] = (struct token) {
					hl,
					begin_word - 1,
					i
				};
			}

			continue;
		}


		// string
		if (line[i] == '"') {
			tokens[ntokens++] = (struct token) {
				HL_PUNCTUATION,
				i,
				i + 1
			};

			state_has_changed = 1;

			i++;

			handle_multiline_lexer_state(
				c,
				line,
				len,
				&i,
				tokens,
				&ntokens,
				HL_STRING,
				state_out
			);

			first_token = 0;
			continue;
		}

		// char
		if (line[i] == '\'') {
			tokens[ntokens++] = (struct token) {
				HL_PUNCTUATION,
				i,
				i + 1
			};

			state_has_changed = 1;

			i++;

			handle_multiline_lexer_state(
				c,
				line,
				len,
				&i,
				tokens,
				&ntokens,
				HL_CHAR,
				state_out
			);

			first_token = 0;
			continue;
		}


		// digit/number
		if (is_number_start(line, i, len)) {
			size_t begin = i;

			consume_number(line, len, &i);

			tokens[ntokens++] = (struct token) {
				HL_NUMBER,
				begin,
				i
			};

			first_token = 0;
			continue;
		}

		// identifier
		if (isalpha((unsigned char) line[i]) || line[i] == '_') {
			// isalpha is used because a variable/function name in C
			// cannot start with a number

			size_t begin = i;
			int is_meth_or_att = 
				(begin > 0 && line[begin - 1] == '.') ||
				(begin > 1 && line[begin - 1] == '>' && line[begin - 2] == '-');

			// but might contain numbers
			while (i < len && is_c_word_char(line[i]))
			{
				i++;
			}

			enum highlight hl;

			if (is_meth_or_att) {
				hl = HL_METHOD_OR_ATTRIB;
				goto create_token;
			}

			int index = -1;

			if (is_keyword(line + begin, i - begin, &index)) {
				hl = C_KEYWORDS[index].highlight;
			} else {
				hl = HL_NORMAL;

				size_t j = i;

				// function
				while (j < len && isspace((unsigned char) line[j])) {
					j++;
				}

				if (j < len && line[j] == '(') {
					hl = HL_FUNCTION;
				}

				if (first_token && j < len && line[j] == ':') {
					hl = HL_FUNCTION;
				}
			}

			if (hl == HL_NORMAL) {
				if (posix && is_posix_key(line + begin, i - begin)) {
					hl = HL_POSIX_TYPE;

					goto create_token;
				}

				if (is_lib_key(line + begin, i - begin)) {
					hl = HL_LIB_TYPE;

					goto create_token;
				}
			}


			if (hl == HL_FUNCTION) {
				if (posix && is_posix(line + begin, i - begin)) {
					hl = HL_POSIX;

					goto create_token;
				}

				if (is_lib(line + begin, i - begin)) {
					hl = HL_LIB_FUNCTION;

					goto create_token;
				}
			}

		create_token:
			tokens[ntokens++] = (struct token) {
				hl,
				begin,
				i
			};

			first_token = 0;
			continue;
		}

		// operator
		size_t op_len;

		if (match_operator(line, len, i, &op_len)) {
			tokens[ntokens++] = (struct token) {
				HL_OPERATOR,
				i,
				i + op_len
			};

			first_token = 0;

			i += op_len;
			continue;
		}


		// punctuation
		if (is_punctuation(line[i])) {
			tokens[ntokens++] = (struct token) {
				HL_PUNCTUATION,
				i,
				i + 1,
			};

			i++;
			first_token = 0;
			continue;
		}

		// normal caractere
		tokens[ntokens++] = (struct token) {
			HL_NORMAL,
			i,
			i + 1
		};

		first_token = 0;

		i++;
	}

	if (c->state == C_LEX_PREPROCESSOR) {
		c->state = C_LEX_NORMAL;
	}

	if (!state_has_changed && state_out) {
		*state_out = state_in;
	}

	return ntokens;
}

static void c_reset(struct lexer* lexer) {
	struct c_lexer* c = (struct c_lexer*) lexer;

	c->state = C_LEX_NORMAL;
}

static void c_destroy(struct lexer* lexer) {
	free(lexer);
}

/// CREATER

struct lexer* c_create_lexer(void) {
	struct c_lexer* c = calloc(1, sizeof(*c));

	c->base.tokenize_line = c_tokenize_line;
	c->base.destroy = c_destroy;
	c->base.reset = c_reset;

	c->state = C_LEX_NORMAL;

	return &c->base;
}

static const struct language_rules c_rules = {
	.pairs = C_PAIRS,
	.pair_count = C_PAIR_COUNT,
	.comment_fmt = comment_fmt,
	.auto_indent = 1,
	.is_word_char = is_c_word_char
};

static const struct language_plugin c_plugin = {
	.name = "c",
	.extensions = extensions,
	.extension_count = EXTENSION_COUNT,
	.reserved_filenames = NULL,
	.reserved_filename_count = 0,
	.create_lexer = c_create_lexer,
	.rules = &c_rules
};

const struct language_plugin *plugin_init(void)
{
    return &c_plugin;
}