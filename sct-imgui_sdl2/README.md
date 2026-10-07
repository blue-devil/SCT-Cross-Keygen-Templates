# SCT (SDL2 + Dear ImGui)

Same app as `../sct-raylib_raygui/` (raylib + raygui), built on **SDL2 + Dear ImGui
1.92** with the official `imgui_impl_sdl2` + `imgui_impl_opengl3` backends:
single "SCT" window with an animated embedded banner, XM music (libxmp-lite)
that autoplays and loops, three tabs, embedded assets, five build targets,
stripped release binaries.

## Build

```
./build.sh                 # release, all targets: aarch64 x64 x86 win-x64 win-x86
./build.sh aarch64         # one target
BUILD=debug ./build.sh     # unstripped -O0 -g build
```

| Target | Binary | Notes |
|---|---|---|
| `aarch64` | `build/aarch64/sct` | native, links shared libSDL2 |
| `x64` / `x86` | `build/x64/sct`, `build/x86/sct` | cross Linux, static |
| `win-x64` / `win-x86` | `build/win-{x64,x86}/sct.exe` | mingw, fully static, GUI subsystem, icon resource |

Needs the installed `sdl2`, `imgui_impl_sdl2`, `imgui_impl_opengl3` and
`imgui` packages for each toolchain used. Release binaries are stripped and
scrubbed: no symbol tables, debug info, home paths or compiler-version
strings.

## Features

- 508x540 window "SCT", animated 500x281 banner at a 4 px margin, window icons
- Tabs: **SCT** (Label1/EditBox1/[P], Label2/EditBox2/[C], [GGG]), **Config**
  (music: Play/Pause/Stop; theme: Dark/Light combo, live; window opacity
  slider 10-100%, default 85%), **About** (large centered "SCT")
- P pastes the clipboard into EditBox1; C copies EditBox2 when non-empty
- Edit boxes: drag-select, triple-click and Ctrl+A select all, Shift+arrows,
  Ctrl+C/X/V, right-click menu, Tab/Shift+Tab between boxes
- Exit button (right-aligned with the P/C/G column), ESC exits when idle and
  leaves an active edit box first, window close always exits
- Music autoplays and loops forever; window opacity is live, whole-window
  (compositor-level), starts at 85% each launch and never goes fully
  transparent (slider floor 10); nothing is read or written on disk at run
  time (no `imgui.ini`)

## Assets

`src/assets/` holds the banner GIF, the XM module and the icon set; they are
embedded at build time by `src/assets_gen.py`. Regenerate icons from the SVG
with `src/icons_gen.sh` (needs `rsvg-convert` + `icotool`).
