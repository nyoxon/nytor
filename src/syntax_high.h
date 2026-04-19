#ifndef SYNTAX_HIGH_H
#define SYNTAX_HIGH_H

#include "buffer.h"

/*
AUXILIARY DEFINITIONS AND FUNCTIONS TO HANDLE SYNTAX HIGHLIGHT
*/

#define COLOR_SIZE 5

#define COMMENT_COLOR "\033[90m"
#define TYPE_COLOR "\033[34m"
#define FLOW_COLOR "\033[31m"
#define SPECIFIER_COLOR "\033[31m"
#define NUMBER_COLOR "\033[36m"
#define FUNCTION_COLOR "\033[33m"
#define MACRO_COLOR "\033[32m"
#define NULL_COLOR "\033[35m"

#define RESET_COLOR "\033[0m"
#define RESET_COLOR_SIZE 4

#define INVERT_COLOR "\033[7m"
#define INVERT_COLOR_SIZE 4

void reset_color();
void invert_color();
void write_color(const char* color);

int check_line_in_selection(size_t y, size_t start_y, size_t end_y);

#define SYN_START 0
#define SYN_END 1

int check_forms_a_comment_fmt(size_t x, const Line* line, int type);
int check_has_a_comment(size_t x, const Line* line);
int check_is_keyword(const char* word, size_t len, int* index);
int check_is_function(size_t x, const Line* line);

size_t get_size_of_word(size_t x, const Line* line);

int is_word_char(char c);
int is_digit(char c);

#endif