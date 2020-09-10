// Copyright (c) 2020 Christopher D. Dickson <cdd@matoya.group>
//
// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at https://mozilla.org/MPL/2.0/.

#include "matoya.h"

#include <stdio.h>

#define COBJMACROS
#include <windows.h>
#include <windowsx.h>
#include <shellapi.h>
#include <shellscalingapi.h>
#include <d3d11.h>
#include <dxgi1_3.h>

#include "mty-tls.h"

#define WINDOW_CLASS_NAME L"MTY_Window"
#define WINDOW_SWAP_CHAIN_FLAGS DXGI_SWAP_CHAIN_FLAG_FRAME_LATENCY_WAITABLE_OBJECT

struct MTY_Window {
	HWND hwnd;
	ATOM class;
	HINSTANCE instance;
	MTY_MsgFunc msg_func;
	const void *opaque;

	RECT clip;
	bool relative;

	uint32_t width;
	uint32_t height;
	ID3D11Device *device;
	ID3D11DeviceContext *context;
	ID3D11Texture2D *back_buffer;
	IDXGISwapChain2 *swap_chain2;
	HANDLE waitable;

	MTY_Renderer *renderer;
};

static HRESULT (WINAPI *_GetDpiForMonitor)(HMONITOR hmonitor, MONITOR_DPI_TYPE dpiType, UINT *dpiX, UINT *dpiY);

static void window_show_cursor(bool show)
{
	CURSORINFO ci = {0};
	ci.cbSize = sizeof(CURSORINFO);
	GetCursorInfo(&ci);

	if (show && !(ci.flags & CURSOR_SHOWING)) {
		ShowCursor(TRUE);

	} else if (!show && (ci.flags & CURSOR_SHOWING)) {
		ShowCursor(FALSE);
	}
}

static LRESULT CALLBACK window_proc(HWND hwnd, UINT msg, WPARAM wparam, LPARAM lparam)
{
	MTY_Window *ctx = (MTY_Window *) GetWindowLongPtr(hwnd, 0);

	MTY_Msg wmsg = {0};
	char drop_name[MTY_PATH_MAX];

	switch (msg) {
		case WM_NCCREATE:
			SetWindowLongPtr(hwnd, 0, (LONG_PTR) ((CREATESTRUCT *) lparam)->lpCreateParams);
			break;
		case WM_CLOSE:
			wmsg.type = MTY_WINDOW_MSG_CLOSE;
			break;
		case WM_DESTROY:
			PostQuitMessage(0);
			break;
		case WM_SETFOCUS:
			if (ctx && ctx->relative)
				ClipCursor(&ctx->clip);
			break;
		case WM_KEYUP:
		case WM_KEYDOWN:
		case WM_SYSKEYUP:
		case WM_SYSKEYDOWN:
			if (ctx && !ctx->relative)
				window_show_cursor(false);

			wmsg.type = MTY_WINDOW_MSG_KEYBOARD;
			wmsg.keyboard.pressed = !(lparam >> 31);
			wmsg.keyboard.scancode = lparam >> 16 & 0x00FF;
			if (lparam >> 24 & 0x01)
				wmsg.keyboard.scancode |= 0x0100;
			break;
		case WM_MOUSEMOVE:
			if (ctx && !ctx->relative)
				window_show_cursor(true);

			wmsg.type = MTY_WINDOW_MSG_MOUSE_MOTION;
			wmsg.mouseMotion.relative = false;
			wmsg.mouseMotion.x = GET_X_LPARAM(lparam);
			wmsg.mouseMotion.y = GET_Y_LPARAM(lparam);
			break;
		case WM_LBUTTONDOWN:
		case WM_LBUTTONUP:
			wmsg.type = MTY_WINDOW_MSG_MOUSE_BUTTON;
			wmsg.mouseButton.button = MTY_MOUSE_BUTTON_L;
			wmsg.mouseButton.pressed = msg == WM_LBUTTONDOWN;
			break;
		case WM_RBUTTONDOWN:
		case WM_RBUTTONUP:
			wmsg.type = MTY_WINDOW_MSG_MOUSE_BUTTON;
			wmsg.mouseButton.button = MTY_MOUSE_BUTTON_R;
			wmsg.mouseButton.pressed = msg == WM_RBUTTONDOWN;
			break;
		case WM_MBUTTONDOWN:
		case WM_MBUTTONUP:
			wmsg.type = MTY_WINDOW_MSG_MOUSE_BUTTON;
			wmsg.mouseButton.button = MTY_MOUSE_BUTTON_MIDDLE;
			wmsg.mouseButton.pressed = msg == WM_MBUTTONDOWN;
			break;
		case WM_MOUSEWHEEL:
			wmsg.type = MTY_WINDOW_MSG_MOUSE_WHEEL;
			wmsg.mouseWheel.x = 0;
			wmsg.mouseWheel.y = GET_WHEEL_DELTA_WPARAM(wparam);
			break;
		case WM_DROPFILES:
			wchar_t namew[MTY_PATH_MAX];

			if (DragQueryFile((HDROP) wparam, 0, namew, MTY_PATH_MAX)) {
				SetForegroundWindow(hwnd);

				MTY_WideToMulti(namew, drop_name, MTY_PATH_MAX);
				wmsg.drop.name = drop_name;
				wmsg.drop.data = MTY_ReadFile(drop_name, &wmsg.drop.size);

				if (wmsg.drop.data)
					wmsg.type = MTY_WINDOW_MSG_DROP;
			}
			break;
		case WM_INPUT:
			wmsg.type = MTY_WINDOW_MSG_MOUSE_MOTION;
			wmsg.mouseMotion.relative = true;

			RAWINPUT raw = {0};
			UINT size = sizeof(RAWINPUT);
			GetRawInputData((HRAWINPUT) lparam, RID_INPUT, &raw, &size, sizeof(RAWINPUTHEADER));

			if (raw.header.dwType == RIM_TYPEMOUSE) {
				wmsg.mouseMotion.x = raw.data.mouse.lLastX;
				wmsg.mouseMotion.y = raw.data.mouse.lLastY;
			}
			break;
	}

	if (wmsg.type != MTY_WINDOW_MSG_NONE) {
		if (ctx && (wmsg.type != MTY_WINDOW_MSG_MOUSE_MOTION || wmsg.mouseMotion.relative == ctx->relative))
			ctx->msg_func(&wmsg, (void *) ctx->opaque);

		if (wmsg.type == MTY_WINDOW_MSG_DROP)
			MTY_Free((void *) wmsg.drop.data);

		return 0;
	}

	return DefWindowProc(hwnd, msg, wparam, lparam);
}

static void window_calc_client_area(uint32_t *width, uint32_t *height)
{
	RECT rect = {0};
	rect.right = *width;
	rect.bottom = *height;
	if (AdjustWindowRectEx(&rect, WS_OVERLAPPEDWINDOW, FALSE, 0)) {
		*width = rect.right - rect.left;
		*height = rect.bottom - rect.top;
	}
}

MTY_Window *MTY_WindowCreate(const char *title, MTY_MsgFunc msg_func, const void *opaque,
	uint32_t width, uint32_t height, bool fullscreen)
{
	bool r = true;

	MTY_Window *ctx = MTY_Alloc(1, sizeof(MTY_Window));
	ctx->msg_func = msg_func;
	ctx->opaque = opaque;

	IDXGIDevice2 *device2 = NULL;
	IUnknown *unknown = NULL;
	IDXGIAdapter *adapter = NULL;
	IDXGIFactory2 *factory2 = NULL;
	IDXGISwapChain1 *swap_chain1 = NULL;

	ctx->instance = GetModuleHandle(NULL);
	if (!ctx->instance) {r = false; goto except;}

	WNDCLASSEX wc = {0};
	wc.cbSize = sizeof(WNDCLASSEX);
	wc.cbWndExtra = sizeof(MTY_Window *);
	wc.lpfnWndProc = window_proc;
	wc.hInstance = ctx->instance;
	wc.hCursor = LoadCursor(NULL, IDC_ARROW);
	wc.lpszClassName = WINDOW_CLASS_NAME;

	wchar_t path[MTY_PATH_MAX];
	GetModuleFileName(ctx->instance, path, MTY_PATH_MAX);
	ExtractIconEx(path, 0, &wc.hIcon, &wc.hIconSm, 1);

	ctx->class = RegisterClassEx(&wc);
	if (ctx->class == 0) {r = false; goto except;}

	RECT rect = {0};
	DWORD style = WS_OVERLAPPEDWINDOW;
	HWND desktop = GetDesktopWindow();
	int32_t x = CW_USEDEFAULT;
	int32_t y = CW_USEDEFAULT;

	if (fullscreen) {
		style = WS_POPUP;

		if (desktop && GetWindowRect(desktop, &rect)) {
			x = rect.left;
			y = rect.top;
			width = rect.right - rect.left;
			height = rect.bottom - rect.top;
		}
	} else {
		window_calc_client_area(&width, &height);

		if (desktop && GetWindowRect(desktop, &rect)) {
			x = (rect.right - width) / 2;
			y = (rect.bottom - height) / 2;
		}
	}

	wchar_t titlew[MTY_TITLE_MAX];
	MTY_MultiToWide(title, titlew, MTY_TITLE_MAX);

	ctx->hwnd = CreateWindowEx(0, WINDOW_CLASS_NAME, titlew, WS_VISIBLE | style,
		x, y, width, height, NULL, NULL, ctx->instance, ctx);
	if (!ctx->hwnd) {r = false; goto except;}

	DragAcceptFiles(ctx->hwnd, TRUE);

	RAWINPUTDEVICE rid = {0};
	rid.usUsagePage = 0x01;
	rid.usUsage = 0x02;
	rid.hwndTarget = ctx->hwnd;
	RegisterRawInputDevices(&rid, 1, sizeof(RAWINPUTDEVICE));

	_GetDpiForMonitor = (void *) GetProcAddress(GetModuleHandleA("shcore.dll"), "GetDpiForMonitor");

	DXGI_SWAP_CHAIN_DESC1 sd = {0};
	sd.Format = DXGI_FORMAT_B8G8R8A8_UNORM;
	sd.BufferUsage = DXGI_USAGE_RENDER_TARGET_OUTPUT;
	sd.SwapEffect = DXGI_SWAP_EFFECT_FLIP_DISCARD;
	sd.SampleDesc.Count = 1;
	sd.BufferCount = 2;
	sd.Flags = WINDOW_SWAP_CHAIN_FLAGS;

	D3D_FEATURE_LEVEL levels[] = {D3D_FEATURE_LEVEL_11_1, D3D_FEATURE_LEVEL_11_0};
	UINT flags = D3D11_CREATE_DEVICE_SINGLETHREADED;
	HRESULT e = D3D11CreateDevice(NULL, D3D_DRIVER_TYPE_HARDWARE, NULL, flags, levels,
		sizeof(levels) / sizeof(D3D_FEATURE_LEVEL), D3D11_SDK_VERSION, &ctx->device, NULL, &ctx->context);
	if (e != S_OK) {r = false; goto except;}

	e = ID3D11Device_QueryInterface(ctx->device, &IID_IDXGIDevice2, &device2);
	if (e != S_OK) {r = false; goto except;}

	e = ID3D11Device_QueryInterface(ctx->device, &IID_IUnknown, &unknown);
	if (e != S_OK) {r = false; goto except;}

	e = IDXGIDevice2_GetParent(device2, &IID_IDXGIAdapter, &adapter);
	if (e != S_OK) {r = false; goto except;}

	e = IDXGIAdapter_GetParent(adapter, &IID_IDXGIFactory2, &factory2);
	if (e != S_OK) {r = false; goto except;}

	e = IDXGIFactory2_CreateSwapChainForHwnd(factory2, unknown, ctx->hwnd, &sd, NULL, NULL, &swap_chain1);
	if (e != S_OK) {r = false; goto except;}

	e = IDXGISwapChain1_QueryInterface(swap_chain1, &IID_IDXGISwapChain2, &ctx->swap_chain2);
	if (e != S_OK) {r = false; goto except;}

	ctx->waitable = IDXGISwapChain2_GetFrameLatencyWaitableObject(ctx->swap_chain2);
	if (!ctx->waitable) {r = false; goto except;}

	e = IDXGISwapChain2_SetMaximumFrameLatency(ctx->swap_chain2, 1);
	if (e != S_OK) {r = false; goto except;}

	e = IDXGIFactory2_MakeWindowAssociation(factory2, ctx->hwnd, DXGI_MWA_NO_WINDOW_CHANGES);
	if (e != S_OK) {r = false; goto except;}

	ctx->renderer = MTY_RendererCreate();

	except:

	if (swap_chain1)
		IDXGISwapChain1_Release(swap_chain1);

	if (factory2)
		IDXGIFactory2_Release(factory2);

	if (adapter)
		IDXGIAdapter_Release(adapter);

	if (unknown)
		IUnknown_Release(unknown);

	if (device2)
		IDXGIDevice2_Release(device2);

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
	wchar_t titlew[MTY_TITLE_MAX];
	MTY_MultiToWide(title, titlew, MTY_TITLE_MAX);

	wchar_t full[MTY_TITLE_MAX];

	if (subtitle) {
		wchar_t subtitlew[MTY_TITLE_MAX];
		MTY_MultiToWide(subtitle, subtitlew, MTY_TITLE_MAX);

		_snwprintf_s(full, MTY_TITLE_MAX, _TRUNCATE, L"%s - %s", titlew, subtitlew);

	} else {
		_snwprintf_s(full, MTY_TITLE_MAX, _TRUNCATE, L"%s", titlew);
	}

	SetWindowText(ctx->hwnd, full);
}

bool MTY_WindowGetSize(MTY_Window *ctx, uint32_t *width, uint32_t *height)
{
	RECT rect = {0};
	if (GetClientRect(ctx->hwnd, &rect)) {
		*width = rect.right - rect.left;
		*height = rect.bottom - rect.top;
		return true;
	}

	return false;
}

void MTY_WindowPoll(MTY_Window *ctx)
{
	for (MSG msg; PeekMessage(&msg, ctx->hwnd, 0, 0, PM_REMOVE | PM_NOYIELD);) {
		TranslateMessage(&msg);
		DispatchMessage(&msg);
	}

	uint32_t width = 0;
	uint32_t height = 0;
	MTY_WindowGetSize(ctx, &width, &height);

	if (width != ctx->width || height != ctx->height) {
		IDXGISwapChain2_ResizeBuffers(ctx->swap_chain2, 0, 0, 0,
			DXGI_FORMAT_UNKNOWN, WINDOW_SWAP_CHAIN_FLAGS);

		ctx->width = width;
		ctx->height = height;
	}
}

static bool window_get_monitor_info(HWND hwnd, MONITORINFOEX *info)
{
	HMONITOR mon = MonitorFromWindow(hwnd, MONITOR_DEFAULTTONEAREST);

	if (mon) {
		memset(info, 0, sizeof(MONITORINFOEX));
		info->cbSize = sizeof(MONITORINFOEX);

		return GetMonitorInfo(mon, (LPMONITORINFO) info);
	}

	return false;
}

uint32_t MTY_WindowGetRefreshRate(MTY_Window *ctx)
{
	MONITORINFOEX info = {0};
	if (window_get_monitor_info(ctx->hwnd, &info)) {
		DEVMODE mode = {0};
		mode.dmSize = sizeof(DEVMODE);

		if (EnumDisplaySettings(info.szDevice, ENUM_CURRENT_SETTINGS, &mode))
			return mode.dmDisplayFrequency;
	}

	return 60;
}

float MTY_WindowGetDPIScale(MTY_Window *ctx)
{
	if (_GetDpiForMonitor) {
		HMONITOR mon = MonitorFromWindow(ctx->hwnd, MONITOR_DEFAULTTONEAREST);

		if (mon) {
			UINT x = 0;
			UINT y = 0;

			if (_GetDpiForMonitor(mon, MDT_EFFECTIVE_DPI, &x, &y) == S_OK)
				return (float) x / 96.0f;
		}
	}

	return 1.0f;
}

void MTY_WindowSetFullscreen(MTY_Window *ctx)
{
	MONITORINFOEX info = {0};
	if (window_get_monitor_info(ctx->hwnd, &info)) {
		uint32_t x = info.rcMonitor.left;
		uint32_t y = info.rcMonitor.top;
		uint32_t w = info.rcMonitor.right - info.rcMonitor.left;
		uint32_t h = info.rcMonitor.bottom - info.rcMonitor.top;

		SetWindowLongPtr(ctx->hwnd, GWL_STYLE, WS_VISIBLE | WS_POPUP);
		SetWindowPos(ctx->hwnd, HWND_TOP, x, y, w, h, SWP_FRAMECHANGED);
	}
}

void MTY_WindowSetSize(MTY_Window *ctx, uint32_t width, uint32_t height)
{
	window_calc_client_area(&width, &height);

	int32_t x = CW_USEDEFAULT;
	int32_t y = CW_USEDEFAULT;

	MONITORINFOEX info = {0};
	if (window_get_monitor_info(ctx->hwnd, &info)) {
		x = (info.rcMonitor.right - width) / 2;
		y = (info.rcMonitor.bottom - height) / 2;
	}

	SetWindowLongPtr(ctx->hwnd, GWL_STYLE, WS_VISIBLE | WS_OVERLAPPEDWINDOW);
	SetWindowPos(ctx->hwnd, NULL, x, y, width, height, SWP_FRAMECHANGED);

	PostMessage(ctx->hwnd, WM_SETICON, ICON_BIG, GetClassLongPtr(ctx->hwnd, GCLP_HICON));
	PostMessage(ctx->hwnd, WM_SETICON, ICON_SMALL, GetClassLongPtr(ctx->hwnd, GCLP_HICONSM));
}

bool MTY_WindowIsFullscreen(MTY_Window *ctx)
{
	return GetWindowLongPtr(ctx->hwnd, GWL_STYLE) & WS_POPUP;
}

void MTY_WindowSetRelativeMouse(MTY_Window *ctx, bool relative)
{
	if (relative && !ctx->relative) {
		ctx->relative = true;

		POINT p = {0};
		GetCursorPos(&p);
		ctx->clip.left = p.x;
		ctx->clip.right = p.x + 1;
		ctx->clip.top = p.y;
		ctx->clip.bottom = p.y + 1;
		ClipCursor(&ctx->clip);
		window_show_cursor(false);

	} else if (!relative && ctx->relative) {
		ctx->relative = false;

		ClipCursor(NULL);
		window_show_cursor(true);
	}
}

bool MTY_WindowIsForeground(MTY_Window *ctx)
{
	return GetForegroundWindow() == ctx->hwnd;
}

void MTY_WindowPresent(MTY_Window *ctx, uint32_t num_frames)
{
	if (WaitForSingleObjectEx(ctx->waitable, INFINITE, TRUE) == WAIT_OBJECT_0)
		IDXGISwapChain2_Present(ctx->swap_chain2, num_frames, 0);

	if (ctx->back_buffer)
		ID3D11Texture2D_Release(ctx->back_buffer);

	ctx->back_buffer = NULL;
}

MTY_Device *MTY_WindowGetDevice(MTY_Window *ctx)
{
	return (MTY_Device *) ctx->device;
}

MTY_Context *MTY_WindowGetContext(MTY_Window *ctx)
{
	return (MTY_Context *) ctx->context;
}

MTY_Texture *MTY_WindowGetBackBuffer(MTY_Window *ctx)
{
	if (!ctx->back_buffer)
		IDXGISwapChain2_GetBuffer(ctx->swap_chain2, 0, &IID_ID3D11Texture2D, &ctx->back_buffer);

	return (MTY_Texture *) ctx->back_buffer;
}

void MTY_WindowDrawQuad(MTY_Window *ctx, const void *image, const MTY_RenderDesc *desc)
{
	if (MTY_WindowGetBackBuffer(ctx)) {
		MTY_RenderDesc mutated = *desc;
		mutated.viewWidth = ctx->width;
		mutated.viewHeight = ctx->height;

		MTY_RendererDrawQuad(ctx->renderer, MTY_GFX_D3D11, (MTY_Device *) ctx->device,
			(MTY_Context *) ctx->context, image, &mutated, (MTY_Texture *) ctx->back_buffer);
	}
}

void MTY_WindowDestroy(MTY_Window **window)
{
	if (!window || !*window)
		return;

	MTY_Window *ctx = *window;

	MTY_RendererDestroy(&ctx->renderer);

	if (ctx->back_buffer)
		ID3D11Texture2D_Release(ctx->back_buffer);

	if (ctx->waitable)
		CloseHandle(ctx->waitable);

	if (ctx->swap_chain2)
		IDXGISwapChain2_Release(ctx->swap_chain2);

	if (ctx->context)
		ID3D11DeviceContext_Release(ctx->context);

	if (ctx->device)
		ID3D11Device_Release(ctx->device);

	if (ctx->hwnd)
		DestroyWindow(ctx->hwnd);

	if (ctx->instance && ctx->class != 0)
		UnregisterClass(WINDOW_CLASS_NAME, ctx->instance);

	MTY_Free(ctx);
	*window = NULL;
}

void *MTY_GLGetProcAddress(const char *name)
{
	void *p = wglGetProcAddress(name);

	if (!p)
		p = GetProcAddress(GetModuleHandleA("opengl32.dll"), name);

	return p;
}
