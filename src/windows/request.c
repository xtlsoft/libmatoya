// Copyright (c) 2020 Christopher D. Dickson <cdd@matoya.group>
//
// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at https://mozilla.org/MPL/2.0/.

#include "matoya.h"

#include <stdio.h>
#include <stdlib.h>

#include <windows.h>
#include <winhttp.h>

#define REQUEST_MAX_URL     768
#define REQUEST_MAX_HEADERS 512
#define REQUEST_USER_AGENT  L""

struct MTY_Request {
	HINTERNET session;
	HINTERNET conn;
	HINTERNET req;
};

bool MTY_RequestCreate(const char *url, const char *method, const char *headers,
	const void *body, size_t size, int32_t timeout, MTY_Request **request)
{
	MTY_Request *ctx = MTY_Alloc(1, sizeof(MTY_Request));
	bool r = true;

	WCHAR url_w[REQUEST_MAX_URL];
	_snwprintf_s(url_w, REQUEST_MAX_URL, _TRUNCATE, L"%hs", url);

	WCHAR method_w[16];
	_snwprintf_s(method_w, 16, _TRUNCATE, L"%hs", method);

	WCHAR headers_w[REQUEST_MAX_HEADERS] = {0};
	if (headers)
		_snwprintf_s(headers_w, REQUEST_MAX_HEADERS, _TRUNCATE, L"%hs", headers);

	WCHAR host_w[REQUEST_MAX_URL];
	WCHAR path_w[REQUEST_MAX_URL];
	URL_COMPONENTS info = {0};
	info.dwStructSize = sizeof(URL_COMPONENTS);
	info.lpszHostName = host_w;
	info.dwHostNameLength = sizeof(host_w) / sizeof(WCHAR);
	info.lpszUrlPath = path_w;
	info.dwUrlPathLength = sizeof(path_w) / sizeof(WCHAR);
	if (!WinHttpCrackUrl(url_w, (DWORD) wcslen(url_w), ICU_ESCAPE, &info)) {
		MTY_Log("'WinHttpCrackUrl' failed with error %x", GetLastError());
		r = false;
		goto except;
	}

	ctx->session = WinHttpOpen(REQUEST_USER_AGENT, WINHTTP_ACCESS_TYPE_DEFAULT_PROXY,
		WINHTTP_NO_PROXY_NAME, WINHTTP_NO_PROXY_BYPASS, 0);
	if (!ctx->session) {
		MTY_Log("'WinHttpOpen' failed with error %x", GetLastError());
		r = false;
		goto except;
	}

	if (!WinHttpSetTimeouts(ctx->session, timeout, timeout, timeout, timeout)) {
		MTY_Log("'WinHttpSetTimeouts' failed with error %x", GetLastError());
		r = false;
		goto except;
	}

	DWORD opt = WINHTTP_FLAG_SECURE_PROTOCOL_TLS1_2;
	WinHttpSetOption(ctx->session, WINHTTP_OPTION_SECURE_PROTOCOLS, &opt, sizeof(DWORD));

	opt = WINHTTP_DECOMPRESSION_FLAG_GZIP | WINHTTP_DECOMPRESSION_FLAG_DEFLATE;
	if (!WinHttpSetOption(ctx->session, WINHTTP_OPTION_DECOMPRESSION, &opt, sizeof(DWORD))) {
		MTY_Log("'WinHttpSetOption' failed with error %x", GetLastError());
		r = false;
		goto except;
	}

	ctx->conn = WinHttpConnect(ctx->session, info.lpszHostName, info.nPort, 0);
	if (!ctx->conn) {
		MTY_Log("'WinHttpConnect' failed with error %x", GetLastError());
		r = false;
		goto except;
	}

	ctx->req = WinHttpOpenRequest(ctx->conn, method_w, info.lpszUrlPath, NULL, WINHTTP_NO_REFERER,
		WINHTTP_DEFAULT_ACCEPT_TYPES, (info.nScheme == INTERNET_SCHEME_HTTPS) ? WINHTTP_FLAG_SECURE : 0);
	if (!ctx->conn) {
		MTY_Log("'WinHttpOpenRequest' failed with error %x", GetLastError());
		r = false;
		goto except;
	}

	if (!WinHttpSendRequest(ctx->req,
		headers ? headers_w : WINHTTP_NO_ADDITIONAL_HEADERS,
		headers ? -1L : 0,
		(void *) body, (DWORD) size, (DWORD) size, 0)) {

		MTY_Log("'WinHttpSendRequest' failed with error %x", GetLastError());
		r = false;
		goto except;
	}

	if (!WinHttpReceiveResponse(ctx->req, NULL)) {
		MTY_Log("'WinHttpReceiveRequest' failed with error %x", GetLastError());
		r = false;
		goto except;
	}

	except:

	if (!request) {
		MTY_RequestDestroy(&ctx);

	} else {
		*request = ctx;

		if (!r)
			MTY_RequestDestroy(request);
	}

	return r;
}

bool MTY_RequestGetStatusCode(MTY_Request *ctx, uint32_t *statusCode)
{
	*statusCode = 0;

	WCHAR code_w[16] = {0};
	DWORD buf_len = sizeof(code_w);
	if (!WinHttpQueryHeaders(ctx->req, WINHTTP_QUERY_STATUS_CODE, NULL, code_w, &buf_len, NULL))
		return false;

	*statusCode = _wtoi(code_w);

	return true;
}

bool MTY_RequestGetBody(MTY_Request *ctx, void **body, size_t *size)
{
	*size = 0;
	*body = NULL;

	DWORD available = 0;

	do {
		if (!WinHttpQueryDataAvailable(ctx->req, &available))
			return false;

		void *buf = MTY_Alloc(available, 1);

		DWORD read = 0;
		BOOL success = WinHttpReadData(ctx->req, buf, available, &read);

		if (success && read > 0) {
			*body = MTY_Realloc(*body, *size + read + 1, 1);
			memcpy((uint8_t *) *body + *size, buf, read);
			*size += read;

			((char *) *body)[*size] = '\0';
		}

		MTY_Free(buf);

		if (!success)
			return false;

	} while (available > 0);

	return true;
}

bool MTY_RequestGetHeader(MTY_Request *ctx, const char *key, char *value, size_t size)
{
	WCHAR key_w[64];
	_snwprintf_s(key_w, 64, _TRUNCATE, L"%hs", key);

	WCHAR value_w[128];
	DWORD buf_len = sizeof(value_w);

	buf_len = 128 * sizeof(WCHAR);
	if (!WinHttpQueryHeaders(ctx->req, WINHTTP_QUERY_CUSTOM, key_w, value_w, &buf_len, NULL))
		return false;

	snprintf(value, size, "%ws", value_w);

	return true;
}

void MTY_RequestDestroy(MTY_Request **request)
{
	if (!request || !*request)
		return;

	MTY_Request *ctx = *request;

	if (ctx->req)
		WinHttpCloseHandle(ctx->req);

	if (ctx->conn)
		WinHttpCloseHandle(ctx->conn);

	if (ctx->session)
		WinHttpCloseHandle(ctx->session);

	MTY_Free(ctx);
	*request = NULL;
}
