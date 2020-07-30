// Copyright (c) 2020 Christopher D. Dickson <cdd@matoya.group>
//
// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at https://mozilla.org/MPL/2.0/.

#include "matoya.h"

#include <windows.h>
#include <timeapi.h>

#include "mty-tls.h"

static MTY_TLS bool TIME_FREQ_INIT;
static MTY_TLS float TIME_FREQUENCY;

int64_t MTY_Timestamp(void)
{
	LARGE_INTEGER ts;
	QueryPerformanceCounter(&ts);

	return ts.QuadPart;
}

float MTY_TimeDiff(int64_t begin, int64_t end)
{
	if (!TIME_FREQ_INIT) {
		LARGE_INTEGER frequency;
		QueryPerformanceFrequency(&frequency);
		TIME_FREQUENCY = (float) frequency.QuadPart / 1000.0f;
		TIME_FREQ_INIT = true;
	}

	return (float) (end - begin) / TIME_FREQUENCY;
}

void MTY_Sleep(uint32_t timeout)
{
	HANDLE timer = CreateWaitableTimer(NULL, TRUE, NULL);
	if (timer) {
		LARGE_INTEGER ft;
		ft.QuadPart = -10000 * (int32_t) timeout;

		if (SetWaitableTimer(timer, &ft, 0, NULL, NULL, FALSE))
			WaitForSingleObject(timer, INFINITE);

		CloseHandle(timer);
	}
}

void MTY_SetTimerResolution(uint32_t res)
{
	timeBeginPeriod(res);
}

void MTY_RevertTimerResolution(uint32_t res)
{
	timeEndPeriod(res);
}
