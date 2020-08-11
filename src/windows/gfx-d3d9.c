// Copyright (c) 2020 Christopher D. Dickson <cdd@matoya.group>
//
// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at https://mozilla.org/MPL/2.0/.

#include "gfx-d3d9.h"

#include "mty-viewport.h"

static
#include "shaders/d3d9/ps.h"

static
#include "shaders/d3d9/vs.h"

#define NUM_STAGING 3

struct gfx_d3d9_res {
	D3DFORMAT format;
	IDirect3DTexture9 *texture;
	IDirect3DBaseTexture9 *base;
	uint32_t width;
	uint32_t height;
};

struct gfx_d3d9 {
	MTY_ColorFormat format;
	struct gfx_d3d9_res staging[NUM_STAGING];
	IDirect3DPixelShader9 *ps;
	IDirect3DVertexShader9 *vs;
	IDirect3DVertexBuffer9 *vb;
	IDirect3DVertexDeclaration9 *vd;
	IDirect3DIndexBuffer9 *ib;
};

bool gfx_d3d9_create(IDirect3DDevice9 *device, struct gfx_d3d9 **gfx)
{
	struct gfx_d3d9 *ctx = *gfx = MTY_Alloc(1, sizeof(struct gfx_d3d9));

	HRESULT e = IDirect3DDevice9_CreateVertexShader(device, (DWORD *) vs, &ctx->vs);
	if (e != D3D_OK) {
		MTY_Log("'IDirect3DDevice9_CreateVertexShader' failed with HRESULT %x", e);
		goto except;
	}

	e = IDirect3DDevice9_CreatePixelShader(device, (DWORD *) ps, &ctx->ps);
	if (e != D3D_OK) {
		MTY_Log("'IDirect3DDevice9_CreatePixelShader' failed with HRESULT %x", e);
		goto except;
	}

	float vertex_data[] = {
		-1.0f, -1.0f,  // position0 (bottom-left)
		 0.0f,  1.0f,  // texcoord0
		-1.0f,  1.0f,  // position1 (top-left)
		 0.0f,  0.0f,  // texcoord1
		 1.0f,  1.0f,  // position2 (top-right)
		 1.0f,  0.0f,  // texcoord2
		 1.0f, -1.0f,  // position3 (bottom-right)
		 1.0f,  1.0f,  // texcoord3
	};

	e = IDirect3DDevice9_CreateVertexBuffer(device, sizeof(vertex_data), 0,
		D3DUSAGE_DYNAMIC | D3DUSAGE_WRITEONLY, D3DPOOL_DEFAULT, &ctx->vb, NULL);
	if (e != D3D_OK) {
		MTY_Log("'IDirect3DDevice9_CreateVertexBuffer' failed with HRESULT %x", e);
		goto except;
	}

	void *ptr = NULL;
	e = IDirect3DVertexBuffer9_Lock(ctx->vb, 0, 0, &ptr, D3DLOCK_DISCARD);
	if (e != D3D_OK) {
		MTY_Log("'IDirect3DVertexBuffer9_Lock' failed with HRESULT %x", e);
		goto except;
	}

	memcpy(ptr, vertex_data, sizeof(vertex_data));

	e = IDirect3DVertexBuffer9_Unlock(ctx->vb);
	if (e != D3D_OK) {
		MTY_Log("'IDirect3DVertexBuffer9_Unlock' failed with HRESULT %x", e);
		goto except;
	}

	//vertex declaration (input layout)
	D3DVERTEXELEMENT9 dec[] = {
		{0, 0, D3DDECLTYPE_FLOAT2, D3DDECLMETHOD_DEFAULT, D3DDECLUSAGE_POSITION, 0},
		{0, 2 * sizeof(float), D3DDECLTYPE_FLOAT2, D3DDECLMETHOD_DEFAULT, D3DDECLUSAGE_TEXCOORD, 0},
		D3DDECL_END()
	};

	e = IDirect3DDevice9_CreateVertexDeclaration(device, dec, &ctx->vd);
	if (e != D3D_OK) {
		MTY_Log("'IDirect3DDevice9_CreateVertexDeclaration' failed with HRESULT %x", e);
		goto except;
	}

	//index buffer
	DWORD index_data[] = {
		0, 1, 2,
		2, 3, 0
	};

	e = IDirect3DDevice9_CreateIndexBuffer(device, sizeof(index_data),
		D3DUSAGE_DYNAMIC | D3DUSAGE_WRITEONLY, D3DFMT_INDEX32, D3DPOOL_DEFAULT, &ctx->ib, NULL);
	if (e != D3D_OK) {
		MTY_Log("'IDirect3DDevice9_CreateIndexBuffer' failed with HRESULT %x", e);
		goto except;
	}

	e = IDirect3DIndexBuffer9_Lock(ctx->ib, 0, 0, &ptr, D3DLOCK_DISCARD);
	if (e != D3D_OK) {
		MTY_Log("'IDirect3DIndexBuffer9_Lock' failed with HRESULT %x", e);
		goto except;
	}

	memcpy(ptr, index_data, sizeof(index_data));

	e = IDirect3DIndexBuffer9_Unlock(ctx->ib);
	if (e != D3D_OK) {
		MTY_Log("'IDirect3DIndexBuffer9_Unlock' failed with HRESULT %x", e);
		goto except;
	}

	except:

	if (e != D3D_OK)
		gfx_d3d9_destroy(gfx);

	return e == D3D_OK;
}

static void gfx_d3d9_destroy_resource(struct gfx_d3d9_res *res)
{
	if (res->base) {
		IDirect3DBaseTexture9_Release(res->base);
		res->base = NULL;
	}

	if (res->texture) {
		IDirect3DTexture9_Release(res->texture);
		res->texture = NULL;
	}

	memset(res, 0, sizeof(struct gfx_d3d9_res));
}

static HRESULT gfx_d3d9_refresh_resource(struct gfx_d3d9_res *res, IDirect3DDevice9 *device,
	D3DFORMAT format, uint32_t width, uint32_t height)
{
	HRESULT e = D3D_OK;

	if (!res->texture || !res->base || width != res->width ||
		height != res->height || format != res->format)
	{
		gfx_d3d9_destroy_resource(res);

		e = IDirect3DDevice9_CreateTexture(device, width, height, 1, D3DUSAGE_DYNAMIC,
			format, D3DPOOL_DEFAULT, &res->texture, NULL);
		if (e != D3D_OK) {
			MTY_Log("'IDirect3DDevice9_CreateTexture' failed with HRESULT %x", e);
			goto except;
		}

		e = IDirect3DTexture9_QueryInterface(res->texture, &IID_IDirect3DBaseTexture9,
			(void **) &res->base);
		if (e != D3D_OK) {
			MTY_Log("'IDirect3DTexture9_QueryInterface' failed with HRESULT %x", e);
			goto except;
		}

		res->width = width;
		res->height = height;
		res->format = format;
	}

	except:

	if (e != D3D_OK)
		gfx_d3d9_destroy_resource(res);

	return e;
}

static HRESULT gfx_d3d9_crop_copy(IDirect3DDevice9 *device, IDirect3DTexture9 *texture,
	const uint8_t *image, uint32_t w, uint32_t h, int32_t fullWidth, uint8_t bpp)
{
	D3DLOCKED_RECT rect;
	HRESULT e = IDirect3DTexture9_LockRect(texture, 0, &rect, NULL, D3DLOCK_DISCARD);
	if (e != D3D_OK) {
		MTY_Log("'IDirect3DTexture9_LockRect' failed with HRESULT %x", e);
		return e;
	}

	for (uint32_t y = 0; y < h; y++)
		memcpy((uint8_t *) rect.pBits + (y * rect.Pitch), image + (y * fullWidth * bpp), w * bpp);

	e = IDirect3DTexture9_UnlockRect(texture, 0);
	if (e != D3D_OK) {
		MTY_Log("'IDirect3DTexture9_UnlockRect' failed with HRESULT %x", e);
		return e;
	}

	return D3D_OK;
}

static HRESULT gfx_d3d9_reload_textures(struct gfx_d3d9 *ctx, IDirect3DDevice9 *device,
	const void *image, const MTY_RenderDesc *desc)
{
	switch (desc->format) {
		case MTY_COLOR_FORMAT_RGBA: {
			// RGBA
			HRESULT e = gfx_d3d9_refresh_resource(&ctx->staging[0], device, D3DFMT_A8R8G8B8, desc->cropWidth, desc->cropHeight);
			if (e != D3D_OK) return e;

			e = gfx_d3d9_crop_copy(device, ctx->staging[0].texture, image, desc->cropWidth, desc->cropHeight, desc->imageWidth, 4);
			if (e != D3D_OK) return e;
			break;
		}
		case MTY_COLOR_FORMAT_NV12: {
			// Y
			HRESULT e = gfx_d3d9_refresh_resource(&ctx->staging[0], device, D3DFMT_L8, desc->cropWidth, desc->cropHeight);
			if (e != D3D_OK) return e;

			e = gfx_d3d9_crop_copy(device, ctx->staging[0].texture, image, desc->cropWidth, desc->cropHeight, desc->imageWidth, 1);
			if (e != D3D_OK) return e;

			// UV
			e = gfx_d3d9_refresh_resource(&ctx->staging[1], device, D3DFMT_A8L8, desc->cropWidth / 2, desc->cropHeight / 2);
			if (e != D3D_OK) return e;

			e = gfx_d3d9_crop_copy(device, ctx->staging[1].texture, (uint8_t *) image + desc->imageWidth * desc->imageHeight, desc->cropWidth / 2, desc->cropHeight / 2, desc->imageWidth / 2, 2);
			if (e != D3D_OK) return e;
			break;
		}
		case MTY_COLOR_FORMAT_I420:
		case MTY_COLOR_FORMAT_I444: {
			uint32_t div = desc->format == MTY_COLOR_FORMAT_I420 ? 2 : 1;

			// Y
			HRESULT e = gfx_d3d9_refresh_resource(&ctx->staging[0], device, D3DFMT_L8, desc->cropWidth, desc->cropHeight);
			if (e != D3D_OK) return e;

			e = gfx_d3d9_crop_copy(device, ctx->staging[0].texture, image, desc->cropWidth, desc->cropHeight, desc->imageWidth, 1);
			if (e != D3D_OK) return e;

			// U
			uint8_t *p = (uint8_t *) image + desc->imageWidth * desc->imageHeight;
			e = gfx_d3d9_refresh_resource(&ctx->staging[1], device, D3DFMT_L8, desc->cropWidth / div, desc->cropHeight / div);
			if (e != D3D_OK) return e;

			e = gfx_d3d9_crop_copy(device, ctx->staging[1].texture, p, desc->cropWidth / div, desc->cropHeight / div, desc->imageWidth / div, 1);
			if (e != D3D_OK) return e;

			// V
			p += (desc->imageWidth / div) * (desc->imageHeight / div);
			e = gfx_d3d9_refresh_resource(&ctx->staging[2], device, D3DFMT_L8, desc->cropWidth / div, desc->cropHeight / div);
			if (e != D3D_OK) return e;

			e = gfx_d3d9_crop_copy(device, ctx->staging[2].texture, p, desc->cropWidth / div, desc->cropHeight / div, desc->imageWidth / div, 1);
			if (e != D3D_OK) return e;
			break;
		}
	}

	return D3D_OK;
}

bool gfx_d3d9_render(struct gfx_d3d9 *ctx, IDirect3DDevice9 *device,
	const void *image, const MTY_RenderDesc *desc, IDirect3DTexture9 *dest)
{
	// Don't do anything until we have real data
	if (desc->format != MTY_COLOR_FORMAT_UNKNOWN)
		ctx->format = desc->format;

	if (ctx->format == MTY_COLOR_FORMAT_UNKNOWN)
		return true;

	// Refresh textures and load texture data
	// If format == MTY_COLOR_FORMAT_UNKNOWN, texture refreshing/loading is skipped and the previous frame is rendered
	HRESULT e = gfx_d3d9_reload_textures(ctx, device, image, desc);
	if (e != D3D_OK) return false;

	// Viewport
	float vpx, vpy, vpw, vph;
	mty_viewport(desc->cropWidth, desc->cropHeight, desc->viewWidth, desc->viewHeight,
		desc->aspectRatio, desc->scale, &vpx, &vpy, &vpw, &vph);

	D3DVIEWPORT9 vp = {0};
	vp.X = lrint(vpx);
	vp.Y = lrint(vpy);
	vp.Width = lrint(vpw);
	vp.Height = lrint(vph);

	e = IDirect3DDevice9_SetViewport(device, &vp);
	if (e != D3D_OK) {
		MTY_Log("'IDirect3DDevice9_SetViewport' failed with HRESULT %x", e);
		return false;
	}

	// Begin render pass (set destination texture if available)
	IDirect3DSurface9 *surface = NULL;

	if (dest) {
		e = IDirect3DTexture9_GetSurfaceLevel(dest, 0, &surface);
		if (e != D3D_OK) {
			MTY_Log("'IDirect3DTexture9_GetSurfaceLevel' failed with HRESULT %x", e);
			goto except;
		}

		e = IDirect3DDevice9_SetRenderTarget(device, 0, surface);
		if (e != D3D_OK) {
			MTY_Log("'IDirect3DDevice9_SetRenderTarget' failed with HRESULT %x", e);
			goto except;
		}

		e = IDirect3DDevice9_Clear(device, 0, NULL, D3DCLEAR_TARGET, D3DCOLOR_XRGB(0, 0, 0), 1.0f, 0);
		if (e != D3D_OK) {
			MTY_Log("'IDirect3DDevice9_Clear' failed with HRESULT %x", e);
			goto except;
		}
	}

	// Vertex shader
	e = IDirect3DDevice9_SetVertexShader(device, ctx->vs);
	if (e != D3D_OK) {
		MTY_Log("'IDirect3DDevice9_SetVertexShader' failed with HRESULT %x", e);
		goto except;
	}

	e = IDirect3DDevice9_SetStreamSource(device, 0, ctx->vb, 0, 4 * sizeof(float));
	if (e != D3D_OK) {
		MTY_Log("'IDirect3DDevice9_SetStreamSource' failed with HRESULT %x", e);
		goto except;
	}

	e = IDirect3DDevice9_SetVertexDeclaration(device, ctx->vd);
	if (e != D3D_OK) {
		MTY_Log("'IDirect3DDevice9_SetVertexDeclaration' failed with HRESULT %x", e);
		goto except;
	}

	e = IDirect3DDevice9_SetIndices(device, ctx->ib);
	if (e != D3D_OK) {
		MTY_Log("'IDirect3DDevice9_SetIndicies' failed with HRESULT %x", e);
		goto except;
	}

	// D3D9 half texel fix
	float texel_offset[4] = {0};
	texel_offset[0] = -1.0f / (float) vp.Width;
	texel_offset[1] = 1.0f / (float) vp.Height;

	e = IDirect3DDevice9_SetVertexShaderConstantF(device, 0, texel_offset, 1);
	if (e != D3D_OK) {
		MTY_Log("'IDirect3DDevice9_SetVertexShaderConstantF' failed with HRESULT %x", e);
		goto except;
	}

	// Pixel shader
	e = IDirect3DDevice9_SetPixelShader(device, ctx->ps);
	if (e != D3D_OK) {
		MTY_Log("'IDirect3DDevice9_SetPixelShader' failed with HRESULT %x", e);
		goto except;
	}

	for (uint8_t x = 0; x < NUM_STAGING; x++) {
		if (ctx->staging[x].base) {
			IDirect3DDevice9_SetTexture(device, x, ctx->staging[x].base);

			e = IDirect3DDevice9_SetSamplerState(device, x, D3DSAMP_MAGFILTER, D3DTEXF_LINEAR);
			if (e != D3D_OK) {
				MTY_Log("'IDirect3DDevice9_SetSamplerState' failed with HRESULT %x", e);
				goto except;
			}

			e = IDirect3DDevice9_SetSamplerState(device, x, D3DSAMP_MINFILTER, D3DTEXF_LINEAR);
			if (e != D3D_OK) {
				MTY_Log("'IDirect3DDevice9_SetSamplerState' failed with HRESULT %x", e);
				goto except;
			}
		}
	}

	float format[4] = {0};
	format[0] = ctx->format;
	e = IDirect3DDevice9_SetPixelShaderConstantF(device, 0, format, 1);
	if (e != D3D_OK) {
		MTY_Log("'IDirect3DDevice9_SetPixelShaderConstantF' failed with HRESULT %x", e);
		goto except;
	}

	// Draw
	e = IDirect3DDevice9_DrawIndexedPrimitive(device, D3DPT_TRIANGLELIST, 0, 0, 4, 0, 2);
	if (e != D3D_OK) {
		MTY_Log("'IDirect3DDevice9_DrawIndexedPrimitive' failed with HRESULT %x", e);
		goto except;
	}

	except:

	if (surface)
		IDirect3DSurface9_Release(surface);

	return e == D3D_OK;
}

void gfx_d3d9_destroy(struct gfx_d3d9 **gfx)
{
	if (!gfx || !*gfx)
		return;

	struct gfx_d3d9 *ctx = *gfx;

	for (uint8_t x = 0; x < NUM_STAGING; x++)
		gfx_d3d9_destroy_resource(&ctx->staging[x]);

	if (ctx->ib)
		IDirect3DIndexBuffer9_Release(ctx->ib);

	if (ctx->vd)
		IDirect3DVertexDeclaration9_Release(ctx->vd);

	if (ctx->vb)
		IDirect3DVertexBuffer9_Release(ctx->vb);

	if (ctx->ps)
		IDirect3DPixelShader9_Release(ctx->ps);

	if (ctx->vs)
		IDirect3DVertexShader9_Release(ctx->vs);

	MTY_Free(ctx);
	*gfx = NULL;
}
