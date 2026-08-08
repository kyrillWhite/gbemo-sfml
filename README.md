# gbemo-sfml

SFML frontend for [gbemo-core](https://github.com/kyrillWhite/gbemo-core) — the
Game Boy (DMG) emulation core.

The core is headless: it has no window, no audio device and no keyboard. This
repository provides that platform layer with SFML — a scaled window for the
160×144 framebuffer, a second window with the VRAM tile atlas, a streaming
audio sink and key bindings — and drives the core's main loop.

Both dependencies are submodules under `libs/` and are built from source, so
no prebuilt binaries are vendored and nothing has to be installed system-wide.

## Layout

```
src/           main loop, input handling, SFML window/audio wrappers
  main.cpp     pacing loop, key handling, audio streaming, presentation
  engine.h/cpp thin SFML RenderWindow + texture blitter
assets/        Roboto-Regular.ttf
libs/
  gbemo-core/  submodule: the emulation core (static library)
  SFML/        submodule: SFML, pinned to the 2.6.2 tag
  build/       out-of-tree CMake build trees for SFML, git-ignored
roms/          ROMs and save files, git-ignored
```

## Building

Requires MinGW `g++` with C++20 support, GNU `make`, CMake ≥ 3.16 and
`mingw32-make` (CMake's generator program for the SFML build).

```
git clone --recurse-submodules <url>
# or, in an existing clone:
make submodules

make                 # debug
make release         # optimized
make run             # debug build, then launch
make clean           # this repo's obj/ and bin/
make clean-libs      # drop the SFML build trees
make clean-all       # both, plus gbemo-core
make x86 release     # 32-bit
make x64 release     # 64-bit (default)
make window release  # build without a console window
```

`x86`, `x64`, `console` and `window` are selectors: they set the architecture
and subsystem for whatever goal you name alongside them, and build nothing on
their own.

The first build of a given architecture and mode configures and compiles SFML
into `libs/build/SFML/<arch>/<mode>/`, which takes a few minutes; afterwards it
is reused. `make clean` deliberately leaves those trees alone.

Everything is linked statically — SFML, gbemo-core and the MinGW runtime — so
the output is a single `emu.exe` next to `openal32.dll`, which stays a DLL
because SFML loads OpenAL as a separate runtime dependency. Artifacts land in
`bin/<debug|release>/<arch>/` together with `assets/`.

### 32-bit

`make x86 …` needs a toolchain targeting `i686-w64-mingw32`. It is picked up
either from `PATH` under that cross name, or from an MSYS2 MINGW32 install —
by default `/c/msys64/mingw32/bin`, overridable:

```
make MINGW32_BIN=/path/to/mingw32/bin x86 release
```

In the MSYS2 case the Makefile also prepends that directory to `PATH`:
`cc1plus` lives outside `bin/` and resolves its runtime DLLs through `PATH`, so
with `mingw64/bin` ahead of it, it loads the 64-bit ones and exits without
printing anything.

## Running

```
bin/release/x64/emu.exe [path/to/rom.gb]
```

Without an argument it loads `roms/rom.gb` relative to the working directory.

## Controls

| Key         | Action        |
| ----------- | ------------- |
| `Z`         | B             |
| `X`         | A             |
| `Enter`     | Start         |
| `Backspace` | Select        |
| Arrows      | D-pad         |
| `S`         | Speed-up flag |
| `Esc`       | Quit          |
