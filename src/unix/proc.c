// Copyright (c) 2020 Christopher D. Dickson <cdd@matoya.group>
//
// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at https://mozilla.org/MPL/2.0/.

#include "matoya.h"

#include <string.h>
#include <stdio.h>
#include <errno.h>

#include <unistd.h>

#include "mty-limits.h"
#include "mty-dlopen.h"
#include "mty-tls.h"

static MTY_TLS char PROC_NAME[MTY_PATH_MAX];
static MTY_TLS char PROC_HOSTNAME[MTY_PATH_MAX];

bool MTY_SOLoad(const char *name, MTY_SO **so)
{
	*so = dlopen(name, MTY_DLOPEN_FLAGS);
	if (!*so) {
		const char *estr = dlerror();
		if (estr)
			MTY_Log("%s", estr);

		MTY_Log("'dlopen' failed to find '%s'", name);
		return false;
	}

	return true;
}

void *MTY_SOSymbolGet(MTY_SO *ctx, const char *name)
{
	void *sym = dlsym(ctx, name);
	if (!sym) {
		const char *estr = dlerror();
		if (estr)
			MTY_Log("%s", estr);

		MTY_Log("'dlsym' failed to find '%s'", name);
	}

	return sym;
}

void MTY_SOUnload(MTY_SO **so)
{
	if (!so || !*so)
		return;

	int32_t e = dlclose(*so);
	if (e != 0) {
		const char *estr = dlerror();
		if (estr) {
			MTY_Log("'dlclose' failed with error %d: '%s'", e, estr);

		} else {
			MTY_Log("'dlclose' failed with error %d", e);
		}
	}

	*so = NULL;
}

const char *MTY_ProcName(void)
{
	memset(PROC_NAME, 0, MTY_PATH_MAX);

	ssize_t n = readlink("/proc/self/exe", PROC_NAME, MTY_PATH_MAX - 1);
	if (n < 0)
		MTY_Log("'readlink' failed with errno %d", errno);

	return PROC_NAME;
}

const char *MTY_Hostname(void)
{
	memset(PROC_HOSTNAME, 0, MTY_PATH_MAX);

	int32_t e = gethostname(PROC_HOSTNAME, MTY_PATH_MAX - 1);
	if (e != 0) {
		MTY_Log("'gethostname' failed with errno %d", errno);
		snprintf(PROC_HOSTNAME, MTY_PATH_MAX, "noname");
	}

	return PROC_HOSTNAME;
}