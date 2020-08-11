// Copyright (c) 2020 Christopher D. Dickson <cdd@matoya.group>
//
// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at https://mozilla.org/MPL/2.0/.

#pragma once


// Interface

#define CURLINFO_LONG             0x200000

#define CURLOPTTYPE_LONG          0
#define CURLOPTTYPE_OBJECTPOINT   10000
#define CURLOPTTYPE_FUNCTIONPOINT 20000
#define CURLOPTTYPE_STRINGPOINT   CURLOPTTYPE_OBJECTPOINT
#define CURLOPTTYPE_SLISTPOINT    CURLOPTTYPE_OBJECTPOINT

#define CURL_GLOBAL_SSL           (1 << 0)
#define CURL_GLOBAL_WIN32         (1 << 1)
#define CURL_GLOBAL_ALL           (CURL_GLOBAL_SSL | CURL_GLOBAL_WIN32)

typedef enum {
	CURLE_OK = 0,
} CURLcode;

typedef enum {
	CURLOPT_WRITEDATA       = CURLOPTTYPE_OBJECTPOINT   + 1,
	CURLOPT_URL             = CURLOPTTYPE_STRINGPOINT   + 2,
	CURLOPT_READDATA        = CURLOPTTYPE_OBJECTPOINT   + 9,
	CURLOPT_WRITEFUNCTION   = CURLOPTTYPE_FUNCTIONPOINT + 11,
	CURLOPT_READFUNCTION    = CURLOPTTYPE_FUNCTIONPOINT + 12,
	CURLOPT_INFILESIZE      = CURLOPTTYPE_LONG          + 14,
	CURLOPT_USERAGENT       = CURLOPTTYPE_STRINGPOINT   + 18,
	CURLOPT_HTTPHEADER      = CURLOPTTYPE_SLISTPOINT    + 23,
	CURLOPT_HEADERDATA      = CURLOPTTYPE_OBJECTPOINT   + 29,
	CURLOPT_CUSTOMREQUEST   = CURLOPTTYPE_STRINGPOINT   + 36,
	CURLOPT_UPLOAD          = CURLOPTTYPE_LONG          + 46,
	CURLOPT_FOLLOWLOCATION  = CURLOPTTYPE_LONG          + 52,
	CURLOPT_HEADERFUNCTION  = CURLOPTTYPE_FUNCTIONPOINT + 79,
	CURLOPT_NOSIGNAL        = CURLOPTTYPE_LONG          + 99,
	CURLOPT_ACCEPT_ENCODING = CURLOPTTYPE_STRINGPOINT   + 102,
	CURLOPT_TIMEOUT_MS      = CURLOPTTYPE_LONG          + 155,
} CURLoption;

typedef enum {
	CURLINFO_RESPONSE_CODE = CURLINFO_LONG + 2,
} CURLINFO;

struct curl_slist {
	char *data;
	struct curl_slist *next;
};

typedef void CURL;

static CURLcode (*curl_global_init)(long flags);
static void (*curl_global_cleanup)(void);
static void (*curl_free)(void *p);
static CURL *(*curl_easy_init)(void);
static char *(*curl_easy_escape)(CURL *handle, const char *string, int length);
static CURLcode (*curl_easy_perform)(CURL *curl);
static CURLcode (*curl_easy_setopt)(CURL *handle, CURLoption option, ...);
static void (*curl_easy_cleanup)(CURL *curl);
static CURLcode (*curl_easy_getinfo)(CURL *curl, CURLINFO info, ...);
static struct curl_slist *(*curl_slist_append)(struct curl_slist *, const char *);
static void (*curl_slist_free_all)(struct curl_slist *);


// Runtime open

static MTY_Atomic32 CURL_DL_LOCK;
static MTY_SO *CURL_DL_SO;
static bool CURL_DL_INIT;

static void curl_dl_global_destroy(void)
{
	if (CURL_DL_INIT)
		curl_global_cleanup();

	MTY_SOUnload(&CURL_DL_SO);
	CURL_DL_INIT = false;
}

static bool curl_dl_global_init(void)
{
	MTY_GlobalLock(&CURL_DL_LOCK);

	if (!CURL_DL_INIT) {
		bool r = true;

		CURL_DL_SO = MTY_SOLoad("libcurl.so.3");

		if (!CURL_DL_SO)
			CURL_DL_SO = MTY_SOLoad("libcurl-gnutls.so.3");

		if (!CURL_DL_SO)
			CURL_DL_SO = MTY_SOLoad("libcurl-nss.so.3");

		if (!CURL_DL_SO) {
			r = false;
			goto except;
		}

		#define LOAD_SYM(so, name) \
			name = MTY_SOSymbolGet(so, #name); \
			if (!name) {r = false; goto except;}

		LOAD_SYM(CURL_DL_SO, curl_global_init);
		LOAD_SYM(CURL_DL_SO, curl_global_cleanup);
		LOAD_SYM(CURL_DL_SO, curl_free);
		LOAD_SYM(CURL_DL_SO, curl_easy_init);
		LOAD_SYM(CURL_DL_SO, curl_easy_escape);
		LOAD_SYM(CURL_DL_SO, curl_easy_perform);
		LOAD_SYM(CURL_DL_SO, curl_easy_setopt);
		LOAD_SYM(CURL_DL_SO, curl_easy_cleanup);
		LOAD_SYM(CURL_DL_SO, curl_easy_getinfo);
		LOAD_SYM(CURL_DL_SO, curl_slist_append);
		LOAD_SYM(CURL_DL_SO, curl_slist_free_all);

		// On Linux, we use RTLD_DEEPBIND so any global deps of curl are not interfered with
		// by a potential global dependecy in the calling application. macOS will always use
		// "DEEPBIND" behavior.
		CURLcode e = curl_global_init(CURL_GLOBAL_ALL);
		if (e != CURLE_OK) {
			MTY_Log("'curl_global_init' failed with error %d", e);
			r = false;
			goto except;
		}

		except:

		if (!r)
			curl_dl_global_destroy();

		CURL_DL_INIT = r;
	}

	MTY_GlobalUnlock(&CURL_DL_LOCK);

	return CURL_DL_INIT;
}
