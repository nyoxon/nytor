#include "vector.h"
#include <stdlib.h>
#include <string.h>
#include <assert.h>

void vector_init(Vector* v, size_t elem_size, Destructor destroy) {
	v->data = NULL;
	v->size = 0;
	v->capacity = 0;
	v->elem_size = elem_size;
	v->destroy = destroy;
	v->printer = NULL;
}

static void vector_assert_non_null(Vector* v) {
	assert(v != NULL && "pointer to the vector is null");
}

static void vector_assert_new_capacity(Vector* v, size_t new_capacity) {
	assert(new_capacity > v->capacity && "new_capacity must be greater than actual capacity");	
}

static void vector_assert_bounds(Vector* v, size_t index) {
	assert(index < v->size && "index out of bounds");
}

static void vector_assert_non_null_destroyer(Vector* v) {
	assert(v->destroy != NULL && "vector has no destroyer, but a function that needs it has been called");
}

static void vector_assert_non_null_printer(Vector* v) {
	assert(v->printer != NULL && "vector has no printer, but a function that needs it has been called. use vector_add_printer() to add a printer to the vector");
}

static void vector_assert_valid_out_or_invalid_destroyer(Vector *v, void* out) {
	assert(!(!out && v->destroy) && "(vector_remove) out is a null pointer but vector has a destroyer (memory leak)");
}

void vector_realloc(Vector* v, size_t new_capacity) {
	// caller must ensure that v is non-nulled

	vector_assert_new_capacity(v, new_capacity);

	v->data = realloc(v->data, new_capacity * v->elem_size);
	v->capacity = new_capacity;
}

void vector_push(Vector* v, void* element) {
	vector_assert_non_null(v);

	if (v->size == v->capacity) {
		size_t new_capacity = v->capacity == 0 ? 4 : v->capacity * 2;
		vector_realloc(v, new_capacity);
	}

	void* target = (char*) v->data + (v->size * v->elem_size);
	memcpy(target, element, v->elem_size);

	v->size++;
}

void* vector_get(Vector* v, size_t index) {
	vector_assert_non_null(v);
	vector_assert_bounds(v, index);

	return (char*) v->data + (index * v->elem_size);
}

void* vector_get_unchecked(Vector* v, size_t index) {
	return (char*) v->data + (index * v->elem_size);	
}

void vector_free(Vector* v) {
	vector_assert_non_null(v);

	if (v->destroy) {
		for (size_t i = 0; i < v->size; i++) {
			void* elem = (char*) v->data + (i * v->elem_size);
			v->destroy(elem);
		}
	}

	free(v->data);
}

static void vector_shift_right(Vector* v, size_t index) {
	// the caller needs to ensure that v and index are valid values

	void* dest = (char*) v->data + (index + 1) * v->elem_size;
	void* src =  (char*) v->data + index * v->elem_size;
	size_t n_bytes = (v->size - index) * v->elem_size;

	memmove(dest, src, n_bytes);
}

static void vector_shift_left(Vector* v, size_t index) {
	// the caller needs to ensure that v and index are valid values

	void* dest = (char*) v->data + index * v->elem_size;
	void* src = (char*) v->data + (index + 1) * v->elem_size;
	size_t n_bytes = (v->size - index - 1) * v->elem_size;

	memmove(dest, src, n_bytes);
}

void vector_insert(Vector* v, size_t index, void* element) {
	vector_assert_non_null(v);
	
	if (index > v->size) {
		return;
	}

	if (v->size == v->capacity) {
		size_t new_capacity = v->capacity == 0 ? 4 : v->capacity * 2;
		vector_realloc(v, new_capacity);
	}

	vector_shift_right(v, index);

	void* target = (char*) v->data + (index * v->elem_size);
	memcpy(target, element, v->elem_size);

	v->size++;
}

void vector_remove(Vector* v, size_t index, void* out) {
	vector_assert_non_null(v);
	vector_assert_bounds(v, index);
	vector_assert_valid_out_or_invalid_destroyer(v, out);

	void* element = (char*) v->data	+ index * v->elem_size;

	if (out) {
		memcpy(out, element, v->elem_size);
	}

	vector_shift_left(v, index);
	v->size--;
}

void vector_remove_and_destroy(Vector* v, size_t index) {
	vector_assert_non_null(v);
	vector_assert_non_null_destroyer(v);
	vector_assert_bounds(v, index);

	void* element = (char*) v->data + index * v->elem_size;

	v->destroy(element);
	vector_shift_left(v, index);
	v->size--;
}

void vector_add_printer(Vector* v, Printer printer) {
	vector_assert_non_null(v);

	v->printer = printer;
}

void vector_print(Vector* v) {
	vector_assert_non_null(v);
	vector_assert_non_null_printer(v);

	for (size_t i = 0; i < v->size; i++) {
		void* element = (char*) v->data + i * v->elem_size;
		v->printer(element);
	}
}

int vector_swap(Vector* v, size_t i, size_t j) {
	vector_assert_non_null(v);
	vector_assert_bounds(v, i);
	vector_assert_bounds(v, j);

	if (i == j) {
		return -1;
	}

	void* a = (char*) v->data + (i * v->elem_size);
	void* b = (char*) v->data + (j * v->elem_size);

	void* tmp = malloc(v->elem_size);

	if (!tmp) {
		return -1;
	}

	memcpy(tmp, a, v->elem_size);
	memcpy(a, b, v->elem_size);
	memcpy(b, tmp, v->elem_size);

	free(tmp);

	return 0;
}