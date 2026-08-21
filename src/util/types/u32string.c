#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <unistd.h>

#define STACK_BUF_SIZE 4096

#include "util/types/u32string.h"

u32string u32string_new() {
	u32string string;
	vector_init(&string.text, sizeof(uint32_t), NULL);

	return string;
}

u32string u32string_with_capacity(size_t capacity) {
	u32string string;
	vector_init(&string.text, sizeof(uint32_t), NULL);

	string.text.data = malloc(capacity * sizeof(uint32_t));
	string.text.capacity = capacity;

	return string;
}

u32string u32string_from_raw(uint32_t* text, size_t size) {
	u32string string;
	vector_init(&string.text, sizeof(uint32_t), NULL);

	string.text.data = text;
	string.text.size = size;
	string.text.capacity = size;

	return string;
}

u32string u32string_from_raw_copy(const uint32_t* text, size_t size) {
	if (size > 0) {
		u32string string = u32string_with_capacity(size);

		memcpy(
			string.text.data,
			text,
			size * sizeof (uint32_t)
		);

		string.text.size = size;

		return string;
	}

	else {
		return u32string_new();
	}
}


u32string u32string_slice(const u32string* string, size_t begin, size_t end) {
	Vector slice = vector_slice(&string->text, begin, end);

	u32string out;
	out.text = slice;

	return out;
}

uint32_t* u32string_into_ptr(u32string* string) {
	return string->text.data;
}
const uint32_t* u32string_into_ptr_const(const u32string* string) {
	return (const uint32_t*) string->text.data;
}

uint32_t* u32_strstr
(
	const uint32_t* haystack,
	size_t haystack_size,
	const uint32_t* needle,
	size_t needle_size
)
{
	if (needle_size == 0) {
		return (uint32_t*) haystack;
	}

	if (needle_size > haystack_size) {
		return NULL;
	}

	for (size_t i = 0; i <= haystack_size - needle_size; i++) {
		size_t j = 0;

		while (j < needle_size &&
			haystack[i + j] == needle[j])
		{
			j++;
		}

		if (j == needle_size) {
			return (uint32_t*) (haystack + i);
		}
	}

	return NULL;	
}


uint32_t u32string_char(const u32string* string, size_t index) {
	return *(const uint32_t*) vector_get_const(&string->text, index);
}

void u32string_free(u32string* string) {
	vector_free(&string->text);
}

void u32string_destructor(void* ptr) {
	u32string* string = ptr;

	u32string_free(string);
}


void u32string_push(u32string* string, uint32_t c) {
	vector_push(&string->text, &c);
}

void u32string_insert(u32string* string, uint32_t c, size_t index) {
	vector_insert(&string->text, index, &c);
}

void u32string_remove(u32string* string, size_t index, uint32_t* out) {
	vector_remove(&string->text, index, out);
}

void u32string_replace_char(u32string* string, uint32_t c, size_t index) {
	vector_replace(&string->text, &c, index);
}

void u32string_remove_range(u32string* string, size_t begin, size_t end) {
	vector_remove_range(&string->text, begin, end);
}

void u32string_replace_text_raw
(
	u32string* string, 
	uint32_t* text, 
	size_t size
) 
{
	u32string_free(string);

	string->text.data = text;
	string->text.size = size;
	string->text.capacity = size;
}

void u32string_replace_text_raw_copy
(
	u32string* string,
	const uint32_t* text,
	size_t size
)
{
	if (!string || !text) {
		return;
	}

	u32string_free(string);

	if (size > 0) {
		u32string_reserve(string, size);

		memcpy(
			string->text.data,
			text,
			size * sizeof (uint32_t)
		);

		string->text.size = size;
	}
}

size_t u32string_size(const u32string* string) {
	return string->text.size;
}


// if use_spaces == 0, the returned value will be the number
// of '\t'. in order to do cursor screen operations, you must
// multiply that value by tab_size
size_t u32string_get_indent(const u32string* string, int use_spaces) {
	size_t indent = 0;
	size_t size = u32string_size(string);

	uint32_t c = (use_spaces) ? U' ' : '\t';

	while (indent < size && u32string_char(string, indent) == c) {
		indent++;
	}

	return indent;
}

// if use_spaces == 0, indent must be the number of '\t' you want
// to set at the beginning of the string
void u32string_set_indent
(
	u32string* string, 
	size_t indent,
	int use_spaces
) 
{
	size_t i = 0;
	size_t size = u32string_size(string);
	uint32_t c = (use_spaces) ? U' ' : '\t';

	while (i < size && u32string_char(string, i) == c) {
		i++;
	}

	size_t new_size = indent + (size - i);
	uint32_t* new_text = malloc(new_size * sizeof(*new_text));

	if (!new_text) {
		return;
	}

	for (size_t j = 0; j < indent; j++) {
		new_text[j] = c;
	}

	memcpy(
		new_text + indent, 
		u32string_into_ptr_const(string) + i, 
		(size - i) * sizeof(*new_text));

	u32string_replace_text_raw(string, new_text, new_size);
}

ssize_t u32string_find
(
	const u32string* string,
	size_t from,
	size_t until,
	const u32string* pattern
) 
{
	if (!string || !pattern) {
		return -1;
	}

	if (from > until) {
		return -1;
	}

	if (pattern->text.size == 0) {
		return -1;
	}

	if (pattern->text.size > string->text.size) {
		return -1;
	}

	if (from > string->text.size) {
		return -1;
	}

	if (until - from < u32string_size(pattern)) {
		return -1;
	}

	size_t limit = until - pattern->text.size + 1;

	for (size_t i = from; i < limit; i++) {
		size_t j = 0;

		while (j < pattern->text.size &&
			  ((uint32_t*) string->text.data)[i + j] ==
			  ((uint32_t*) pattern->text.data)[j])
		{
			j++;
		}

		if (j == pattern->text.size) {
			return i;
		}
	}

	return -1;
}

void u32string_reserve(u32string* string, size_t capacity) {
	if (capacity <= string->text.capacity) {
		return;
	}

	size_t new_capacity = string->text.capacity ? string->text.capacity : 8;

	while (new_capacity < capacity) {
		new_capacity *= 2;
	}

	uint32_t* new_text = realloc(
		string->text.data,
		new_capacity * sizeof(*new_text)
	);

	if (!new_text) {
		abort();
	}

	string->text.data = new_text;
	string->text.capacity = new_capacity;
}

void u32string_append_raw
(
	u32string* string, 
	const uint32_t* text, 
	size_t size
) 
{
	if (!string) {
		return;
	}

	if (!text || size == 0) {
		return;
	}

	u32string_reserve(string, u32string_size(string) + size);

	memcpy(
		u32string_into_ptr(string) + u32string_size(string),
		text,
		size  * sizeof(*text)
	);

	string->text.size += size;
}

void u32string_appendu8
(
	u32string * string,
	const char* text
)
{
	if (!string || !text || strlen(text) == 0) {
		return;
	}

	size_t size = strlen(text);

	for (size_t i = 0; i < size; i++) {
		u32string_push(string, (unsigned char) text[i]);
	}
}

void u32string_prepend_raw
(
	u32string* string,
	const uint32_t* text,
	size_t size
)
{
	if (!string) {
		return;
	}

	if (!text || size == 0) {
		return;
	}

	u32string_reserve(string, string->text.size + size);

	memmove(
		(uint32_t*) string->text.data + size,
		(uint32_t*) string->text.data,
		string->text.size * sizeof(uint32_t)
	);

	memcpy(
		(uint32_t*) string->text.data,
		text,
		size * sizeof(uint32_t)
	);

	string->text.size += size;
}

void u32string_insert_range_raw
(
	u32string* string,
	size_t index,
	const uint32_t* text,
	size_t count
)
{
	if (!string || !text) {
		return;
	}

	if (index > string->text.size) {
		return;
	}

	if (count == 0) {
		return;
	}

	u32string_reserve(string, string->text.size + count);

	memmove(
		(uint32_t*) string->text.data + (index + count),
		(uint32_t*) string->text.data + index,
		(string->text.size - index) * sizeof(uint32_t)
	);

	memcpy(
		(uint32_t*) string->text.data + index,
		text,
		count * sizeof(uint32_t)
	);

	string->text.size += count;
}

u32string u32string_clone(const u32string* string) {
	u32string out = u32string_with_capacity(string->text.capacity);
 
	for (size_t i = 0; i < u32string_size(string); i++) {
		u32string_push(&out, u32string_char(string, i));
	}

	return out;
}

void u32string_cloner(const void* src, void* dst) {
	const u32string* str1 = src;
	u32string* str2 = dst;

	*str2 = u32string_clone(str1);
}

ssize_t u32string_strstr
(
	u32string* haystack, 
	u32string* needle
) 
{
	if (u32string_is_empty(needle)) {
		return 0;
	}

	size_t needle_size = u32string_size(needle);
	size_t haystack_size = u32string_size(haystack);

	if (needle_size > haystack_size) {
		return -1;
	}

	for (size_t i = 0; i <= haystack_size - needle_size; i++) {
		size_t j = 0;

		while (j < needle_size &&
			u32string_char(haystack, i + j) == u32string_char(needle, j))
		{
			j++;
		}

		if (j == needle_size) {
			return i;
		}
	}

	return -1;
}

uint32_t* u32string_chr(const u32string* string, uint32_t c) {
	if (!string || u32string_is_empty(string)) {
		return NULL;
	}

	for (size_t i = 0; i < u32string_size(string); i++) {
		if (u32string_char(string, i) == c) {
			return u32string_into_ptr((u32string*) string) + i;
		}
	}

	return NULL;
}

uint32_t* u32string_rchr(const u32string* string, uint32_t c) {
	if (!string || u32string_is_empty(string)) {
		return NULL;
	}

	for (size_t i = u32string_size(string); i-- > 0;) {
		if (u32string_char(string, i) == c) {
			return u32string_into_ptr((u32string*) string) + i;
		}
	}

	return NULL;
}

int u32string_equal(const u32string* a, const u32string* b) {
	size_t a_size = u32string_size(a);
	size_t b_size = u32string_size(b);

	if (a_size < b_size) {
		return 0;
	}

	else if (a_size > b_size) {
		return 0;
	}

	for (size_t i = 0; i < a_size; i++) {
		if (u32string_char(a, i) != u32string_char(b, i)) {
			return 0;
		}
	}

	return 1;
}

int u32string_equaln(const u32string* a, const u32string* b, size_t n) {
	size_t a_size = u32string_size(a);
	size_t b_size = u32string_size(b);

	if (a_size < n || b_size < n) {
		return a_size == b_size && u32string_equal(a, b);
	}

	for (size_t i = 0; i < n; i++) {
		if (u32string_char(a, i) != u32string_char(b, i)) {
			return 0;
		}
	}

	return 1;
}

int u32string_equalu32
(
	const u32string* a, 
	const uint32_t* b, 
	size_t b_size
)
{
	size_t a_size = u32string_size(a);

	if (a_size < b_size) {
		return 0;
	}

	else if (a_size > b_size) {
		return 0;
	}

	for (size_t i = 0; i < a_size; i++) {
		if (u32string_char(a, i) != b[i]) {
			return 0;
		}
	}

	return 1;	
}

int u32string_equalnu32
(
	const u32string* a, 
	const uint32_t* b,
	size_t b_size, 
	size_t n
)
{
	size_t a_size = u32string_size(a);

	if (a_size < n || b_size < n) {
		return a_size == b_size && u32string_equalu32(a, b, b_size);
	}

	for (size_t i = 0; i < n; i++) {
		if (u32string_char(a, i) != b[i]) {
			return 0;
		}
	}

	return 1;	
}

int u32string_equalu8(const u32string* a, const char* b) {
	size_t i = 0;

	while (b[i]) {
		if (i >= u32string_size(a)) {
			return 0;
		}

		if (u32string_char(a, i) != (unsigned char) b[i]) {
			return 0;
		}

		i++;
	}

	return 1;
}

int u32string_equalnu8(const u32string* a, const char* b, size_t n) {
	size_t a_size = u32string_size(a);
	size_t b_size = strlen(b);

	if (a_size < n || b_size < n) {
		return a_size == b_size && u32string_equalu8(a, b);
	}

	for (size_t i = 0; i < n; i++) {
		if (u32string_char(a, i) != (unsigned char) b[i]) {
			return 0;
		}
	}

	return 1;
}

int u32string_equalpu8(const u32string* a, size_t pos, const char* b) {
	size_t i = 0;

	while (b[i]) {
		if (pos + i >= u32string_size(a)) {
			return 0;
		}

		if (u32string_char(a, pos + i) != (unsigned char) b[i]) {
			return 0;
		}

		i++;
	}

	return 1;
}

size_t u32string_common_prefix(const u32string* a, const u32string* b) {
	size_t a_size = u32string_size(a);
	size_t b_size = u32string_size(b);
	size_t min_size = (a_size < b_size)
		? a_size
		: b_size;

	size_t len = 0;

	for (size_t i = 0; i < min_size; i++) {
		if (u32string_char(a, i) != u32string_char(b, i)) {
			return len;
		}

		len++;
	}

	return len;
}

int u32string_is_comment
(
	const u32string* string, 
	size_t indent,
	const char* comment_fmt,
	size_t comment_fmt_size
) 
{
	if (indent + comment_fmt_size > u32string_size(string)) {
		return 0;
	}

	return u32string_equalpu8(string, indent, comment_fmt);
}

int u32string_is_empty(const u32string* string) {
	return (string->text.size == 0);
}

long u32string_stol(const u32string* string) {
	long value = 0;

	size_t size = u32string_size(string);
	const uint32_t* text = u32string_into_ptr_const(string);

	for (size_t i = 0; i < size; i++) {
		if (*text < U'0' || *text > U'9') {
			return -1;
		}

		value = value * 10 + (*text - U'0');
		text++;
	}

	return value;	
}

int u32_isspace(uint32_t c) {
	return c == U' '  ||
		   c == U'\t' ||
		   c == U'\n';
}


int u32_encode(uint32_t cp, char out[4]) {
	if (cp <= 0x7F) {
		out[0] = cp;

		return 1;
	}

	if (cp <= 0x7FF) {
		out[0] = 0xC0 | (cp >> 6);
		out[1] = 0x80 | (cp & 0x3F);

		return 2;
	}

	if (cp <= 0xFFFF) {
		out[0] = 0xE0 | (cp >> 12);
		out[1] = 0x80 | ((cp >> 6) & 0x3F);
		out[2] = 0x80 | (cp & 0x3F);

		return 3;
	}

	out[0] = 0xF0 | (cp >> 18);
	out[1] = 0x80 | ((cp >> 12) & 0x3F);
	out[2] = 0x80 | ((cp >> 6) & 0x3F);
	out[3] = 0x80 | (cp & 0x3F);

	return 4;
}

int u32_decode(const char* text, size_t size, uint32_t* cp) {
	const unsigned char* s = (const unsigned char*) text;

	if (size == 0) {
		return -1;
	}

	if ((s[0] & 0x80) == 0) {
		*cp = s[0];
		return 1;
	}

	if ((s[0] & 0xE0) == 0xC0) {
		if (size < 2) {
			return -1;
		}

		*cp = ((s[0] & 0x1F) << 6) |
			   (s[1] & 0x3F);

		return 2;
	}

	if ((s[0] & 0xF0) == 0xE0) {
		if (size < 3) {
			return -1;
		}

		*cp = ((s[0] & 0x0F) << 12) |
			  ((s[1] & 0x3F) << 6) |
			   (s[2] & 0x3F);

		return 3;
	}

	if ((s[0] & 0xF8) == 0xF0) {
		if (size < 4) {
			return -1;
		}

		*cp = ((s[0] & 0x07) << 18) |
			  ((s[1] & 0x3F) << 12) |
			  ((s[2] & 0x3F) << 6) |
			   (s[3] & 0x3F);

		return 4;
	}

	return -1;	
}

size_t utf8_to_u32
(
	const char* text, 
	uint32_t* out, 
	size_t out_size
) 
{
	if (!text || !out) {
		return 0;
	}

	size_t size = strlen(text);

	if (out_size > size) {
		out_size = size;
	}

	size_t in = 0;

	while (in < out_size) {
		int n = u32_decode(text + in, size - in, &out[in]);

		if (n < 0) {
			return in;
		}

		in += n;
	}

	return in;
}

char* u32string_into_u8(const u32string* string) {
	if (!string) {
		return NULL;
	}

	size_t size = u32string_size(string);
	char* out = malloc(size * 4 + 1);

	if (!out) {
		return NULL;
	}
	size_t out_pos = 0;

	for (size_t i = 0; i < size; i++) {
		char utf8[4];

		int n = u32_encode(u32string_char(string, i), utf8);

		memcpy(out + out_pos, utf8, n);

		out_pos += n;
	}

	out[out_pos] = '\0';

	return out;
}

u32string u32string_from(const char* text) {
	size_t size = strlen(text);

	u32string string = u32string_with_capacity(size);

	size_t in = 0;

	while (in < size) {
		uint32_t cp;
		int n = u32_decode(text + in, size - in, &cp);

		if (n < 0) {
			return u32string_new();
		}

		vector_push(&string.text, &cp);
		in += n;
	}

	return string;
}

size_t u32string_print_range(const u32string* string, size_t start, size_t n) {
	if (!string || !string->text.data) {
		return 0;
	}

	size_t string_size = u32string_size(string);

	if (start > string_size) {
		start = string_size;
	}

	size_t size = string_size - start;

	if (n > size) {
		n = size;
	}

	int len;
	size_t bytes = 0;

	if (4 * n <= STACK_BUF_SIZE) {
		char buf[STACK_BUF_SIZE];

		for (size_t i = start; i < start + n; i++) {
			len = u32_encode(u32string_char(string, i), buf + bytes);

			bytes += (size_t) len;
		}

		write(STDOUT_FILENO, buf, bytes);
	}

	// malloc fallback
	else {
		char* buf = malloc(4 * n * sizeof(*buf));

		if (!buf) {
			return 0;
		}

		for (size_t i = start; i < start + n; i++) {
			len = u32_encode(u32string_char(string, i), buf + bytes);

			bytes += (size_t) len;
		}

		write(STDOUT_FILENO, buf, bytes);

		free(buf);		
	}

	return bytes;
}

size_t u32string_print(const u32string* string) {
	return u32string_print_range(string, 0, u32string_size(string));
}

size_t u32string_printn(const u32string* string, size_t n) {
	return u32string_print_range(string, 0, n);
}

void u32_print_cp(uint32_t cp) {
    char utf8[4];
    int len = u32_encode(cp, utf8);

    write(STDOUT_FILENO, utf8, len);
}

void u32_print(const uint32_t* string, size_t size) {
	if (size == 0) {
		return;
	}

	char buf[4 * size]; // VLA bla and blue

	size_t bytes = 0;
	int len;

	for (size_t i = 0; i < size; i++) {
		len = u32_encode(string[i], buf + bytes);

		bytes += (size_t) len;
	}

	write(STDOUT_FILENO, buf, bytes);
}


int u32_is_printable(uint32_t cp) {
	if (cp > 0x10FFFF) {
		return 0;
	}

	if (cp < 0x20) {
		return 0;
	}

	if (cp >= 0x7F && cp <= 0x9F) {
		return 0;
	}

	return 1;
}

char* u32_to_utf8(const uint32_t* text, size_t len) {
	if (!text) {
		return NULL;
	}

	char* out = malloc(len * 4 + 1);

	size_t out_pos = 0;

	for (size_t i = 0; i < len; i++) {
		char utf8[4];

		int n = u32_encode(text[i], utf8);

		memcpy(out + out_pos, utf8, n);

		out_pos += n;
	}

	out[out_pos] = '\0';

	return out;
}

long u32stol(const u32string* s) {
	long value = 0;

	uint32_t* end = u32string_into_ptr((u32string*) s);
	size_t size = u32string_size(s);

	for (size_t i = 0; i < size; i++) {
		if (*end < U'0' || *end > U'9') {
			return -1;
		}

		value = value * 10 + (*end - '0');
		end++;
	}

	return value;
}

void u32cpy
(
	uint32_t* dst, size_t dst_size, 
	const uint32_t* src, size_t src_size
)
{
	if (dst_size < src_size) {
		return;
	}

	for (size_t i = 0; i < src_size; i++) {
		dst[i] = src[i];
	}
}

void u32ncpy
(
	uint32_t* dst, size_t dst_size, 
	const uint32_t* src, size_t src_size,
	size_t n
)
{
	if (dst_size < src_size) {
		return;
	}

	size_t end = (n < src_size)
		? n
		: src_size;

	for (size_t i = 0; i < end; i++) {
		dst[i] = src[i];
	}
}


/// ---  HASH_TABLE CALLBACKS  ---

// djb2
size_t hash_u32string(const void* ptr) {
	const u32string* string = ptr;
	size_t size = u32string_size(string);

	size_t hash = 5381;
	uint32_t c;

	size_t x = 0;
	while (x < size && (c = u32string_char(string, x++))) {
		hash = hash * 33 + c;
	}

	return hash;
}

bool equals_u32string(const void* a, const void* b) {
	const u32string* str1 = a;
	const u32string* str2 = b;

	return u32string_equal(str1, str2);
}