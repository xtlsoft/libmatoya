// Copyright (c) 2020 Christopher D. Dickson <cdd@matoya.group>
//
// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at https://mozilla.org/MPL/2.0/.

#include "matoya.h"

#include "gfx-gl.h"
#include "gfx-d3d9.h"
#include "gfx-d3d11.h"
#include "gfx-metal.h"

#if defined(MTY_GL_ES)
	#define SHADER_VERSION "#version 100\n"
#else
	#define SHADER_VERSION "#version 110\n"
#endif

struct MTY_Renderer {
	MTY_GFX api;
	MTY_Device *device;

	struct gfx_gl *gl;
	struct gfx_d3d9 *d3d9;
	struct gfx_d3d11 *d3d11;
	struct gfx_metal *metal;
};

bool MTY_RendererCreate(MTY_Renderer **renderer)
{
	*renderer = MTY_Alloc(1, sizeof(MTY_Renderer));

	return true;
}

static void render_destroy_api(MTY_Renderer *ctx)
{
	switch (ctx->api) {
		case MTY_GFX_GL:
			gfx_gl_destroy(&ctx->gl);
			break;
		case MTY_GFX_D3D9:
			gfx_d3d9_destroy(&ctx->d3d9);
			break;
		case MTY_GFX_D3D11:
			gfx_d3d11_destroy(&ctx->d3d11);
			break;
		case MTY_GFX_METAL:
			gfx_metal_destroy(&ctx->metal);
			break;
	}

	ctx->api = MTY_GFX_NONE;
	ctx->device = NULL;
}

static bool render_create_api(MTY_Renderer *ctx, MTY_GFX api, MTY_Device *device)
{
	switch (api) {
		case MTY_GFX_GL:
			return !ctx->gl ? gfx_gl_create(&ctx->gl, SHADER_VERSION) : true;
		case MTY_GFX_D3D9:
			return !ctx->d3d9 ? gfx_d3d9_create((IDirect3DDevice9 *) device, &ctx->d3d9) : true;
		case MTY_GFX_D3D11:
			return !ctx->d3d11 ? gfx_d3d11_create((ID3D11Device *) device, &ctx->d3d11) : true;
		case MTY_GFX_METAL:
			return !ctx->metal ? gfx_metal_create(device, &ctx->metal) : true;
		default:
			break;
	}

	return false;
}

bool MTY_RendererDrawQuad(MTY_Renderer *ctx, MTY_GFX api, MTY_Device *device, MTY_Context *context,
	const void *image, const MTY_RenderDesc *desc, MTY_Texture *dest)
{
	// Metal only requires the context passed as an id<MTLCommandQueue>, device will be NULL
	if (api == MTY_GFX_METAL)
		device = gfx_metal_device(context);

	// Any change in API or device means we need to rebuild the state
	if (ctx->api != api || ctx->device != device)
		render_destroy_api(ctx);

	// Compile shaders, create buffers and staging areas
	if (!render_create_api(ctx, api, device))
		return false;

	ctx->device = device;
	ctx->api = api;

	switch (ctx->api) {
		case MTY_GFX_D3D9:
			return gfx_d3d9_render(ctx->d3d9, (IDirect3DDevice9 *) device, image, desc, (IDirect3DTexture9 *) dest);
		case MTY_GFX_D3D11:
			return gfx_d3d11_render(ctx->d3d11, (ID3D11Device *) device, (ID3D11DeviceContext *) context,
				image, desc, (ID3D11Texture2D *) dest);
		case MTY_GFX_GL:
			return gfx_gl_render(ctx->gl, image, desc, (GLuint) (uintptr_t) dest);
		case MTY_GFX_METAL:
			return gfx_metal_render(ctx->metal, context, image, desc, dest);
	}

	return false;
}

void MTY_RendererDestroy(MTY_Renderer **renderer)
{
	if (!renderer || !*renderer)
		return;

	MTY_Renderer *ctx = *renderer;

	render_destroy_api(ctx);

	MTY_Free(ctx);
	*renderer = NULL;
}
