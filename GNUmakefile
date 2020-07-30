UNAME_S = $(shell uname -s)
ARCH = $(shell uname -m)
NAME = libmatoya

.SUFFIXES: .vert .frag

.vert.h:
	hexdump -ve '1/1 "0x%.2x,"' $< | (echo 'static const GLchar VERT[]={' && cat && echo '0x00};') > $@

.frag.h:
	hexdump -ve '1/1 "0x%.2x,"' $< | (echo 'static const GLchar FRAG[]={' && cat && echo '0x00};') > $@

.m.o:
	$(CC) $(OCFLAGS)  -c -o $@ $<

OBJS = \
	src/compress.o \
	src/crypto.o \
	src/fs.o \
	src/json.o \
	src/log.o \
	src/memory.o \
	src/proc.o \
	src/sort.o \
	src/hash.o \
	src/list.o \
	src/queue.o \
	src/thread.o \

OBJS := $(OBJS) \
	src/unix/crypto.o \
	src/unix/fs.o \
	src/unix/memory.o \
	src/unix/proc.o \
	src/unix/request.o \
	src/unix/thread.o \
	src/unix/time.o \
	src/gfx-gl.o

INCLUDES = \
	-Isrc \
	-Isrc/unix \
	-Ideps

DEFS = \
	-D_POSIX_C_SOURCE=200112L

FLAGS = \
	-Wall \
	-Wextra \
	-Wshadow \
	-Wno-unused-parameter \
	-Wno-switch \
	-std=c99 \
	-fPIC

ifdef DEBUG
FLAGS := $(FLAGS) -O0 -g
else
FLAGS := $(FLAGS) -O3 -fvisibility=hidden
ifdef LTO
FLAGS := $(FLAGS) -flto
endif
endif

############
### WASM ###
############
ifdef WASM
ifdef EMSDK

CC = emcc
AR = emar

ARCH := emscripten

else
WASI_SDK = $(HOME)/wasi-sdk-11.0

CC = $(WASI_SDK)/bin/clang --sysroot=$(WASI_SDK)/share/wasi-sysroot
AR = $(WASI_SDK)/bin/ar

ARCH := wasm32

endif

OBJS := $(OBJS) \
	src/unix/web/window.o \
	src/unix/linux/render.o

SHADERS = \
	src/shaders/GL/vs.h \
	src/shaders/GL/fs.h

DEFS := $(DEFS) \
	-DMTY_GL_EXTERNAL \
	-DMTY_GL_ES

OS = web
INCLUDES := $(INCLUDES) -Isrc/unix/web

else
#############
### LINUX ###
#############
ifeq ($(UNAME_S), Linux)

OBJS := $(OBJS) \
	src/unix/linux/audio.o \
	src/unix/linux/window.o \
	src/unix/linux/render.o

DEFS := $(DEFS) \
	-DMTY_CRYPTO_EXTERNAL

SHADERS = \
	src/shaders/GL/vs.h \
	src/shaders/GL/fs.h

SYS_LIBS = \
	-lc \
	-lm \
	-ldl \
	-lpthread

OS = linux
INCLUDES := $(INCLUDES) -Isrc/unix/linux
endif

#############
### APPLE ###
#############
ifeq ($(UNAME_S), Darwin)

.SUFFIXES: .metal

.metal.h:
	hexdump -ve '1/1 "0x%.2x,"' $< | (echo 'static const char MTL_LIBRARY[]={' && cat && echo '0x00};') > $@

ifndef TARGET
TARGET = macosx
endif

ifndef ARCH
ARCH = x86_64
endif

ifeq ($(TARGET), macosx)
OS = macos
else ifeq ($(TARGET), iphoneos)
OS = ios
else ifeq ($(TARGET), iphonesimulator)
OS = ios
else ifeq ($(TARGET), appletvos)
OS = tvos
else ifeq ($(TARGET), appletvsimulator)
OS = tvos
endif

SHADERS = \
	src/shaders/GL/vs.h \
	src/shaders/GL/fs.h \
	src/unix/apple/shaders/shaders.h

OBJS := $(OBJS) \
	src/unix/apple/gfx-metal.o \
	src/unix/apple/render.o \
	src/unix/apple/audio.o \
	src/unix/apple/$(OS)/window.o

DEFS := $(DEFS) \
	-DMTY_GL_EXTERNAL \
	-DMTY_CRYPTO_EXTERNAL

SYS_LIBS = \
	-lc \
	-framework AppKit \
	-framework QuartzCore \
	-framework Metal \
	-framework AudioToolbox

FLAGS := $(FLAGS) \
	-isysroot $(shell xcrun --sdk $(TARGET) --show-sdk-path) \
	-arch $(ARCH)

INCLUDES := $(INCLUDES) -Isrc/unix/apple -Isrc/unix/apple/$(OS)
endif
endif

CFLAGS = $(INCLUDES) $(DEFS) $(FLAGS)
OCFLAGS = $(CFLAGS) -fobjc-arc

all: clean-build clear $(SHADERS)
	make objs -j4

objs: $(OBJS)
	mkdir -p bin/$(OS)/$(ARCH)
	$(AR) -crs bin/$(OS)/$(ARCH)/$(NAME).a $(OBJS)

# The 'shared' target is here to test a full link and required system dependencies
shared: clean-build clear $(SHADERS)
	make objs-shared -j4

objs-shared: $(OBJS)
	$(CC) -Wl,--unresolved-symbols=report-all -shared -nodefaultlibs -o $(NAME).so $(OBJS) $(SYS_LIBS)

###############
### ANDROID ###
###############

### Downloads ###
# https://developer.android.com/ndk/downloads -> Put in ~/android-ndk-xxx

ANDROID_NDK = $(HOME)/android-ndk-r21d

android: clear $(SHADERS)
	@$(ANDROID_NDK)/ndk-build -j4 \
		NDK_PROJECT_PATH=. \
		NDK_APPLICATION_MK=Application.mk \
		--no-print-directory \
		| grep -v 'fcntl(): Operation not supported'

clean: clean-build
	@rm -rf bin
	@rm -rf obj
	@rm -f $(SHADERS)

clean-build:
	@rm -f $(OBJS)
	@rm -f $(NAME).so

clear:
	@clear
