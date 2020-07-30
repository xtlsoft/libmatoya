// Copyright (c) 2020 Christopher D. Dickson <cdd@matoya.group>
//
// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at https://mozilla.org/MPL/2.0/.

#include "matoya.h"

#include <winsock2.h>

uint16_t MTY_Swap16(uint16_t value)
{
	return _byteswap_ushort(value);
}

uint32_t MTY_Swap32(uint32_t value)
{
	return _byteswap_ulong(value);
}

uint64_t MTY_Swap64(uint64_t value)
{
	return _byteswap_uint64(value);
}

uint16_t MTY_SwapToBE16(uint16_t value)
{
	return htons(value);
}

uint32_t MTY_SwapToBE32(uint32_t value)
{
	return htonl(value);
}

uint64_t MTY_SwapToBE64(uint64_t value)
{
	return htonll(value);
}

uint16_t MTY_SwapFromBE16(uint16_t value)
{
	return ntohs(value);
}

uint32_t MTY_SwapFromBE32(uint32_t value)
{
	return ntohl(value);
}

uint64_t MTY_SwapFromBE64(uint64_t value)
{
	return ntohll(value);
}

char *MTY_WideToMulti(const wchar_t *src, char *dst, size_t len)
{
	if (!dst || len == 0) {
		len = wcslen(src) + 1;
		dst = MTY_Alloc(len, 4); // Maximum of 4 bytes per character
	}

	int32_t n = WideCharToMultiByte(CP_UTF8, WC_ERR_INVALID_CHARS, src, -1, dst,
		(int32_t) len, NULL, NULL);

	if (n > 0) {
		dst[n] = '\0';

	} else {
		MTY_Log("'WideCharToMultiByte' failed with return value %d", n);
		memset(dst, 0, len);
	}

	return dst;
}

wchar_t *MTY_MultiToWide(const char *src, wchar_t *dst, uint32_t len)
{
	if (!dst || len == 0) {
		len = (uint32_t) strlen(src) + 1;
		dst = MTY_Alloc(len, sizeof(WCHAR));
	}

	int32_t n = MultiByteToWideChar(CP_UTF8, 0, src, -1, dst, (int32_t) len);

	if (n != (int32_t) strlen(src) + 1) {
		MTY_Log("'MultiByteToWideChar' failed with return value %d", n);
		memset(dst, 0, len * sizeof(WCHAR));
	}

	return dst;
}
