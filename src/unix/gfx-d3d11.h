// Copyright (c) 2020 Christopher D. Dickson <cdd@matoya.group>
//
// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at https://mozilla.org/MPL/2.0/.

#pragma once

struct gfx_d3d11;

#define gfx_d3d11_create(mty_device, gfx) (MTY_Log("MTY_GFX_D3D11 is unimplemented"), false)
#define gfx_d3d11_render(ctx, device, context, image, desc, dest) false
#define gfx_d3d11_destroy(gfx)
