// Copyright (c) 2020 Christopher D. Dickson <cdd@matoya.group>
//
// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at https://mozilla.org/MPL/2.0/.

#include "matoya.h"

#include <ntstatus.h>

#define WIN32_NO_STATUS
#include <windows.h>
#include <bcrypt.h>

struct MTY_AESGCM {
	BCRYPT_ALG_HANDLE ahandle;
	BCRYPT_KEY_HANDLE khandle;
};

bool MTY_AESGCMCreate(const void *key, size_t keySize, MTY_AESGCM **aesgcm)
{
	MTY_AESGCM *ctx = *aesgcm = MTY_Alloc(1, sizeof(MTY_AESGCM));
	bool r = true;

	NTSTATUS e = BCryptOpenAlgorithmProvider(&ctx->ahandle, BCRYPT_AES_ALGORITHM, NULL, 0);
	if (e != STATUS_SUCCESS) {
		MTY_Log("'BCryptOpenAlgorithmProvider' failed with error %x", e);
		r = false;
		goto except;
	}

	e = BCryptSetProperty(ctx->ahandle, BCRYPT_CHAINING_MODE, (UCHAR *) BCRYPT_CHAIN_MODE_GCM,
		(ULONG) sizeof(BCRYPT_CHAIN_MODE_GCM), 0);
	if (e != STATUS_SUCCESS) {
		MTY_Log("'BCryptSetProperty' failed with error %x", e);
		r = false;
		goto except;
	}

	e = BCryptGenerateSymmetricKey(ctx->ahandle, &ctx->khandle, NULL, 0, (UCHAR *) key, (ULONG) keySize, 0);
	if (e != STATUS_SUCCESS) {
		MTY_Log("'BCryptGenerateSymmetricKey' failed with error %x", e);
		r = false;
		goto except;
	}

	except:

	if (!r)
		MTY_AESGCMDestroy(aesgcm);

	return r;
}

bool MTY_AESGCMEncrypt(MTY_AESGCM *ctx, const void *plainText, size_t textSize, const void *nonce,
	size_t nonceSize, void *hash, size_t hashSize, void *cipherText)
{
	BCRYPT_AUTHENTICATED_CIPHER_MODE_INFO info = {0};
	BCRYPT_INIT_AUTH_MODE_INFO(info);
	info.pbNonce = (UCHAR *) nonce;
	info.cbNonce = (ULONG) nonceSize;
	info.pbTag = hash;
	info.cbTag = (ULONG) hashSize;

	ULONG output = 0;
	NTSTATUS e = BCryptEncrypt(ctx->khandle, (UCHAR *) plainText, (ULONG) textSize, &info,
		NULL, 0, cipherText, (ULONG) textSize, &output, 0);

	return e == STATUS_SUCCESS && output == textSize;
}

bool MTY_AESGCMDecrypt(MTY_AESGCM *ctx, const void *cipherText, size_t textSize, const void *nonce,
	size_t nonceSize, const void *hash, size_t hashSize, void *plainText)
{
	BCRYPT_AUTHENTICATED_CIPHER_MODE_INFO info = {0};
	BCRYPT_INIT_AUTH_MODE_INFO(info);
	info.pbNonce = (UCHAR *) nonce;
	info.cbNonce = (ULONG) nonceSize;
	info.pbTag = (UCHAR *) hash;
	info.cbTag = (ULONG) hashSize;

	ULONG output = 0;
	NTSTATUS e = BCryptDecrypt(ctx->khandle, (UCHAR *) cipherText, (ULONG) textSize, &info,
		NULL, 0, plainText, (ULONG) textSize, &output, 0);

	return e == STATUS_SUCCESS && output == textSize;
}

void MTY_AESGCMDestroy(MTY_AESGCM **aesgcm)
{
	if (!aesgcm || !*aesgcm)
		return;

	MTY_AESGCM *ctx = *aesgcm;

	if (ctx->khandle)
		BCryptDestroyKey(ctx->khandle);

	if (ctx->ahandle)
		BCryptCloseAlgorithmProvider(ctx->ahandle, 0);

	MTY_Free(ctx);
	*aesgcm = NULL;
}
