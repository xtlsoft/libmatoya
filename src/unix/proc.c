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

#include "mty-dlopen.h"
#include "mty-tls.h"
#include "mty-procname.h"

static MTY_TLS char PROC_NAME[MTY_PATH_MAX];
static MTY_TLS char PROC_HOSTNAME[MTY_PATH_MAX];

MTY_SO *MTY_SOLoad(const char *name)
{
	MTY_SO *so = dlopen(name, MTY_DLOPEN_FLAGS);
	if (!so) {
		const char *estr = dlerror();
		if (estr)
			MTY_Log("%s", estr);

		MTY_Log("'dlopen' failed to find '%s'", name);
	}

	return so;
}

void *MTY_SOGetSymbol(MTY_SO *so, const char *name)
{
	void *sym = dlsym(so, name);
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

const char *MTY_ProcessName(void)
{
	mty_proc_name(PROC_NAME, MTY_PATH_MAX);

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
