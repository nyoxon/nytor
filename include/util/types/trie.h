#ifndef TRIE_H
#define TRIE_H

#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>

typedef struct TrieNode TrieNode;

typedef struct {
	uint32_t key;
	TrieNode* node;
} TrieChild;

struct TrieNode {
	TrieChild* children;
	size_t size;
	size_t capacity;

	bool terminal;

	uint64_t frequency;
	double score;
};

typedef struct {
	TrieNode* root;
} Trie;

typedef void (*TrieVisitFn)
(
	const uint32_t* word,
	size_t length,
	TrieNode* node,
	void* userdata
);

void trie_free(Trie* trie);

TrieNode* trie_node_create();

int trie_insert
(
	Trie* trie,
	const uint32_t* str,
	size_t length
);

// same as trie_insert, but increment the frequency by n
int trie_ninsert
(
	Trie* trie,
	const uint32_t* str,
	size_t length,
	size_t n
);

int trie_remove
(
	Trie* trie,
	const uint32_t* word,
	size_t length
);

// same as trie_remove, but decrement the frequency by n
int trie_nremove
(
	Trie* trie,
	const uint32_t* word,
	size_t length,
	size_t n
);

TrieNode* trie_find
(
	Trie* trie,
	const uint32_t* str,
	size_t length
);

void trie_prefix_search
(
	Trie* trie,
	const uint32_t* prefix,
	size_t prefix_size,
	TrieVisitFn visit,
	void* userdata
);

void trie_build
(
	Trie* trie,
	const uint32_t* data,
	size_t size,
	int (*is_word_char)(uint32_t c)
);

size_t trie_count(Trie* trie);

#endif