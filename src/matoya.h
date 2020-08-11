// Copyright (c) 2020 Christopher D. Dickson <cdd@matoya.group>
//
// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at https://mozilla.org/MPL/2.0/.

#pragma once

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>
#include <stdarg.h>

#if !defined(MTY_EXPORT)
	#define MTY_EXPORT
#endif

#ifdef __cplusplus
extern "C" {
#endif


/// @module audio

typedef struct MTY_Audio MTY_Audio;

MTY_EXPORT bool
MTY_AudioCreate(MTY_Audio **audio, uint32_t sample_rate);

MTY_EXPORT uint32_t
MTY_AudioGetQueuedFrames(MTY_Audio *ctx);

MTY_EXPORT bool
MTY_AudioIsPlaying(MTY_Audio *ctx);

MTY_EXPORT void
MTY_AudioPlay(MTY_Audio *ctx);

MTY_EXPORT void
MTY_AudioStop(MTY_Audio *ctx);

MTY_EXPORT void
MTY_AudioQueue(MTY_Audio *ctx, const int16_t *frames, uint32_t count);

MTY_EXPORT void
MTY_AudioDestroy(MTY_Audio **audio);


/// @module cmacro

#define MTY_MIN(a, b)  ((a) > (b) ? (b) : (a))
#define MTY_MAX(a, b)  ((a) > (b) ? (a) : (b))
#define MTY_ALIGN16(v) ((v) + 15 & ~((uintptr_t) 15))
#define MTY_ALIGN32(v) ((v) + 31 & ~((uintptr_t) 31))


/// @module compress

typedef enum {
	MTY_IMAGE_PNG     = 1,
	MTY_IMAGE_JPG     = 2,
	MTY_IMAGE_BMP     = 3,
	MTY_IMAGE_MAKE_32 = 0x7FFFFFFF,
} MTY_Image;

MTY_EXPORT bool
MTY_ImageCompress(const void *input, uint32_t width, uint32_t height, MTY_Image type, void **output, size_t *outputSize);

MTY_EXPORT bool
MTY_ImageDecompress(const void *input, size_t size, void **output, uint32_t *width, uint32_t *height);

MTY_EXPORT void *
MTY_ImageCrop(const void *image, uint32_t cropWidth, uint32_t cropHeight, uint32_t *width, uint32_t *height);

MTY_EXPORT bool
MTY_Compress(const void *input, size_t inputSize, void **output, size_t *outputSize);

MTY_EXPORT bool
MTY_Decompress(const void *input, size_t inputSize, void **output, size_t *outputSize);


/// @module crypto

#define MTY_CRYPTO_SHA1_SIZE       20
#define MTY_CRYPTO_SHA1_HEX_SIZE   41

#define MTY_CRYPTO_SHA256_SIZE     32
#define MTY_CRYPTO_SHA256_HEX_SIZE 65

typedef enum {
	MTY_ALGORITHM_SHA1       = 1,
	MTY_ALGORITHM_SHA1_HEX   = 2,
	MTY_ALGORITHM_SHA256     = 3,
	MTY_ALGORITHM_SHA256_HEX = 4,
	MTY_ALGORITHM_MAKE_32    = 0x7FFFFFFF,
} MTY_Algorithm;

typedef struct MTY_AESGCM MTY_AESGCM;

MTY_EXPORT uint32_t
MTY_CryptoCRC32(const void *data, size_t size);

MTY_EXPORT uint32_t
MTY_CryptoDJB2(const char *str);

MTY_EXPORT void
MTY_CryptoBytesToHex(const void *bytes, size_t size, char *hex, size_t hexSize);

MTY_EXPORT void
MTY_CryptoHexToBytes(const char *hex, void *bytes, size_t size);

MTY_EXPORT void
MTY_CryptoBytesToBase64(const void *bytes, size_t size, char *b64, size_t b64Size);

MTY_EXPORT void
MTY_CryptoHash(MTY_Algorithm algo, const void *input, size_t inputSize, const void *key,
	size_t keySize, void *output, size_t outputSize);

MTY_EXPORT bool
MTY_CryptoHashFile(MTY_Algorithm algo, const char *path, const void *key, size_t keySize,
	void *output, size_t outputSize);

MTY_EXPORT void
MTY_CryptoRandom(void *output, size_t size);

MTY_EXPORT uint32_t
MTY_CryptoRandomUInt(uint32_t minVal, uint32_t maxVal);

MTY_EXPORT bool
MTY_AESGCMCreate(const void *key, size_t keySize, MTY_AESGCM **aesgcm);

MTY_EXPORT bool
MTY_AESGCMEncrypt(MTY_AESGCM *ctx, const void *plainText, size_t textSize, const void *nonce,
	size_t nonceSize, void *hash, size_t hashSize, void *cipherText);

MTY_EXPORT bool
MTY_AESGCMDecrypt(MTY_AESGCM *ctx, const void *cipherText, size_t textSize, const void *nonce,
	size_t nonceSize, const void *hash, size_t hashSize, void *plainText);

MTY_EXPORT void
MTY_AESGCMDestroy(MTY_AESGCM **aesgcm);


/// @module fs

#define MTY_PATH_MAX 1280

typedef enum {
	MTY_DIR_CWD     = 1,
	MTY_DIR_HOME    = 2,
	MTY_DIR_PROGRAM = 3,
	MTY_DIR_MAKE_32 = 0x7FFFFFFF,
} MTY_FsDir;

typedef struct {
	char *path;
	char *name;
	bool dir;
} MTY_FileDesc;

typedef struct {
	MTY_FileDesc *files;
	uint32_t len;
} MTY_FileList;

typedef struct MTY_LockFile MTY_LockFile;

MTY_EXPORT bool
MTY_FsRead(const char *path, void **data, size_t *size);

MTY_EXPORT bool
MTY_FsWrite(const char *path, const void *data, size_t size);

MTY_EXPORT bool
MTY_FsWriteText(const char *path, const char *fmt, ...);

MTY_EXPORT bool
MTY_FsAppendText(const char *path, const char *fmt, ...);

MTY_EXPORT bool
MTY_FsDelete(const char *path);

MTY_EXPORT bool
MTY_FsExists(const char *path);

MTY_EXPORT bool
MTY_FsMkdir(const char *path);

MTY_EXPORT const char *
MTY_FsPath(const char *dir, const char *file);

MTY_EXPORT bool
MTY_FsCopy(const char *src, const char *dst);

MTY_EXPORT const char *
MTY_FsGetDir(MTY_FsDir dir);

MTY_EXPORT bool
MTY_FsLockCreate(const char *path, MTY_LockFile **lock);

MTY_EXPORT void
MTY_FsLockDestroy(MTY_LockFile **lock);

MTY_EXPORT const char *
MTY_FsName(const char *path, bool extension);

MTY_EXPORT MTY_FileList *
MTY_FsFileList(const char *path, const char *filter);

MTY_EXPORT void
MTY_FsFreeFileList(MTY_FileList **fileList);


/// @module json

typedef struct MTY_JSON MTY_JSON;

MTY_EXPORT bool
MTY_JSONParse(const char *input, MTY_JSON **output);

MTY_EXPORT char *
MTY_JSONStringify(const MTY_JSON *json);

MTY_EXPORT bool
MTY_JSONReadFile(const char *path, MTY_JSON **output);

MTY_EXPORT bool
MTY_JSONWriteFile(const char *path, const MTY_JSON *json);

MTY_EXPORT MTY_JSON *
MTY_JSONDuplicate(const MTY_JSON *json);

MTY_EXPORT uint32_t
MTY_JSONLength(const MTY_JSON *json);

MTY_EXPORT void
MTY_JSONDestroy(MTY_JSON **json);

MTY_EXPORT bool
MTY_JSONToString(const MTY_JSON *json, char *value, size_t size);

MTY_EXPORT bool
MTY_JSONToInt(const MTY_JSON *json, int32_t *value);

MTY_EXPORT bool
MTY_JSONToUInt(const MTY_JSON *json, uint32_t *value);

MTY_EXPORT bool
MTY_JSONToFloat(const MTY_JSON *json, float *value);

MTY_EXPORT bool
MTY_JSONToBool(const MTY_JSON *json, bool *value);

MTY_EXPORT bool
MTY_JSONIsNull(const MTY_JSON *json);

MTY_EXPORT MTY_JSON *
MTY_JSONFromString(const char *value);

MTY_EXPORT MTY_JSON *
MTY_JSONFromInt(int32_t value);

MTY_EXPORT MTY_JSON *
MTY_JSONFromUInt(uint32_t value);

MTY_EXPORT MTY_JSON *
MTY_JSONFromFloat(float value);

MTY_EXPORT MTY_JSON *
MTY_JSONFromBool(bool value);

MTY_EXPORT MTY_JSON *
MTY_JSONNull(void);

MTY_EXPORT MTY_JSON *
MTY_JSONObj(void);

MTY_EXPORT MTY_JSON *
MTY_JSONArray(void);

MTY_EXPORT bool
MTY_JSONObjKeyExists(const MTY_JSON *json, const char *key);

MTY_EXPORT void
MTY_JSONObjKeyDelete(MTY_JSON *json, const char *key);

MTY_EXPORT const MTY_JSON *
MTY_JSONObjGet(const MTY_JSON *json, const char *key);

MTY_EXPORT void
MTY_JSONObjSet(MTY_JSON *json, const char *key, const MTY_JSON *value);

MTY_EXPORT bool
MTY_JSONArrayIndexExists(const MTY_JSON *json, uint32_t index);

MTY_EXPORT void
MTY_JSONArrayIndexDelete(MTY_JSON *json, uint32_t index);

MTY_EXPORT const MTY_JSON *
MTY_JSONArrayGet(const MTY_JSON *json, uint32_t index);

MTY_EXPORT void
MTY_JSONArraySet(MTY_JSON *json, uint32_t index, const MTY_JSON *value);

MTY_EXPORT void
MTY_JSONArrayAppend(MTY_JSON *json, const MTY_JSON *value);


/// @module log

#define MTY_Log(msg, ...)   MTY_LogParams(__FUNCTION__, msg, ##__VA_ARGS__)
#define MTY_Fatal(msg, ...) MTY_FatalParams(__FUNCTION__, msg, ##__VA_ARGS__)

MTY_EXPORT void
MTY_SetLogCallback(void (*callback)(const char *msg, void *opaque), const void *opaque);

MTY_EXPORT void
MTY_DisableLogging(bool disabled);

MTY_EXPORT void
MTY_LogParams(const char *func, const char *msg, ...);

MTY_EXPORT void
MTY_FatalParams(const char *func, const char *msg, ...);

MTY_EXPORT const char *
MTY_GetLog(void);


/// @module mem

MTY_EXPORT void *
MTY_Alloc(size_t nelem, size_t elsize);

MTY_EXPORT void *
MTY_AllocAligned(size_t size, size_t align);

MTY_EXPORT void *
MTY_Realloc(void *mem, size_t nelem, size_t elsize);

MTY_EXPORT void *
MTY_Dup(const void *mem, size_t size);

MTY_EXPORT void *
MTY_Strdup(const void *str);

MTY_EXPORT void
MTY_Free(void *mem);

MTY_EXPORT void
MTY_FreeAligned(void *mem);

MTY_EXPORT uint16_t
MTY_Swap16(uint16_t value);

MTY_EXPORT uint32_t
MTY_Swap32(uint32_t value);

MTY_EXPORT uint64_t
MTY_Swap64(uint64_t value);

MTY_EXPORT uint16_t
MTY_SwapToBE16(uint16_t value);

MTY_EXPORT uint32_t
MTY_SwapToBE32(uint32_t value);

MTY_EXPORT uint64_t
MTY_SwapToBE64(uint64_t value);

MTY_EXPORT uint16_t
MTY_SwapFromBE16(uint16_t value);

MTY_EXPORT uint32_t
MTY_SwapFromBE32(uint32_t value);

MTY_EXPORT uint64_t
MTY_SwapFromBE64(uint64_t value);

MTY_EXPORT bool
MTY_WideToMulti(const wchar_t *src, char *dst, size_t len);

MTY_EXPORT char *
MTY_WideToMultiD(const wchar_t *src);

MTY_EXPORT bool
MTY_MultiToWide(const char *src, wchar_t *dst, uint32_t len);

MTY_EXPORT wchar_t *
MTY_MultiToWideD(const char *src);


/// @module proc

typedef struct MTY_SO MTY_SO;

MTY_EXPORT MTY_SO *
MTY_SOLoad(const char *name);

MTY_EXPORT void *
MTY_SOSymbolGet(MTY_SO *ctx, const char *name);

MTY_EXPORT void
MTY_SOUnload(MTY_SO **so);

MTY_EXPORT const char *
MTY_ProcName(void);

MTY_EXPORT bool
MTY_ProcRestart(int32_t argc, char * const *argv);

MTY_EXPORT const char *
MTY_Hostname(void);


/// @module render

typedef enum {
	MTY_GFX_NONE    = 0,
	MTY_GFX_GL      = 1,
	MTY_GFX_D3D9    = 2,
	MTY_GFX_D3D11   = 3,
	MTY_GFX_METAL   = 4,
	MTY_GFX_MAKE_32 = 0x7FFFFFFF,
} MTY_GFX;

typedef enum {
	MTY_COLOR_FORMAT_UNKNOWN = 0,
	MTY_COLOR_FORMAT_RGBA    = 1,
	MTY_COLOR_FORMAT_NV12    = 2,
	MTY_COLOR_FORMAT_I420    = 3,
	MTY_COLOR_FORMAT_I444    = 4,
	MTY_COLOR_FORMAT_MAKE_32 = 0x7FFFFFFF,
} MTY_ColorFormat;

typedef enum {
	MTY_FILTER_NEAREST        = 1,
	MTY_FILTER_LINEAR         = 2,
	MTY_FILTER_GAUSSIAN_SHARP = 3,
	MTY_FILTER_GAUSSIAN_SOFT  = 4,
	MTY_FILTER_MAKE_32        = 0x7FFFFFFF,
} MTY_Filter;

typedef enum {
	MTY_EFFECT_NONE         = 0,
	MTY_EFFECT_SCANLINES    = 1,
	MTY_EFFECT_SCANLINES_X2 = 2,
	MTY_EFFECT_MAKE_32      = 0x7FFFFFFF,
} MTY_Effect;

typedef struct MTY_RenderDesc {
	MTY_ColorFormat format;
	MTY_Filter filter;
	MTY_Effect effect;
	uint32_t imageWidth;
	uint32_t imageHeight;
	uint32_t cropWidth;
	uint32_t cropHeight;
	uint32_t viewWidth;
	uint32_t viewHeight;
	float aspectRatio;
	float scale;
} MTY_RenderDesc;

typedef struct MTY_Device MTY_Device;
typedef struct MTY_Context MTY_Context;
typedef struct MTY_Texture MTY_Texture;
typedef struct MTY_Renderer MTY_Renderer;

MTY_EXPORT bool
MTY_RendererCreate(MTY_Renderer **renderer);

MTY_EXPORT bool
MTY_RendererDrawQuad(MTY_Renderer *ctx, MTY_GFX api, MTY_Device *device, MTY_Context *context,
	const void *image, const MTY_RenderDesc *desc, MTY_Texture *dest);

MTY_EXPORT void
MTY_RendererDestroy(MTY_Renderer **renderer);


/// @module request

#define MTY_URL_MAX 1024

typedef struct MTY_Request MTY_Request;

MTY_EXPORT bool
MTY_RequestCreate(const char *url, const char *method, const char *headers,
	const void *body, size_t size, int32_t timeout, MTY_Request **request);

MTY_EXPORT bool
MTY_RequestGetStatusCode(MTY_Request *ctx, uint32_t *statusCode);

MTY_EXPORT bool
MTY_RequestGetBody(MTY_Request *ctx, void **body, size_t *size);

MTY_EXPORT bool
MTY_RequestGetHeader(MTY_Request *ctx, const char *key, char *value, size_t size);

MTY_EXPORT void
MTY_RequestDestroy(MTY_Request **response);


/// @module sort

MTY_EXPORT void
MTY_Sort(void *base, size_t nElements, size_t size, int32_t (*compare)(const void *a, const void *b));


/// @module struct

typedef struct MTY_ListNode {
	void *value;
	struct MTY_ListNode *prev;
	struct MTY_ListNode *next;
} MTY_ListNode;

typedef struct MTY_Hash MTY_Hash;
typedef struct MTY_Queue MTY_Queue;
typedef struct MTY_List MTY_List;

MTY_EXPORT void
MTY_HashCreate(uint32_t numBuckets, MTY_Hash **hash);

MTY_EXPORT void *
MTY_HashGet(MTY_Hash *ctx, const char *key);

MTY_EXPORT void *
MTY_HashGetInt(MTY_Hash *ctx, int64_t key);

MTY_EXPORT void *
MTY_HashSet(MTY_Hash *ctx, const char *key, const void *value);

MTY_EXPORT void *
MTY_HashSetInt(MTY_Hash *ctx, int64_t key, const void *value);

MTY_EXPORT void *
MTY_HashPop(MTY_Hash *ctx, const char *key);

MTY_EXPORT void *
MTY_HashPopInt(MTY_Hash *ctx, int64_t key);

MTY_EXPORT bool
MTY_HashNextKey(MTY_Hash *ctx, uint64_t *iter, const char **key);

MTY_EXPORT bool
MTY_HashNextKeyInt(MTY_Hash *ctx, uint64_t *iter, int64_t *key);

MTY_EXPORT void
MTY_HashDestroy(MTY_Hash **hash, void (*freeFunc)(void *value));

MTY_EXPORT void
MTY_QueueCreate(uint32_t len, size_t bufSize, MTY_Queue **queue);

MTY_EXPORT uint32_t
MTY_QueueLength(MTY_Queue *ctx);

MTY_EXPORT bool
MTY_QueuePush(MTY_Queue *ctx, const void *opaque, size_t size);

MTY_EXPORT bool
MTY_QueuePop(MTY_Queue *ctx, int32_t timeout, void **opaque, size_t *size);

MTY_EXPORT bool
MTY_QueuePushBegin(MTY_Queue *ctx, void **buffer);

MTY_EXPORT void
MTY_QueuePushEnd(MTY_Queue *ctx, size_t size);

MTY_EXPORT bool
MTY_QueuePopBegin(MTY_Queue *ctx, int32_t timeout, void **buffer, size_t *size);

MTY_EXPORT bool
MTY_QueuePopLastBegin(MTY_Queue *ctx, int32_t timeout, void **buffer, size_t *size);

MTY_EXPORT void
MTY_QueuePopEnd(MTY_Queue *ctx);

MTY_EXPORT void
MTY_QueueFlush(MTY_Queue *ctx, void (*freeFunc)(void *value));

MTY_EXPORT void
MTY_QueueDestroy(MTY_Queue **queue);

MTY_EXPORT void
MTY_ListCreate(MTY_List **list);

MTY_EXPORT MTY_ListNode *
MTY_ListFirst(MTY_List *ctx);

MTY_EXPORT void
MTY_ListAppend(MTY_List *ctx, void *value);

MTY_EXPORT void *
MTY_ListRemove(MTY_List *ctx, MTY_ListNode *node);

MTY_EXPORT void
MTY_ListDestroy(MTY_List **list, void (*freeFunc)(void *value));


/// @module thread

typedef enum {
	MTY_THREAD_STATE_EMPTY    = 0,
	MTY_THREAD_STATE_RUNNING  = 1,
	MTY_THREAD_STATE_DETACHED = 2,
	MTY_THREAD_STATE_DONE     = 3,
	MTY_THREAD_STATE_MAKE_32  = 0x7FFFFFFF,
} MTY_ThreadState;

typedef struct {
	volatile int32_t value;
} MTY_Atomic32;

typedef struct {
	volatile int64_t value;
} MTY_Atomic64;

typedef struct MTY_Thread MTY_Thread;
typedef struct MTY_Mutex MTY_Mutex;
typedef struct MTY_Cond MTY_Cond;
typedef struct MTY_RWLock MTY_RWLock;
typedef struct MTY_Sync MTY_Sync;
typedef struct MTY_ThreadPool MTY_ThreadPool;

MTY_EXPORT void
MTY_ThreadCreate(void *(*func)(void *opaque), const void *opaque, MTY_Thread **thread);

MTY_EXPORT int64_t
MTY_ThreadIDGet(MTY_Thread *ctx);

MTY_EXPORT void *
MTY_ThreadDestroy(MTY_Thread **thread);

MTY_EXPORT void
MTY_MutexCreate(MTY_Mutex **mutex);

MTY_EXPORT void
MTY_MutexLock(MTY_Mutex *ctx);

MTY_EXPORT bool
MTY_MutexTryLock(MTY_Mutex *ctx);

MTY_EXPORT void
MTY_MutexUnlock(MTY_Mutex *ctx);

MTY_EXPORT void
MTY_MutexDestroy(MTY_Mutex **mutex);

MTY_EXPORT void
MTY_CondCreate(MTY_Cond **cond);

MTY_EXPORT bool
MTY_CondWait(MTY_Cond *ctx, MTY_Mutex *mutex, int32_t timeout);

MTY_EXPORT void
MTY_CondWake(MTY_Cond *ctx);

MTY_EXPORT void
MTY_CondWakeAll(MTY_Cond *ctx);

MTY_EXPORT void
MTY_RWLockCreate(MTY_RWLock **rwlock);

MTY_EXPORT void
MTY_RWLockReader(MTY_RWLock *ctx);

MTY_EXPORT void
MTY_RWLockWriter(MTY_RWLock *ctx);

MTY_EXPORT void
MTY_RWLockUnlock(MTY_RWLock *ctx);

MTY_EXPORT void
MTY_RWLockDestroy(MTY_RWLock **rwlock);

MTY_EXPORT void
MTY_CondDestroy(MTY_Cond **cond);

MTY_EXPORT void
MTY_SyncCreate(MTY_Sync **sync);

MTY_EXPORT bool
MTY_SyncWait(MTY_Sync *ctx, int32_t timeout);

MTY_EXPORT void
MTY_SyncWake(MTY_Sync *ctx);

MTY_EXPORT void
MTY_SyncDestroy(MTY_Sync **sync);

MTY_EXPORT void
MTY_ThreadPoolCreate(uint32_t maxThreads, MTY_ThreadPool **pool);

MTY_EXPORT uint32_t
MTY_ThreadPoolStart(MTY_ThreadPool *ctx, void (*func)(void *opaque), const void *opaque);

MTY_EXPORT void
MTY_ThreadPoolDetach(MTY_ThreadPool *ctx, uint32_t index, void (*detach)(void *opaque));

MTY_EXPORT MTY_ThreadState
MTY_ThreadPoolState(MTY_ThreadPool *ctx, uint32_t index, void **opaque);

MTY_EXPORT void
MTY_ThreadPoolDestroy(MTY_ThreadPool **pool, void (*detach)(void *opaque));

MTY_EXPORT void
MTY_Atomic32Set(MTY_Atomic32 *atomic, int32_t value);

MTY_EXPORT void
MTY_Atomic64Set(MTY_Atomic64 *atomic, int64_t value);

MTY_EXPORT int32_t
MTY_Atomic32Get(MTY_Atomic32 *atomic);

MTY_EXPORT int64_t
MTY_Atomic64Get(MTY_Atomic64 *atomic);

MTY_EXPORT int32_t
MTY_Atomic32Add(MTY_Atomic32 *atomic, int32_t value);

MTY_EXPORT int64_t
MTY_Atomic64Add(MTY_Atomic64 *atomic, int64_t value);

MTY_EXPORT bool
MTY_Atomic32CAS(MTY_Atomic32 *atomic, int32_t oldValue, int32_t newValue);

MTY_EXPORT bool
MTY_Atomic64CAS(MTY_Atomic64 *atomic, int64_t oldValue, int64_t newValue);

MTY_EXPORT void
MTY_GlobalLock(MTY_Atomic32 *lock);

MTY_EXPORT void
MTY_GlobalUnlock(MTY_Atomic32 *lock);


/// @module time

MTY_EXPORT int64_t
MTY_Timestamp(void);

MTY_EXPORT float
MTY_TimeDiff(int64_t begin, int64_t end);

MTY_EXPORT void
MTY_Sleep(uint32_t timeout);

MTY_EXPORT void
MTY_SetTimerResolution(uint32_t res);

MTY_EXPORT void
MTY_RevertTimerResolution(uint32_t res);


// @module window

#define MTY_TITLE_MAX 1024

typedef enum {
	MTY_WINDOW_MSG_NONE         = 0,
	MTY_WINDOW_MSG_CLOSE        = 1,
	MTY_WINDOW_MSG_KEYBOARD     = 2,
	MTY_WINDOW_MSG_MOUSE_WHEEL  = 3,
	MTY_WINDOW_MSG_MOUSE_BUTTON = 4,
	MTY_WINDOW_MSG_MOUSE_MOTION = 5,
	MTY_WINDOW_MSG_DROP         = 6,
	MTY_WINDOW_MSG_MAKE_32      = 0x7FFFFFFF,
} MTY_WindowMsgType;

typedef enum {
	MTY_SCANCODE_NONE      = 0x000,
	MTY_SCANCODE_ESCAPE    = 0x001,
	MTY_SCANCODE_TAB       = 0x00f,
	MTY_SCANCODE_W         = 0x011,
	MTY_SCANCODE_C         = 0x02e,
	MTY_SCANCODE_X         = 0x02d,
	MTY_SCANCODE_Y         = 0x015,
	MTY_SCANCODE_Z         = 0x02c,
	MTY_SCANCODE_V         = 0x02f,
	MTY_SCANCODE_R         = 0x013,
	MTY_SCANCODE_A         = 0x01E,
	MTY_SCANCODE_S         = 0x01F,
	MTY_SCANCODE_D         = 0x020,
	MTY_SCANCODE_L         = 0x026,
	MTY_SCANCODE_O         = 0x018,
	MTY_SCANCODE_P         = 0x019,
	MTY_SCANCODE_M         = 0x032,
	MTY_SCANCODE_T         = 0x014,
	MTY_SCANCODE_1         = 0x002,
	MTY_SCANCODE_SEMICOLON = 0x027,
	MTY_SCANCODE_LSHIFT    = 0x02A,
	MTY_SCANCODE_SPACE     = 0x039,
	MTY_SCANCODE_LEFT      = 0x14b,
	MTY_SCANCODE_RIGHT     = 0x14d,
	MTY_SCANCODE_UP        = 0x148,
	MTY_SCANCODE_DOWN      = 0x150,
	MTY_SCANCODE_PAGEUP    = 0x149,
	MTY_SCANCODE_PAGEDOWN  = 0x151,
	MTY_SCANCODE_ENTER     = 0x01c,
	MTY_SCANCODE_HOME      = 0x147,
	MTY_SCANCODE_END       = 0x14f,
	MTY_SCANCODE_INSERT    = 0x152,
	MTY_SCANCODE_DELETE    = 0x153,
	MTY_SCANCODE_BACKSPACE = 0x00e,
	MTY_SCANCODE_RSHIFT    = 0x036,
	MTY_SCANCODE_LCTRL     = 0x01d,
	MTY_SCANCODE_RCTRL     = 0x11d,
	MTY_SCANCODE_LALT      = 0x038,
	MTY_SCANCODE_RALT      = 0x138,
	MTY_SCANCODE_LGUI      = 0x15b,
	MTY_SCANCODE_RGUI      = 0x15c,
	MTY_SCANCODE_MAX       = 0x200,
	MTY_SCANCODE_MAKE_32   = 0x7FFFFFFF,
} MTY_Scancode;

typedef enum {
	MTY_MOUSE_BUTTON_NONE    = 0,
	MTY_MOUSE_BUTTON_L       = 1,
	MTY_MOUSE_BUTTON_R       = 2,
	MTY_MOUSE_BUTTON_MIDDLE  = 3,
	MTY_MOUSE_BUTTON_MAKE_32 = 0x7FFFFFFF,
} MTY_MouseButton;

typedef struct {
	MTY_WindowMsgType type;

	union {
		struct {
			bool pressed;
			MTY_Scancode scancode;
		} keyboard;

		struct {
			int32_t x;
			int32_t y;
		} mouseWheel;

		struct {
			MTY_MouseButton button;
			bool pressed;
		} mouseButton;

		struct {
			bool relative;
			int32_t x;
			int32_t y;
		} mouseMotion;

		struct {
			const char *name;
			const void *data;
			size_t size;
		} drop;
	};
} MTY_WindowMsg;

typedef struct MTY_Window MTY_Window;

typedef void (*MTY_WindowMsgFunc)(const MTY_WindowMsg *wmsg, void *opaque);
typedef bool (*MTY_AppFunc)(void *opaque);

MTY_EXPORT bool
MTY_WindowCreate(const char *title, MTY_WindowMsgFunc msg_func, const void *opaque,
	uint32_t width, uint32_t height, bool fullscreen, MTY_Window **window);

MTY_EXPORT void
MTY_AppRun(MTY_AppFunc func, const void *opaque);

MTY_EXPORT void
MTY_WindowSetTitle(MTY_Window *ctx, const char *title, const char *subtitle);

MTY_EXPORT void
MTY_WindowPoll(MTY_Window *ctx);

MTY_EXPORT bool
MTY_WindowIsForeground(MTY_Window *ctx);

MTY_EXPORT uint32_t
MTY_WindowGetRefreshRate(MTY_Window *ctx);

MTY_EXPORT float
MTY_WindowGetDPIScale(MTY_Window *ctx);

MTY_EXPORT void
MTY_WindowSetFullscreen(MTY_Window *ctx);

MTY_EXPORT void
MTY_WindowSetWindowed(MTY_Window *ctx, uint32_t width, uint32_t height);

MTY_EXPORT bool
MTY_WindowIsFullscreen(MTY_Window *ctx);

MTY_EXPORT void
MTY_WindowPresent(MTY_Window *ctx, uint32_t num_frames);

MTY_EXPORT MTY_Device *
MTY_WindowGetDevice(MTY_Window *ctx);

MTY_EXPORT MTY_Context *
MTY_WindowGetContext(MTY_Window *ctx);

MTY_EXPORT MTY_Texture *
MTY_WindowGetBackBuffer(MTY_Window *ctx);

MTY_EXPORT void
MTY_WindowDrawQuad(MTY_Window *ctx, const void *image, const MTY_RenderDesc *desc);

MTY_EXPORT void
MTY_WindowDestroy(MTY_Window **window);

MTY_EXPORT void *
MTY_GLGetProcAddress(const char *name);


#ifdef __cplusplus
}
#endif
