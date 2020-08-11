// Copyright (c) 2020 Christopher D. Dickson <cdd@matoya.group>
//
// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at https://mozilla.org/MPL/2.0/.

#pragma once

#include "matoya.h"

struct gfx_metal;

bool gfx_metal_create(MTY_Device *mty_device, struct gfx_metal **gfx);
bool gfx_metal_render(struct gfx_metal *ctx, MTY_Context *mty_context,
	const void *image, const MTY_RenderDesc *desc, MTY_Texture *mty_dest);
void gfx_metal_destroy(struct gfx_metal **gfx);

MTY_Device *gfx_metal_device(MTY_Context *context);
