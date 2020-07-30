OS = windows
ARCH = %%Platform%%
NAME = matoya

.SUFFIXES : .ps4 .vs4 .vert .frag

.vs4.h:
	fxc $(FXCFLAGS) /Fh $@ /T vs_4_0 /Vn $(*B) $<

.ps4.h:
	fxc $(FXCFLAGS) /Fh $@ /T ps_4_0 /Vn $(*B) $<

.vert.h:
	echo static const GLchar VERT[]={ > $@ && powershell "Format-Hex $< | %{$$_ -replace '^.*?   ',''} | %{$$_ -replace '  .*',' '} | %{$$_ -replace '(\w\w)','0x$$1,'}" >> $@ && echo 0x00}; >> $@

.frag.h:
	echo static const GLchar FRAG[]={ > $@ && powershell "Format-Hex $< | %{$$_ -replace '^.*?   ',''} | %{$$_ -replace '  .*',' '} | %{$$_ -replace '(\w\w)','0x$$1,'}" >> $@ && echo 0x00}; >> $@

OBJS = \
	src\compress.obj \
	src\crypto.obj \
	src\fs.obj \
	src\json.obj \
	src\log.obj \
	src\memory.obj \
	src\proc.obj \
	src\sort.obj \
	src\hash.obj \
	src\list.obj \
	src\queue.obj \
	src\thread.obj \
	src\gfx-gl.obj

OBJS = $(OBJS) \
	src\windows\audio.obj \
	src\windows\cryptow.obj \
	src\windows\fsw.obj \
	src\windows\memoryw.obj \
	src\windows\procw.obj \
	src\windows\render.obj \
	src\windows\request.obj \
	src\windows\threadw.obj \
	src\windows\timew.obj \
	src\windows\window.obj \
	src\windows\gfx-d3d11.obj

SHADERS = \
	src\shaders\GL\vs.h \
	src\shaders\GL\fs.h \
	src\windows\shaders\vs.h \
	src\windows\shaders\ps.h

INCLUDES = \
	-Isrc \
	-Isrc\windows \
	-Ideps

DEFS = \
	-DWIN32_LEAN_AND_MEAN \
	-DUNICODE

FXCFLAGS = \
	/O3 \
	/Ges \
	/nologo

FLAGS = \
	/W4 \
	/MT \
	/MP \
	/volatile:iso \
	/wd4100 \
	/wd4152 \
	/wd4201 \
	/wd4204 \
	/nologo

LIB_FLAGS = \
	/nologo

SYS_LIBS = \
	libvcruntime.lib \
	libucrt.lib \
	libcmt.lib \
	kernel32.lib \
	user32.lib \
	shell32.lib \
	d3d11.lib \
	dxguid.lib \
	ole32.lib \
	uuid.lib \
	winmm.lib \
	shcore.lib \
	bcrypt.lib \
	shlwapi.lib \
	winhttp.lib \
	ws2_32.lib

!IFDEF DEBUG
FLAGS = $(FLAGS) /Oy- /Ob0 /Zi
!ELSE
FLAGS = $(FLAGS) /O2 /Gy /GS- /Gw
!IFDEF LTO
FLAGS = $(FLAGS) /GL
LIB_FLAGS = $(LIB_FLAGS) /LTCG
!ENDIF
!ENDIF

CFLAGS = $(INCLUDES) $(DEFS) $(FLAGS)

all: clean-build clear $(SHADERS) $(OBJS)
	mkdir bin\$(OS)\$(ARCH)
	lib /out:bin\$(OS)\$(ARCH)\$(NAME).lib $(LIB_FLAGS) *.obj

# The 'shared' target is here to test a full link and required system dependencies
shared: clean-build clear $(SHADERS) $(OBJS)
	link /out:$(NAME).dll /dll /nodefaultlib $(LIB_FLAGS) $(SYS_LIBS) *.obj

clean-build:
	@-rmdir /s /q bin 2>nul
	@-del /q *.obj 2>nul
	@-del /q *.lib 2>nul
	@-del /q *.pdb 2>nul
	@-del /q $(NAME).dll 2>nul

clean: clean-build
	@-del /q $(SHADERS) 2>nul

clear:
	@cls
