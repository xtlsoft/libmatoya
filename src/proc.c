// Copyright (c) 2020 Christopher D. Dickson <cdd@matoya.group>
//
// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at https://mozilla.org/MPL/2.0/.

#include "matoya.h"

#include "mty-proc.h"

bool MTY_ProcRestart(int32_t argc, const char **argv)
{
	bool free_argvn = false;
	const char **argvn = argv;

	if (argc > 0) {
		free_argvn = true;
		argvn = MTY_Alloc(argc + 1, sizeof(const char *));
		for (int32_t x = 0; x < argc; x++)
			argvn[x] = argv[x];
	}

	bool r = proc_execv(MTY_ProcName(), argvn);

	if (free_argvn)
		MTY_Free((void *) argvn);

	return r;
}
