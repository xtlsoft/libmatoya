// Copyright (c) 2020 Christopher D. Dickson <cdd@matoya.group>
//
// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at https://mozilla.org/MPL/2.0/.

#include "matoya.h"

#include <time.h>

#include <unistd.h>

int64_t MTY_Timestamp(void)
{
	struct timespec ts = {0};
	clock_gettime(CLOCK_MONOTONIC, &ts);

	uint64_t r = ts.tv_sec * 1000 * 1000;
	r += ts.tv_nsec / 1000;

	return r;
}

float MTY_TimeDiff(int64_t begin, int64_t end)
{
	return (float) (end - begin) / 1000.0f;
}

void MTY_Sleep(uint32_t timeout)
{
	struct timespec ts = {0};
	ts.tv_sec = timeout / 1000;
	ts.tv_nsec = (timeout % 1000) * 1000 * 1000;

	nanosleep(&ts, NULL);
}

void MTY_SetTimerResolution(uint32_t res)
{
}

void MTY_RevertTimerResolution(uint32_t res)
{
}
