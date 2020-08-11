// Copyright (c) 2020 Christopher D. Dickson <cdd@matoya.group>
//
// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at https://mozilla.org/MPL/2.0/.

#include "matoya.h"

#include <assert.h>
#include <limits.h>
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#if defined(__x86_64__) || defined(_M_X64) || defined(__i386) || defined(_M_IX86)
	#include <emmintrin.h>
#endif

#if defined(MTY_NEON)
	#define STBI_NEON
	#include <arm_neon.h>
#endif

#define STBI_MALLOC(size)        MTY_Alloc(size, 1)
#define STBI_REALLOC(ptr, size)  MTY_Realloc(ptr, size, 1)
#define STBI_FREE(ptr)           MTY_Free(ptr)
#define STBI_ASSERT(x)

#define STBIW_MALLOC(size)       MTY_Alloc(size, 1)
#define STBIW_REALLOC(ptr, size) MTY_Realloc(ptr, size, 1)
#define STBIW_FREE(ptr)          MTY_Free(ptr)
#define STBIW_ASSERT(x)

#include "stb/stb_image.h"
#include "stb/stb_image_write.h"

struct image_write {
	void *output;
	size_t size;
};

bool MTY_ImageDecompress(const void *input, size_t size, void **output, uint32_t *width, uint32_t *height)
{
	int32_t channels = 0;
	*output = stbi_load_from_memory(input, (int32_t) size, (int32_t *) width, (int32_t *) height, &channels, 4);

	if (!*output) {
		if (stbi__g_failure_reason)
			MTY_Log("%s", stbi__g_failure_reason);

		MTY_Log("'stbi_load_from_memory' failed");
		return false;
	}

	return true;
}

static void image_center_crop(uint32_t w, uint32_t h, uint32_t target_w, uint32_t target_h,
	uint32_t *crop_w, uint32_t *crop_h)
{
	float m = 1.0f;

	if (w < target_w) {
		m = (float) target_w / (float) w;
		w = target_w;
		h = lrint((float) h * m);
	}

	if (h < target_h) {
		m = (float) target_h / (float) h;
		h = target_h;
		w = lrint((float) w * m);
	}

	*crop_w = (target_w > 0 && w > target_w) ? lrint((float) (w - target_w) / m) : 0;
	*crop_h = (target_h > 0 && h > target_h) ? lrint((float) (h - target_h) / m) : 0;
}

void *MTY_ImageCrop(const void *image, uint32_t cropWidth, uint32_t cropHeight, uint32_t *width, uint32_t *height)
{
	uint32_t crop_width = 0;
	uint32_t crop_height = 0;
	image_center_crop(*width, *height, cropWidth, cropHeight, &crop_width, &crop_height);

	if (crop_width > 0 || crop_height > 0) {
		uint32_t x = crop_width / 2;
		uint32_t y = crop_height / 2;
		uint32_t crop_w = *width - crop_width;
		uint32_t crop_h = *height - crop_height;

		uint8_t *cropped = MTY_Alloc(crop_w * crop_h, 4);
		for (uint32_t h = y; h < *height - y && h - y < crop_h; h++)
			memcpy(cropped + ((h - y) * crop_w * 4), (uint8_t *) image + (h * *width * 4) + (x * 4), crop_w * 4);

		*width = crop_w;
		*height = crop_h;

		return cropped;
	}

	return NULL;
}

static void image_compress_write_func(void *context, void *data, int size)
{
	struct image_write *ctx = (struct image_write *) context;
	ctx->size = size;

	ctx->output = MTY_Alloc(ctx->size, 1);
	memcpy(ctx->output, data, ctx->size);
}

bool MTY_ImageCompress(const void *input, uint32_t width, uint32_t height, MTY_Image type, void **output, size_t *outputSize)
{
	struct image_write ctx = {0};
	int32_t e = 0;

	switch (type) {
		case MTY_IMAGE_PNG:
			e = stbi_write_png_to_func(image_compress_write_func, &ctx, width, height, 4, input, width * 4);
			break;
		case MTY_IMAGE_JPG:
			// Quality defaults to 90
			e = stbi_write_jpg_to_func(image_compress_write_func, &ctx, width, height, 4, input, 0);
			break;
		case MTY_IMAGE_BMP:
			e = stbi_write_bmp_to_func(image_compress_write_func, &ctx, width, height, 4, input);
			break;
	}

	if (e != 0) {
		*outputSize = ctx.size;
		*output = ctx.output;
		return true;

	} else {
		MTY_Log("'stbi_write_xxx_to_func' failed");
		MTY_Free(ctx.output);
		return false;
	}
}

bool MTY_Compress(const void *input, size_t inputSize, void **output, size_t *outputSize)
{
	int32_t out = 0;

	// Quality defaults to 5
	*output = stbi_zlib_compress((unsigned char *) input, (int32_t) inputSize, &out, 0);
	*outputSize = out;

	if (!*output) {
		MTY_Log("'stbi_zlib_compress' failed");
		return false;
	}

	return true;
}

bool MTY_Decompress(const void *input, size_t inputSize, void **output, size_t *outputSize)
{
	int32_t out = 0;
	*output = stbi_zlib_decode_malloc(input, (int32_t) inputSize, &out);
	*outputSize = out;

	if (!*output) {
		if (stbi__g_failure_reason)
			MTY_Log("%s", stbi__g_failure_reason);

		MTY_Log("'stbi_zlib_decode_malloc' failed");
		return false;
	}

	return true;
}
