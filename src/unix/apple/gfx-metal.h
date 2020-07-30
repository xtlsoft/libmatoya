// Copyright (c) 2020 Christopher D. Dickson <cdd@matoya.group>
//
// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at https://mozilla.org/MPL/2.0/.

#pragma once

#include "matoya.h"

#include <Metal/Metal.h>

struct gfx_metal;

bool gfx_metal_init(id<MTLDevice> device, struct gfx_metal **gfx);
void gfx_metal_render(struct gfx_metal *ctx, id<MTLCommandQueue> cq,
	const void *image, const MTY_RenderDesc *desc, id<MTLTexture> dest);
void gfx_metal_destroy(struct gfx_metal **gfx);
