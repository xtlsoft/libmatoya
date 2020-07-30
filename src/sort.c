// Copyright (c) 2020 Christopher D. Dickson <cdd@matoya.group>
//
// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at https://mozilla.org/MPL/2.0/.

#include "matoya.h"

#include <string.h>
#include <stdlib.h>

struct element {
	int32_t (*compare)(const void *, const void *);
	void *orig;
};

static int sort_compare(const void *a, const void *b)
{
	struct element *element_a = (struct element *) a;
	struct element *element_b = (struct element *) b;

	int32_t r = element_a->compare(element_a->orig, element_b->orig);

	return r != 0 ? r : (int) ((uint8_t *) element_a->orig - (uint8_t *) element_b->orig);
}

void MTY_Sort(void *base, size_t nElements, size_t size, int32_t (*compare)(const void *a, const void *b))
{
	// Temporary copy of the base array for wrapping
	uint8_t *tmp = MTY_Alloc(nElements, size);
	memcpy(tmp, base, nElements * size);

	// Wrap the base array elements in a struct containing the original index and compare function
	struct element *wrapped = MTY_Alloc(nElements, sizeof(struct element));

	for (size_t x = 0; x < nElements; x++) {
		wrapped[x].compare = compare;
		wrapped[x].orig = tmp + x * size;
	}

	// Perform qsort using the original compare function, falling back to index comparison
	qsort(wrapped, nElements, sizeof(struct element), sort_compare);

	// Copy the reordered elements back to the base array
	for (size_t x = 0; x < nElements; x++)
		memcpy((uint8_t *) base + x * size, wrapped[x].orig, size);

	MTY_Free(tmp);
	MTY_Free(wrapped);
}
