#include <string.h>
#include <stdlib.h>
#include <stdint.h>
#include <stdio.h>
#include <assert.h>

#include "util/types/hash.h"

#define HASH_TABLE_INITIAL_CAPACITY 8
#define HASH_TABLE_LOAD_FACTOR 0.75

HashTable hash_table_new
(
	size_t key_size,
	Destructor key_destroy,
	
	size_t value_size,
	Destructor value_destroy,
	
	Hash hash,
	Equals equals
)
{
	HashTable ht = {
		.key_size = key_size,
		.value_size = value_size,
		
		.key_destroy = key_destroy,
		.value_destroy = value_destroy,
		
		.hash = hash,
		.equals = equals,
		
		.capacity = 0,
		.size = 0,
		.used = 0,
		.entries = NULL
	};
	
	return ht;
}

static int hash_table_resize
(
	HashTable* ht,
	size_t new_capacity
)
{
	HashEntry* entries = calloc(new_capacity, sizeof(*entries));
	
	if (!entries) {
		return -1;
	}
	
	for (size_t i = 0; i < ht->capacity; i++) {
		HashEntry* old = &ht->entries[i];
		
		if (old->state != HASH_ENTRY_OCCUPIED) {
			continue;
		}
		
		size_t index = ht->hash(old->key) % new_capacity;
		
		while (entries[index].state == HASH_ENTRY_OCCUPIED) {
			index = (index + 1) % new_capacity;
		}
		
		entries[index] = *old;
	}
	
	free(ht->entries);
	
	ht->entries = entries;
	ht->capacity = new_capacity;
	
	ht->used = ht->size;
	
	return 0;
}

static void hash_entry_free
(
	HashEntry* entry,
	Destructor key_destroy,
	Destructor value_destroy
)
{
	if (!entry) {
		return;
	}

	if (key_destroy) {
		key_destroy(entry->key);
	}

	if (value_destroy) {
		value_destroy(entry->value);
	}

	free(entry->key);
	free(entry->value);
}

void hash_table_free(HashTable* ht) {
	if (!ht) {
		return;
	}

	for (size_t i = 0; i < ht->capacity; i++) {
		HashEntry* entry = &ht->entries[i];

		if (entry->state != HASH_ENTRY_OCCUPIED) {
			continue;
		}

		hash_entry_free(
			entry,
			ht->key_destroy,
			ht->value_destroy
		);
	}

	free(ht->entries);

	ht->entries = NULL;
	ht->capacity = 0;
	ht->size = 0;
	ht->used = 0;
}

static int hash_entry_init
(
	HashTable* ht,
	HashEntry* entry,
	const void* key,
	const void* value
)
{
	entry->key = malloc(ht->key_size);
	entry->value = malloc(ht->value_size);
	
	if (!entry->key || !entry->value) {
		free(entry->key);
		free(entry->value);

		return -1;
	}
	
	memcpy(entry->key, key, ht->key_size);
	memcpy(entry->value, value, ht->value_size);
	
	entry->state = HASH_ENTRY_OCCUPIED;
	
	return 0;
}

int hash_table_insert
(
	HashTable* ht,
	const void* key,
	const void* value
)
{
	if (ht->capacity == 0 || ht->used + 1 > ht->capacity - ht->capacity / 4) {
		size_t capacity = (ht->capacity)
			? ht->capacity * 2
			: HASH_TABLE_INITIAL_CAPACITY;
		
		if (hash_table_resize(ht, capacity) < 0) {
			return -1;
		}
	}
	
	size_t index = ht->hash(key) % ht->capacity;
	size_t deleted_index = SIZE_MAX;
	
	for (;;) {
		HashEntry* entry = &ht->entries[index];
		
		if (entry->state == HASH_ENTRY_OCCUPIED) {
			if (ht->equals(entry->key, key)) {
				// key already exists
				// substitues the value
				
				if (ht->value_destroy) {
					ht->value_destroy(entry->value);
				}
				
				memcpy(
					entry->value,
					value,
					ht->value_size
				);
				
				return 0;
			}
		}
		
		else if (entry->state == HASH_ENTRY_DELETED) {
			// the first slot will be stored,
			// but the search continues cause
			// may exist a equal key further
			if (deleted_index == SIZE_MAX) {
				deleted_index = index;
			}
		}
		
		else { // HASH_ENTRY_EMPTY
			// the search ends
			HashEntry* entry;
			
			if (deleted_index != SIZE_MAX) {
				entry = &ht->entries[deleted_index];
			}
			
			else {
				entry = &ht->entries[index];
			}
			
			int was_empty = (entry->state == HASH_ENTRY_EMPTY);
			
			if (hash_entry_init(ht, entry, key, value) < 0) {
				return -1;
			}
			
			if (was_empty) {
				ht->used++;
			}
			
			ht->size++;
			
			return 0;
		}
		
		index = (index + 1) % ht->capacity;
	}		
}


static HashEntry* hash_table_find_entry
(
	HashTable* ht,
	const void* key
)
{
	if (ht->capacity == 0) {
		return NULL;
	}

	size_t index = ht->hash(key) % ht->capacity;

	for (;;) {
		HashEntry* entry = &ht->entries[index];

		if (entry->state == HASH_ENTRY_EMPTY) {
			return NULL;
		}

		if (entry->state == HASH_ENTRY_OCCUPIED && 
			ht->equals(entry->key, key))
		{
			return entry;
		}

		index = (index + 1) % ht->capacity;
	}	
}

void* hash_table_get
(
	HashTable* ht,
	const void* key
)
{
	HashEntry* entry = hash_table_find_entry(ht, key);

	return (entry) ? entry->value : NULL;
}

int hash_table_remove
(
	HashTable* ht,
	const void* key
)
{
	HashEntry* entry = hash_table_find_entry(ht, key);

	if (!entry) {
		return -1;
	}

	hash_entry_free(
		entry,
		ht->key_destroy,
		ht->value_destroy
	);

	entry->key = NULL;
	entry->value = NULL;
	entry->state = HASH_ENTRY_DELETED;

	ht->size--;

	return 0;
}