// Copyright (c) 2020 Christopher D. Dickson <cdd@matoya.group>
//
// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at https://mozilla.org/MPL/2.0/.

#include "matoya.h"

#include <errno.h>

#include "mty-fopen.h"

#define FS_READ_ALLOC 512

bool MTY_FsRead(const char *path, void **data, size_t *size)
{
	size_t size_tmp;

	if (!size)
		size = &size_tmp;

	*data = NULL;
	*size = 0;

	FILE *f = NULL;
	bool r = fs_open(path, "rb", &f);
	if (!r)
		return false;

	while (true) {
		*data = MTY_Realloc(*data, *size + FS_READ_ALLOC + 1, 1);

		size_t n = fread((uint8_t *) *data + *size, 1, FS_READ_ALLOC, f);
		*size += n;

		((uint8_t *) *data)[*size] = '\0';

		if (n != FS_READ_ALLOC) {
			int32_t e = ferror(f);

			if (e != 0) {
				MTY_Log("'fread' failed with ferror %d", e);
				r = false;

				MTY_Free(*data);
				*data = NULL;
				*size = 0;
			}

			break;
		}
	}

	fclose(f);

	return r;
}

static bool fs_write(const char *path, const void *data, size_t size, const char *mode)
{
	FILE *f = NULL;
	bool r = fs_open(path, mode, &f);
	if (!r)
		return r;

	size_t n = fwrite(data, 1, size, f);

	if (n != size) {
		MTY_Log("'fwrite' failed with ferror %d", ferror(f));
		r = false;
	}

	fclose(f);

	return r;
}

bool MTY_FsWrite(const char *path, const void *data, size_t size)
{
	return fs_write(path, data, size, "wb");
}

bool MTY_FsAppend(const char *path, const void *data, size_t size)
{
	return fs_write(path, data, size, "ab");
}

void MTY_FsFreeList(MTY_FileDesc **fi, uint32_t len)
{
	if (!fi || !*fi)
		return;

	for (uint32_t x = 0; x < len; x++) {
		MTY_Free((char *) (*fi)[x].name);
		MTY_Free((char *) (*fi)[x].path);
	}

	MTY_Free(*fi);
	*fi = NULL;
}
