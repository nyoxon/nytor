
☺️á
#include <stdlib.h>
#include <stddef.h>
#include <ctype.h>
#include <string.h>

#include "c_lexer.h"

static const char *extensions[] = {
    ".c",
    ".h",
    NULL
};



struct keyword {
    const char* name;
    uint8_t len;

    enum highlight highlight;
};

struct operator {
    const char* text;
    uint8_t len;
};

struct punctuation {
    char text;
};

static const struct keyword C_KEYWORDS[] = {
    {"int",         3, HL_TYPE}, 
    {"void",        4, HL_TYPE},
    {"float",       5, HL_TYPE},
    {"double",      6, HL_TYPE},
    {"short",       5, HL_TYPE},
    {"long",        4, HL_TYPE},
    {"char",        4, HL_TYPE},
    {"unsigned",    8, HL_TYPE},
    {"signed",      6, HL_TYPE},
    {"struct",      6, HL_TYPE},
    {"typedef",     7, HL_TYPE},
    {"auto",        4, HL_TYPE},
    {"enum",        4, HL_TYPE},
    {"union",       5, HL_TYPE},

    {"NULL",        4, HL_CONSTANT},
    {"true",        4, HL_CONSTANT},
    {"false",       5, HL_CONSTANT},

    {"extern",      6, HL_SPECIFIER},
    {"static",      6, HL_SPECIFIER},
    {"const",       5, HL_SPECIFIER},
    {"sizeof",      6, HL_SPECIFIER},
    {"volatile",    8, HL_SPECIFIER},
    {"inline",      6, HL_SPECIFIER},
    {"restrict",    8, HL_SPECIFIER},

    {"return",      6, HL_KEYWORD}, 
    {"if",          2, HL_KEYWORD}, 
    {"else",        4, HL_KEYWORD}, 
    {"for",         3, HL_KEYWORD}, 
    {"case",        4, HL_KEYWORD},
    {"while",       5, HL_KEYWORD},
    {"continue",    8, HL_KEYWORD},
    {"break",       5, HL_KEYWORD},
    {"default",     7, HL_KEYWORD},
    {"do",          2, HL_KEYWORD},
    {"goto",        4, HL_KEYWORD},
    {"register",    8, HL_KEYWORD},
    {"switch",      6, HL_KEYWORD},

    {"include",     7, HL_PREPROCESSOR},
    {"define",      6, HL_PREPROCESSOR},
    {"ifndef",      6, HL_PREPROCESSOR},
    {"endif",       5, HL_PREPROCESSOR}
};

#define C_KEYWORD_COUNT sizeof(C_KEYWORDS) / sizeof(C_KEYWORDS[0])

static const struct operator C_OPERATORS[] = {
    {"<<=",         3},
    {">>=",         3},

    {"--",          2},
    {"++",          2},
    {"==",          2},
    {">=",          2},
    {"<=",          2},
    {"!=",          2},
    {"&&",          2},
    {"||",          2},
    {">>",          2},
    {"<<",          2},
    {"+=",          2},
    {"-=",          2},
    {"*=",          2},
    {"/=",          2},
    {"%=",          2},
    {"&=",          2},
    {"|=",          2},
    {"^=",          2},
    {"->",          2},

    {"+",           1},
    {"-",           1},
    {"/",           1},
    {"*",           1},
    {"%",           1},

    {"=",           1},
    {">",           1},
    {"<",           1},

    {"!",           1},

    {"&",           1},
    {"|",           1},
    {"^",           1},
    {"~",           1},

    {"?",           1}
};

#define C_OPERATOR_COUNT sizeof(C_OPERATORS) / sizeof(C_OPERATORS[0])

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
    {','}
};

#define C_PUNCTUATION_COUNT sizeof(C_PUNCTUATIONS) / sizeof(C_PUNCTUATIONS[0])

static int is_keyword(const char* word, size_t len, int* index) {
    for (size_t i = 0; i < C_KEYWORD_COUNT; i++) {
        if (C_KEYWORDS[i].len != len) {
            continue;
        }

        if (memcmp(word, C_KEYWORDS[i].name, len) == 0) {
            *index = i;
            return 1;
        }
    }

    return 0;
}

static int match_operator
(
    const char* line,
    size_t len,
    size_t pos,
    size_t* op_len
)
{
    for (size_t i = 0; i < C_OPERATOR_COUNT; i++) {
        size_t n = C_OPERATORS[i].len;

        if (pos + n <= len &&
            strncmp(line + pos, C_OPERATORS[i].text, n) == 0)
        {
            *op_len = n;
            return 1;
        }
    }

    return 0;
}

static int is_punctuation(char c) {
    for (size_t i = 0; i < C_PUNCTUATION_COUNT; i++) {
        if (c == C_PUNCTUATIONS[i].text) {
            return 1;
        }
    }

    return 0;   
}

static int is_number_start
(
    const char* line,
    size_t i,
    size_t len
)
{
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
    const char* line,
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



struct c_lexer {
    struct lexer base;

    int in_block_comment;

    const struct keyword* keywords;
    size_t keyword_count;
};

/// language_plugin CALLBACKS

static size_t c_tokenize_line
(
    struct lexer* lexer,
    const char* line,
    size_t len,
    struct token* tokens,
    size_t max_tokens   
)
{
    struct c_lexer* c = (struct c_lexer*) lexer;

    size_t ntokens = 0;
    size_t i = 0;

    int first_token = 1;

    while (i < len && ntokens < max_tokens) {
        while (i < len && isspace((unsigned char) line[i])) {
            i++;
        }

        if (c->in_block_comment) {
            size_t begin = i;

            while (i + 1 < len && 
                !(line[i] == '*' && line[i + 1] == '/'))
            {
                i++;
            }

            if (i + 1 < len) {
                i += 2;
                c->in_block_comment = 0;
            } else {
                i = len;
            }

            tokens[ntokens++] = (struct token) {
                HL_COMMENT,
                begin,
                i
            };

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

            break;
        }


        //  comment /*
        if (!c->in_block_comment &&
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
                c->in_block_comment = 0;
            } else {
                c->in_block_comment = 1;
                i = len;
            }

            tokens[ntokens++] = (struct token) {
                HL_COMMENT,
                begin,
                i
            };

            continue;
        }


        // preprocess
        if (first_token && i < len && line[i] == '#') {
            tokens[ntokens++] = (struct token) {
                HL_PREPROCESSOR,
                i,
                len
            };

            return ntokens;
        }


        // string
        if (line[i] == '"') {
            size_t begin = i++;

            while (i < len) {
                if (line[i] == '\\') {
                    i += 2;
                    continue;
                }

                if (line[i] == '"') {
                    i++;
                    break;
                }

                i++;
            }

            tokens[ntokens++] = (struct token) {
                HL_STRING,
                begin,
                i
            };

            continue;
        }


        // char
        if (line[i] == '\'') {
            size_t begin = i++;

            while (i < len) {
                if (line[i] == '\\') {
                    i += 2;
                    continue;
                }

                if (line[i] == '\'') {
                    i++;
                    break;
                }

                i++;
            }

            tokens[ntokens++] = (struct token) {
                HL_CHAR,
                begin,
                i
            };

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

            continue;
        }

        // identifier
        if (isalpha(line[i]) || line[i] == '_') {
            size_t begin = i;

            while (i < len &&
                (isalnum(line[i]) ||
                line[i] == '_'))
            {
                i++;
            }

            int index = -1;
            enum highlight hl;

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
            }

            tokens[ntokens++] = (struct token) {
                hl,
                begin,
                i
            };

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
            continue;
        }

        // normal caractere
        tokens[ntokens++] = (struct token) {
            HL_NORMAL,
            i,
            i + 1
        };

        i++;
    }

    return ntokens;
}

static void c_reset(struct lexer* lexer) {
    struct c_lexer* c = (struct c_lexer*) lexer;

    c->in_block_comment = 0;
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
    c->keywords = C_KEYWORDS;
    c->keyword_count = C_KEYWORD_COUNT;

    return &c->base;
}

static const struct pair C_PAIRS[] = {
    {'{', '}', 1, 1},
    {'(', ')', 1, 1},
    {'[', ']', 1, 1},
    {'"', '"', 0, 0},
    {'\'', '\'', 0, 1}
};

#define C_PAIR_COUNT sizeof(C_PAIRS) / sizeof(C_PAIRS[0])

static const struct language_rules c_rules = {
    .pairs = C_PAIRS,
    .pair_count = C_PAIR_COUNT,
    .indent_width = 4,
    .auto_indent = 1
};

static const struct language_plugin c_plugin = {
    .name = "c",
    .extensions = extensions,
    .create_lexer = c_create_lexer,
    .rules = &c_rules
};

const struct language_plugin *plugin_init(void)
{
    return &c_plugin;
}