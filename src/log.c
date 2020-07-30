// Copyright (c) 2020 Christopher D. Dickson <cdd@matoya.group>
//
// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at https://mozilla.org/MPL/2.0/.

#include "matoya.h"

#include <stdio.h>
#include <stdlib.h>

#include "mty-tls.h"

#define LOG_LEN 256

static void log_none(const char *msg, void *opaque)
{
}

// Global
static void (*LOG_CALLBACK)(const char *msg, void *opaque) = log_none;
static void *LOG_OPAQUE;

// Thread local
static MTY_TLS char LOG_MSG[LOG_LEN];

static void log_internal(const char *msg, va_list args)
{
	vsnprintf(LOG_MSG, LOG_LEN, msg, args);
	LOG_CALLBACK(LOG_MSG, LOG_OPAQUE);
}

void MTY_Log(const char *msg, ...)
{
	va_list args;
	va_start(args, msg);
	log_internal(msg, args);
	va_end(args);
}

void MTY_Fatal(const char *msg, ...)
{
	va_list args;
	va_start(args, msg);
	log_internal(msg, args);
	va_end(args);

	abort();
}

void MTY_SetLogCallback(void (*callback)(const char *msg, void *opaque), const void *opaque)
{
	LOG_CALLBACK = callback;
	LOG_OPAQUE = (void *) opaque;
}

const char *MTY_GetLog(void)
{
	return LOG_MSG;
}
