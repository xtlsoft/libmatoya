// Copyright (c) 2020 Christopher D. Dickson <cdd@matoya.group>
//
// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at https://mozilla.org/MPL/2.0/.

#pragma once

#include <stdbool.h>
#include <stdio.h>
#include <errno.h>

static bool fs_open(const char *path, const char *mode, FILE **file)
{
	*file = fopen(path, mode);
	if (!*file) {
		MTY_Log("'fopen' failed with errno %d", errno);
		return false;
	}

	return true;
}
