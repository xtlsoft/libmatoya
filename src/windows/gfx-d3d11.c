// Copyright (c) 2020 Christopher D. Dickson <cdd@matoya.group>
//
// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at https://mozilla.org/MPL/2.0/.

#include "gfx-d3d11.h"

#include <math.h>

#include "shaders/ps.h"
#include "shaders/vs.h"

#define NUM_STAGING 3

struct psvars {
	float width;
	float height;
	float constrain_w;
	float constrain_h;
	uint32_t filter;
	uint32_t effect;
	uint32_t format;
	uint32_t __pad[1];
};

struct gfx_d3d11_res {
	DXGI_FORMAT format;
	ID3D11Texture2D *texture;
	ID3D11Resource *resource;
	ID3D11ShaderResourceView *srv;
	uint32_t width;
	uint32_t height;
};

struct gfx_d3d11 {
	MTY_ColorFormat format;
	struct psvars psvars;
	struct gfx_d3d11_res staging[NUM_STAGING];
	ID3D11VertexShader *vs;
	ID3D11PixelShader *ps;
	ID3D11Buffer *vb;
	ID3D11Buffer *ib;
	ID3D11Buffer *psb;
	ID3D11InputLayout *il;
	ID3D11SamplerState *ss_nearest;
	ID3D11SamplerState *ss_linear;
};

HRESULT gfx_d3d11_init(ID3D11Device *device, struct gfx_d3d11 **gfx)
{
	struct gfx_d3d11 *ctx = *gfx = MTY_Alloc(1, sizeof(struct gfx_d3d11));

	HRESULT e = ID3D11Device_CreateVertexShader(device, vs, sizeof(vs), NULL, &ctx->vs);
	e = ID3D11Device_CreatePixelShader(device, ps, sizeof(ps), NULL, &ctx->ps);
	if (e != S_OK) goto except;

	float vertex_data[] = {
		-1.0f, -1.0f,  // position0 (bottom-left)
		 0.0f,  1.0f,  // texcoord0
		-1.0f,  1.0f,  // position1 (top-left)
		 0.0f,  0.0f,  // texcoord1
		 1.0f,  1.0f,  // position2 (top-right)
		 1.0f,  0.0f,  // texcoord2
		 1.0f, -1.0f,  // position3 (bottom-right)
		 1.0f,  1.0f   // texcoord3
	};

	D3D11_BUFFER_DESC bd = {0};
	bd.ByteWidth = sizeof(vertex_data);
	bd.BindFlags = D3D11_BIND_VERTEX_BUFFER;

	D3D11_SUBRESOURCE_DATA srd = {0};
	srd.pSysMem = vertex_data;
	e = ID3D11Device_CreateBuffer(device, &bd, &srd, &ctx->vb);
	if (e != S_OK) goto except;

	DWORD index_data[] = {
		0, 1, 2,
		2, 3, 0
	};

	D3D11_BUFFER_DESC ibd = {0};
	ibd.ByteWidth = sizeof(index_data);
	ibd.BindFlags = D3D11_BIND_INDEX_BUFFER;

	D3D11_SUBRESOURCE_DATA isrd = {0};
	isrd.pSysMem = index_data;
	e = ID3D11Device_CreateBuffer(device, &ibd, &isrd, &ctx->ib);
	if (e != S_OK) goto except;

	D3D11_BUFFER_DESC psbd = {0};
	psbd.ByteWidth = sizeof(struct psvars);
	psbd.Usage = D3D11_USAGE_DYNAMIC;
	psbd.BindFlags = D3D11_BIND_CONSTANT_BUFFER;
	psbd.CPUAccessFlags = D3D11_CPU_ACCESS_WRITE;
	e = ID3D11Device_CreateBuffer(device, &psbd, NULL, &ctx->psb);
	if (e != S_OK) goto except;

	D3D11_INPUT_ELEMENT_DESC ied[] = {
		{"POSITION", 0, DXGI_FORMAT_R32G32_FLOAT, 0, 0, D3D11_INPUT_PER_VERTEX_DATA, 0},
		{"TEXCOORD", 0, DXGI_FORMAT_R32G32_FLOAT, 0, 2 * sizeof(float), D3D11_INPUT_PER_VERTEX_DATA, 0}
	};

	e = ID3D11Device_CreateInputLayout(device, ied, 2, vs, sizeof(vs), &ctx->il);
	if (e != S_OK) goto except;

	D3D11_SAMPLER_DESC sdesc = {0};
	sdesc.Filter = D3D11_FILTER_MIN_MAG_MIP_POINT;
	sdesc.AddressU = D3D11_TEXTURE_ADDRESS_CLAMP;
	sdesc.AddressV = D3D11_TEXTURE_ADDRESS_CLAMP;
	sdesc.AddressW = D3D11_TEXTURE_ADDRESS_CLAMP;
	e = ID3D11Device_CreateSamplerState(device, &sdesc, &ctx->ss_nearest);
	if (e != S_OK) goto except;

	sdesc.Filter = D3D11_FILTER_MIN_MAG_MIP_LINEAR;
	e = ID3D11Device_CreateSamplerState(device, &sdesc, &ctx->ss_linear);
	if (e != S_OK) goto except;

	except:

	if (e != S_OK)
		gfx_d3d11_destroy(gfx);

	return e;
}

static void gfx_d3d11_destroy_resource(struct gfx_d3d11_res *res)
{
	if (res->srv)
		ID3D11ShaderResourceView_Release(res->srv);

	if (res->resource)
		ID3D11Resource_Release(res->resource);

	if (res->texture)
		ID3D11Texture2D_Release(res->texture);

	memset(res, 0, sizeof(struct gfx_d3d11_res));
}

static HRESULT gfx_d3d11_refresh_resource(struct gfx_d3d11_res *res, ID3D11Device *device,
	DXGI_FORMAT format, uint32_t width, uint32_t height)
{
	HRESULT e = S_OK;

	if (!res->texture || !res->srv || !res->resource || width != res->width ||
		height != res->height || format != res->format)
	{
		gfx_d3d11_destroy_resource(res);

		D3D11_TEXTURE2D_DESC desc = {0};
		desc.Width = width;
		desc.Height = height;
		desc.MipLevels = 1;
		desc.ArraySize = 1;
		desc.Format = format;
		desc.SampleDesc.Count = 1;
		desc.Usage = D3D11_USAGE_DYNAMIC;
		desc.BindFlags = D3D11_BIND_SHADER_RESOURCE;
		desc.CPUAccessFlags = D3D11_CPU_ACCESS_WRITE;

		e = ID3D11Device_CreateTexture2D(device, &desc, NULL, &res->texture);
		if (e != S_OK) goto except;

		e = ID3D11Texture2D_QueryInterface(res->texture, &IID_ID3D11Resource, &res->resource);
		if (e != S_OK) goto except;

		D3D11_SHADER_RESOURCE_VIEW_DESC srvd = {0};
		srvd.Format = format;
		srvd.ViewDimension = D3D11_SRV_DIMENSION_TEXTURE2D;
		srvd.Texture2D.MipLevels = 1;

		e = ID3D11Device_CreateShaderResourceView(device, res->resource, &srvd, &res->srv);
		if (e != S_OK) goto except;

		res->width = width;
		res->height = height;
		res->format = format;
	}

	except:

	if (e != S_OK)
		gfx_d3d11_destroy_resource(res);

	return e;
}

static HRESULT gfx_d3d11_update_resource(ID3D11DeviceContext *context,
	ID3D11Resource *resource, const void *data, size_t size)
{
	D3D11_MAPPED_SUBRESOURCE res = {0};
	HRESULT e = ID3D11DeviceContext_Map(context, resource, 0, D3D11_MAP_WRITE_DISCARD, 0, &res);

	if (e == S_OK) {
		memcpy(res.pData, data, size);
		ID3D11DeviceContext_Unmap(context, resource, 0);
	}

	return e;
}

static HRESULT gfx_d3d11_crop_copy(ID3D11DeviceContext *context, ID3D11Resource *texture,
	const uint8_t *image, uint32_t w, uint32_t h, int32_t fullWidth, uint8_t bpp)
{
	D3D11_MAPPED_SUBRESOURCE res;
	HRESULT e = ID3D11DeviceContext_Map(context, texture, 0, D3D11_MAP_WRITE_DISCARD, 0, &res);
	if (e != S_OK) return e;

	for (uint32_t y = 0; y < h; y++)
		memcpy((uint8_t *) res.pData + (y * res.RowPitch), image + (y * fullWidth * bpp), w * bpp);

	ID3D11DeviceContext_Unmap(context, texture, 0);

	return e;
}

static D3D11_VIEWPORT gfx_d3d11_viewport(UINT width, UINT height, uint32_t view_w, uint32_t view_h,
	float aspect_ratio, float scale)
{
	uint32_t constrain_w = lrint(scale * (float) width);
	uint32_t constrain_h = lrint(scale * (float) height);

	if (constrain_w == 0 || constrain_h == 0 || view_w < constrain_w || view_h < constrain_h) {
		constrain_w = view_w;
		constrain_h = view_h;
	}

	float swidth = (float) constrain_w;
	float sheight = swidth / aspect_ratio;

	if (swidth > (float) view_w) {
		swidth = (float) view_w;
		sheight = swidth / aspect_ratio;
	}

	if (sheight > (float) view_h) {
		sheight = (float) view_h;
		swidth = sheight * aspect_ratio;
	}

	D3D11_VIEWPORT viewport = {0};
	viewport.Width = roundf(swidth);
	viewport.Height = roundf(sheight);
	viewport.TopLeftX = roundf(((float) view_w - viewport.Width) / 2.0f);
	viewport.TopLeftY = roundf(((float) view_h - viewport.Height) / 2.0f);

	return viewport;
}

static void gfx_d3d11_draw(struct gfx_d3d11 *ctx, ID3D11DeviceContext *context, MTY_Filter filter)
{
	UINT stride = 4 * sizeof(float);
	UINT offset = 0;

	ID3D11DeviceContext_VSSetShader(context, ctx->vs, NULL, 0);
	ID3D11DeviceContext_PSSetShader(context, ctx->ps, NULL, 0);

	for (uint8_t x = 0; x < NUM_STAGING; x++)
		if (ctx->staging[x].srv)
			ID3D11DeviceContext_PSSetShaderResources(context, x, 1, &ctx->staging[x].srv);

	ID3D11DeviceContext_PSSetSamplers(context, 0, 1, filter == MTY_FILTER_NEAREST ? &ctx->ss_nearest : &ctx->ss_linear);
	ID3D11DeviceContext_PSSetConstantBuffers(context, 0, 1, &ctx->psb);
	ID3D11DeviceContext_IASetVertexBuffers(context, 0, 1, &ctx->vb, &stride, &offset);
	ID3D11DeviceContext_IASetIndexBuffer(context, ctx->ib, DXGI_FORMAT_R32_UINT, 0);
	ID3D11DeviceContext_IASetInputLayout(context, ctx->il);
	ID3D11DeviceContext_IASetPrimitiveTopology(context, D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST);

	ID3D11DeviceContext_DrawIndexed(context, 6, 0, 0);
}

static HRESULT gfx_d3d11_reload_textures(struct gfx_d3d11 *ctx, ID3D11Device *device, ID3D11DeviceContext *context,
	const void *image, const MTY_RenderDesc *desc)
{
	switch (desc->format) {
		case MTY_COLOR_FORMAT_RGBA: {
			// RGBA
			HRESULT e = gfx_d3d11_refresh_resource(&ctx->staging[0], device, DXGI_FORMAT_B8G8R8A8_UNORM, desc->cropWidth, desc->cropHeight);
			if (e != S_OK) return e;

			e = gfx_d3d11_crop_copy(context, ctx->staging[0].resource, image, desc->cropWidth, desc->cropHeight, desc->imageWidth, 4);
			if (e != S_OK) return e;
			break;
		}
		case MTY_COLOR_FORMAT_NV12: {
			// Y
			HRESULT e = gfx_d3d11_refresh_resource(&ctx->staging[0], device, DXGI_FORMAT_R8_UNORM, desc->cropWidth, desc->cropHeight);
			if (e != S_OK) return e;

			e = gfx_d3d11_crop_copy(context, ctx->staging[0].resource, image, desc->cropWidth, desc->cropHeight, desc->imageWidth, 1);
			if (e != S_OK) return e;

			// UV
			e = gfx_d3d11_refresh_resource(&ctx->staging[1], device, DXGI_FORMAT_R8G8_UNORM, desc->cropWidth / 2, desc->cropHeight / 2);
			if (e != S_OK) return e;

			e = gfx_d3d11_crop_copy(context, ctx->staging[1].resource, (uint8_t *) image + desc->imageWidth * desc->imageHeight, desc->cropWidth / 2, desc->cropHeight / 2, desc->imageWidth / 2, 2);
			if (e != S_OK) return e;
			break;
		}
		case MTY_COLOR_FORMAT_I420:
		case MTY_COLOR_FORMAT_I444: {
			uint32_t div = desc->format == MTY_COLOR_FORMAT_I420 ? 2 : 1;

			// Y
			HRESULT e = gfx_d3d11_refresh_resource(&ctx->staging[0], device, DXGI_FORMAT_R8_UNORM, desc->cropWidth, desc->cropHeight);
			if (e != S_OK) return e;

			e = gfx_d3d11_crop_copy(context, ctx->staging[0].resource, image, desc->cropWidth, desc->cropHeight, desc->imageWidth, 1);
			if (e != S_OK) return e;

			// U
			uint8_t *p = (uint8_t *) image + desc->imageWidth * desc->imageHeight;
			e = gfx_d3d11_refresh_resource(&ctx->staging[1], device, DXGI_FORMAT_R8_UNORM, desc->cropWidth / div, desc->cropHeight / div);
			if (e != S_OK) return e;

			e = gfx_d3d11_crop_copy(context, ctx->staging[1].resource, p, desc->cropWidth / div, desc->cropHeight / div, desc->imageWidth / div, 1);
			if (e != S_OK) return e;

			// V
			p += (desc->imageWidth / div) * (desc->imageHeight / div);
			e = gfx_d3d11_refresh_resource(&ctx->staging[2], device, DXGI_FORMAT_R8_UNORM, desc->cropWidth / div, desc->cropHeight / div);
			if (e != S_OK) return e;

			e = gfx_d3d11_crop_copy(context, ctx->staging[2].resource, p, desc->cropWidth / div, desc->cropHeight / div, desc->imageWidth / div, 1);
			if (e != S_OK) return e;
			break;
		}
	}

	return S_OK;
}

HRESULT gfx_d3d11_render(struct gfx_d3d11 *ctx, ID3D11Device *device, ID3D11DeviceContext *context,
	const void *image, const MTY_RenderDesc *desc, ID3D11Texture2D *dest)
{
	// Don't do anything until we have real data
	if (desc->format != MTY_COLOR_FORMAT_UNKNOWN)
		ctx->format = desc->format;

	if (ctx->format == MTY_COLOR_FORMAT_UNKNOWN)
		return S_OK;

	// Refresh textures and load texture data
	// If format == MTY_COLOR_FORMAT_UNKNOWN, texture refreshing/loading is skipped and the previous frame is rendered
	HRESULT e = gfx_d3d11_reload_textures(ctx, device, context, image, desc);
	if (e != S_OK) return e;

	// Viewport calculation
	D3D11_VIEWPORT vp = gfx_d3d11_viewport(desc->cropWidth, desc->cropHeight,
		desc->viewWidth, desc->viewHeight, desc->aspectRatio, desc->scale);

	ID3D11DeviceContext_RSSetViewports(context, 1, &vp);

	// Uniforms affecting shader branching (constant buffer)
	ID3D11Resource *cbresource = NULL;
	e = ID3D11Buffer_QueryInterface(ctx->psb, &IID_ID3D11Resource, &cbresource);

	if (e == S_OK) {
		ctx->psvars.width = (float) desc->cropWidth;
		ctx->psvars.height = (float) desc->cropHeight;
		ctx->psvars.constrain_w = (float) vp.Width;
		ctx->psvars.constrain_h = (float) vp.Height;
		ctx->psvars.filter = desc->filter;
		ctx->psvars.effect = desc->effect;

		// Do not update the shader's format in the case of unknown
		if (desc->format != MTY_COLOR_FORMAT_UNKNOWN)
			ctx->psvars.format = desc->format;

		gfx_d3d11_update_resource(context, cbresource, &ctx->psvars, sizeof(struct psvars));
		ID3D11Resource_Release(cbresource);
	}

	// Render target
	ID3D11Resource *resource = NULL;
	ID3D11RenderTargetView *rtv = NULL;

	// If destination texture is NULL, use whatever is in the context state
	if (dest) {
		e = ID3D11Texture2D_QueryInterface(dest, &IID_ID3D11Resource, &resource);

		if (e == S_OK)
			e = ID3D11Device_CreateRenderTargetView(device, resource, NULL, &rtv);

		if (e == S_OK) {
			ID3D11DeviceContext_OMSetRenderTargets(context, 1, &rtv, NULL);

			FLOAT clear_color[4] = {0.0f, 0.0f, 0.0f, 1.0f};
			ID3D11DeviceContext_ClearRenderTargetView(context, rtv, clear_color);
		}

		if (e != S_OK)
			return e;
	}

	// Drawing pipline
	gfx_d3d11_draw(ctx, context, desc->filter);

	// Cleanup
	if (rtv)
		ID3D11RenderTargetView_Release(rtv);

	if (resource)
		ID3D11Resource_Release(resource);

	return e;
}

void gfx_d3d11_destroy(struct gfx_d3d11 **gfx)
{
	if (!gfx || !*gfx)
		return;

	struct gfx_d3d11 *ctx = *gfx;

	for (uint8_t x = 0; x < NUM_STAGING; x++)
		gfx_d3d11_destroy_resource(&ctx->staging[x]);

	if (ctx->ss_linear)
		ID3D11SamplerState_Release(ctx->ss_linear);

	if (ctx->ss_nearest)
		ID3D11SamplerState_Release(ctx->ss_nearest);

	if (ctx->il)
		ID3D11InputLayout_Release(ctx->il);

	if (ctx->psb)
		ID3D11Buffer_Release(ctx->psb);

	if (ctx->ib)
		ID3D11Buffer_Release(ctx->ib);

	if (ctx->vb)
		ID3D11Buffer_Release(ctx->vb);

	if (ctx->ps)
		ID3D11PixelShader_Release(ctx->ps);

	if (ctx->vs)
		ID3D11VertexShader_Release(ctx->vs);

	MTY_Free(ctx);
	*gfx = NULL;
}
