// Copyright (c) 2020 Christopher D. Dickson <cdd@matoya.group>
//
// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at https://mozilla.org/MPL/2.0/.

#define _DEFAULT_SOURCE  // DT_DIR
#define _DARWIN_C_SOURCE // flock, DT_DIR

#include "matoya.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>
#include <errno.h>

#include <unistd.h>
#include <fcntl.h>
#include <sys/stat.h>
#include <sys/file.h>
#include <dirent.h>

#include "mty-fs.h"
#include "mty-tls.h"
#include "mty-procname.h"

static MTY_TLS char FS_CWD[MTY_PATH_MAX];
static MTY_TLS char FS_HOME[MTY_PATH_MAX];
static MTY_TLS char FS_PATH[MTY_PATH_MAX];
static MTY_TLS char FS_PROGRAM[MTY_PATH_MAX];
static MTY_TLS char FS_FILE_NAME[MTY_PATH_MAX];

bool MTY_FsDelete(const char *path)
{
	int32_t e = remove(path);

	if (e == -1) {
		MTY_Log("'remove' failed with errno %d", errno);
		return false;
	}

	return true;
}

bool MTY_FsExists(const char *path)
{
	return access(path, F_OK) != -1;
}

bool MTY_FsMkdir(const char *path)
{
	char *tmp = MTY_Strdup(path);
	size_t len = strlen(tmp);

	while (len > 0 && tmp[len - 1] == '/') {
		tmp[len - 1] = '\0';
		len--;
	}

	for (size_t x = 1; x < len; x++) {
		if (tmp[x] == '/') {
			tmp[x] = '\0';
			if (mkdir(tmp, S_IRWXU) == -1 && errno != EEXIST)
				MTY_Log("'mkdir' failed with errno %d", errno);
			tmp[x] = '/';
		}
	}

	if (mkdir(tmp, S_IRWXU) == -1 && errno != EEXIST)
		MTY_Log("'mkdir' failed with errno %d", errno);

	MTY_Free(tmp);

	return true;
}

const char *MTY_FsPath(const char *dir, const char *file)
{
	char *safe_dir = MTY_Strdup(dir);
	char *safe_file = MTY_Strdup(file);

	snprintf(FS_PATH, MTY_PATH_MAX, "%s/%s", safe_dir, safe_file);

	MTY_Free(safe_dir);
	MTY_Free(safe_file);

	return FS_PATH;
}

bool MTY_FsCopy(const char *src, const char *dst)
{
	void *data = NULL;
	size_t size = 0;
	bool r = MTY_FsRead(src, &data, &size);

	if (r) {
		r = MTY_FsWrite(dst, data, size);
		MTY_Free(data);
	}

	return r;
}

const char *MTY_FsGetDir(MTY_FsDir dir)
{
	switch (dir) {
		case MTY_DIR_CWD: {
			memset(FS_CWD, 0, MTY_PATH_MAX);

			if (getcwd(FS_CWD, MTY_PATH_MAX)) {
				return FS_CWD;

			} else {
				MTY_Log("'getcwd' failed with errno %d", errno);
			}

			break;
		}
		case MTY_DIR_HOME: {
			memset(FS_HOME, 0, MTY_PATH_MAX);

			const struct passwd *pw = getpwuid(getuid());
			if (pw) {
				snprintf(FS_HOME, MTY_PATH_MAX, "%s", pw->pw_dir);
				return FS_HOME;

			} else {
				MTY_Log("'getpwuid' failed with errno %d", errno);
			}

			break;
		}
		case MTY_DIR_PROGRAM: {
			if (mty_proc_name(FS_PROGRAM, MTY_PATH_MAX)) {
				char *name = strrchr(FS_PROGRAM, '/');

				if (name)
					name[0] = '\0';

				return FS_PROGRAM;
			}

			break;
		}
	}

	return ".";
}

bool MTY_FsLockCreate(const char *path, MTY_LockFile **lock)
{
	int32_t f = open(path, O_RDWR | O_CREAT, S_IRWXU);

	if (f != -1) {
		if (flock(f, LOCK_EX | LOCK_NB) == 0) {
			*lock = (MTY_LockFile *) (intptr_t) f;

		} else {
			MTY_Log("'flock' failed with errno %d", errno);

			if (close(f) != 0)
				MTY_Log("'close' failed with errno %d", errno);

			return false;
		}
	} else {
		MTY_Log("'open' failed with errno %d", errno);
		return false;
	}

	return true;
}

void MTY_FsLockDestroy(MTY_LockFile **lock)
{
	if (!lock || !*lock)
		return;

	MTY_LockFile *ctx = *lock;
	int32_t f = (intptr_t) ctx;

	if (flock(f, LOCK_UN) != 0)
		MTY_Log("'flock' failed with errno %d", errno);

	if (close(f) != 0)
		MTY_Log("'close' failed with errno %d", errno);

	*lock = NULL;
}

const char *MTY_FsName(const char *path, bool extension)
{
	const char *name = strrchr(path, '/');
	name = name ? name + 1 : path;

	snprintf(FS_FILE_NAME, MTY_PATH_MAX, "%s", name);

	if (!extension) {
		char *ext = strrchr(FS_FILE_NAME, '.');

		if (ext)
			*ext = '\0';
	}

	return FS_FILE_NAME;
}

static int32_t fs_file_compare(const void *p1, const void *p2)
{
	MTY_FileDesc *fi1 = (MTY_FileDesc *) p1;
	MTY_FileDesc *fi2 = (MTY_FileDesc *) p2;

	if (fi1->dir && !fi2->dir) {
		return -1;

	} else if (!fi1->dir && fi2->dir) {
		return 1;

	} else {
		int32_t r = strcasecmp(fi1->name, fi2->name);

		if (r != 0)
			return r;

		return -strcmp(fi1->name, fi2->name);
	}
}

MTY_FileList *MTY_FsFileList(const char *path, const char *filter)
{
	MTY_FileList *fl = MTY_Alloc(1, sizeof(MTY_FileList));
	char *pathd = MTY_Strdup(path);

	bool ok = false;

	struct dirent *ent = NULL;
	DIR *dir = opendir(pathd);
	if (dir) {
		ent = readdir(dir);
		ok = ent;
	}

	while (ok) {
		char *name = ent->d_name;
		bool is_dir = ent->d_type == DT_DIR;

		if (is_dir || strstr(name, filter ? filter : "")) {
			fl->files = MTY_Realloc(fl->files, fl->len + 1, sizeof(MTY_FileDesc));

			fl->files[fl->len].dir = is_dir;
			fl->files[fl->len].name = MTY_Strdup(name);
			fl->files[fl->len].path = MTY_Strdup(MTY_FsPath(pathd, name));
			fl->len++;
		}

		ent = readdir(dir);
		if (!ent) {
			closedir(dir);
			ok = false;
		}
	}

	MTY_Free(pathd);

	if (fl->len > 0)
		MTY_Sort(fl->files, fl->len, sizeof(MTY_FileDesc), fs_file_compare);

	return fl;
}
