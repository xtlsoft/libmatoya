// Copyright (c) 2020 Christopher D. Dickson <cdd@matoya.group>
//
// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at https://mozilla.org/MPL/2.0/.

#define _DEFAULT_SOURCE // htobe64, be64toh (mty-swap.h)
#define _DARWIN_C_SOURCE // htonll, ntohll (mty-swap.h)

#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <wchar.h>

#include "matoya.h"

uint16_t MTY_Swap16(uint16_t value)
{
	return __builtin_bswap16(value);
}

uint32_t MTY_Swap32(uint32_t value)
{
	return __builtin_bswap32(value);
}

uint64_t MTY_Swap64(uint64_t value)
{
	return __builtin_bswap64(value);
}

uint16_t MTY_SwapToBE16(uint16_t value)
{
	#if __BYTE_ORDER__ == __ORDER_LITTLE_ENDIAN__
		return MTY_Swap16(value);
	#else
		return value;
	#endif
}

uint32_t MTY_SwapToBE32(uint32_t value)
{
	#if __BYTE_ORDER__ == __ORDER_LITTLE_ENDIAN__
		return MTY_Swap32(value);
	#else
		return value;
	#endif
}

uint64_t MTY_SwapToBE64(uint64_t value)
{
	#if __BYTE_ORDER__ == __ORDER_LITTLE_ENDIAN__
		return MTY_Swap64(value);
	#else
		return value;
	#endif
}

uint16_t MTY_SwapFromBE16(uint16_t value)
{
	#if __BYTE_ORDER__ == __ORDER_LITTLE_ENDIAN__
		return MTY_Swap16(value);
	#else
		return value;
	#endif
}

uint32_t MTY_SwapFromBE32(uint32_t value)
{
	#if __BYTE_ORDER__ == __ORDER_LITTLE_ENDIAN__
		return MTY_Swap32(value);
	#else
		return value;
	#endif
}

uint64_t MTY_SwapFromBE64(uint64_t value)
{
	#if __BYTE_ORDER__ == __ORDER_LITTLE_ENDIAN__
		return MTY_Swap64(value);
	#else
		return value;
	#endif
}

char *MTY_WideToMulti(const wchar_t *src, char *dst, size_t len)
{
	if (!dst || len == 0) {
		len = wcslen(src) + 1;
		dst = MTY_Alloc(len, 4); // Maximum of 4 bytes per character
	}

	size_t n = wcstombs(dst, src, len);

	if (n > 0 && n != (size_t) -1) {
		dst[n] = '\0';

	} else {
		MTY_Log("'wcstombs' failed with errno %d", errno);
		memset(dst, 0, len);
	}

	return dst;
}

wchar_t *MTY_MultiToWide(const char *src, wchar_t *dst, uint32_t len)
{
	if (!dst || len == 0) {
		len = (uint32_t) strlen(src) + 1;
		dst = MTY_Alloc(len, sizeof(wchar_t));
	}

	size_t n = mbstowcs(dst, src, len);

	if (n != strlen(src)) {
		MTY_Log("'mbstowcs' failed with errno %d", errno);
		memset(dst, 0, len * sizeof(wchar_t));
	}

	return dst;
}
