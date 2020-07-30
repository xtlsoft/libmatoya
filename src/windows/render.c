// Copyright (c) 2020 Christopher D. Dickson <cdd@matoya.group>
//
// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at https://mozilla.org/MPL/2.0/.

#include "matoya.h"

#include "gfx-d3d11.h"
#include "gfx-gl.h"

struct MTY_Renderer {
	MTY_GFX api;
	MTY_Device *device;

	struct gfx_d3d11 *d3d11;
	struct gfx_gl *gl;
};

bool MTY_RendererCreate(MTY_Renderer **renderer)
{
	*renderer = MTY_Alloc(1, sizeof(MTY_Renderer));

	return true;
}

static void render_destroy_api(MTY_Renderer *ctx)
{
	switch (ctx->api) {
		case MTY_GFX_D3D11:
			gfx_d3d11_destroy(&ctx->d3d11);
			break;
		case MTY_GFX_GL:
			gfx_gl_destroy(&ctx->gl);
			break;
	}

	ctx->api = MTY_GFX_NONE;
	ctx->device = NULL;
}

static bool render_init_api(MTY_Renderer *ctx, MTY_GFX api, MTY_Device *device)
{
	switch (api) {
		case MTY_GFX_D3D11:
			if (!ctx->d3d11)
				return gfx_d3d11_init((ID3D11Device *) device, &ctx->d3d11) == S_OK;

			return true;
		case MTY_GFX_GL:
			return !ctx->gl ? gfx_gl_create(&ctx->gl, "#version 110\n") : true;
		default:
			MTY_Log("MTY_GFX %d is unimplemented", api);
			break;
	}

	return false;
}

bool MTY_RendererDrawQuad(MTY_Renderer *ctx, MTY_GFX api, MTY_Device *device, MTY_Context *context,
	const void *image, const MTY_RenderDesc *desc, MTY_Texture *dest)
{
	if (ctx->api != api || ctx->device != device)
		render_destroy_api(ctx);

	if (!render_init_api(ctx, api, device))
		return false;

	ctx->device = device;
	ctx->api = api;

	switch (ctx->api) {
		case MTY_GFX_D3D11:
			return gfx_d3d11_render(ctx->d3d11, (ID3D11Device *) device, (ID3D11DeviceContext *) context,
				image, desc, (ID3D11Texture2D *) dest);

		case MTY_GFX_GL:
			return gfx_gl_render(ctx->gl, image, desc, (GLuint) (uintptr_t) dest);
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
