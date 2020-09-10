// Copyright (c) 2020 Christopher D. Dickson <cdd@matoya.group>
//
// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at https://mozilla.org/MPL/2.0/.

#pragma once

#include "matoya.h"

#if defined(MTY_GL_INCLUDE)
	#include MTY_GL_INCLUDE
#else
	#include "GL/glcorearb30.h"
#endif

struct gfx_gl_rtv {
	GLenum format;
	GLuint texture;
	GLuint fb;
	uint32_t w;
	uint32_t h;
};

struct gfx_gl;

bool gfx_gl_create(struct gfx_gl **gfx, const char *version);
bool gfx_gl_render(struct gfx_gl *ctx, const void *image, const MTY_RenderDesc *desc, GLuint dest);
void gfx_gl_destroy(struct gfx_gl **gfx);

void gfx_gl_rtv_destroy(struct gfx_gl_rtv *rtv);
void gfx_gl_rtv_refresh(struct gfx_gl_rtv *rtv, GLint internal, GLenum format, uint32_t w, uint32_t h);
void gfx_gl_rtv_blit_to_back_buffer(struct gfx_gl_rtv *rtv);

void gfx_gl_finish(void);
