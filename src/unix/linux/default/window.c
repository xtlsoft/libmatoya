// Copyright (c) 2020 Christopher D. Dickson <cdd@matoya.group>
//
// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at https://mozilla.org/MPL/2.0/.

#include "matoya.h"

#include <stdlib.h>
#include <stdio.h>
#include <math.h>

#include "x-dl.h"
#include "gfx-gl.h"

#define WINDOW_MASK (   \
	ExposureMask      | \
	KeyPressMask      | \
	KeyReleaseMask    | \
	ButtonPressMask   | \
	ButtonReleaseMask | \
	PointerMotionMask   \
)

struct MTY_Window {
	Display *display;
	Window xwindow;
	MTY_MsgFunc msg_func;
	const void *opaque;
	float dpi;

	uint32_t width;
	uint32_t height;
	GLuint back_buffer;
	GLXContext gl_ctx;
	MTY_Renderer *renderer;
	struct gfx_gl_rtv rtv;

	// Client messages
	Atom wm_delete_window;
};

static void __attribute__((destructor)) window_global_destroy(void)
{
	x_dl_global_destroy();
}

MTY_Window *MTY_WindowCreate(const char *title, MTY_MsgFunc msg_func, const void *opaque,
	uint32_t width, uint32_t height, bool fullscreen)
{
	if (!x_dl_global_init())
		return false;

	MTY_Window *ctx = MTY_Alloc(1, sizeof(MTY_Window));
	ctx->opaque = opaque;
	ctx->msg_func = msg_func;

	bool r = true;

	ctx->display = XOpenDisplay(NULL);
	if (!ctx->display) {r = false; goto except;}

	GLint attr[] = {GLX_RGBA, GLX_DEPTH_SIZE, 24, GLX_DOUBLEBUFFER, None};

	// TODO Choose appropriate screen number here through additional functions
	// GLX is tied very early to window creation
	XVisualInfo *vis = glXChooseVisual(ctx->display, 0, attr);
	if (!vis) {r = false; goto except;}

	// Get root window, calc center
	Window root = XDefaultRootWindow(ctx->display);

	// Get DPI
	const char *dpi_s = XGetDefault(ctx->display, "Xft", "dpi");
	ctx->dpi = dpi_s ? (float) atoi(dpi_s) / (float) 96.0f : 1.0f;

	XWindowAttributes root_attr = {0};
	XGetWindowAttributes(ctx->display, root, &root_attr);
	int32_t x = (root_attr.width - width) / 2;
	int32_t y = (root_attr.height - height) / 2;

	XSetWindowAttributes swa = {0};
	swa.colormap = XCreateColormap(ctx->display, root, vis->visual, AllocNone);
	swa.event_mask = WINDOW_MASK;

	ctx->xwindow = XCreateWindow(ctx->display, root, 0, 0, width, height, 0, vis->depth,
		InputOutput, vis->visual, CWColormap | CWEventMask, &swa);

	XMapWindow(ctx->display, ctx->xwindow);
	XMoveWindow(ctx->display, ctx->xwindow, x, y);
	MTY_WindowSetTitle(ctx, title, NULL);

	// Resister custom messages from the window manager
	ctx->wm_delete_window = XInternAtom(ctx->display, "WM_DELETE_WINDOW", False);
	XSetWMProtocols(ctx->display, ctx->xwindow, &ctx->wm_delete_window, 1);

	// GL context
	ctx->gl_ctx = glXCreateContext(ctx->display, vis, NULL, GL_TRUE);
	glXMakeCurrent(ctx->display, ctx->xwindow, ctx->gl_ctx);

	ctx->renderer = MTY_RendererCreate();

	except:

	if (!r)
		MTY_WindowDestroy(&ctx);

	return ctx;
}

void MTY_AppRun(MTY_AppFunc func, const void *opaque)
{
	while (func((void *) opaque));
}

void MTY_WindowSetTitle(MTY_Window *ctx, const char *title, const char *subtitle)
{
	char ctitle[MTY_TITLE_MAX];
	if (subtitle) {
		snprintf(ctitle, MTY_TITLE_MAX, "%s - %s", title, subtitle);
	} else {
		snprintf(ctitle, MTY_TITLE_MAX, "%s", title);
	}

	XStoreName(ctx->display, ctx->xwindow, ctitle);
}

bool MTY_WindowGetSize(MTY_Window *ctx, uint32_t *width, uint32_t *height)
{
	XWindowAttributes attr = {0};
	XGetWindowAttributes(ctx->display, ctx->xwindow, &attr);

	*width = attr.width;
	*height = attr.height;

	return true;
}

static MTY_Scancode window_x_to_mty(KeySym sym)
{
	switch (sym) {
		case XK_Escape: return MTY_SCANCODE_ESCAPE;
		case XK_Tab: return MTY_SCANCODE_TAB;
		case XK_w: return MTY_SCANCODE_W;
		case XK_c: return MTY_SCANCODE_C;
		case XK_x: return MTY_SCANCODE_X;
		case XK_y: return MTY_SCANCODE_Y;
		case XK_z: return MTY_SCANCODE_Z;
		case XK_v: return MTY_SCANCODE_V;
		case XK_r: return MTY_SCANCODE_R;
		case XK_a: return MTY_SCANCODE_A;
		case XK_s: return MTY_SCANCODE_S;
		case XK_d: return MTY_SCANCODE_D;
		case XK_l: return MTY_SCANCODE_L;
		case XK_o: return MTY_SCANCODE_O;
		case XK_p: return MTY_SCANCODE_P;
		case XK_m: return MTY_SCANCODE_M;
		case XK_t: return MTY_SCANCODE_T;
		case XK_1: return MTY_SCANCODE_1;
		case XK_semicolon: return MTY_SCANCODE_SEMICOLON;
		case XK_Shift_L: return MTY_SCANCODE_LSHIFT;
		case XK_space: return MTY_SCANCODE_SPACE;
		case XK_Left: return MTY_SCANCODE_LEFT;
		case XK_Right: return MTY_SCANCODE_RIGHT;
		case XK_Up: return MTY_SCANCODE_UP;
		case XK_Down: return MTY_SCANCODE_DOWN;
		case XK_Page_Up: return MTY_SCANCODE_PAGEUP;
		case XK_Page_Down: return MTY_SCANCODE_PAGEDOWN;
		case XK_Return: return MTY_SCANCODE_ENTER;
		case XK_Home: return MTY_SCANCODE_HOME;
		case XK_End: return MTY_SCANCODE_END;
		case XK_Insert: return MTY_SCANCODE_INSERT;
		case XK_Delete: return MTY_SCANCODE_DELETE;
		case XK_BackSpace: return MTY_SCANCODE_BACKSPACE;
		case XK_Shift_R: return MTY_SCANCODE_RSHIFT;
		case XK_Control_L: return MTY_SCANCODE_LCTRL;
		case XK_Control_R: return MTY_SCANCODE_RCTRL;
		case XK_Alt_L: return MTY_SCANCODE_LALT;
		case XK_Alt_R: return MTY_SCANCODE_RALT;
		// case XK_: return MTY_SCANCODE_LGUI;
		// case XK_: return MTY_SCANCODE_RGUI;
	}

	return MTY_SCANCODE_NONE;
}

void MTY_WindowPoll(MTY_Window *ctx)
{
	while (XPending(ctx->display) > 0) {
		MTY_Msg wmsg = {0};

		XEvent event = {0};
		XNextEvent(ctx->display, &event);

		switch (event.type) {
			case KeyPress:
			case KeyRelease: {
				wmsg.type = MTY_WINDOW_MSG_KEYBOARD;
				wmsg.keyboard.pressed = event.type == KeyPress;
				wmsg.keyboard.scancode = window_x_to_mty(XLookupKeysym(&event.xkey, 0));
				break;
			}
			case ButtonPress:
			case ButtonRelease:
				wmsg.type = MTY_WINDOW_MSG_MOUSE_BUTTON;
				wmsg.mouseButton.pressed = event.type == ButtonPress;

				switch (event.xbutton.button) {
					case 1: wmsg.mouseButton.button = MTY_MOUSE_BUTTON_L;      break;
					case 2: wmsg.mouseButton.button = MTY_MOUSE_BUTTON_MIDDLE; break;
					case 3: wmsg.mouseButton.button = MTY_MOUSE_BUTTON_R;      break;

					// Mouse wheel
					case 4:
					case 5:
						wmsg.type = MTY_WINDOW_MSG_MOUSE_WHEEL;
						wmsg.mouseWheel.x = 0;
						wmsg.mouseWheel.y = event.xbutton.button == 4 ? -100 : 100;
						break;
				}
				break;
			case MotionNotify:
				wmsg.type = MTY_WINDOW_MSG_MOUSE_MOTION;
				wmsg.mouseMotion.relative = false;
				wmsg.mouseMotion.x = event.xmotion.x;
				wmsg.mouseMotion.y = event.xmotion.y;
				break;
			case ClientMessage:
				if ((Atom) event.xclient.data.l[0] == ctx->wm_delete_window)
					wmsg.type = MTY_WINDOW_MSG_CLOSE;
				break;
		}

		if (wmsg.type != MTY_WINDOW_MSG_NONE)
			ctx->msg_func(&wmsg, (void *) ctx->opaque);
	}
}

bool MTY_WindowIsForeground(MTY_Window *ctx)
{
	int revert = 0;
	Window w = 0;

	XGetInputFocus(ctx->display, &w, &revert);

	return w == ctx->xwindow;
}

uint32_t MTY_WindowGetRefreshRate(MTY_Window *ctx)
{
	return 60;
}

float MTY_WindowGetDPIScale(MTY_Window *ctx)
{
	return ctx->dpi;
}

void MTY_WindowSetFullscreen(MTY_Window *ctx)
{
	Hints hints = {0};
	hints.flags = 2;
	Atom prop = XInternAtom(ctx->display, "_MOTIF_WM_HINTS", True);

	XChangeProperty(ctx->display, ctx->xwindow, prop, prop, 32, PropModeReplace, (unsigned char *) &hints, 5);
	XMapWindow(ctx->display, ctx->xwindow);
}

void MTY_WindowSetSize(MTY_Window *ctx, uint32_t width, uint32_t height)
{
}

bool MTY_WindowIsFullscreen(MTY_Window *ctx)
{
	return false;
}

void MTY_WindowPresent(MTY_Window *ctx, uint32_t num_frames)
{
	MTY_WindowGetBackBuffer(ctx);

	gfx_gl_rtv_blit_to_back_buffer(&ctx->rtv);
	ctx->back_buffer = 0;

	glXSwapBuffers(ctx->display, ctx->xwindow);
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
		MTY_WindowGetSize(ctx, &ctx->width, &ctx->height);

		gfx_gl_rtv_refresh(&ctx->rtv, GL_RGBA8, GL_RGBA, ctx->width, ctx->height);
		ctx->back_buffer = ctx->rtv.texture;
	}

	return (MTY_Texture *) (size_t) ctx->back_buffer;
}

void MTY_WindowDrawQuad(MTY_Window *ctx, const void *image, const MTY_RenderDesc *desc)
{
	MTY_WindowGetBackBuffer(ctx);

	MTY_RenderDesc mutated = *desc;
	mutated.viewWidth = ctx->width;
	mutated.viewHeight = ctx->height;

	MTY_RendererDrawQuad(ctx->renderer, MTY_GFX_GL, NULL, NULL, image, &mutated,
		(MTY_Texture *) (uintptr_t) ctx->back_buffer);
}

void MTY_WindowDestroy(MTY_Window **window)
{
	if (!window || !*window)
		return;

	MTY_Window *ctx = *window;

	MTY_RendererDestroy(&ctx->renderer);
	gfx_gl_rtv_destroy(&ctx->rtv);

	if (ctx->display)
		XCloseDisplay(ctx->display);

	MTY_Free(ctx);
	*window = NULL;
}

void *MTY_GLGetProcAddress(const char *name)
{
	if (!x_dl_global_init())
		return NULL;

	return glXGetProcAddress((const GLubyte *) name);
}
