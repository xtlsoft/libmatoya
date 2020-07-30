// Copyright (c) 2020 Christopher D. Dickson <cdd@matoya.group>
//
// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at https://mozilla.org/MPL/2.0/.

#include "matoya.h"

#include <stdio.h>
#include <stdlib.h>

#include "gfx-gl.h"

void web_get_size(uint32_t *width, uint32_t *height);
void web_resize_canvas(void);
void web_set_title(const char *title);
void web_create_canvas(void);
void web_raf(MTY_AppFunc func, const void *opaque);
void web_register_drag(void);
float web_get_pixel_ratio(void);
void web_attach_events(MTY_Window *w, void *malloc, void *free, void *mouse_motion,
	void *mouse_button, void *mouse_wheel, void *keyboard, void *drop);

struct MTY_Window {
	MTY_Hash *h;
	MTY_WindowMsgFunc msg_func;
	const void *opaque;

	GLint back_buffer;
	MTY_Renderer *renderer;
	struct gfx_gl_rtv rtv;
};

static void window_hash_codes(MTY_Hash *h)
{
	MTY_HashSet(h, "Key1", (void *) MTY_SCANCODE_1);
	MTY_HashSet(h, "KeyA", (void *) MTY_SCANCODE_A);
	MTY_HashSet(h, "KeyC", (void *) MTY_SCANCODE_C);
	MTY_HashSet(h, "KeyD", (void *) MTY_SCANCODE_D);
	MTY_HashSet(h, "KeyL", (void *) MTY_SCANCODE_L);
	MTY_HashSet(h, "KeyM", (void *) MTY_SCANCODE_M);
	MTY_HashSet(h, "KeyO", (void *) MTY_SCANCODE_O);
	MTY_HashSet(h, "KeyP", (void *) MTY_SCANCODE_P);
	MTY_HashSet(h, "KeyR", (void *) MTY_SCANCODE_R);
	MTY_HashSet(h, "KeyS", (void *) MTY_SCANCODE_S);
	MTY_HashSet(h, "KeyT", (void *) MTY_SCANCODE_T);
	MTY_HashSet(h, "KeyV", (void *) MTY_SCANCODE_V);
	MTY_HashSet(h, "KeyW", (void *) MTY_SCANCODE_W);
	MTY_HashSet(h, "KeyX", (void *) MTY_SCANCODE_X);
	MTY_HashSet(h, "KeyY", (void *) MTY_SCANCODE_Y);
	MTY_HashSet(h, "KeyZ", (void *) MTY_SCANCODE_Z);
	MTY_HashSet(h, "Space", (void *) MTY_SCANCODE_SPACE);
	MTY_HashSet(h, "ArrowDown", (void *) MTY_SCANCODE_DOWN);
	MTY_HashSet(h, "ArrowLeft", (void *) MTY_SCANCODE_LEFT);
	MTY_HashSet(h, "ArrowUp", (void *) MTY_SCANCODE_UP);
	MTY_HashSet(h, "ArrowRight", (void *) MTY_SCANCODE_RIGHT);
	MTY_HashSet(h, "Escape", (void *) MTY_SCANCODE_ESCAPE);
	MTY_HashSet(h, "Semicolon", (void *) MTY_SCANCODE_SEMICOLON);
	MTY_HashSet(h, "ShiftLeft", (void *) MTY_SCANCODE_LSHIFT);
	MTY_HashSet(h, "ShiftRight", (void *) MTY_SCANCODE_RSHIFT);
	MTY_HashSet(h, "ControlLeft", (void *) MTY_SCANCODE_LCTRL);
	MTY_HashSet(h, "ControlRight", (void *) MTY_SCANCODE_RCTRL);
	MTY_HashSet(h, "AltLeft", (void *) MTY_SCANCODE_LALT);
	MTY_HashSet(h, "AltRight", (void *) MTY_SCANCODE_RALT);
}

static void window_mouse_motion(MTY_Window *ctx, int32_t x, int32_t y)
{
	MTY_WindowMsg msg = {0};
	msg.type = MTY_WINDOW_MSG_MOUSE_MOTION;
	msg.mouseMotion.x = x;
	msg.mouseMotion.y = y;

	ctx->msg_func(&msg, (void *) ctx->opaque);
}

static void window_mouse_button(MTY_Window *ctx, bool pressed, int32_t button)
{
	MTY_WindowMsg msg = {0};
	msg.type = MTY_WINDOW_MSG_MOUSE_BUTTON;
	msg.mouseButton.pressed = pressed;
	msg.mouseButton.button = button == 1 ? MTY_MOUSE_BUTTON_L :
		button == 2 ? MTY_MOUSE_BUTTON_MIDDLE : button == 3 ? MTY_MOUSE_BUTTON_R : 0;

	ctx->msg_func(&msg, (void *) ctx->opaque);
}

static void window_mouse_wheel(MTY_Window *ctx, int32_t x, int32_t y)
{
	MTY_WindowMsg msg = {0};
	msg.type = MTY_WINDOW_MSG_MOUSE_WHEEL;
	msg.mouseWheel.x = x;
	msg.mouseWheel.y = y;

	ctx->msg_func(&msg, (void *) ctx->opaque);
}

static void window_keyboard(MTY_Window *ctx, bool pressed, const char *code)
{
	MTY_WindowMsg msg = {0};
	msg.keyboard.scancode = (MTY_Scancode) MTY_HashGet(ctx->h, code);

	if (msg.keyboard.scancode != 0) {
		msg.type = MTY_WINDOW_MSG_KEYBOARD;
		msg.keyboard.pressed = pressed;

		ctx->msg_func(&msg, (void *) ctx->opaque);
	}
}

static void window_drop(MTY_Window *ctx, const char *name, const void *data, size_t size)
{
	MTY_WindowMsg msg = {0};
	msg.type = MTY_WINDOW_MSG_DROP;
	msg.drop.name = name;
	msg.drop.data = data;
	msg.drop.size = size;

	ctx->msg_func(&msg, (void *) ctx->opaque);
}

bool MTY_WindowCreate(const char *title, MTY_WindowMsgFunc msg_func, const void *opaque,
	uint32_t width, uint32_t height, bool fullscreen, MTY_Window **window)
{
	MTY_Window *ctx = *window = MTY_Alloc(1, sizeof(MTY_Window));
	ctx->opaque = opaque;
	ctx->msg_func = msg_func;

	MTY_HashCreate(100, &ctx->h);
	window_hash_codes(ctx->h);

	web_create_canvas();
	web_attach_events(ctx, malloc, free, window_mouse_motion,
		window_mouse_button, window_mouse_wheel, window_keyboard, window_drop);

	MTY_WindowSetTitle(ctx, title, NULL);
	MTY_RendererCreate(&ctx->renderer);

	return true;
}

void MTY_AppRun(MTY_AppFunc func, const void *opaque)
{
	web_raf(func, opaque);
}

void MTY_WindowSetTitle(MTY_Window *ctx, const char *title, const char *subtitle)
{
	char ctitle[MTY_TITLE_MAX];
	if (subtitle) {
		snprintf(ctitle, MTY_TITLE_MAX, "%s - %s", title, subtitle);
	} else {
		snprintf(ctitle, MTY_TITLE_MAX, "%s", title);
	}

	web_set_title(ctitle);
}

void MTY_WindowPoll(MTY_Window *ctx)
{
}

bool MTY_WindowIsForeground(MTY_Window *ctx)
{
	return true;
}

uint32_t MTY_WindowGetRefreshRate(MTY_Window *ctx)
{
	return 60;
}

float MTY_WindowGetDPIScale(MTY_Window *ctx)
{
	return web_get_pixel_ratio();
}

void MTY_WindowSetFullscreen(MTY_Window *ctx)
{
}

void MTY_WindowSetWindowed(MTY_Window *ctx, uint32_t width, uint32_t height)
{
}

bool MTY_WindowIsFullscreen(MTY_Window *ctx)
{
	return false;
}

void MTY_WindowPresent(MTY_Window *ctx, uint32_t num_frames)
{
	web_resize_canvas();
	MTY_WindowGetBackBuffer(ctx);

	gfx_gl_rtv_blit_to_back_buffer(&ctx->rtv);
	ctx->back_buffer = 0;
}

MTY_Device *MTY_WindowGetDevice(MTY_Window *ctx)
{
	return NULL;
}

MTY_Context *MTY_WindowGetContext(MTY_Window *ctx)
{
	return NULL;
}

MTY_Texture *MTY_WindowGetBackBuffer(MTY_Window *ctx)
{
	if (!ctx->back_buffer) {
		uint32_t width = 0;
		uint32_t height = 0;
		web_get_size(&width, &height);

		gfx_gl_rtv_refresh(&ctx->rtv, GL_RGBA, width, height);
		ctx->back_buffer = ctx->rtv.texture;
	}

	return (MTY_Texture *) (size_t) ctx->back_buffer;
}

void MTY_WindowDrawQuad(MTY_Window *ctx, const void *image, const MTY_RenderDesc *desc)
{
	MTY_WindowGetBackBuffer(ctx);

	MTY_RenderDesc mutated = *desc;
	web_get_size(&mutated.viewWidth, &mutated.viewHeight);

	MTY_RendererDrawQuad(ctx->renderer, MTY_GFX_GL, NULL, NULL, image, &mutated,
		(MTY_Texture *) (uintptr_t) ctx->back_buffer);
}

void MTY_WindowDestroy(MTY_Window **window)
{
	if (!window || !*window)
		return;

	MTY_Window *ctx = *window;

	MTY_HashDestroy(&ctx->h, NULL);
	MTY_RendererDestroy(&ctx->renderer);
	gfx_gl_rtv_destroy(&ctx->rtv);

	MTY_Free(ctx);
	*window = NULL;
}
