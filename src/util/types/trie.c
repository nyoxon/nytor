#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <assert.h>
#include <inttypes.h>
#include <ctype.h>

#include "util/types/trie.h"

TrieNode* trie_node_create() {
	TrieNode* node = malloc(sizeof(*node));

	if (!node) {
		return NULL;
	}

	node->children = NULL;
	node->size = 0;
	node->capacity = 0;

	node->terminal = false;
	
	node->frequency = 0;
	node->score = 0.0;

	return node;
}

static void trie_node_free(TrieNode* node) {
	if (!node) {
		return;
	}

	for (size_t i = 0; i < node->size; i++) {
		trie_node_free(node->children[i].node);
	}

	free(node->children);
	free(node);
}

void trie_free(Trie* trie) {
	if (!trie) {
		return;
	}

	trie_node_free(trie->root);

	trie->root = NULL;
}

static TrieNode* trie_find_child
(
	TrieNode* node,
	uint32_t key
)
{
	for (size_t i = 0; i < node->size; i++) {
		if (node->children[i].key == key) {
			return node->children[i].node;
		}
	}

	return NULL;
}

static TrieNode* trie_add_child
(
	TrieNode* node,
	uint32_t key
)
{
	TrieNode* child = trie_node_create();

	if (!child) {
		return NULL;
	}

	if (node->size == node->capacity) {
		size_t cap = (node->capacity == 0)
			? 4
			: node->capacity * 2;

		TrieChild* children = realloc(
			node->children,
			cap * sizeof(*children)
		);

		if (!children) {
			free(child);
			return NULL;
		}

		node->children = children;
		node->capacity = cap;
	}

	node->children[node->size++] = (TrieChild) {
		.key = key,
		.node = child
	};

	return child;
}


int trie_insert
(
	Trie* trie,
	const uint32_t* str,
	size_t length
)
{
	TrieNode* node = trie->root;

	for (size_t i = 0; i < length; i++) {
		TrieNode* child = trie_find_child(node, str[i]);

		if (!child) {
			child = trie_add_child(node, str[i]);

			if (!child) {
				return -1;
			}
		}

		node = child;
	}

	node->terminal = true;
	node->frequency++;

	return 0;
}

int trie_ninsert
(
	Trie* trie,
	const uint32_t* str,
	size_t length,
	size_t n
)
{
	if (n == 0) {
		return 0;
	}

	TrieNode* node = trie->root;

	for (size_t i = 0; i < length; i++) {
		TrieNode* child = trie_find_child(node, str[i]);

		if (!child) {
			child = trie_add_child(node, str[i]);

			if (!child) {
				return -1;
			}
		}

		node = child;
	}

	node->terminal = true;
	node->frequency += n;

	return 0;
}

static int trie_remove_child
(
	TrieNode* parent,
	uint32_t key
)
{
	for (size_t i = 0; i < parent->size; i++) {
		if (parent->children[i].key != key) {
			continue;
		}

		parent->children[i] = parent->children[parent->size - 1];

		parent->size--;

		return 0;
	}

	return -1;
}

int trie_remove
(
	Trie* trie,
	const uint32_t* word,
	size_t length
)
{
	TrieNode* node = trie->root;

	TrieNode** path = malloc((length + 1) * sizeof(*path));

	if (!path) {
		return -1;
	}

	path[0] = trie->root;

	for (size_t i = 0; i < length; i++) {
		TrieNode* child = trie_find_child(node, word[i]);

		if (!child) {
			free(path);

			return 0;
		}

		node = child;
		path[i + 1] = node;
	}

	if (node->frequency == 0) {
		free(path);
		return 0;
	}

	node->frequency--;

	if (node->frequency > 0 || node->size > 0) {
		free(path);

		return 0;
	}

	for (size_t i = length; i > 0; i--) {
		TrieNode* current = path[i];
		TrieNode* parent = path[i - 1];

		if (current->frequency > 0 || current->size > 0) {
			break;
		}

		trie_remove_child(parent, word[i - 1]);
		trie_node_free(current);
	}

	free(path);

	return 0;	
}

int trie_nremove
(
	Trie* trie,
	const uint32_t* word,
	size_t length,
	size_t n
)
{
	if (n == 0) {
		return 0;
	}

	TrieNode* node = trie->root;

	TrieNode** path = malloc((length + 1) * sizeof(*path));

	if (!path) {
		return -1;
	}

	path[0] = trie->root;

	for (size_t i = 0; i < length; i++) {
		TrieNode* child = trie_find_child(node, word[i]);

		if (!child) {
			free(path);

			return 0;
		}

		node = child;
		path[i + 1] = node;
	}

	if (node->frequency == 0) {
		free(path);
		return 0;
	}

	node->frequency = (node->frequency > n)
		? node->frequency - n
		: 0;

	if (node->frequency > 0 || node->size > 0) {
		free(path);

		return 0;
	}

	for (size_t i = length; i > 0; i--) {
		TrieNode* current = path[i];
		TrieNode* parent = path[i - 1];

		if (current->frequency > 0 || current->size > 0) {
			break;
		}

		trie_remove_child(parent, word[i - 1]);
		trie_node_free(current);
	}

	free(path);

	return 0;	
}


TrieNode* trie_find
(
	Trie* trie,
	const uint32_t* str,
	size_t length
)
{
	TrieNode* node = trie->root;

	for (size_t i = 0; i < length; i++) {
		node = trie_find_child(node, str[i]);

		if (!node) {
			return NULL;
		}
	}

	if (!node->terminal) {
		return NULL;
	}

	return node;
}

typedef struct {
	uint32_t* word;
	size_t size;
	size_t capacity;
} TrieDFSContext;

static int trie_dfs
(
	TrieNode* node,
	TrieDFSContext* context,
	TrieVisitFn visit,
	void* userdata
)
{
	if (node->terminal) {
		uint32_t* word = (context)
			? context->word
			: NULL;

		size_t size = (context)
			? context->size
			: 0;

		visit(
			word,
			size,
			node,
			userdata
		);
	}

	for (size_t i = 0; i < node->size; i++) {
		TrieChild* child = &node->children[i];

		if (context &&
			context->size >= context->capacity) 
		{
			size_t new_capacity = context->capacity == 0
				? 1
				: context->capacity * 2;

			uint32_t* new_word = realloc(
				context->word,
				new_capacity * sizeof(*context->word)
			);

			if (!new_word) {
				return -1;
			}

			context->word = new_word;
			context->capacity = new_capacity;
		}

		if (context) {
			(context->word)[context->size++] = child->key;
		}

		if (trie_dfs(
			child->node,
			context,
			visit,
			userdata
		) < 0)
		{
			return -1;
		}

		if (context) {
			context->size--;
		}
	}

	return 0;
}


static void trie_count_word
(
	const uint32_t* word,
	size_t length,
	TrieNode* node,
	void* userdata
)
{
	(void) word;
	(void) length;
	(void) node;

	size_t* count = userdata;
	(*count)++;
}

size_t trie_count(Trie* trie) {
	size_t count = 0;

	trie_dfs(
		trie->root,
		NULL,
		trie_count_word,
		&count
	);

	return count;
}

void trie_prefix_search
(
	Trie* trie,
	const uint32_t* prefix,
	size_t prefix_size,
	TrieVisitFn visit,
	void* userdata
)
{
	TrieNode* node = trie->root;

	for (size_t i = 0; i < prefix_size; i++) {
		node = trie_find_child(node, prefix[i]);

		if (!node) {
			return;
		}
	}

	size_t capacity = prefix_size;
	uint32_t* word = malloc(capacity * sizeof(*word));

	if (!word) {
		return;
	}

	memcpy(word, prefix, prefix_size * sizeof(*word));

	TrieDFSContext context = {
		.word = word,
		.size = prefix_size,
		.capacity = capacity
	};

	trie_dfs(
		node,
		&context,
		visit,
		userdata
	);

	free(context.word);
}

static void print_codepoint(uint32_t c) {
    if (c < 128 && isprint((unsigned char)c))
        printf("%c", (char)c);
}

static void trie_print_node
(
	const TrieNode* node,
	size_t depth
)
{
    for (size_t i = 0; i < node->size; i++) {
        const TrieChild *child = &node->children[i];

        print_codepoint(child->key);

        if (child->node->terminal) {
        	printf(", freq = %" PRIu64 , child->node->frequency);
	        putchar('\n');
        }

        trie_print_node(
            child->node,
            depth + 1
        );
    }	
}

void trie_node_print
(
	const TrieNode* node
)
{
	trie_print_node(node, 0);
}

void trie_build
(
	Trie* trie,
	const uint32_t* data,
	size_t size,
	int (*is_word_char)(uint32_t c)
)
{
	if (!is_word_char) {
		return;
	}
	
	size_t start = 0;

	for (size_t i = 0; i < size; i++) {
		uint32_t cp = data[i];

		if (!is_word_char(cp)) {
			if (i > start) {
				trie_insert(trie, data + start, i - start);
			}

			start = i + 1;
		}
	}

	if (start < size) {
		trie_insert(trie, data + start, size - start);
	}
}