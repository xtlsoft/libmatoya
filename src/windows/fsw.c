// Copyright (c) 2020 Christopher D. Dickson <cdd@matoya.group>
//
// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at https://mozilla.org/MPL/2.0/.

#include "matoya.h"

#include <stdio.h>

#include <windows.h>
#include <shlwapi.h>
#include <shlobj_core.h>

#include "mty-tls.h"

static MTY_TLS char FS_CWD[MTY_PATH_MAX];
static MTY_TLS char FS_HOME[MTY_PATH_MAX];
static MTY_TLS char FS_PATH[MTY_PATH_MAX];
static MTY_TLS char FS_PROGRAM_DIR[MTY_PATH_MAX];
static MTY_TLS char FS_FILE_NAME[MTY_PATH_MAX];

bool MTY_FsDelete(const char *path)
{
	WCHAR *wpath = MTY_MultiToWide(path, NULL, 0);

	BOOL success = DeleteFile(wpath);
	MTY_Free(wpath);

	if (!success) {
		MTY_Log("'DeleteFile' failed with error %x", GetLastError());
		return false;
	}

	return true;
}

bool MTY_FsExists(const char *path)
{
	SetLastError(0);
	WCHAR *wpath = MTY_MultiToWide(path, NULL, 0);

	BOOL success = PathFileExists(wpath);
	MTY_Free(wpath);

	if (!success) {
		DWORD e = GetLastError();
		if (e != 0)
			MTY_Log("'PathFileExists' failed with error %x", e);
	}

	return success;
}

bool MTY_FsMkdir(const char *path)
{
	WCHAR *wpath = MTY_MultiToWide(path, NULL, 0);

	BOOL success = CreateDirectory(wpath, NULL);
	MTY_Free(wpath);

	if (!success) {
		MTY_Log("'CreateDirectory' failed with error %x", GetLastError());
		return false;
	}

	return true;
}

const char *MTY_FsPath(const char *dir, const char *file)
{
	char *safe_dir = MTY_Strdup(dir);
	char *safe_file = MTY_Strdup(file);

	snprintf(FS_PATH, MTY_PATH_MAX, "%s\\%s", safe_dir, safe_file);

	MTY_Free(safe_dir);
	MTY_Free(safe_file);

	return FS_PATH;
}

const char *MTY_FsGetDir(MTY_FsDir dir)
{
	WCHAR tmp[MTY_PATH_MAX];

	switch (dir) {
		case MTY_DIR_CWD: {
			memset(FS_CWD, 0, MTY_PATH_MAX);
			FS_CWD[0] = '.';

			DWORD n = GetCurrentDirectory(MTY_PATH_MAX - 1, tmp);
			if (n > 0) {
				MTY_WideToMulti(tmp, FS_CWD, MTY_PATH_MAX);

			} else {
				MTY_Log("'GetCurrentDirectory' failed with error %x", GetLastError());
			}

			return FS_CWD;
		}
		case MTY_DIR_HOME: {
			memset(FS_HOME, 0, MTY_PATH_MAX);
			FS_HOME[0] = '.';

			HRESULT e = SHGetFolderPath(NULL, CSIDL_APPDATA, NULL, SHGFP_TYPE_CURRENT, tmp);
			if (e == S_OK) {
				MTY_WideToMulti(tmp, FS_HOME, MTY_PATH_MAX);

			} else {
				MTY_Log("'SHGetFolderPath' failed with hresult %x", e);
			}

			return FS_HOME;
		}
		case MTY_DIR_PROGRAM: {
			memset(FS_PROGRAM_DIR, 0, MTY_PATH_MAX);
			FS_PROGRAM_DIR[0] = '.';

			DWORD n = GetModuleFileName(NULL, tmp, MTY_PATH_MAX);

			if (n > 0) {
				MTY_WideToMulti(tmp, FS_PROGRAM_DIR, MTY_PATH_MAX);
				char *name = strrchr(FS_PROGRAM_DIR, '\\');

				if (name)
					name[0] = '\0';

			} else {
				MTY_Log("'GetModuleFileName' failed with error %x", GetLastError());
			}

			return FS_PROGRAM_DIR;
		}
	}

	return ".";
}

bool MTY_FsLockCreate(const char *path, MTY_LockFile **lock)
{
	// FIXME Not unicode compatible

	OFSTRUCT buffer = {0};
	*lock = (MTY_LockFile *) (intptr_t) OpenFile(path, &buffer, OF_CREATE | OF_SHARE_EXCLUSIVE);

	if (*lock == (MTY_LockFile *) HFILE_ERROR) {
		MTY_Log("'OpenFile' failed with error %x", GetLastError());
		return false;
	}

	return true;
}

void MTY_FsLockDestroy(MTY_LockFile **lock)
{
	if (!lock || !*lock)
		return;

	MTY_LockFile *ctx = *lock;

	CloseHandle((HANDLE) ctx);

	*lock = NULL;
}

const char *MTY_FsName(const char *path, bool extension)
{
	// FIXME Not unicode compatible?

	const char *name = strrchr(path, '\\');
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
	// FIXME Not unicode compatible

	MTY_FileDesc *fi1 = (MTY_FileDesc *) p1;
	MTY_FileDesc *fi2 = (MTY_FileDesc *) p2;

	if (fi1->dir && !fi2->dir) {
		return -1;
	} else if (!fi1->dir && fi2->dir) {
		return 1;
	} else {
		int32_t r = _stricmp(fi1->name, fi2->name);

		if (r != 0)
			return r;

		return -strcmp(fi1->name, fi2->name);
	}
}

uint32_t MTY_FsList(const char *path, MTY_FileDesc **fi)
{
	// FIXME Not unicode compatible

	*fi = NULL;
	uint32_t n = 0;
	WIN32_FIND_DATAA ent;

	size_t path_wildcard_len = strlen(path) + 3;
	char *path_wildcard = MTY_Alloc(path_wildcard_len, 1);
	snprintf(path_wildcard, path_wildcard_len, "%s\\*", path);

	HANDLE dir = FindFirstFileA(path_wildcard, &ent);
	bool ok = (dir != INVALID_HANDLE_VALUE);

	MTY_Free(path_wildcard);

	while (ok) {
		char *name = ent.cFileName;
		bool is_dir = ent.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY;

		if ((is_dir && strcmp(name, ".")) || strstr(name, ".nes")) {
			*fi = MTY_Realloc(*fi, (n + 1), sizeof(MTY_FileDesc));

			size_t name_size = strlen(name) + 2;
			size_t path_size = name_size + strlen(path) + 2;
			(*fi)[n].name = MTY_Alloc(name_size, 1);
			(*fi)[n].path = MTY_Alloc(path_size, 1);

			snprintf((char *) (*fi)[n].name, name_size, "%s%s", name,
				(is_dir && strcmp(name, "..")) ? "\\" : "");
			snprintf((char *) (*fi)[n].path, path_size, "%s\\%s", path, name);
			(*fi)[n].dir = is_dir;
			n++;
		}

		ok = FindNextFileA(dir, &ent);

		if (!ok)
			FindClose(dir);
	}

	if (n > 0)
		qsort(*fi, n, sizeof(MTY_FileDesc), fs_file_compare);

	return n;
}
