// Copyright (c) 2020 Christopher D. Dickson <cdd@matoya.group>
//
// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at https://mozilla.org/MPL/2.0/.

#pragma once

struct gfx_d3d9;

#define gfx_d3d9_create(mty_device, gfx) (MTY_Log("MTY_GFX_D3D9 is unimplemented"), false)
#define gfx_d3d9_render(ctx, device, image, desc, dest) false
#define gfx_d3d9_destroy(gfx)
