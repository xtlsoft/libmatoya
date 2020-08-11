// Copyright (c) 2020 Christopher D. Dickson <cdd@matoya.group>
//
// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at https://mozilla.org/MPL/2.0/.

#include "matoya.h"

#include <CommonCrypto/CommonCryptor.h>

#include "CommonCryptorSPI.h"

struct MTY_AESGCM {
	CCCryptorRef dec;
	CCCryptorRef enc;
};

bool MTY_AESGCMCreate(const void *key, size_t keySize, MTY_AESGCM **aesgcm)
{
	MTY_AESGCM *ctx = *aesgcm = MTY_Alloc(1, sizeof(MTY_AESGCM));

	CCCryptorStatus e = CCCryptorCreateWithMode(kCCEncrypt, kCCModeGCM, kCCAlgorithmAES128,
		0, NULL, key, keySize, NULL, 0, 0, 0, &ctx->enc);
	if (e != kCCSuccess) {
		MTY_Log("'CCCryptoCreateWithMode' failed with error %d", e);
		goto except;
	}

	e = CCCryptorCreateWithMode(kCCDecrypt, kCCModeGCM, kCCAlgorithmAES128,
		0, NULL, key, keySize, NULL, 0, 0, 0, &ctx->dec);
	if (e != kCCSuccess) {
		MTY_Log("'CCCryptoCreateWithMode' failed with error %d", e);
		goto except;
	}

	except:

	if (e != kCCSuccess)
		MTY_AESGCMDestroy(aesgcm);

	return e == kCCSuccess;
}

bool MTY_AESGCMEncrypt(MTY_AESGCM *ctx, const void *plainText, size_t textSize, const void *nonce,
	size_t nonceSize, void *hash, size_t hashSize, void *cipherText)
{
	CCCryptorStatus e = CCCryptorGCMReset(ctx->enc);
	if (e != kCCSuccess) {
		MTY_Log("'CCCryptorReset' failed with error %d", e);
		return false;
	}

	e = CCCryptorGCMAddIV(ctx->enc, nonce, nonceSize);
	if (e != kCCSuccess) {
		MTY_Log("'CCCryptorGCMAddIV' failed with error %d", e);
		return false;
	}

	e = CCCryptorGCMEncrypt(ctx->enc, plainText, textSize, cipherText);
	if (e != kCCSuccess) {
		MTY_Log("'CCCryptorGCMEncrypt' failed with error %d", e);
		return false;
	}

	e = CCCryptorGCMFinal(ctx->enc, hash, &hashSize);
	if (e != kCCSuccess) {
		MTY_Log("'CCCryptorGCMFinal' failed with error %d", e);
		return false;
	}

	return true;
}

bool MTY_AESGCMDecrypt(MTY_AESGCM *ctx, const void *cipherText, size_t textSize, const void *nonce,
	size_t nonceSize, const void *hash, size_t hashSize, void *plainText)
{
	CCCryptorStatus e = CCCryptorGCMReset(ctx->dec);
	if (e != kCCSuccess) {
		MTY_Log("'CCCryptorReset' failed with error %d", e);
		return false;
	}

	e = CCCryptorGCMAddIV(ctx->dec, nonce, nonceSize);
	if (e != kCCSuccess) {
		MTY_Log("'CCCryptorGCMAddIV' failed with error %d", e);
		return false;
	}

	e = CCCryptorGCMDecrypt(ctx->dec, cipherText, textSize, plainText);
	if (e != kCCSuccess) {
		MTY_Log("'CCCryptorGCMDecrypt' failed with error %d", e);
		return false;
	}

	uint8_t hashf[32];
	size_t hashf_size = 32;
	e = CCCryptorGCMFinal(ctx->dec, hashf, &hashf_size);
	if (e != kCCSuccess) {
		MTY_Log("'CCCryptorGCMFinal' failed with error %d", e);
		return false;
	}

	if (memcmp(hash, hashf, MTY_MIN(hashf_size, hashSize))) {
		MTY_Log("Authentication hash mismatch");
		return false;
	}

	return true;
}

void MTY_AESGCMDestroy(MTY_AESGCM **aesgcm)
{
	if (!aesgcm || !*aesgcm)
		return;

	MTY_AESGCM *ctx = *aesgcm;

	if (ctx->enc)
		CCCryptorRelease(ctx->enc);

	if (ctx->dec)
		CCCryptorRelease(ctx->dec);

	MTY_Free(ctx);
	*aesgcm = NULL;
}
