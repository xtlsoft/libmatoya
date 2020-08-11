// Copyright (c) 2020 Christopher D. Dickson <cdd@matoya.group>
//
// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at https://mozilla.org/MPL/2.0/.

#pragma once

#include "matoya.h"

#include <stdint.h>

#define COBJMACROS
#include <d3d9.h>

struct gfx_d3d9;

bool gfx_d3d9_create(IDirect3DDevice9 *device, struct gfx_d3d9 **gfx);
bool gfx_d3d9_render(struct gfx_d3d9 *ctx, IDirect3DDevice9 *device,
	const void *image, const MTY_RenderDesc *desc, IDirect3DTexture9 *dest);
void gfx_d3d9_destroy(struct gfx_d3d9 **gfx);
