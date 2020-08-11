// Copyright (c) 2020 Christopher D. Dickson <cdd@matoya.group>
//
// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at https://mozilla.org/MPL/2.0/.

#include "gfx-gl.h"

#include <stdio.h>

#include "gl-dl.h"
#include "mty-viewport.h"
#include "shaders/GL/vs.h"
#include "shaders/GL/fs.h"

#define NUM_STAGING 3

struct gfx_gl {
	MTY_ColorFormat format;
	struct gfx_gl_rtv staging[NUM_STAGING];

	GLuint vs;
	GLuint fs;
	GLuint prog;
	GLuint vb;
	GLuint eb;
	GLuint dest_fb;

	GLuint loc_tex[NUM_STAGING];
	GLuint loc_pos;
	GLuint loc_uv;
	GLuint loc_scale;
	GLuint loc_format;

	float scale;
};

struct gfx_gl_state {
	GLint array_buffer;
	GLint fb;
	GLenum active_texture;
	GLint program;
	GLint texture;
	GLint viewport[4];
	GLfloat color_clear_value[4];
	GLboolean enable_blend;
	GLboolean enable_cull_face;
	GLboolean enable_depth_test;
	GLboolean enable_scissor_test;
};

static void gfx_gl_push_state(struct gfx_gl_state *s)
{
	glGetIntegerv(GL_ACTIVE_TEXTURE, (GLint *) &s->active_texture);
	glGetIntegerv(GL_ARRAY_BUFFER_BINDING, &s->array_buffer);
	glGetIntegerv(GL_CURRENT_PROGRAM, &s->program);
	glGetIntegerv(GL_TEXTURE_BINDING_2D, &s->texture);
	glGetIntegerv(GL_VIEWPORT, s->viewport);
	glGetIntegerv(GL_DRAW_FRAMEBUFFER_BINDING, (GLint *) &s->fb);
	glGetFloatv(GL_COLOR_CLEAR_VALUE, s->color_clear_value);
	s->enable_blend = glIsEnabled(GL_BLEND);
	s->enable_cull_face = glIsEnabled(GL_CULL_FACE);
	s->enable_depth_test = glIsEnabled(GL_DEPTH_TEST);
	s->enable_scissor_test = glIsEnabled(GL_SCISSOR_TEST);
}

static void gfx_gl_pop_state(struct gfx_gl_state *s)
{
	glActiveTexture(s->active_texture);
	glBindBuffer(GL_ARRAY_BUFFER, s->array_buffer);
	glUseProgram(s->program);
	glBindTexture(GL_TEXTURE_2D, s->texture);
	glViewport(s->viewport[0], s->viewport[1], s->viewport[2], s->viewport[3]);
	glBindFramebuffer(GL_DRAW_FRAMEBUFFER, s->fb);
	glClearColor(s->color_clear_value[0], s->color_clear_value[1], s->color_clear_value[2], s->color_clear_value[3]);
	if (s->enable_blend) glEnable(GL_BLEND); else glDisable(GL_BLEND);
	if (s->enable_cull_face) glEnable(GL_CULL_FACE); else glDisable(GL_CULL_FACE);
	if (s->enable_depth_test) glEnable(GL_DEPTH_TEST); else glDisable(GL_DEPTH_TEST);
	if (s->enable_scissor_test) glEnable(GL_SCISSOR_TEST); else glDisable(GL_SCISSOR_TEST);
}

static void gfx_gl_push_fbs(GLuint *rfb, GLuint *dfb)
{
	glGetIntegerv(GL_READ_FRAMEBUFFER_BINDING, (GLint *) rfb);
	glGetIntegerv(GL_DRAW_FRAMEBUFFER_BINDING, (GLint *) dfb);
}

static void gfx_gl_pop_fbs(GLuint rfb, GLuint dfb)
{
	glBindFramebuffer(GL_READ_FRAMEBUFFER, rfb);
	glBindFramebuffer(GL_DRAW_FRAMEBUFFER, dfb);
}

static void gfx_gl_log_shader_errors(struct gfx_gl *ctx, GLuint shader)
{
	GLint n = 0;
	glGetShaderiv(shader, GL_INFO_LOG_LENGTH, &n);

	if (n > 0) {
		char *log = MTY_Alloc(n, 1);

		glGetShaderInfoLog(shader, n, NULL, log);
		MTY_Log("%s", log);
		MTY_Free(log);
	}
}

bool gfx_gl_create(struct gfx_gl **gfx, const char *version)
{
	if (!gl_dl_global_init())
		return false;

	struct gfx_gl *ctx = *gfx = MTY_Alloc(1, sizeof(struct gfx_gl));

	bool r = true;

	struct gfx_gl_state state = {0};
	gfx_gl_push_state(&state);

	GLint status = GL_FALSE;
	const GLchar *vs[2] = {version, VERT};
	ctx->vs = glCreateShader(GL_VERTEX_SHADER);
	glShaderSource(ctx->vs, 2, vs, NULL);
	glCompileShader(ctx->vs);
	glGetShaderiv(ctx->vs, GL_COMPILE_STATUS, &status);
	if (status == GL_FALSE) {
		gfx_gl_log_shader_errors(ctx, ctx->vs);
		r = false;
		goto except;
	}

	const GLchar *fs[2] = {version, FRAG};
	ctx->fs = glCreateShader(GL_FRAGMENT_SHADER);
	glShaderSource(ctx->fs, 2, fs, NULL);
	glCompileShader(ctx->fs);
	glGetShaderiv(ctx->fs, GL_COMPILE_STATUS, &status);
	if (status == GL_FALSE) {
		gfx_gl_log_shader_errors(ctx, ctx->fs);
		r = false;
		goto except;
	}

	ctx->prog = glCreateProgram();
	glAttachShader(ctx->prog, ctx->vs);
	glAttachShader(ctx->prog, ctx->fs);
	glLinkProgram(ctx->prog);

	ctx->loc_pos = glGetAttribLocation(ctx->prog, "position");
	ctx->loc_uv = glGetAttribLocation(ctx->prog, "texcoord");
	ctx->loc_scale = glGetUniformLocation(ctx->prog, "scale");
	ctx->loc_format = glGetUniformLocation(ctx->prog, "format");

	for (uint8_t x = 0; x < NUM_STAGING; x++) {
		char name[32];
		snprintf(name, 32, "tex%u", x);
		ctx->loc_tex[x] = glGetUniformLocation(ctx->prog, name);
	}

	glGenBuffers(1, &ctx->vb);
	GLfloat vertices[] = {
		-1.0f,  1.0f,	// Position 0
		 0.0f,  0.0f,	// TexCoord 0
		-1.0f, -1.0f,	// Position 1
		 0.0f,  1.0f,	// TexCoord 1
		 1.0f, -1.0f,	// Position 2
		 1.0f,  1.0f,	// TexCoord 2
		 1.0f,  1.0f,	// Position 3
		 1.0f,  0.0f	// TexCoord 3
	};

	glBindBuffer(GL_ARRAY_BUFFER, ctx->vb);
	glBufferData(GL_ARRAY_BUFFER, sizeof(vertices), vertices, GL_STATIC_DRAW);

	glGenBuffers(1, &ctx->eb);
	GLshort elements[] = {
		0, 1, 2,
		2, 3, 0
	};

	glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, ctx->eb);
	glBufferData(GL_ELEMENT_ARRAY_BUFFER, sizeof(elements), elements, GL_STATIC_DRAW);

	glGenFramebuffers(1, &ctx->dest_fb);

	GLenum e = glGetError();
	if (e != GL_NO_ERROR) {
		MTY_Log("'glGetError' returned %d", e);
		r = false;
		goto except;
	}

	except:

	gfx_gl_pop_state(&state);

	if (!r)
		gfx_gl_destroy(gfx);

	return r;
}

static void gfx_gl_reload_textures(struct gfx_gl *ctx, const void *image, const MTY_RenderDesc *desc)
{
	switch (desc->format) {
		case MTY_COLOR_FORMAT_RGBA: {
			gfx_gl_rtv_refresh(&ctx->staging[0], GL_RGBA8, GL_RGBA, desc->imageWidth, desc->cropHeight);
			glBindTexture(GL_TEXTURE_2D, ctx->staging[0].texture);
			glTexSubImage2D(GL_TEXTURE_2D, 0, 0, 0, desc->imageWidth, desc->cropHeight, GL_RGBA, GL_UNSIGNED_BYTE, image);
			break;
		}
		case MTY_COLOR_FORMAT_NV12: {
			// Y
			gfx_gl_rtv_refresh(&ctx->staging[0], GL_R8, GL_RED, desc->imageWidth, desc->cropHeight);
			glBindTexture(GL_TEXTURE_2D, ctx->staging[0].texture);
			glTexSubImage2D(GL_TEXTURE_2D, 0, 0, 0, desc->imageWidth, desc->cropHeight, GL_RED, GL_UNSIGNED_BYTE, image);

			// UV
			gfx_gl_rtv_refresh(&ctx->staging[1], GL_RG8, GL_RG, desc->imageWidth / 2, desc->cropHeight / 2);
			glBindTexture(GL_TEXTURE_2D, ctx->staging[1].texture);
			glTexSubImage2D(GL_TEXTURE_2D, 0, 0, 0, desc->imageWidth / 2, desc->cropHeight / 2, GL_RG, GL_UNSIGNED_BYTE, (uint8_t *) image + desc->imageWidth * desc->imageHeight);
			break;
		}
		case MTY_COLOR_FORMAT_I420:
		case MTY_COLOR_FORMAT_I444: {
			uint32_t div = desc->format == MTY_COLOR_FORMAT_I420 ? 2 : 1;

			// Y
			gfx_gl_rtv_refresh(&ctx->staging[0], GL_R8, GL_RED, desc->imageWidth, desc->cropHeight);
			glBindTexture(GL_TEXTURE_2D, ctx->staging[0].texture);
			glTexSubImage2D(GL_TEXTURE_2D, 0, 0, 0, desc->imageWidth, desc->cropHeight, GL_RED, GL_UNSIGNED_BYTE, image);

			// U
			uint8_t *p = (uint8_t *) image + desc->imageWidth * desc->imageHeight;
			gfx_gl_rtv_refresh(&ctx->staging[1], GL_R8, GL_RED, desc->imageWidth / div, desc->cropHeight / div);
			glBindTexture(GL_TEXTURE_2D, ctx->staging[1].texture);
			glTexSubImage2D(GL_TEXTURE_2D, 0, 0, 0, desc->imageWidth / div, desc->cropHeight / div, GL_RED, GL_UNSIGNED_BYTE, p);

			// V
			p += (desc->imageWidth / div) * (desc->imageHeight / div);
			gfx_gl_rtv_refresh(&ctx->staging[2], GL_R8, GL_RED, desc->imageWidth / div, desc->cropHeight / div);
			glBindTexture(GL_TEXTURE_2D, ctx->staging[2].texture);
			glTexSubImage2D(GL_TEXTURE_2D, 0, 0, 0, desc->imageWidth / div, desc->cropHeight / div, GL_RED, GL_UNSIGNED_BYTE, p);
			break;
		}
	}
}

bool gfx_gl_render(struct gfx_gl *ctx, const void *image, const MTY_RenderDesc *desc, GLuint dest)
{
	// Don't do anything until we have real data
	if (desc->format != MTY_COLOR_FORMAT_UNKNOWN) {
		ctx->scale = (float) desc->cropWidth / (float) desc->imageWidth;
		ctx->format = desc->format;
	}

	if (ctx->format == MTY_COLOR_FORMAT_UNKNOWN)
		return true;

	struct gfx_gl_state state = {0};
	gfx_gl_push_state(&state);

	// Refresh staging texture dimensions
	gfx_gl_reload_textures(ctx, image, desc);

	// Viewport
	float vpx, vpy, vpw, vph;
	mty_viewport(desc->cropWidth, desc->cropHeight, desc->viewWidth, desc->viewHeight,
		desc->aspectRatio, desc->scale, &vpx, &vpy, &vpw, &vph);
	glViewport(lrint(vpx), lrint(vpy), lrint(vpw), lrint(vph));

	// Begin render pass (set destination texture if available)
	if (dest) {
		glBindFramebuffer(GL_FRAMEBUFFER, ctx->dest_fb);
		glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, dest, 0);

		glClearColor(0.0f, 0.0f, 0.0f, 1.0f);
		glClear(GL_COLOR_BUFFER_BIT);
	}

	// Context state, set vertex and fragment shaders
	glDisable(GL_BLEND);
	glDisable(GL_CULL_FACE);
	glDisable(GL_DEPTH_TEST);
	glDisable(GL_SCISSOR_TEST);
	glUseProgram(ctx->prog);

	// Vertex shader
	glBindBuffer(GL_ARRAY_BUFFER, ctx->vb);
	glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, ctx->eb);
	glEnableVertexAttribArray(ctx->loc_pos);
	glEnableVertexAttribArray(ctx->loc_uv);
	glVertexAttribPointer(ctx->loc_pos, 2, GL_FLOAT, GL_FALSE, 4 * sizeof(GLfloat), 0);
	glVertexAttribPointer(ctx->loc_uv,  2, GL_FLOAT, GL_FALSE, 4 * sizeof(GLfloat), (void *) (2 * sizeof(GLfloat)));

	// Fragment shader
	for (uint8_t x = 0; x < NUM_STAGING; x++) {
		if (ctx->staging[x].texture) {
			glActiveTexture(GL_TEXTURE0 + x);
			glBindTexture(GL_TEXTURE_2D, ctx->staging[x].texture);
			glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
			glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
			glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
			glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
			glUniform1i(ctx->loc_tex[x], x);
		}
	}

	glUniform1f(ctx->loc_scale, ctx->scale); // Used instead of a crop copy mechanism
	glUniform1i(ctx->loc_format, ctx->format);

	// Draw
	glDrawElements(GL_TRIANGLES, 6, GL_UNSIGNED_SHORT, 0);

	gfx_gl_pop_state(&state);

	return true;
}

void gfx_gl_destroy(struct gfx_gl **gfx)
{
	if (!gfx || !*gfx)
		return;

	struct gfx_gl *ctx = *gfx;

	for (uint8_t x = 0; x < NUM_STAGING; x++)
		gfx_gl_rtv_destroy(&ctx->staging[x]);

	if (ctx->vb)
		glDeleteBuffers(1, &ctx->vb);

	if (ctx->eb)
		glDeleteBuffers(1, &ctx->eb);

	if (ctx->prog) {
		if (ctx->vs)
			glDetachShader(ctx->prog, ctx->vs);

		if (ctx->fs)
			glDetachShader(ctx->prog, ctx->fs);
	}

	if (ctx->vs)
		glDeleteShader(ctx->vs);

	if (ctx->fs)
		glDeleteShader(ctx->fs);

	if (ctx->prog)
		glDeleteProgram(ctx->prog);

	if (ctx->dest_fb)
		glDeleteFramebuffers(1, &ctx->dest_fb);

	MTY_Free(ctx);
	*gfx = NULL;
}


// Render Target Views

void gfx_gl_rtv_destroy(struct gfx_gl_rtv *rtv)
{
	if (!gl_dl_global_init())
		return;

	if (rtv->texture) {
		glDeleteTextures(1, &rtv->texture);
		rtv->texture = 0;
	}

	if (rtv->fb) {
		glDeleteFramebuffers(1, &rtv->fb);
		rtv->fb = 0;
	}
}

void gfx_gl_rtv_refresh(struct gfx_gl_rtv *rtv, GLint internal, GLenum format, uint32_t w, uint32_t h)
{
	if (!gl_dl_global_init())
		return;

	if (!rtv->texture || !rtv->fb || rtv->w != w || rtv->h != h || rtv->format != format) {
		gfx_gl_rtv_destroy(rtv);

		GLuint rfb = 0;
		GLuint dfb = 0;
		GLuint tex = 0;
		gfx_gl_push_fbs(&rfb, &dfb);
		glGetIntegerv(GL_TEXTURE_BINDING_2D, (GLint *) &tex);

		glGenTextures(1, &rtv->texture);
		glBindTexture(GL_TEXTURE_2D, rtv->texture);
		glTexImage2D(GL_TEXTURE_2D, 0, internal, w, h, 0, format, GL_UNSIGNED_BYTE, NULL);

		glGenFramebuffers(1, &rtv->fb);
		glBindFramebuffer(GL_FRAMEBUFFER, rtv->fb);
		glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, rtv->texture, 0);

		rtv->w = w;
		rtv->h = h;
		rtv->format = format;

		gfx_gl_pop_fbs(rfb, dfb);
		glBindTexture(GL_TEXTURE_2D, tex);
	}
}

void gfx_gl_rtv_blit_to_back_buffer(struct gfx_gl_rtv *rtv)
{
	if (!gl_dl_global_init())
		return;

	GLuint rfb = 0;
	GLuint dfb = 0;
	gfx_gl_push_fbs(&rfb, &dfb);

	glBindFramebuffer(GL_READ_FRAMEBUFFER, rtv->fb);
	glBindFramebuffer(GL_DRAW_FRAMEBUFFER, 0);
	glBlitFramebuffer(0, 0, rtv->w, rtv->h, 0, 0, rtv->w, rtv->h, GL_COLOR_BUFFER_BIT, GL_NEAREST);

	gfx_gl_pop_fbs(rfb, dfb);
}


// Misc

void gfx_gl_finish(void)
{
	if (!gl_dl_global_init())
		return;

	glFinish();
}
