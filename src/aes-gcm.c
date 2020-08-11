// Copyright (c) 2020 Christopher D. Dickson <cdd@matoya.group>
//
// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at https://mozilla.org/MPL/2.0/.

#include "matoya.h"

#include <stdlib.h>
#include <stdint.h>
#include <string.h>

#if defined(__arm__) || defined(__aarch64__)
	#include "sse2neon.h"
#else
	#include <immintrin.h>
#endif


// Helpers

typedef union {
	uint8_t u8[16];
	uint32_t u32[4];
	uint64_t u64[2];
} P128;

#define BSWAP_MASK \
	_mm_set_epi8(0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15)

#define rotr32(x, n) \
	(((x) >> (n)) | ((x) << (32 - (n))))


// AES-NI

static void aes128_enc(const __m128i *k, const __m128i *plainText, __m128i *cipherText)
{
	__m128i m = _mm_xor_si128(*plainText, k[0]);

	m = _mm_aesenc_si128(m, k[1]);
	m = _mm_aesenc_si128(m, k[2]);
	m = _mm_aesenc_si128(m, k[3]);
	m = _mm_aesenc_si128(m, k[4]);
	m = _mm_aesenc_si128(m, k[5]);
	m = _mm_aesenc_si128(m, k[6]);
	m = _mm_aesenc_si128(m, k[7]);
	m = _mm_aesenc_si128(m, k[8]);
	m = _mm_aesenc_si128(m, k[9]);

	*cipherText = _mm_aesenclast_si128(m, k[10]);
}


// PCLMULQDQ

static void gfmul(const __m128i *a, const __m128i *b, __m128i *res)
{
	__m128i tmp2, tmp3, tmp4, tmp5, tmp6, tmp7, tmp8, tmp9;

	tmp3 = _mm_clmulepi64_si128(*a, *b, 0x00);
	tmp4 = _mm_clmulepi64_si128(*a, *b, 0x10);
	tmp5 = _mm_clmulepi64_si128(*a, *b, 0x01);
	tmp6 = _mm_clmulepi64_si128(*a, *b, 0x11);
	tmp4 = _mm_xor_si128(tmp4, tmp5);
	tmp5 = _mm_slli_si128(tmp4, 8);
	tmp4 = _mm_srli_si128(tmp4, 8);
	tmp3 = _mm_xor_si128(tmp3, tmp5);
	tmp6 = _mm_xor_si128(tmp6, tmp4);
	tmp7 = _mm_srli_epi32(tmp3, 31);
	tmp8 = _mm_srli_epi32(tmp6, 31);
	tmp3 = _mm_slli_epi32(tmp3, 1);
	tmp6 = _mm_slli_epi32(tmp6, 1);
	tmp9 = _mm_srli_si128(tmp7, 12);
	tmp8 = _mm_slli_si128(tmp8, 4);
	tmp7 = _mm_slli_si128(tmp7, 4);
	tmp3 = _mm_or_si128(tmp3, tmp7);
	tmp6 = _mm_or_si128(tmp6, tmp8);
	tmp6 = _mm_or_si128(tmp6, tmp9);
	tmp7 = _mm_slli_epi32(tmp3, 31);
	tmp8 = _mm_slli_epi32(tmp3, 30);
	tmp9 = _mm_slli_epi32(tmp3, 25);
	tmp7 = _mm_xor_si128(tmp7, tmp8);
	tmp7 = _mm_xor_si128(tmp7, tmp9);
	tmp8 = _mm_srli_si128(tmp7, 4);
	tmp7 = _mm_slli_si128(tmp7, 12);
	tmp3 = _mm_xor_si128(tmp3, tmp7);
	tmp2 = _mm_srli_epi32(tmp3, 1);
	tmp4 = _mm_srli_epi32(tmp3, 2);
	tmp5 = _mm_srli_epi32(tmp3, 7);
	tmp2 = _mm_xor_si128(tmp2, tmp4);
	tmp2 = _mm_xor_si128(tmp2, tmp5);
	tmp2 = _mm_xor_si128(tmp2, tmp8);
	tmp3 = _mm_xor_si128(tmp3, tmp2);

	*res = _mm_xor_si128(tmp6, tmp3);
}

static void gfmul4(__m128i H1, __m128i H2, __m128i H3, __m128i H4,
	__m128i X1, __m128i X2, __m128i X3, __m128i X4, __m128i *res)
{
	// Algorithm by Krzysztof Jankowski, Pierre Laurent - Intel

	__m128i H1_X1_lo, H1_X1_hi,
	H2_X2_lo, H2_X2_hi,
	H3_X3_lo, H3_X3_hi,
	H4_X4_lo, H4_X4_hi,
	lo, hi;
	__m128i tmp0, tmp1, tmp2, tmp3;
	__m128i tmp4, tmp5, tmp6, tmp7;
	__m128i tmp8, tmp9;

	H1_X1_lo = _mm_clmulepi64_si128(H1, X1, 0x00);
	H2_X2_lo = _mm_clmulepi64_si128(H2, X2, 0x00);
	H3_X3_lo = _mm_clmulepi64_si128(H3, X3, 0x00);
	H4_X4_lo = _mm_clmulepi64_si128(H4, X4, 0x00);

	lo = _mm_xor_si128(H1_X1_lo, H2_X2_lo);
	lo = _mm_xor_si128(lo, H3_X3_lo);
	lo = _mm_xor_si128(lo, H4_X4_lo);

	H1_X1_hi = _mm_clmulepi64_si128(H1, X1, 0x11);
	H2_X2_hi = _mm_clmulepi64_si128(H2, X2, 0x11);
	H3_X3_hi = _mm_clmulepi64_si128(H3, X3, 0x11);
	H4_X4_hi = _mm_clmulepi64_si128(H4, X4, 0x11);

	hi = _mm_xor_si128(H1_X1_hi, H2_X2_hi);
	hi = _mm_xor_si128(hi, H3_X3_hi);
	hi = _mm_xor_si128(hi, H4_X4_hi);

	tmp0 = _mm_shuffle_epi32(H1, 78);
	tmp4 = _mm_shuffle_epi32(X1, 78);
	tmp0 = _mm_xor_si128(tmp0, H1);
	tmp4 = _mm_xor_si128(tmp4, X1);
	tmp1 = _mm_shuffle_epi32(H2, 78);
	tmp5 = _mm_shuffle_epi32(X2, 78);
	tmp1 = _mm_xor_si128(tmp1, H2);
	tmp5 = _mm_xor_si128(tmp5, X2);
	tmp2 = _mm_shuffle_epi32(H3, 78);
	tmp6 = _mm_shuffle_epi32(X3, 78);
	tmp2 = _mm_xor_si128(tmp2, H3);
	tmp6 = _mm_xor_si128(tmp6, X3);
	tmp3 = _mm_shuffle_epi32(H4, 78);
	tmp7 = _mm_shuffle_epi32(X4, 78);
	tmp3 = _mm_xor_si128(tmp3, H4);
	tmp7 = _mm_xor_si128(tmp7, X4);

	tmp0 = _mm_clmulepi64_si128(tmp0, tmp4, 0x00);
	tmp1 = _mm_clmulepi64_si128(tmp1, tmp5, 0x00);
	tmp2 = _mm_clmulepi64_si128(tmp2, tmp6, 0x00);
	tmp3 = _mm_clmulepi64_si128(tmp3, tmp7, 0x00);

	tmp0 = _mm_xor_si128(tmp0, lo);
	tmp0 = _mm_xor_si128(tmp0, hi);
	tmp0 = _mm_xor_si128(tmp1, tmp0);
	tmp0 = _mm_xor_si128(tmp2, tmp0);
	tmp0 = _mm_xor_si128(tmp3, tmp0);

	tmp4 = _mm_slli_si128(tmp0, 8);
	tmp0 = _mm_srli_si128(tmp0, 8);

	lo = _mm_xor_si128(tmp4, lo);
	hi = _mm_xor_si128(tmp0, hi);

	tmp3 = lo;
	tmp6 = hi;

	tmp7 = _mm_srli_epi32(tmp3, 31);
	tmp8 = _mm_srli_epi32(tmp6, 31);
	tmp3 = _mm_slli_epi32(tmp3, 1);
	tmp6 = _mm_slli_epi32(tmp6, 1);

	tmp9 = _mm_srli_si128(tmp7, 12);
	tmp8 = _mm_slli_si128(tmp8, 4);
	tmp7 = _mm_slli_si128(tmp7, 4);
	tmp3 = _mm_or_si128(tmp3, tmp7);
	tmp6 = _mm_or_si128(tmp6, tmp8);
	tmp6 = _mm_or_si128(tmp6, tmp9);

	tmp7 = _mm_slli_epi32(tmp3, 31);
	tmp8 = _mm_slli_epi32(tmp3, 30);
	tmp9 = _mm_slli_epi32(tmp3, 25);

	tmp7 = _mm_xor_si128(tmp7, tmp8);
	tmp7 = _mm_xor_si128(tmp7, tmp9);
	tmp8 = _mm_srli_si128(tmp7, 4);
	tmp7 = _mm_slli_si128(tmp7, 12);
	tmp3 = _mm_xor_si128(tmp3, tmp7);

	tmp2 = _mm_srli_epi32(tmp3, 1);
	tmp4 = _mm_srli_epi32(tmp3, 2);
	tmp5 = _mm_srli_epi32(tmp3, 7);
	tmp2 = _mm_xor_si128(tmp2, tmp4);
	tmp2 = _mm_xor_si128(tmp2, tmp5);
	tmp2 = _mm_xor_si128(tmp2, tmp8);
	tmp3 = _mm_xor_si128(tmp3, tmp2);

	*res = _mm_xor_si128(tmp6, tmp3);
}


// Key expansion

static const uint8_t SBOX[256] = {
	0x63, 0x7c, 0x77, 0x7b, 0xf2, 0x6b, 0x6f, 0xc5, 0x30, 0x01, 0x67, 0x2b, 0xfe, 0xd7, 0xab, 0x76,
	0xca, 0x82, 0xc9, 0x7d, 0xfa, 0x59, 0x47, 0xf0, 0xad, 0xd4, 0xa2, 0xaf, 0x9c, 0xa4, 0x72, 0xc0,
	0xb7, 0xfd, 0x93, 0x26, 0x36, 0x3f, 0xf7, 0xcc, 0x34, 0xa5, 0xe5, 0xf1, 0x71, 0xd8, 0x31, 0x15,
	0x04, 0xc7, 0x23, 0xc3, 0x18, 0x96, 0x05, 0x9a, 0x07, 0x12, 0x80, 0xe2, 0xeb, 0x27, 0xb2, 0x75,
	0x09, 0x83, 0x2c, 0x1a, 0x1b, 0x6e, 0x5a, 0xa0, 0x52, 0x3b, 0xd6, 0xb3, 0x29, 0xe3, 0x2f, 0x84,
	0x53, 0xd1, 0x00, 0xed, 0x20, 0xfc, 0xb1, 0x5b, 0x6a, 0xcb, 0xbe, 0x39, 0x4a, 0x4c, 0x58, 0xcf,
	0xd0, 0xef, 0xaa, 0xfb, 0x43, 0x4d, 0x33, 0x85, 0x45, 0xf9, 0x02, 0x7f, 0x50, 0x3c, 0x9f, 0xa8,
	0x51, 0xa3, 0x40, 0x8f, 0x92, 0x9d, 0x38, 0xf5, 0xbc, 0xb6, 0xda, 0x21, 0x10, 0xff, 0xf3, 0xd2,
	0xcd, 0x0c, 0x13, 0xec, 0x5f, 0x97, 0x44, 0x17, 0xc4, 0xa7, 0x7e, 0x3d, 0x64, 0x5d, 0x19, 0x73,
	0x60, 0x81, 0x4f, 0xdc, 0x22, 0x2a, 0x90, 0x88, 0x46, 0xee, 0xb8, 0x14, 0xde, 0x5e, 0x0b, 0xdb,
	0xe0, 0x32, 0x3a, 0x0a, 0x49, 0x06, 0x24, 0x5c, 0xc2, 0xd3, 0xac, 0x62, 0x91, 0x95, 0xe4, 0x79,
	0xe7, 0xc8, 0x37, 0x6d, 0x8d, 0xd5, 0x4e, 0xa9, 0x6c, 0x56, 0xf4, 0xea, 0x65, 0x7a, 0xae, 0x08,
	0xba, 0x78, 0x25, 0x2e, 0x1c, 0xa6, 0xb4, 0xc6, 0xe8, 0xdd, 0x74, 0x1f, 0x4b, 0xbd, 0x8b, 0x8a,
	0x70, 0x3e, 0xb5, 0x66, 0x48, 0x03, 0xf6, 0x0e, 0x61, 0x35, 0x57, 0xb9, 0x86, 0xc1, 0x1d, 0x9e,
	0xe1, 0xf8, 0x98, 0x11, 0x69, 0xd9, 0x8e, 0x94, 0x9b, 0x1e, 0x87, 0xe9, 0xce, 0x55, 0x28, 0xdf,
	0x8c, 0xa1, 0x89, 0x0d, 0xbf, 0xe6, 0x42, 0x68, 0x41, 0x99, 0x2d, 0x0f, 0xb0, 0x54, 0xbb, 0x16,
};

static const uint8_t RCON[11] = {
	0x8d, 0x01, 0x02, 0x04, 0x08, 0x10, 0x20, 0x40, 0x80, 0x1b, 0x36
};

static void aes_key_expansion(P128 *exp, const P128 *key)
{
	exp[0].u64[0] = key->u64[0];
	exp[0].u64[1] = key->u64[1];

	for (uint8_t i = 4; i < 44; i++) {
		P128 tmp;
		tmp.u32[0] = exp->u32[i - 1];

		if (i % 4 == 0) {
			tmp.u32[0] = rotr32(tmp.u32[0], 8);

			tmp.u8[0] = SBOX[tmp.u8[0]];
			tmp.u8[1] = SBOX[tmp.u8[1]];
			tmp.u8[2] = SBOX[tmp.u8[2]];
			tmp.u8[3] = SBOX[tmp.u8[3]];

			tmp.u8[0] ^= RCON[i / 4];
		}

		exp->u32[i] = exp->u32[i - 4] ^ tmp.u32[0];
	}
}

static void ghash16(const __m128i *H, const __m128i *in, __m128i *ghash)
{
	*ghash = _mm_xor_si128(*ghash, _mm_shuffle_epi8(*in, BSWAP_MASK));
	gfmul(ghash, H, ghash);
}

static void cbincr(__m128i *p)
{
	__m128i a = _mm_shuffle_epi8(*p, BSWAP_MASK);
	a = _mm_add_epi32(a, _mm_set_epi32(0, 0, 0, 1));
	*p = _mm_shuffle_epi8(a, BSWAP_MASK);
}

static void aes_gctr(const __m128i *k, const __m128i *H, const __m128i *icb,
	const P128 *x, size_t xlen, P128 *y, __m128i *ghash, const P128 *crypt)
{
	__m128i cb = *icb;
	cbincr(&cb);

	size_t n4 = xlen / 64;
	size_t n = xlen / 16;
	size_t rem = xlen  % 16;

	// 4 blocks at a time
	for (size_t i = 0; i < n4; i++) {
		// XXX The code within this loop must be aggressively optimized

		__m128i yi[4];
		aes128_enc(k, &cb, &yi[0]);
		cbincr(&cb);

		aes128_enc(k, &cb, &yi[1]);
		cbincr(&cb);

		aes128_enc(k, &cb, &yi[2]);
		cbincr(&cb);

		aes128_enc(k, &cb, &yi[3]);
		cbincr(&cb);

		__m128i xi[4];
		xi[0] = _mm_loadu_si128((const __m128i *) &x[i * 4]);
		xi[1] = _mm_loadu_si128((const __m128i *) &x[i * 4 + 1]);
		xi[2] = _mm_loadu_si128((const __m128i *) &x[i * 4 + 2]);
		xi[3] = _mm_loadu_si128((const __m128i *) &x[i * 4 + 3]);

		yi[0] = _mm_xor_si128(yi[0], xi[0]);
		yi[1] = _mm_xor_si128(yi[1], xi[1]);
		yi[2] = _mm_xor_si128(yi[2], xi[2]);
		yi[3] = _mm_xor_si128(yi[3], xi[3]);

		_mm_storeu_si128((__m128i *) &y[i * 4], yi[0]);
		_mm_storeu_si128((__m128i *) &y[i * 4 + 1], yi[1]);
		_mm_storeu_si128((__m128i *) &y[i * 4 + 2], yi[2]);
		_mm_storeu_si128((__m128i *) &y[i * 4 + 3], yi[3]);

		__m128i in[4];
		in[0] = _mm_shuffle_epi8(crypt == x ? xi[0] : yi[0], BSWAP_MASK);
		in[1] = _mm_shuffle_epi8(crypt == x ? xi[1] : yi[1], BSWAP_MASK);
		in[2] = _mm_shuffle_epi8(crypt == x ? xi[2] : yi[2], BSWAP_MASK);
		in[3] = _mm_shuffle_epi8(crypt == x ? xi[3] : yi[3], BSWAP_MASK);

		in[0] = _mm_xor_si128(*ghash, in[0]);
		gfmul4(H[0], H[1], H[2], H[3], in[3], in[2], in[1], in[0], ghash);
	}

	// 1 block at a time
	for (size_t i = n4 * 4; i < n; i++) {
		__m128i yi;
		aes128_enc(k, &cb, &yi);
		cbincr(&cb);

		__m128i xi = _mm_loadu_si128((const __m128i *) &x[i]);
		yi = _mm_xor_si128(yi, xi);
		_mm_storeu_si128((__m128i *) &y[i], yi);
		ghash16(&H[0], crypt == x ? &xi : &yi, ghash);
	}

	// Remaining data in last block
	if (rem > 0) {
		__m128i tmp;
		__m128i tmp2 = _mm_setzero_si128();
		aes128_enc(k, &cb, &tmp);

		for (size_t i = 0; i < rem; i++) {
			y[n].u8[i] = x[n].u8[i] ^ ((uint8_t *) &tmp)[i];
			((uint8_t *) &tmp2)[i] = crypt[n].u8[i];
		}

		ghash16(&H[0], &tmp2, ghash);
	}
}


// Public

struct MTY_AESGCM {
	__m128i H[4];
	__m128i k[11];
};

bool MTY_AESGCMCreate(const void *key, size_t keySize, MTY_AESGCM **aesgcm)
{
	MTY_AESGCM *ctx = *aesgcm = MTY_AllocAligned(sizeof(MTY_AESGCM), 16);

	P128 k[11];
	aes_key_expansion(k, key);

	for (uint8_t x = 0; x < 11; x++)
		ctx->k[x] = _mm_loadu_si128((__m128i *) &k[x]);

	__m128i H = _mm_setzero_si128();
	aes128_enc(ctx->k, &H, &H);

	ctx->H[0] = _mm_shuffle_epi8(H, BSWAP_MASK);

	gfmul(&ctx->H[0], &ctx->H[0], &ctx->H[1]);
	gfmul(&ctx->H[0], &ctx->H[1], &ctx->H[2]);
	gfmul(&ctx->H[0], &ctx->H[2], &ctx->H[3]);

	return true;
}

static void aes_gcm(MTY_AESGCM *ctx, const P128 *nonce, const P128 *crypt, const P128 *in,
	P128 *out, size_t size, P128 *tag)
{
	// Set up the IV with 12 bytes from the nonce and the 4 byte counter
	__m128i iv = _mm_set_epi32(0x01000000, nonce->u32[2], nonce->u32[1], nonce->u32[0]);

	// Encrypt or decrypt the data while generating the ghash
	__m128i ghash = _mm_setzero_si128();
	aes_gctr(ctx->k, ctx->H, &iv, in, size, out, &ghash, crypt);

	// ghash needs to be multiplied by the length of the data then reversed
	__m128i len = _mm_shuffle_epi8(_mm_set_epi64x(0, size * 8), BSWAP_MASK);
	ghash16(&ctx->H[0], &len, &ghash);
	ghash = _mm_shuffle_epi8(ghash, BSWAP_MASK);

	// Encrypt the tag
	__m128i tmp;
	aes128_enc(ctx->k, &iv, &tmp);
	ghash = _mm_xor_si128(tmp, ghash);
	_mm_storeu_si128((__m128i *) tag, ghash);
}

bool MTY_AESGCMEncrypt(MTY_AESGCM *ctx, const void *plainText, size_t textSize, const void *nonce,
	size_t nonceSize, void *hash, size_t hashSize, void *cipherText)
{
	aes_gcm(ctx, nonce, cipherText, plainText, cipherText, textSize, hash);

	return true;
}

bool MTY_AESGCMDecrypt(MTY_AESGCM *ctx, const void *cipherText, size_t textSize, const void *nonce,
	size_t nonceSize, const void *hash, size_t hashSize, void *plainText)
{
	P128 tag;
	aes_gcm(ctx, nonce, cipherText, cipherText, plainText, textSize, &tag);

	const P128 *itag = hash;

	if (tag.u64[0] != itag->u64[0] || tag.u64[1] != itag->u64[1])
		return false;

	return true;
}

void MTY_AESGCMDestroy(MTY_AESGCM **aesgcm)
{
	if (!aesgcm || !*aesgcm)
		return;

	MTY_AESGCM *ctx = *aesgcm;

	memset(ctx, 0, sizeof(MTY_AESGCM));

	MTY_FreeAligned(ctx);
	*aesgcm = NULL;
}
