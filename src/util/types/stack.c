#include <stdlib.h>
#include <string.h>
#include <assert.h>

#include "util/types/stack.h"

void stack_init
(
	Stack* s,
	size_t capacity,
	size_t elem_size, 
	Destructor destroy
) 
{
	s->data = malloc(capacity * elem_size);

	s->size = 0;
	s->capacity = capacity;
	s->elem_size = elem_size;
	s->start = 0;

	s->destroy = destroy;
}

void* stack_at(Stack* s, size_t i) {
	size_t index = (s->start + i) % s->capacity;

	return (char*) s->data + index * s->elem_size;
}

void stack_free(Stack* s) {
	if (s->destroy) {
		for (size_t i = 0; i < s->size; i++) {
			s->destroy(stack_at(s, i));
		}
	}

	free(s->data);
}

void stack_push(Stack* s, const void* element) {
	if (s->size == s->capacity) {
		if (s->destroy) {
			s->destroy(stack_at(s, 0));
		}

		memcpy(stack_at(s, 0), element, s->elem_size);

		s->start = (s->start + 1) % s->capacity;
		return;
	}

	memcpy(
		stack_at(s, s->size),
		element,
		s->elem_size);

	s->size++;
}

void stack_pop(Stack* s, void* out) {
	if (s->size == 0) {
		return;
	}

	void* elem = stack_at(s, s->size - 1);

	if (out) {
		memcpy(out, elem, s->elem_size);
	}

	else if (s->destroy) {
		s->destroy(elem);
	}

	s->size--;
}

void* stack_peek(Stack* s) {
	if (s->size == 0) {
		return NULL;
	}

	return stack_at(s, s->size - 1);
}

void stack_clear(Stack* s) {
	if (!s || !s->data || s->size == 0) {
		return;
	}

	stack_free(s);

	s->data = malloc(s->capacity * s->elem_size);
	s->size = 0;
	s->start = 0;
}

int stack_empty(Stack* s) {
	return s->size == 0;
}