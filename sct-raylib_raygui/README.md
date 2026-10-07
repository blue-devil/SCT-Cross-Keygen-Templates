# SCT

Single-window desktop app built with **raylib 6.0 + raygui** (default light theme) and
**libxmp-lite** for module music playback. No runtime assets: the animated banner GIF and
the XM music are embedded into the executable as byte arrays.

## Features

- Window "SCT" (560x560) with an animated 500x281 banner (`src/assets/banner.gif`)
- Tab bar:
  - **SCT**: `Label1/EditBox1/[P]`, `Label2/EditBox2/[C]`, `[G]` aligned under P and C
  - **Config**: `music` group box with Play / Pause / Stop; `window` group box
    with an Opacity slider (10-100%, default 85%, live, whole-window)
  - **About**: SCT centered
- Custom `SCTGuiTextBox` (src/sct_textbox.c) with a real selection model on top of
  the raygui look: Ctrl+A/C/X/V, triple-click select-all, Backspace/Delete/type
  over selection, right-click context menu.
  Logic self-check: run with `SCT_SELFTEST=1` (logs 8 assertions).
- Music (`src/assets/music.xm`) autoplays at launch, rendered by libxmp-lite into a
  raylib `AudioStream` (all libxmp calls happen on the audio thread; the UI posts
  atomic commands)

## Layout

```
src/            application sources (main.c, banner.c, xmplayer.c, assets_gen.py)
src/assets/     banner.gif, music.xm (build inputs; embedded by assets_gen.py)
third_party/    raygui.h (kept), raylib/ (cloned at release 6.0, gitignored)
prebuilt/       libxmp-lite static libraries + header for all 5 targets
build.sh        builds every target below
build/<target>/ sct binaries (sct or sct.exe)
```

## Build

Requires: cmake, ninja/make, python3, plus native gcc and the mingw/linux cross
toolchains for the targets you want.

```
cd sct-raylib_raygui
git clone --depth 1 --branch 6.0 https://github.com/raysan5/raylib third_party/raylib
./build.sh                 # all: aarch64 x64 x86 win-x64 win-x86
./build.sh win-x64         # subset
```

Targets produced:

| Target         | Binary                | Notes                          |
|----------------|-----------------------|--------------------------------|
| `aarch64`      | `build/aarch64/sct`   | native Linux aarch64           |
| `x64`          | `build/x64/sct`       | cross Linux x86-64             |
| `x86`          | `build/x86/sct`       | cross Linux i686               |
| `win-x64`      | `build/win-x64/sct.exe` | mingw-w64 Windows x64        |
| `win-x86`      | `build/win-x86/sct.exe` | mingw-w64 Windows i386       |

Everything except libc/GL/X11 (Linux) and the Windows system DLLs is statically linked:
raylib, raygui, libxmp-lite and both embedded assets.

## Changing assets

Replace `src/assets/banner.gif` (500x281 recommended) or `src/assets/music.xm` and
re-run `./build.sh`. `assets_gen.py` regenerates `src/assets.c/h` on every build.
