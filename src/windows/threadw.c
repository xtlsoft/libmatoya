// Copyright (c) 2020 Christopher D. Dickson <cdd@matoya.group>
//
// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at https://mozilla.org/MPL/2.0/.

#include "matoya.h"

#include <stdlib.h>

#include <windows.h>



/*** THREAD ***/

struct MTY_Thread {
	HANDLE thread;
	bool detach;
	void *(*func)(void *opaque);
	void *opaque;
	void *ret;
};

static DWORD WINAPI thread_func(LPVOID *lpParameter)
{
	MTY_Thread *ctx = (MTY_Thread *) lpParameter;

	ctx->ret = ctx->func(ctx->opaque);

	if (ctx->detach)
		MTY_Free(ctx);

	return 0;
}

void MTY_ThreadCreate(void *(*func)(void *opaque), const void *opaque, MTY_Thread **thread)
{
	MTY_Thread *ctx = MTY_Alloc(1, sizeof(MTY_Thread));
	ctx->func = func;
	ctx->opaque = (void *) opaque;
	ctx->detach = !thread;

	if (thread)
		*thread = ctx;

	ctx->thread = CreateThread(NULL, 0, thread_func, ctx, 0, NULL);

	if (!ctx->thread)
		MTY_Fatal("'CreateThread' failed with error %x", GetLastError());

	if (ctx->detach) {
		if (!CloseHandle(ctx->thread))
			MTY_Fatal("'CloseHandle' failed with error %x", GetLastError());

		ctx->thread = NULL;
	}
}

int64_t MTY_ThreadIDGet(MTY_Thread *ctx)
{
	return ctx ? GetThreadId(ctx->thread) : GetCurrentThreadId();
}

void *MTY_ThreadDestroy(MTY_Thread **thread)
{
	if (!thread || !*thread)
		return NULL;

	MTY_Thread *ctx = *thread;

	if (ctx->thread) {
		if (WaitForSingleObject(ctx->thread, INFINITE) == WAIT_FAILED)
			MTY_Fatal("'WaitForSingleObject' failed with error %x", GetLastError());

		if (!CloseHandle(ctx->thread))
			MTY_Fatal("'CloseHandle' failed with error %x", GetLastError());
	}

	void *ret = ctx->ret;

	MTY_Free(ctx);
	*thread = NULL;

	return ret;
}



/*** MUTEX ***/

struct MTY_Mutex {
	CRITICAL_SECTION mutex;
};

void MTY_MutexCreate(MTY_Mutex **mutex)
{
	MTY_Mutex *ctx = *mutex = MTY_Alloc(1, sizeof(MTY_Mutex));

	InitializeCriticalSection(&ctx->mutex);
}

void MTY_MutexLock(MTY_Mutex *ctx)
{
	EnterCriticalSection(&ctx->mutex);
}

bool MTY_MutexTryLock(MTY_Mutex *ctx)
{
	return TryEnterCriticalSection(&ctx->mutex);
}

void MTY_MutexUnlock(MTY_Mutex *ctx)
{
	LeaveCriticalSection(&ctx->mutex);
}

void MTY_MutexDestroy(MTY_Mutex **mutex)
{
	if (!mutex || !*mutex)
		return;

	MTY_Mutex *ctx = *mutex;

	DeleteCriticalSection(&ctx->mutex);

	MTY_Free(ctx);
	*mutex = NULL;
}



/*** COND ***/

struct MTY_Cond {
	CONDITION_VARIABLE cond;
};

void MTY_CondCreate(MTY_Cond **cond)
{
	MTY_Cond *ctx = *cond = MTY_Alloc(1, sizeof(MTY_Cond));

	InitializeConditionVariable(&ctx->cond);
}

bool MTY_CondWait(MTY_Cond *ctx, MTY_Mutex *mutex, int32_t timeout)
{
	bool r = true;
	timeout = timeout < 0 ? INFINITE : timeout;

	BOOL success = SleepConditionVariableCS(&ctx->cond, &mutex->mutex, timeout);

	if (!success) {
		DWORD e = GetLastError();

		if (e == ERROR_TIMEOUT) {
			r = false;

		} else {
			MTY_Fatal("'SleepConditionVariableCS' failed with error %x", e);
		}
	}

	return r;
}

void MTY_CondWake(MTY_Cond *ctx)
{
	WakeConditionVariable(&ctx->cond);
}

void MTY_CondWakeAll(MTY_Cond *ctx)
{
	WakeAllConditionVariable(&ctx->cond);
}

void MTY_CondDestroy(MTY_Cond **cond)
{
	if (!cond || !*cond)
		return;

	MTY_Cond *ctx = *cond;

	MTY_Free(ctx);
	*cond = NULL;
}



/*** ATOMIC ***/

void MTY_Atomic32Set(MTY_Atomic32 *atomic, int32_t value)
{
	InterlockedExchange((volatile LONG *) &atomic->value, value);
}

void MTY_Atomic64Set(MTY_Atomic64 *atomic, int64_t value)
{
	InterlockedExchange64(&atomic->value, value);
}

int32_t MTY_Atomic32Get(MTY_Atomic32 *atomic)
{
	return InterlockedOr((volatile LONG *) &atomic->value, 0);
}

int64_t MTY_Atomic64Get(MTY_Atomic64 *atomic)
{
	return InterlockedOr64(&atomic->value, 0);
}

int32_t MTY_Atomic32Add(MTY_Atomic32 *atomic, int32_t value)
{
	return InterlockedAdd((volatile LONG *) &atomic->value, value);
}

int64_t MTY_Atomic64Add(MTY_Atomic64 *atomic, int64_t value)
{
	return InterlockedAdd64(&atomic->value, value);
}

bool MTY_Atomic32CAS(MTY_Atomic32 *atomic, int32_t oldValue, int32_t newValue)
{
	return InterlockedCompareExchange((volatile LONG *) &atomic->value, newValue, oldValue) == oldValue;
}

bool MTY_Atomic64CAS(MTY_Atomic64 *atomic, int64_t oldValue, int64_t newValue)
{
	return InterlockedCompareExchange64(&atomic->value, newValue, oldValue) == oldValue;
}