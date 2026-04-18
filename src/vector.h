#ifndef VECTOR_H
#define VECTOR_H

#include <stddef.h>

/* a simple implementation of a vector for internal use */

typedef void (*Destructor)(void*);
// typedef void (*Printer)(void*);

typedef struct {
	void* data;
	size_t size;
	size_t capacity;
	size_t elem_size;
	Destructor destroy;
	// Printer printer;
} Vector;

void vector_init(Vector* v, size_t elem_size, Destructor destroy);
void vector_realloc(Vector* v, size_t new_capacity);
void vector_push(Vector* v, void* element);
void vector_free(Vector* v);
void vector_insert(Vector* v, size_t index, void* element);
void vector_remove(Vector* v, size_t index, void* out);
void vector_remove_and_destroy(Vector* v, size_t index);
// void vector_add_printer(Vector* v, Printer printer);
// void vector_print(Vector* v);

int vector_swap(Vector* v, size_t i, size_t j);

void* vector_get(Vector* v, size_t index);
// void* vector_get_unchecked(Vector* v, size_t index);



#endif