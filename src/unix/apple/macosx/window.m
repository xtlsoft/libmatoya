// Copyright (c) 2020 Christopher D. Dickson <cdd@matoya.group>
//
// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at https://mozilla.org/MPL/2.0/.

#include "matoya.h"

#include <AppKit/AppKit.h>
#include <Carbon/Carbon.h>
#include <Metal/Metal.h>
#include <QuartzCore/CAMetalLayer.h>

@interface MTYWindow : NSWindow
	- (BOOL)windowShouldClose:(NSWindow *)sender;
@end

@implementation MTYWindow : NSWindow
	bool _closed = false;

	- (BOOL)windowShouldClose:(NSWindow *)sender
	{
		_closed = true;
		return NO;
	}

	- (bool)wasClosed
	{
		bool closed = _closed;
		_closed = false;

		return closed;
	}
@end

struct MTY_Window {
	MTYWindow *nswindow;
	MTY_MsgFunc msg_func;
	const void *opaque;

	bool relative;

	CAMetalLayer *layer;
	CVDisplayLinkRef display_link;
	dispatch_semaphore_t semaphore;
	id<CAMetalDrawable> back_buffer;
	id<MTLCommandQueue> cq;

	MTY_Renderer *renderer;
};

static CVReturn window_display_link(CVDisplayLinkRef displayLink, const CVTimeStamp *inNow,
	const CVTimeStamp *inOutputTime, CVOptionFlags flagsIn, CVOptionFlags *flagsOut, void *displayLinkContext)
{
	MTY_Window *ctx = (MTY_Window *) displayLinkContext;

	dispatch_semaphore_signal(ctx->semaphore);

	return 0;
}

MTY_Window *MTY_WindowCreate(const char *title, MTY_MsgFunc msg_func, const void *opaque,
	uint32_t width, uint32_t height, bool fullscreen)
{
	bool r = true;
	MTY_Window *ctx = MTY_Alloc(1, sizeof(MTY_Window));
	ctx->msg_func = msg_func;
	ctx->opaque = opaque;

	[NSApplication sharedApplication];
	[NSApp setActivationPolicy:NSApplicationActivationPolicyRegular];
	[NSApp finishLaunching];

	NSRect rect = NSMakeRect(0, 0, width, height);
	NSWindowStyleMask style = NSWindowStyleMaskTitled | NSWindowStyleMaskClosable | NSWindowStyleMaskResizable;
	ctx->nswindow = [[MTYWindow alloc] initWithContentRect:rect styleMask:style
		backing:NSBackingStoreBuffered defer:NO];

	ctx->nswindow.title = [NSString stringWithUTF8String:title];
	[ctx->nswindow center];

	[ctx->nswindow makeKeyAndOrderFront:ctx->nswindow];

	ctx->layer = [CAMetalLayer layer];
	ctx->layer.device = MTLCreateSystemDefaultDevice();
	ctx->layer.pixelFormat = MTLPixelFormatBGRA8Unorm;

	ctx->cq = [ctx->layer.device newCommandQueue];

	ctx->nswindow.contentView.wantsLayer = YES;
	ctx->nswindow.contentView.layer = ctx->layer;

	CVReturn e = CVDisplayLinkCreateWithCGDisplay(CGMainDisplayID(), &ctx->display_link);
	if (e != 0) {r = false; goto except;}

	ctx->semaphore = dispatch_semaphore_create(0);

	CVDisplayLinkSetOutputCallback(ctx->display_link, window_display_link, ctx);
	CVDisplayLinkStart(ctx->display_link);

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

	NSString *nss = [NSString stringWithUTF8String:ctitle];
	ctx->nswindow.title = nss;
}

bool MTY_WindowGetSize(MTY_Window *ctx, uint32_t *width, uint32_t *height)
{
	CGSize size = ctx->nswindow.contentView.frame.size;
	CGFloat scale = ctx->nswindow.screen.backingScaleFactor;

	*width = lrint(size.width * scale);
	*height = lrint(size.height * scale);

	return true;
}

static MTY_Scancode window_keycode_to_wmsg(unsigned short kc)
{
	switch (kc) {
		case kVK_ANSI_A: return MTY_SCANCODE_A;
		case kVK_ANSI_S: return MTY_SCANCODE_S;
		case kVK_ANSI_D: return MTY_SCANCODE_D;
		case kVK_ANSI_F: return MTY_SCANCODE_NONE;
		case kVK_ANSI_H: return MTY_SCANCODE_NONE;
		case kVK_ANSI_G: return MTY_SCANCODE_NONE;
		case kVK_ANSI_Z: return MTY_SCANCODE_NONE;
		case kVK_ANSI_X: return MTY_SCANCODE_NONE;
		case kVK_ANSI_C: return MTY_SCANCODE_NONE;
		case kVK_ANSI_V: return MTY_SCANCODE_NONE;
		case kVK_ANSI_B: return MTY_SCANCODE_NONE;
		case kVK_ANSI_Q: return MTY_SCANCODE_NONE;
		case kVK_ANSI_W: return MTY_SCANCODE_W;
		case kVK_ANSI_E: return MTY_SCANCODE_NONE;
		case kVK_ANSI_R: return MTY_SCANCODE_R;
		case kVK_ANSI_Y: return MTY_SCANCODE_NONE;
		case kVK_ANSI_T: return MTY_SCANCODE_T;
		case kVK_ANSI_1: return MTY_SCANCODE_NONE;
		case kVK_ANSI_2: return MTY_SCANCODE_NONE;
		case kVK_ANSI_3: return MTY_SCANCODE_NONE;
		case kVK_ANSI_4: return MTY_SCANCODE_NONE;
		case kVK_ANSI_6: return MTY_SCANCODE_NONE;
		case kVK_ANSI_5: return MTY_SCANCODE_NONE;
		case kVK_ANSI_Equal: return MTY_SCANCODE_NONE;
		case kVK_ANSI_9: return MTY_SCANCODE_NONE;
		case kVK_ANSI_7: return MTY_SCANCODE_NONE;
		case kVK_ANSI_Minus: return MTY_SCANCODE_NONE;
		case kVK_ANSI_8: return MTY_SCANCODE_NONE;
		case kVK_ANSI_0: return MTY_SCANCODE_NONE;
		case kVK_ANSI_RightBracket: return MTY_SCANCODE_NONE;
		case kVK_ANSI_O: return MTY_SCANCODE_O;
		case kVK_ANSI_U: return MTY_SCANCODE_NONE;
		case kVK_ANSI_LeftBracket: return MTY_SCANCODE_NONE;
		case kVK_ANSI_I: return MTY_SCANCODE_NONE;
		case kVK_ANSI_P: return MTY_SCANCODE_NONE;
		case kVK_ANSI_L: return MTY_SCANCODE_L;
		case kVK_ANSI_J: return MTY_SCANCODE_NONE;
		case kVK_ANSI_Quote: return MTY_SCANCODE_NONE;
		case kVK_ANSI_K: return MTY_SCANCODE_NONE;
		case kVK_ANSI_Semicolon: return MTY_SCANCODE_SEMICOLON;
		case kVK_ANSI_Backslash: return MTY_SCANCODE_NONE;
		case kVK_ANSI_Comma: return MTY_SCANCODE_NONE;
		case kVK_ANSI_Slash: return MTY_SCANCODE_NONE;
		case kVK_ANSI_N: return MTY_SCANCODE_NONE;
		case kVK_ANSI_M: return MTY_SCANCODE_NONE;
		case kVK_ANSI_Period: return MTY_SCANCODE_NONE;
		case kVK_ANSI_Grave: return MTY_SCANCODE_NONE;
		case kVK_ANSI_KeypadDecimal: return MTY_SCANCODE_NONE;
		case kVK_ANSI_KeypadMultiply: return MTY_SCANCODE_NONE;
		case kVK_ANSI_KeypadPlus: return MTY_SCANCODE_NONE;
		case kVK_ANSI_KeypadClear: return MTY_SCANCODE_NONE;
		case kVK_ANSI_KeypadDivide: return MTY_SCANCODE_NONE;
		case kVK_ANSI_KeypadEnter: return MTY_SCANCODE_NONE;
		case kVK_ANSI_KeypadMinus: return MTY_SCANCODE_NONE;
		case kVK_ANSI_KeypadEquals: return MTY_SCANCODE_NONE;
		case kVK_ANSI_Keypad0: return MTY_SCANCODE_NONE;
		case kVK_ANSI_Keypad1: return MTY_SCANCODE_NONE;
		case kVK_ANSI_Keypad2: return MTY_SCANCODE_NONE;
		case kVK_ANSI_Keypad3: return MTY_SCANCODE_NONE;
		case kVK_ANSI_Keypad4: return MTY_SCANCODE_NONE;
		case kVK_ANSI_Keypad5: return MTY_SCANCODE_NONE;
		case kVK_ANSI_Keypad6: return MTY_SCANCODE_NONE;
		case kVK_ANSI_Keypad7: return MTY_SCANCODE_NONE;
		case kVK_ANSI_Keypad8: return MTY_SCANCODE_NONE;
		case kVK_ANSI_Keypad9: return MTY_SCANCODE_NONE;
		case kVK_Return: return MTY_SCANCODE_ENTER;
		case kVK_Tab: return MTY_SCANCODE_NONE;
		case kVK_Space: return MTY_SCANCODE_SPACE;
		case kVK_Delete: return MTY_SCANCODE_NONE;
		case kVK_Escape: return MTY_SCANCODE_ESCAPE;
		case kVK_Command: return MTY_SCANCODE_NONE;
		case kVK_Shift: return MTY_SCANCODE_LSHIFT;
		case kVK_CapsLock: return MTY_SCANCODE_NONE;
		case kVK_Option: return MTY_SCANCODE_NONE;
		case kVK_Control: return MTY_SCANCODE_LCTRL;
		case kVK_RightShift: return MTY_SCANCODE_RSHIFT;
		case kVK_RightOption: return MTY_SCANCODE_NONE;
		case kVK_RightControl: return MTY_SCANCODE_RCTRL;
		case kVK_Function: return MTY_SCANCODE_NONE;
		case kVK_F17: return MTY_SCANCODE_NONE;
		case kVK_VolumeUp: return MTY_SCANCODE_NONE;
		case kVK_VolumeDown: return MTY_SCANCODE_NONE;
		case kVK_Mute: return MTY_SCANCODE_NONE;
		case kVK_F18: return MTY_SCANCODE_NONE;
		case kVK_F19: return MTY_SCANCODE_NONE;
		case kVK_F20: return MTY_SCANCODE_NONE;
		case kVK_F5: return MTY_SCANCODE_NONE;
		case kVK_F6: return MTY_SCANCODE_NONE;
		case kVK_F7: return MTY_SCANCODE_NONE;
		case kVK_F3: return MTY_SCANCODE_NONE;
		case kVK_F8: return MTY_SCANCODE_NONE;
		case kVK_F9: return MTY_SCANCODE_NONE;
		case kVK_F11: return MTY_SCANCODE_NONE;
		case kVK_F13: return MTY_SCANCODE_NONE;
		case kVK_F16: return MTY_SCANCODE_NONE;
		case kVK_F14: return MTY_SCANCODE_NONE;
		case kVK_F10: return MTY_SCANCODE_NONE;
		case kVK_F12: return MTY_SCANCODE_NONE;
		case kVK_F15: return MTY_SCANCODE_NONE;
		case kVK_Help: return MTY_SCANCODE_NONE;
		case kVK_Home: return MTY_SCANCODE_NONE;
		case kVK_PageUp: return MTY_SCANCODE_NONE;
		case kVK_ForwardDelete: return MTY_SCANCODE_NONE;
		case kVK_F4: return MTY_SCANCODE_NONE;
		case kVK_End: return MTY_SCANCODE_NONE;
		case kVK_F2: return MTY_SCANCODE_NONE;
		case kVK_PageDown: return MTY_SCANCODE_NONE;
		case kVK_F1: return MTY_SCANCODE_NONE;
		case kVK_LeftArrow: return MTY_SCANCODE_LEFT;
		case kVK_RightArrow: return MTY_SCANCODE_RIGHT;
		case kVK_DownArrow: return MTY_SCANCODE_DOWN;
		case kVK_UpArrow: return MTY_SCANCODE_UP;
		case kVK_ISO_Section: return MTY_SCANCODE_NONE;
		case kVK_JIS_Yen: return MTY_SCANCODE_NONE;
		case kVK_JIS_Underscore: return MTY_SCANCODE_NONE;
		case kVK_JIS_KeypadComma: return MTY_SCANCODE_NONE;
		case kVK_JIS_Eisu: return MTY_SCANCODE_NONE;
		case kVK_JIS_Kana: return MTY_SCANCODE_NONE;
		default:
			break;
	}

	return MTY_SCANCODE_NONE;
}

void MTY_WindowPoll(MTY_Window *ctx)
{
	CGSize size = ctx->nswindow.contentView.frame.size;
	uint32_t scale = lrint(ctx->nswindow.screen.backingScaleFactor);

	while (true) {
		NSEvent *event = [NSApp nextEventMatchingMask:NSEventMaskAny untilDate:nil inMode:NSDefaultRunLoopMode dequeue:YES];
		if (!event)
			break;

		bool block_app = false;
		MTY_Msg wmsg = {0};

		switch (event.type) {
			case NSEventTypeKeyUp:
			case NSEventTypeKeyDown:
			case NSEventTypeFlagsChanged: {
				wmsg.keyboard.scancode = window_keycode_to_wmsg(event.keyCode);

				if (wmsg.keyboard.scancode != MTY_SCANCODE_NONE) {
					block_app = true;
					wmsg.type = MTY_WINDOW_MSG_KEYBOARD;

					if (event.type == NSEventTypeFlagsChanged) {
						switch (event.keyCode) {
							case kVK_Shift:         wmsg.keyboard.pressed = event.modifierFlags & NX_DEVICELSHIFTKEYMASK;       break;
							case kVK_CapsLock:      wmsg.keyboard.pressed = event.modifierFlags & NSEventModifierFlagCapsLock;  break;
							case kVK_Option:        wmsg.keyboard.pressed = event.modifierFlags & NX_DEVICELALTKEYMASK;         break;
							case kVK_Control:       wmsg.keyboard.pressed = event.modifierFlags & NX_DEVICELCTLKEYMASK;         break;
							case kVK_RightShift:    wmsg.keyboard.pressed = event.modifierFlags & NX_DEVICERSHIFTKEYMASK;       break;
							case kVK_RightOption:   wmsg.keyboard.pressed = event.modifierFlags & NX_DEVICERALTKEYMASK;         break;
							case kVK_RightControl:  wmsg.keyboard.pressed = event.modifierFlags & NX_DEVICERCTLKEYMASK;         break;
							case kVK_Command:       wmsg.keyboard.pressed = event.modifierFlags & NX_COMMANDMASK;               break;
							default:
								block_app = false;
								wmsg.type = MTY_WINDOW_MSG_NONE;
								break;
						}
					} else {
						wmsg.keyboard.pressed = event.type == NSEventTypeKeyDown;
					}
				}
				break;
			}
			case NSEventTypeScrollWheel:
				wmsg.type = MTY_WINDOW_MSG_MOUSE_WHEEL;
				wmsg.mouseWheel.x = lrint(event.deltaX);
				wmsg.mouseWheel.y = lrint(event.deltaY);
				break;
			case NSEventTypeLeftMouseDown:
			case NSEventTypeLeftMouseUp:
			case NSEventTypeRightMouseDown:
			case NSEventTypeRightMouseUp:
			case NSEventTypeOtherMouseDown:
			case NSEventTypeOtherMouseUp:
				wmsg.type = MTY_WINDOW_MSG_MOUSE_BUTTON;
				wmsg.mouseButton.pressed = event.type == NSEventTypeLeftMouseDown || event.type == NSEventTypeRightMouseDown ||
					event.type == NSEventTypeOtherMouseDown;

				switch (event.buttonNumber) {
					case 0: wmsg.mouseButton.button = MTY_MOUSE_BUTTON_L; break;
					case 1: wmsg.mouseButton.button = MTY_MOUSE_BUTTON_R; break;
				}
				break;
			case NSEventTypeLeftMouseDragged:
			case NSEventTypeRightMouseDragged:
			case NSEventTypeOtherMouseDragged:
			case NSEventTypeMouseMoved: {
				if (ctx->relative) {
					wmsg.type = MTY_WINDOW_MSG_MOUSE_MOTION;
					wmsg.mouseMotion.relative = true;
					wmsg.mouseMotion.x = event.deltaX;
					wmsg.mouseMotion.y = event.deltaY;

				} else {
					NSPoint p = [ctx->nswindow mouseLocationOutsideOfEventStream];
					int32_t x = lrint(p.x);
					int32_t y = lrint(size.height - p.y);

					if (x >= 0 && y >= 0 && x <= size.width && y <= size.height) {
						wmsg.type = MTY_WINDOW_MSG_MOUSE_MOTION;
						wmsg.mouseMotion.x = x * scale;
						wmsg.mouseMotion.y = y * scale;
					}
				}
				break;
			}
		}

		if ([ctx->nswindow wasClosed])
			wmsg.type = MTY_WINDOW_MSG_CLOSE;

		if (wmsg.type != MTY_WINDOW_MSG_NONE)
			ctx->msg_func(&wmsg, (void *) ctx->opaque);

		if (!block_app)
			[NSApp sendEvent:event];
	}

	size.width *= scale;
	size.height *= scale;

	if (size.width != ctx->layer.drawableSize.width || size.height != ctx->layer.drawableSize.height)
		ctx->layer.drawableSize = size;
}

bool MTY_WindowIsForeground(MTY_Window *ctx)
{
	return ctx->nswindow.isKeyWindow;
}

void MTY_WindowSetRelativeMouse(MTY_Window *ctx, bool relative)
{
	if (relative && !ctx->relative) {
		ctx->relative = true;
		CGAssociateMouseAndMouseCursorPosition(NO);
		CGDisplayHideCursor(kCGDirectMainDisplay);

	} else if (!relative && ctx->relative) {
		ctx->relative = false;
		CGAssociateMouseAndMouseCursorPosition(YES);
		CGDisplayShowCursor(kCGDirectMainDisplay);
	}
}

uint32_t MTY_WindowGetRefreshRate(MTY_Window *ctx)
{
	NSNumber *n = ctx->nswindow.screen.deviceDescription[@"NSScreenNumber"];
	CGDirectDisplayID display_id = [n intValue];

	CGDisplayModeRef mode = CGDisplayCopyDisplayMode(display_id);

	// FIXME This doesn't seem to be correct

	CGDisplayModeRelease(mode);

	return 60;
}

float MTY_WindowGetDPIScale(MTY_Window *ctx)
{
	// macOS scales the display as though it switches resolutions,
	// so all we need to report is the high DPI device multiplier

	return [ctx->nswindow screen].backingScaleFactor;
}

void MTY_WindowSetFullscreen(MTY_Window *ctx)
{
	if (!MTY_WindowIsFullscreen(ctx))
		[ctx->nswindow toggleFullScreen:ctx->nswindow];
}

void MTY_WindowSetSize(MTY_Window *ctx, uint32_t width, uint32_t height)
{
}

bool MTY_WindowIsFullscreen(MTY_Window *ctx)
{
	return [ctx->nswindow styleMask] & NSWindowStyleMaskFullScreen;
}

void MTY_WindowPresent(MTY_Window *ctx, uint32_t num_frames)
{
	for (uint32_t x = 0; x < num_frames; x++)
		dispatch_semaphore_wait(ctx->semaphore, DISPATCH_TIME_FOREVER);

	id<MTLCommandBuffer> cb = [ctx->cq commandBuffer];

	MTY_WindowGetBackBuffer(ctx);

	[cb presentDrawable:ctx->back_buffer];
	[cb commit];

	ctx->back_buffer = nil;
}

MTY_Device *MTY_WindowGetDevice(MTY_Window *ctx)
{
	return (__bridge MTY_Device *) ctx->cq.device;
}

MTY_Context *MTY_WindowGetContext(MTY_Window *ctx)
{
	return (__bridge MTY_Context *) ctx->cq;
}

MTY_Texture *MTY_WindowGetBackBuffer(MTY_Window *ctx)
{
	@autoreleasepool {
		if (!ctx->back_buffer)
			ctx->back_buffer = [ctx->layer nextDrawable];

		return (__bridge MTY_Texture *) ctx->back_buffer.texture;
	}
}

void MTY_WindowDrawQuad(MTY_Window *ctx, const void *image, const MTY_RenderDesc *desc)
{
	MTY_WindowGetBackBuffer(ctx);

	MTY_RenderDesc mutated = *desc;
	mutated.viewWidth = ctx->layer.drawableSize.width;
	mutated.viewHeight = ctx->layer.drawableSize.height;

	MTY_RendererDrawQuad(ctx->renderer, MTY_GFX_METAL, (__bridge MTY_Device *) ctx->cq.device,
		(__bridge MTY_Context *) ctx->cq, image, &mutated, (__bridge MTY_Texture *) ctx->back_buffer.texture);
}

void MTY_WindowDestroy(MTY_Window **window)
{
	if (!window || !*window)
		return;

	MTY_Window *ctx = *window;

	MTY_RendererDestroy(&ctx->renderer);

	ctx->nswindow = nil;
	ctx->layer = nil;
	ctx->cq = nil;
	ctx->semaphore = nil;
	ctx->back_buffer = nil;

	if (ctx->display_link) {
		if (CVDisplayLinkIsRunning(ctx->display_link))
			CVDisplayLinkStop(ctx->display_link);

		CVDisplayLinkRelease(ctx->display_link);
	}

	MTY_Free(ctx);
	*window = NULL;
}
