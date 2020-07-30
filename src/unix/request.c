// Copyright (c) 2020 Christopher D. Dickson <cdd@matoya.group>
//
// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at https://mozilla.org/MPL/2.0/.

#include "matoya.h"

#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include <ctype.h>

#include "curl-dl.h"

#define REQUEST_USER_AGENT  ""

struct MTY_Request {
	uint8_t *write_buffer;
	size_t write_size;

	const uint8_t *read_buffer;
	size_t read_size;

	MTY_Hash *headers;
	uint32_t code;
};

static void __attribute__((destructor)) request_global_destroy(void)
{
	curl_dl_global_destroy();
}

static size_t request_write(void *ptr, size_t size, size_t nmemb, void *opaque)
{
	MTY_Request *ctx = (MTY_Request *) opaque;
	size_t realsize = size * nmemb;

	ctx->write_buffer = MTY_Realloc(ctx->write_buffer, ctx->write_size + realsize, 1);
	memcpy(ctx->write_buffer + ctx->write_size, ptr, realsize);
	ctx->write_size += realsize;

	return realsize;
}

static size_t request_read(void *ptr, size_t size, size_t nmemb, void *opaque)
{
	MTY_Request *ctx = (MTY_Request *) opaque;
	size_t realsize = size * nmemb;

	if (realsize > ctx->read_size)
		realsize = ctx->read_size;

	memcpy(ptr, ctx->read_buffer, realsize);
	ctx->read_buffer += realsize;
	ctx->read_size -= realsize;

	return realsize;
}

static size_t request_headers(void *buffer, size_t size, size_t nmemb, void *opaque)
{
	MTY_Request *ctx = (MTY_Request *) opaque;
	size_t realsize = size * nmemb;

	char *pair = MTY_Alloc(realsize + 1, 1);
	memcpy(pair, buffer, realsize);

	char *ptr = NULL;
	char *tok = strtok_r(pair, ": ", &ptr);
	if (tok) {
		char *key = MTY_Strdup(tok);
		tok = strtok_r(NULL, ": ", &ptr);
		if (tok) {
			for (size_t x = 0; x < strlen(key); x++)
				key[x] = (char) tolower(key[x]);

			char *value = MTY_Strdup(tok);
			MTY_HashSet(ctx->headers, key, value);
		}

		MTY_Free(key);
	}

	MTY_Free(pair);

	return realsize;
}

static void request_set_headers(const char *headers, struct curl_slist **cheaders)
{
	char *mheaders = MTY_Strdup(headers);

	char *ptr = NULL;
	char *tok = strtok_r(mheaders, "\n", &ptr);

	while (tok) {
		*cheaders = curl_slist_append(*cheaders, tok);
		tok = strtok_r(NULL, "\n", &ptr);
	}

	MTY_Free(mheaders);
}

bool MTY_RequestCreate(const char *url, const char *method, const char *headers,
	const void *body, size_t size, int32_t timeout, MTY_Request **request)
{
	if (!curl_dl_global_init())
		return false;

	MTY_Request *ctx = MTY_Alloc(1, sizeof(MTY_Request));
	bool r = true;
	struct curl_slist *cheaders = NULL;
	char *url_escaped = NULL;

	if (size > INT32_MAX)
		size = INT32_MAX;

	CURL *curl = curl_easy_init();
	if (!curl) {
		MTY_Log("'curl_easy_init' failed");
		r = false;
		goto except;
	}

	url_escaped = curl_easy_escape(curl, url, strlen(url));
	if (!url_escaped) {
		MTY_Log("'curl_easy_escape' failed");
		r = false;
		goto except;
	}

	MTY_HashCreate(0, &ctx->headers);

	curl_easy_setopt(curl, CURLOPT_URL, url_escaped);
	curl_easy_setopt(curl, CURLOPT_ACCEPT_ENCODING, "gzip, deflate");
	curl_easy_setopt(curl, CURLOPT_CUSTOMREQUEST, method);
	curl_easy_setopt(curl, CURLOPT_FOLLOWLOCATION, 1L);
	curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, request_write);
	curl_easy_setopt(curl, CURLOPT_WRITEDATA, ctx);
	curl_easy_setopt(curl, CURLOPT_HEADERFUNCTION, request_headers);
	curl_easy_setopt(curl, CURLOPT_HEADERDATA, ctx);
	curl_easy_setopt(curl, CURLOPT_USERAGENT, REQUEST_USER_AGENT);
	curl_easy_setopt(curl, CURLOPT_NOSIGNAL, 1L);

	if (body) {
		ctx->read_buffer = body;
		ctx->read_size = size;
		curl_easy_setopt(curl, CURLOPT_UPLOAD, 1L);
		curl_easy_setopt(curl, CURLOPT_READFUNCTION, request_read);
		curl_easy_setopt(curl, CURLOPT_READDATA, ctx);
		curl_easy_setopt(curl, CURLOPT_INFILESIZE, (long) size);
		cheaders = curl_slist_append(cheaders, "Expect:");
	}

	if (timeout >= 0)
		curl_easy_setopt(curl, CURLOPT_TIMEOUT_MS, (long) timeout);

	if (headers)
		request_set_headers(headers, &cheaders);

	if (cheaders)
		curl_easy_setopt(curl, CURLOPT_HTTPHEADER, cheaders);

	CURLcode e = curl_easy_perform(curl);
	if (e != CURLE_OK) {
		MTY_Log("'curl_easy_perform' failed with error %d", e);
		goto except;
	}

	long code = 0;
	curl_easy_getinfo(curl, CURLINFO_RESPONSE_CODE, &code);
	ctx->code = code;

	except:

	if (cheaders)
		curl_slist_free_all(cheaders);

	if (curl)
		curl_easy_cleanup(curl);

	if (!request) {
		MTY_RequestDestroy(&ctx);

	if (url_escaped)
		curl_free(url_escaped);

	} else {
		*request = ctx;

		if (!r)
			MTY_RequestDestroy(request);
	}

	return r;
}

bool MTY_RequestGetStatusCode(MTY_Request *ctx, uint32_t *statusCode)
{
	*statusCode = ctx->code;

	return true;
}

bool MTY_RequestGetBody(MTY_Request *ctx, void **body, size_t *size)
{
	if (ctx->write_buffer) {
		*body = ctx->write_buffer;
		ctx->write_buffer = NULL;

		*size = ctx->write_size;
		ctx->write_size = 0;

		return true;
	}

	return false;
}

bool MTY_RequestGetHeader(MTY_Request *ctx, const char *key, char *value, size_t size)
{
	char *header_value = MTY_HashGet(ctx->headers, key);
	if (!header_value)
		return false;

	snprintf(value, size, "%s", header_value);

	return true;
}

void MTY_RequestDestroy(MTY_Request **request)
{
	if (!request || !*request)
		return;

	MTY_Request *ctx = *request;

	MTY_Free(ctx->write_buffer);
	MTY_HashDestroy(&ctx->headers, free);

	MTY_Free(ctx);
	*request = NULL;
}
