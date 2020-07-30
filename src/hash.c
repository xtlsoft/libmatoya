// Copyright (c) 2020 Christopher D. Dickson <cdd@matoya.group>
//
// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at https://mozilla.org/MPL/2.0/.

#include "matoya.h"

#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <inttypes.h>

#define HASH_DEFAULT_BUCKETS 100

struct hash_node {
	char *key;
	const void *val;
};

struct hash_bucket {
	uint32_t num_nodes;
	struct hash_node *nodes;
};

struct MTY_Hash {
	uint32_t num_buckets;
	struct hash_bucket *buckets;
};

void MTY_HashCreate(uint32_t numBuckets, MTY_Hash **hash)
{
	MTY_Hash *ctx = *hash = MTY_Alloc(1, sizeof(MTY_Hash));
	ctx->num_buckets = numBuckets == 0 ? HASH_DEFAULT_BUCKETS : numBuckets;

	ctx->buckets = MTY_Alloc(ctx->num_buckets, sizeof(struct hash_bucket));
}

static bool hash_next_key(MTY_Hash *ctx, uint64_t *iter, const char **key)
{
	*key = NULL;

	uint32_t *bucket = (uint32_t *) iter;
	uint32_t *node = (uint32_t *) iter + 1;

	for (; *bucket < ctx->num_buckets; (*bucket)++) {
		struct hash_bucket *b = &ctx->buckets[*bucket];

		for (; *node < b->num_nodes; (*node)++) {
			struct hash_node *n = &b->nodes[*node];

			if (n->key) {
				*key = n->key;
				(*node)++;
				break;
			}
		}

		if (*key)
			break;

		if (*node == b->num_nodes)
			*node = 0;
	}

	return *key ? true : false;
}

bool MTY_HashNextKey(MTY_Hash *ctx, uint64_t *iter, const char **key)
{
	return hash_next_key(ctx, iter, key);
}

bool MTY_HashNextKeyInt(MTY_Hash *ctx, uint64_t *iter, int64_t *key)
{
	const char *key_str;
	bool r = hash_next_key(ctx, iter, &key_str);

	r = r && key_str[0] == '#';

	if (r)
		*key = r ? strtoll(key_str + 1, NULL, 16) : 0;

	return r;
}

void MTY_HashDestroy(MTY_Hash **hash, void (*freeFunc)(void *value))
{
	if (!hash || !*hash)
		return;

	MTY_Hash *ctx = *hash;

	for (uint32_t x = 0; x < ctx->num_buckets; x++) {
		struct hash_bucket *b = &ctx->buckets[x];

		for (uint32_t y = 0; y < b->num_nodes; y++) {
			struct hash_node *n = &b->nodes[y];

			MTY_Free(n->key);

			if (freeFunc && n->val)
				freeFunc((void *) n->val);
		}

		MTY_Free(b->nodes);
	}

	MTY_Free(ctx->buckets);

	MTY_Free(ctx);
	hash = NULL;
}

static void *hash_get(MTY_Hash *ctx, const char *key, bool remove)
{
	struct hash_bucket *b = &ctx->buckets[MTY_CryptoDJB2(key) % ctx->num_buckets];

	for (uint32_t x = 0; x < b->num_nodes; x++) {
		struct hash_node *n = &b->nodes[x];

		if (n->key && !strcmp(key, n->key)) {
			const void *r = n->val;

			if (remove) {
				MTY_Free(n->key);
				n->key = NULL;
				n->val = NULL;
			}

			return (void *) r;
		}
	}

	return NULL;
}

void *MTY_HashGet(MTY_Hash *ctx, const char *key)
{
	return hash_get(ctx, key, false);
}

void *MTY_HashGetInt(MTY_Hash *ctx, int64_t key)
{
	char key_str[32];
	snprintf(key_str, 32, "#%" PRIx64, key);

	return hash_get(ctx, key_str, false);
}

void *MTY_HashPop(MTY_Hash *ctx, const char *key)
{
	return hash_get(ctx, key, true);
}

void *MTY_HashPopInt(MTY_Hash *ctx, int64_t key)
{
	char key_str[32];
	snprintf(key_str, 32, "#%" PRIx64, key);

	return hash_get(ctx, key_str, true);
}

static void *hash_set(MTY_Hash *ctx, const char *key, const void *value)
{
	struct hash_bucket *b = &ctx->buckets[MTY_CryptoDJB2(key) % ctx->num_buckets];
	struct hash_node *n = NULL;

	for (uint32_t x = 0; x < b->num_nodes; x++) {
		struct hash_node *this_n = &b->nodes[x];

		if (!this_n->key) {
			n = this_n;

		} else if (this_n->key && !strcmp(this_n->key, key)) {
			const void *r = this_n->val;
			this_n->val = value;

			return (void *) r;
		}
	}

	if (!n) {
		b->nodes = MTY_Realloc(b->nodes, ++b->num_nodes, sizeof(struct hash_node));
		n = &b->nodes[b->num_nodes - 1];
	}

	n->key = MTY_Strdup(key);
	n->val = value;

	return NULL;
}

void *MTY_HashSet(MTY_Hash *ctx, const char *key, const void *value)
{
	return hash_set(ctx, key, value);
}

void *MTY_HashSetInt(MTY_Hash *ctx, int64_t key, const void *value)
{
	char key_str[32];
	snprintf(key_str, 32, "#%" PRIx64, key);

	return hash_set(ctx, key_str, value);
}