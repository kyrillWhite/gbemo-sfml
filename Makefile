# Build configuration
CORE_DIR ?= libs/gbemo-core
SFML_DIR ?= libs/SFML
ARCH ?= x64
GUI ?= console

# "make x86 release" / "make window release": the selector words set variables
# and build nothing themselves, so the real goal is built once, not once per
# selector and once more for the defaults.
SELECTORS := x86 x64 console window
GOALS := $(filter-out $(SELECTORS),$(MAKECMDGOALS))
ifneq ($(filter x86,$(MAKECMDGOALS)),)
	ARCH := x86
endif
ifneq ($(filter x64,$(MAKECMDGOALS)),)
	ARCH := x64
endif
ifneq ($(filter window,$(MAKECMDGOALS)),)
	GUI := window
endif
ifneq ($(filter console,$(MAKECMDGOALS)),)
	GUI := console
endif

MKDIR := mkdir -p
RM := rm -rf
CP := cp -f
EXE := .exe
CMAKE ?= cmake

ARCH_FLAGS :=
ifeq ($(ARCH),x86)
	ARCH_FLAGS := -m32
endif

CXX ?= g++

# 32-bit needs a toolchain targeting i686-w64-mingw32. Either it is on PATH
# under its cross-compiler name, or it is the MSYS2 MINGW32 environment, whose
# g++ is plain "g++" inside its own bin directory.
MINGW32_BIN ?= /c/msys64/mingw32/bin
ifeq ($(ARCH),x86)
	ifneq ($(shell which i686-w64-mingw32-g++ 2>/dev/null),)
		CXX := i686-w64-mingw32-g++
	else ifneq ($(wildcard $(MINGW32_BIN)/g++.exe),)
		CXX := $(MINGW32_BIN)/g++.exe
		# cc1plus sits outside bin/ and resolves its runtime DLLs through PATH.
		# With mingw64/bin ahead of us it loads the 64-bit ones and dies without
		# printing anything, so put the 32-bit environment first.
		export PATH := $(MINGW32_BIN):$(PATH)
	else
		$(error x86 build requires i686-w64-mingw32-g++ on PATH, or an MSYS2 \
			MINGW32 install at MINGW32_BIN (currently $(MINGW32_BIN)))
	endif
endif
CC := $(subst g++,gcc,$(CXX))

# CMake is a native Windows program, so it needs Windows-shaped paths for the
# tools we hand it; everything else stays relative to this directory.
native = $(shell cygpath -m "$(1)" 2>/dev/null || echo "$(1)")
CXX_NATIVE := $(call native,$(shell which $(CXX) 2>/dev/null || echo $(CXX)))
CC_NATIVE := $(call native,$(shell which $(CC) 2>/dev/null || echo $(CC)))
CMAKE_MAKE_NATIVE := $(call native,$(shell which mingw32-make 2>/dev/null))

# --- SFML, built from the submodule -----------------------------------------

SFML_INCLUDE_DIR := $(SFML_DIR)/include
SFML_EXTLIBS_DIR := $(SFML_DIR)/extlibs/libs-mingw/$(ARCH)
SFML_EXTBIN_DIR := $(SFML_DIR)/extlibs/bin/$(ARCH)
SFML_BUILD_DIR_DEBUG := libs/build/SFML/$(ARCH)/debug
SFML_BUILD_DIR_RELEASE := libs/build/SFML/$(ARCH)/release
SFML_STAMP_DEBUG := $(SFML_BUILD_DIR_DEBUG)/lib/libsfml-graphics-s-d.a
SFML_STAMP_RELEASE := $(SFML_BUILD_DIR_RELEASE)/lib/libsfml-graphics-s.a

# Third-party static libs SFML links against, shipped inside its submodule.
SFML_EXT_LIBS := -L$(SFML_EXTLIBS_DIR) -lfreetype -lopenal32 \
	-lFLAC -lvorbisenc -lvorbisfile -lvorbis -logg
SFML_SYS_LIBS := -lopengl32 -lgdi32 -lwinmm

SFML_LIBS_RELEASE := -L$(SFML_BUILD_DIR_RELEASE)/lib \
	-lsfml-graphics-s -lsfml-window-s -lsfml-audio-s -lsfml-system-s
SFML_LIBS_DEBUG := -L$(SFML_BUILD_DIR_DEBUG)/lib \
	-lsfml-graphics-s-d -lsfml-window-s-d -lsfml-audio-s-d -lsfml-system-s-d
ifeq ($(GUI),window)
	SFML_LIBS_RELEASE += -lsfml-main
	SFML_LIBS_DEBUG += -lsfml-main-d
endif
SFML_LIBS_RELEASE += $(SFML_EXT_LIBS) $(SFML_SYS_LIBS)
SFML_LIBS_DEBUG += $(SFML_EXT_LIBS) $(SFML_SYS_LIBS)

# Shared between both modes; SFML_STATIC switches its headers to static linkage.
SFML_CMAKE_FLAGS := -G "MinGW Makefiles" \
	-DCMAKE_MAKE_PROGRAM="$(CMAKE_MAKE_NATIVE)" \
	-DCMAKE_C_COMPILER="$(CC_NATIVE)" \
	-DCMAKE_CXX_COMPILER="$(CXX_NATIVE)" \
	-DBUILD_SHARED_LIBS=OFF \
	-DSFML_BUILD_NETWORK=OFF \
	-DSFML_BUILD_EXAMPLES=OFF \
	-DSFML_BUILD_DOC=OFF

# --- gbemo-core, built from the submodule ------------------------------------

CORE_INCLUDE_DIR := $(CORE_DIR)/src
CORE_LIB_DEBUG := $(CORE_DIR)/bin/debug/$(ARCH)/libgbemo.a
CORE_LIB_RELEASE := $(CORE_DIR)/bin/release/$(ARCH)/libgbemo.a

# --- this project ------------------------------------------------------------

CXXFLAGS_BASE := -std=c++20 -DSFML_STATIC -Isrc -I$(CORE_INCLUDE_DIR) \
	-I$(SFML_INCLUDE_DIR) $(ARCH_FLAGS)
# Pull in libstdc++/libgcc/winpthread too, so the executable needs nothing from
# the MinGW installation at runtime.
LDFLAGS := $(ARCH_FLAGS) -static

FLAGS :=
ifeq ($(GUI),window)
	FLAGS := -mwindows
endif

MY_SRC := $(wildcard src/*.cpp)
OBJ_DIR_DEBUG = obj/debug/$(ARCH)
OBJ_DIR_RELEASE = obj/release/$(ARCH)
BIN_DIR_DEBUG = bin/debug/$(ARCH)
BIN_DIR_RELEASE = bin/release/$(ARCH)
MY_OBJ_DEBUG = $(patsubst src/%.cpp,$(OBJ_DIR_DEBUG)/%.o,$(MY_SRC))
MY_OBJ_RELEASE = $(patsubst src/%.cpp,$(OBJ_DIR_RELEASE)/%.o,$(MY_SRC))
OUT_DEBUG = $(BIN_DIR_DEBUG)/emu$(EXE)
OUT_RELEASE = $(BIN_DIR_RELEASE)/emu$(EXE)

.PHONY: all debug release clean clean-libs clean-all run submodules \
        core-debug core-release sfml-debug sfml-release \
        copy-dlls copy-dlls-debug copy-assets copy-assets-debug

all: debug

debug: CXXFLAGS := $(CXXFLAGS_BASE) -D_DEBUG -g
debug: sfml-debug core-debug $(OUT_DEBUG) copy-dlls-debug copy-assets-debug

release: CXXFLAGS := $(CXXFLAGS_BASE) -O3
release: sfml-release core-release $(OUT_RELEASE) copy-dlls copy-assets

submodules:
	git submodule update --init --recursive

# The core is a library project under libs/; build it with the same arch.
core-debug:
	@$(MAKE) -C $(CORE_DIR) ARCH=$(ARCH) debug

core-release:
	@$(MAKE) -C $(CORE_DIR) ARCH=$(ARCH) release

# SFML is configured and built out of tree, once per arch and mode.
sfml-debug: $(SFML_STAMP_DEBUG)
sfml-release: $(SFML_STAMP_RELEASE)

$(SFML_STAMP_DEBUG):
	$(CMAKE) -S $(SFML_DIR) -B $(SFML_BUILD_DIR_DEBUG) $(SFML_CMAKE_FLAGS) \
		-DCMAKE_BUILD_TYPE=Debug
	$(CMAKE) --build $(SFML_BUILD_DIR_DEBUG) --parallel

$(SFML_STAMP_RELEASE):
	$(CMAKE) -S $(SFML_DIR) -B $(SFML_BUILD_DIR_RELEASE) $(SFML_CMAKE_FLAGS) \
		-DCMAKE_BUILD_TYPE=Release
	$(CMAKE) --build $(SFML_BUILD_DIR_RELEASE) --parallel

$(OUT_DEBUG): $(MY_OBJ_DEBUG) | $(BIN_DIR_DEBUG)
	$(CXX) $(CXXFLAGS) $(LDFLAGS) $(MY_OBJ_DEBUG) -o $(OUT_DEBUG) $(CORE_LIB_DEBUG) $(SFML_LIBS_DEBUG) $(FLAGS)

$(OUT_RELEASE): $(MY_OBJ_RELEASE) | $(BIN_DIR_RELEASE)
	$(CXX) $(CXXFLAGS) $(LDFLAGS) $(MY_OBJ_RELEASE) -o $(OUT_RELEASE) $(CORE_LIB_RELEASE) $(SFML_LIBS_RELEASE) $(FLAGS)

$(OBJ_DIR_DEBUG)/%.o: src/%.cpp | $(OBJ_DIR_DEBUG)
	$(CXX) $(CXXFLAGS) -c $< -o $@

$(OBJ_DIR_RELEASE)/%.o: src/%.cpp | $(OBJ_DIR_RELEASE)
	$(CXX) $(CXXFLAGS) -c $< -o $@

$(OBJ_DIR_DEBUG):
	@$(MKDIR) $(OBJ_DIR_DEBUG)

$(OBJ_DIR_RELEASE):
	@$(MKDIR) $(OBJ_DIR_RELEASE)

$(BIN_DIR_DEBUG):
	@$(MKDIR) $(BIN_DIR_DEBUG)

$(BIN_DIR_RELEASE):
	@$(MKDIR) $(BIN_DIR_RELEASE)

run: debug
	./$(OUT_DEBUG)

clean:
	@$(RM) obj
	@$(RM) bin

# SFML takes minutes to build, so it is not part of "clean".
clean-libs:
	@$(RM) libs/build

clean-all: clean clean-libs
	@$(MAKE) -C $(CORE_DIR) clean

# OpenAL stays a DLL even in a static build: SFML loads it as a separate
# runtime dependency.
copy-dlls: | $(BIN_DIR_RELEASE)
	@$(CP) "$(SFML_EXTBIN_DIR)/openal32.dll" "$(BIN_DIR_RELEASE)/"

copy-dlls-debug: | $(BIN_DIR_DEBUG)
	@$(CP) "$(SFML_EXTBIN_DIR)/openal32.dll" "$(BIN_DIR_DEBUG)/"

copy-assets: | $(BIN_DIR_RELEASE)
	@$(MKDIR) "$(BIN_DIR_RELEASE)/assets"
	@$(CP) assets/* "$(BIN_DIR_RELEASE)/assets/"

copy-assets-debug: | $(BIN_DIR_DEBUG)
	@$(MKDIR) "$(BIN_DIR_DEBUG)/assets"
	@$(CP) assets/* "$(BIN_DIR_DEBUG)/assets/"

ifeq ($(GOALS),)
# "make x86" on its own still means "build the default target for x86".
$(SELECTORS): all
else
$(SELECTORS):
	@:
endif
