#ifndef U32STRING_H
#define U32STRING_H

#include <unistd.h>
#include <stddef.h>
#include <stdint.h>

#include "util/types/vector.h"

typedef struct {
	Vector text;
} u32string;

void u32string_free(u32string* string);

u32string u32string_new();
u32string u32string_with_capacity(size_t capacity);
u32string u32string_from(const char* text);
u32string u32string_from_raw(uint32_t* text, size_t size);
u32string u32string_from_raw_copy(const uint32_t* text, size_t size);
u32string u32string_slice(u32string* string, size_t begin, size_t end);
u32string u32string_clone(const u32string* string);

void u32string_push(u32string* string, uint32_t c);
void u32string_insert(u32string* string, uint32_t c, size_t index);
void u32string_remove(u32string* string, size_t index, uint32_t* out);
void u32string_replace_char(u32string* string, uint32_t c, size_t index);
void u32string_remove_range(u32string* string, size_t begin, size_t end);

void u32string_replace_text_raw
(
	u32string* string, 
	uint32_t* text, 
	size_t size
);

void u32string_replace_text_raw_copy
(
	u32string* string,
	const uint32_t* text,
	size_t size
);

size_t u32string_print_range(const u32string* string, size_t start, size_t n);
size_t u32string_print(const u32string* string);
size_t u32string_printn(const u32string* string, size_t n);

void u32string_set_indent
(
	u32string* string, 
	size_t indent,
	int use_spaces
);

void u32string_reserve(u32string* string, size_t capacity);

void u32string_append_raw
(
	u32string* string, 
	const uint32_t* text, 
	size_t size
);

void u32string_appendu8
(
	u32string * string,
	const char* text
);

void u32string_prepend_raw
(
	u32string* string,
	const uint32_t* text,
	size_t size
);

void u32string_insert_range_raw
(
	u32string* string,
	size_t index,
	const uint32_t* text,
	size_t count
);

ssize_t u32string_strstr(u32string* haystack, u32string* needle);
uint32_t u32string_char(const u32string* string, size_t index);

uint32_t* u32string_chr(const u32string* string, uint32_t c);
uint32_t* u32string_rchr(const u32string* string, uint32_t c);

char* u32string_into_u8(const u32string* string);
uint32_t* u32string_into_ptr(u32string* string);
const uint32_t* u32string_into_ptr_const(const u32string* string);

uint32_t* u32_strstr
(
	const uint32_t* haystack,
	size_t haystack_size,
	const uint32_t* needle,
	size_t needle_size
);

int u32string_is_empty(const u32string* string);

int u32string_equal(const u32string* a, const u32string* b);
int u32string_equaln(const u32string* a, const u32string* b, size_t n);

int u32string_equalu32
(
	const u32string* a, 
	const uint32_t* b, 
	size_t b_size
);

int u32string_equalnu32
(
	const u32string* a, 
	const uint32_t* b,
	size_t b_size, 
	size_t n
);

int u32string_equalu8(const u32string* a, const char* b);
int u32string_equalnu8(const u32string* a, const char* b, size_t n);
int u32string_equalpu8(const u32string* a, size_t pos, const char* b);

size_t u32string_common_prefix(const u32string* a, const u32string* b);

long u32string_stol(const u32string* string);

int u32string_is_comment
(
	const u32string* string, 
	size_t indent,
	const char* comment_fmt,
	size_t comment_fmt_size
);

size_t u32string_size(const u32string* string);
size_t u32string_get_indent(const u32string* string, int use_spaces);

ssize_t u32string_find
(
	const u32string* string,
	size_t from,
	size_t until,
	const u32string* pattern
);

int u32_encode(uint32_t cp, char out[4]);
int u32_decode(const char* text, size_t size, uint32_t* cp);

void u32cpy
(
	uint32_t* dst, size_t dst_size, 
	const uint32_t* src, size_t src_size
);

void u32ncpy
(
	uint32_t* dst, size_t dst_size, 
	const uint32_t* src, size_t src_size,
	size_t n
);

long u32stol(const u32string* s);
int u32_is_printable(uint32_t cp);
char* u32_to_utf8(const uint32_t* text, size_t len);
int u32_isspace(uint32_t c);

void u32_print(uint32_t cp);


#endif
