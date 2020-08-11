// Copyright (c) 2020 Christopher D. Dickson <cdd@matoya.group>
//
// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at https://mozilla.org/MPL/2.0/.

#include "matoya.h"

#include <EGL/egl.h>
#include <android/native_window.h>
#include <android/native_window_jni.h>

#include <android/log.h>

#include "gfx-gl.h"


static struct window_gfx {
	MTY_Mutex *mutex;
	ANativeWindow *window;
	bool ready;
	bool init;
	int32_t w;
	int32_t h;

	EGLDisplay display;
	EGLSurface surface;
	EGLContext context;
} GFX;

JNIEXPORT void JNICALL Java_group_matoya_merton_MainSurface_mty_1set_1surface(JNIEnv *env, jobject instance, jobject surface)
{
	GFX.window = ANativeWindow_fromSurface(env, surface);

	if (GFX.window)
		GFX.ready = true;
}

JNIEXPORT void JNICALL Java_group_matoya_merton_MainSurface_mty_1surface_1dims(JNIEnv *env, jobject instance, jint w, jint h)
{
	GFX.w = w;
	GFX.h = h;
}

JNIEXPORT void JNICALL Java_group_matoya_merton_MainSurface_mty_1unset_1surface(JNIEnv *env, jobject instance)
{
	if (GFX.surface)
		eglDestroySurface(GFX.display, GFX.surface);

	if (GFX.window)
		ANativeWindow_release(GFX.window);

	GFX.surface = 0;
	GFX.window = NULL;

	GFX.ready = false;
	GFX.init = false;
}

JNIEXPORT void JNICALL Java_group_matoya_merton_MainSurface_mty_1surface_1lock(JNIEnv *env, jobject instance)
{
	MTY_MutexLock(GFX.mutex);
}

JNIEXPORT void JNICALL Java_group_matoya_merton_MainSurface_mty_1surface_1unlock(JNIEnv *env, jobject instance)
{
	MTY_MutexUnlock(GFX.mutex);
}



int main(int argc, char **argv);

JNIEXPORT void JNICALL Java_group_matoya_merton_MainActivity_mty_1global_1init(JNIEnv *env, jobject instance)
{
	MTY_MutexCreate(&GFX.mutex);
}

JNIEXPORT void JNICALL Java_group_matoya_merton_AppThread_mty_1start(JNIEnv *env, jobject instance, jstring jname)
{
	const char *cname = (*env)->GetStringUTFChars(env, jname, 0);
	char *name = MTY_Strdup(cname);

	(*env)->ReleaseStringUTFChars(env, jname, cname);

	// __android_log_print(ANDROID_LOG_INFO, "MTY", "START");

	main(1, &name);

	MTY_Free(name);
}


struct MTY_Window {
	MTY_WindowMsgFunc msg_func;
	const void *opaque;

	GLuint back_buffer;
	MTY_Renderer *renderer;
	struct gfx_gl_rtv rtv;
};

static void window_destroy_members(MTY_Window *ctx)
{
	MTY_RendererDestroy(&ctx->renderer);
	gfx_gl_rtv_destroy(&ctx->rtv);

	if (GFX.display) {
		if (GFX.context) {
			eglMakeCurrent(GFX.display, 0, 0, 0);
			eglDestroyContext(GFX.display, GFX.context);
		}

		if (GFX.surface)
			eglDestroySurface(GFX.display, GFX.surface);

		eglTerminate(GFX.display);
	}

	GFX.display = 0;
	GFX.surface = 0;
	GFX.context = 0;

	ctx->back_buffer = 0;
}

static bool window_check(MTY_Window *ctx)
{
	bool r = true;

	if (!GFX.ready)
		return false;

	if (GFX.ready && GFX.init)
		goto except;

	window_destroy_members(ctx);

	GFX.display = eglGetDisplay(0);
	eglInitialize(GFX.display, NULL, NULL);

	const EGLint attribs[] = {
		EGL_SURFACE_TYPE, EGL_WINDOW_BIT,
		EGL_BLUE_SIZE, 8,
		EGL_GREEN_SIZE, 8,
		EGL_RED_SIZE, 8,
		EGL_NONE
	};

	EGLint n;
	EGLConfig config;
	eglChooseConfig(GFX.display, attribs, &config, 1, &n);

	GFX.surface = eglCreateWindowSurface(GFX.display, config, GFX.window, NULL);

	const EGLint attrib[] = {EGL_CONTEXT_CLIENT_VERSION, 3, EGL_NONE};
	GFX.context = eglCreateContext(GFX.display, config, 0, attrib);

	eglMakeCurrent(GFX.display, GFX.surface, GFX.surface, GFX.context);

	MTY_RendererCreate(&ctx->renderer);

	GFX.init = true;

	except:

	if (!r)
		window_destroy_members(ctx);

	return r;
}

bool MTY_WindowCreate(const char *title, MTY_WindowMsgFunc msg_func, const void *opaque,
	uint32_t width, uint32_t height, bool fullscreen, MTY_Window **window)
{
	MTY_Window *ctx = *window = MTY_Alloc(1, sizeof(MTY_Window));
	ctx->msg_func = msg_func;
	ctx->opaque = opaque;

	return true;
}

void MTY_AppRun(MTY_AppFunc func, const void *opaque)
{
	while (func((void *) opaque));
}

void MTY_WindowSetTitle(MTY_Window *ctx, const char *title, const char *subtitle)
{
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
	return 2.0f;
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
	MTY_MutexLock(GFX.mutex);

	MTY_WindowGetBackBuffer(ctx);

	if (window_check(ctx)) {
		gfx_gl_rtv_blit_to_back_buffer(&ctx->rtv);
		ctx->back_buffer = 0;

		eglSwapBuffers(GFX.display, GFX.surface);
	}

	MTY_MutexUnlock(GFX.mutex);
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
	if (GFX.ready && GFX.init) {
		if (!ctx->back_buffer) {
			gfx_gl_rtv_refresh(&ctx->rtv, GL_RGBA8, GL_RGBA, GFX.w, GFX.h);
			ctx->back_buffer = ctx->rtv.texture;
		}

	} else {
		ctx->back_buffer = 0;
	}

	return (MTY_Texture *) (size_t) ctx->back_buffer;
}

void MTY_WindowDrawQuad(MTY_Window *ctx, const void *image, const MTY_RenderDesc *desc)
{
	if (GFX.ready && GFX.init) {
		MTY_WindowGetBackBuffer(ctx);
		MTY_RendererDrawQuad(ctx->renderer, MTY_GFX_GL, NULL, NULL, image, desc,
			(MTY_Texture *) (uintptr_t) ctx->back_buffer);
	}
}

void MTY_WindowDestroy(MTY_Window **window)
{
	if (!window || !*window)
		return;

	MTY_Window *ctx = *window;

	MTY_MutexLock(GFX.mutex);

	window_destroy_members(ctx);

	MTY_MutexUnlock(GFX.mutex);

	MTY_Free(ctx);
	*window = NULL;
}
