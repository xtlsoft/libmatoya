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
#include "mty-limits.h"
#include "mty-tls.h"

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
	int32_t e = mkdir(path, 0777);

	if (e != 0) {
		MTY_Log("'mkdir' failed with errno %d", errno);
		return false;
	}

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

const char *MTY_FsGetDir(MTY_FsDir dir)
{
	switch (dir) {
		case MTY_DIR_CWD: {
			memset(FS_CWD, 0, MTY_PATH_MAX);
			FS_CWD[0] = '.';

			if (!getcwd(FS_CWD, MTY_PATH_MAX))
				MTY_Log("'getcwd' failed with errno %d", errno);

			return FS_CWD;
		}
		case MTY_DIR_HOME: {
			memset(FS_HOME, 0, MTY_PATH_MAX);
			FS_HOME[0] = '.';

			struct passwd *pw = getpwuid(getuid());
			if (pw) {
				snprintf(FS_HOME, MTY_PATH_MAX, "%s", pw->pw_dir);

			} else {
				MTY_Log("'getpwuid' failed with errno %d", errno);
			}

			return FS_HOME;
		}
		case MTY_DIR_PROGRAM: {
			int32_t n = readlink("/proc/self/exe", FS_PROGRAM, MTY_PATH_MAX);

			if (n > 0 && n < MTY_PATH_MAX) {
				FS_PROGRAM[n] = '\0';

				char *name = strrchr(FS_PROGRAM, '/');

				if (name)
					name[0] = '\0';

			} else {
				snprintf(FS_PROGRAM, MTY_PATH_MAX, ".");
			}

			return FS_PROGRAM;
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
			close(f);
			MTY_Log("'flock' failed with errno %d", errno);
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

	flock(f, LOCK_UN);
	close(f);

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

uint32_t MTY_FsList(const char *path, MTY_FileDesc **fi)
{
	*fi = NULL;
	uint32_t n = 0;
	struct dirent *ent = NULL;

	bool ok = false;
	DIR *dir = opendir(path);

	if (dir) {
		ent = readdir(dir);
		ok = ent;
	}

	while (ok) {
		char *name = ent->d_name;
		bool is_dir = ent->d_type == DT_DIR;

		if (is_dir || strstr(name, ".nes")) {
			*fi = MTY_Realloc(*fi, (n + 1), sizeof(MTY_FileDesc));

			size_t name_size = strlen(name) + 1;
			size_t path_size = name_size + strlen(path) + 1;
			(*fi)[n].name = MTY_Alloc(name_size, 1);
			(*fi)[n].path = MTY_Alloc(path_size, 1);

			snprintf((char *) (*fi)[n].name, name_size, "%s", name);
			snprintf((char *) (*fi)[n].path, path_size, "%s/%s", path, name);

			(*fi)[n].dir = is_dir;
			n++;
		}

		ok = ((ent = readdir(dir)), ent ? 1 : 0);
		if (!ok) closedir(dir);
	}

	if (n > 0)
		qsort(*fi, n, sizeof(MTY_FileDesc), fs_file_compare);

	return n;
}
