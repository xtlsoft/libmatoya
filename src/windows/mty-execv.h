// Copyright (c) 2020 Christopher D. Dickson <cdd@matoya.group>
//
// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at https://mozilla.org/MPL/2.0/.

#pragma once

#include <stdbool.h>
#include <process.h>

static bool mty_execv(const char *name, char * const *argv)
{
	// FIXME This is probably not unicode compatible

	_execv(name, argv);
	MTY_Log("'_execv' failed with errno %d", errno);

	return false;
}