// Copyright (c) 2020 Christopher D. Dickson <cdd@matoya.group>
//
// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at https://mozilla.org/MPL/2.0/.

#pragma once

#include "matoya.h"

#include <stdint.h>

#define COBJMACROS
#include <d3d11.h>

struct gfx_d3d11;

bool gfx_d3d11_create(ID3D11Device *device, struct gfx_d3d11 **gfx);
bool gfx_d3d11_render(struct gfx_d3d11 *ctx, ID3D11Device *device, ID3D11DeviceContext *context,
	const void *image, const MTY_RenderDesc *desc, ID3D11Texture2D *dest);
void gfx_d3d11_destroy(struct gfx_d3d11 **gfx);
