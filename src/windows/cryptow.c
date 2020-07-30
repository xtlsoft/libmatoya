// Copyright (c) 2020 Christopher D. Dickson <cdd@matoya.group>
//
// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at https://mozilla.org/MPL/2.0/.

#include "matoya.h"

#include <stdio.h>

#include <ntstatus.h>

#define WIN32_NO_STATUS
#include <windows.h>
#include <bcrypt.h>



/*** AES GCM ***/

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

	e = BCryptGenerateSymmetricKey(ctx->ahandle, &ctx->khandle, NULL, 0, (UCHAR *) key, (ULONG) keySize, 0);
	if (e != STATUS_SUCCESS) {
		MTY_Log("'BCryptGenerateSymmetricKey' failed with error %x", e);
		r = false;
		goto except;
	}

	e = BCryptSetProperty(ctx->khandle, BCRYPT_CHAINING_MODE, (UCHAR *) BCRYPT_CHAIN_MODE_GCM,
		(ULONG) sizeof(BCRYPT_CHAIN_MODE_GCM), 0);
	if (e != STATUS_SUCCESS) {
		MTY_Log("'BCryptSetProperty' failed with error %x", e);
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



/*** HASH ***/

static void crypto_hash(const void *input, size_t inputSize, const void *key, size_t keySize,
	WCHAR *alg_id, void *output, size_t outputSize)
{
	BCRYPT_ALG_HANDLE ahandle = NULL;
	BCRYPT_HASH_HANDLE hhandle = NULL;

	NTSTATUS e = BCryptOpenAlgorithmProvider(&ahandle, alg_id, NULL, key ? BCRYPT_ALG_HANDLE_HMAC_FLAG : 0);
	if (e != STATUS_SUCCESS) {
		MTY_Log("'BCryptOpenAlgorithmProvider' failed with error %x", e);
		goto except;
	}

	e = BCryptCreateHash(ahandle, &hhandle, NULL, 0, (UCHAR *) key, (ULONG) keySize, 0);
	if (e != STATUS_SUCCESS) {
		MTY_Log("'BCryptCreateHash' failed with error %x", e);
		goto except;
	}

	e = BCryptHashData(hhandle, (UCHAR *) input, (ULONG) inputSize, 0);
	if (e != STATUS_SUCCESS) {
		MTY_Log("'BCryptHashData' failed with error %x", e);
		goto except;
	}

	DWORD size = 0;
	ULONG written = 0;
	e = BCryptGetProperty(hhandle, BCRYPT_HASH_LENGTH, (UCHAR *) &size, sizeof(DWORD), &written, 0);
	if (e != STATUS_SUCCESS) {
		MTY_Log("'BCryptGetProperty' failed with error %x", e);
		goto except;
	}

	if (size > outputSize) {
		MTY_Log("'outputSize' must be at least %u", size);
		goto except;
	}

	e = BCryptFinishHash(hhandle, output, size, 0);
	if (e != STATUS_SUCCESS) {
		MTY_Log("'BCryptFinishHash' failed with error %x", e);
		goto except;
	}

	except:

	if (hhandle)
		BCryptDestroyHash(hhandle);

	if (ahandle)
		BCryptCloseAlgorithmProvider(ahandle, 0);
}

void MTY_CryptoHash(MTY_Algorithm algo, const void *input, size_t inputSize, const void *key,
	size_t keySize, void *output, size_t outputSize)
{
	switch (algo) {
		case MTY_ALGORITHM_SHA1:
			crypto_hash(input, inputSize, key, keySize, BCRYPT_SHA1_ALGORITHM, output, outputSize);
			break;
		case MTY_ALGORITHM_SHA1_HEX: {
			uint8_t bytes[MTY_CRYPTO_SHA1_SIZE];
			crypto_hash(input, inputSize, key, keySize, BCRYPT_SHA1_ALGORITHM, bytes, sizeof(bytes));
			MTY_CryptoBytesToHex(bytes, sizeof(bytes), output, outputSize);
			break;
		}
		case MTY_ALGORITHM_SHA256:
			crypto_hash(input, inputSize, key, keySize, BCRYPT_SHA256_ALGORITHM, output, outputSize);
			break;
		case MTY_ALGORITHM_SHA256_HEX: {
			uint8_t bytes[MTY_CRYPTO_SHA256_SIZE];
			crypto_hash(input, inputSize, key, keySize, BCRYPT_SHA256_ALGORITHM, bytes, sizeof(bytes));
			MTY_CryptoBytesToHex(bytes, sizeof(bytes), output, outputSize);
			break;
		}
	}
}



/*** OTHER ***/

void MTY_CryptoRandom(void *output, size_t size)
{
	NTSTATUS e = BCryptGenRandom(NULL, output, (ULONG) size, BCRYPT_USE_SYSTEM_PREFERRED_RNG);

	if (e != STATUS_SUCCESS)
		MTY_Log("'BCryptGenRandom' failed with error %x", e);
}
