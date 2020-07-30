// Copyright (c) 2020 Christopher D. Dickson <cdd@matoya.group>
//
// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at https://mozilla.org/MPL/2.0/.

#pragma once

#include <stdbool.h>
#include <stdio.h>

static bool fs_open(const char *path, const char *mode, FILE **file)
{
	wchar_t *wpath = MTY_MultiToWide(path, NULL, 0);
	wchar_t *wmode = MTY_MultiToWide(mode, NULL, 0);

	int32_t e = _wfopen_s(file, wpath, wmode);

	MTY_Free(wpath);
	MTY_Free(wmode);

	if (e != 0) {
		MTY_Log("'_wfopen_s' failed with error %d", e);
		return false;
	}

	return true;
}
