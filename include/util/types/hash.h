#ifndef HASH_H
#define HASH_H

#include <stddef.h>
#include <stdbool.h>

typedef void (*Destructor)(void* ptr);
typedef size_t (*Hash)(const void*);
typedef bool (*Equals)(const void*, const void*);

typedef enum {
	HASH_ENTRY_EMPTY,
	HASH_ENTRY_OCCUPIED,
	HASH_ENTRY_DELETED
} HashEntryState;

typedef struct {
	void* key;	
	void* value;
	
	HashEntryState state;
} HashEntry;

typedef struct {
	HashEntry* entries;
	size_t capacity;
	size_t size; // OCCUPIED
	size_t used; // OCCUPIED + DELETED
	
	size_t key_size;
	size_t value_size;
		
	Hash hash;
	Equals equals;	

	Destructor key_destroy;
	Destructor value_destroy;
} HashTable;

HashTable hash_table_new
(
	size_t key_size,
	Destructor key_destroy,

	size_t value_size,
	Destructor value_destroy,
	
	Hash hash,
	Equals equals
);

void hash_table_free(HashTable* ht);

int hash_table_insert
(
	HashTable* table,
	const void* key,
	const void* value
);

void* hash_table_get
(
	HashTable* ht,
	const void* key
);

int hash_table_remove
(
	HashTable* ht,
	const void* key
);


#endif