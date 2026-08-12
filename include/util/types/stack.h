#ifndef STACK_H
#define STACK_H

// must considerer stack->destroy
// but that concept is not used in the project until now

typedef void (*Destructor) (void*);

typedef struct {
	void* data;

	size_t size;
	size_t capacity;
	size_t elem_size;

	size_t start;

	Destructor destroy;
} Stack;

void stack_init
(
	Stack* s,
	size_t capacity,
	size_t elem_size, 
	Destructor destroy
);
void stack_free(Stack* s);

void stack_realloc(Stack* s);
void stack_push(Stack* s, const void* element);
void stack_pop(Stack* s, void* out);
void* stack_peek(Stack* s);
void stack_clear(Stack* s);

int stack_empty(Stack* s);

#endif