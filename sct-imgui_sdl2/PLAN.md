# SCT on SDL2 + Dear ImGui: plan (rev 2 — stack change)

Rebuild the `sct-raylib_raygui/` app (raylib + raygui) as an SDL2 + Dear ImGui app (C++), same user-visible behavior on the same five targets. `sct-raylib_raygui/` stays untouched; this is a sibling project.

## 0. Why the stack changed (rev 2)

The original plan used raylib + rlImGui. That path is dead on the user's Windows:

- rlImGui's renderer (raylib immediate-mode) draws **nothing** on the user's Windows/Parallels GL — black window with working input — with **both** the system mingw raylib and the source-built raylib that the raygui app renders fine with.
- Dear ImGui's **official** backends (`imgui_impl_opengl3` renderer) are the battle-tested path; rlImGui's custom renderer is not maintained to that standard.
- The user's `cross-examples/sdl2_imgui` build — SDL2 + official `imgui_impl_sdl2` + `imgui_impl_opengl3` — is **confirmed working by the user on Windows x86 and x64**.
- raylib and ImGui overlap (windows/input/GL); SDL2 is the thinner, standard base for an ImGui-centric app.

## 1. Feature parity contract (unchanged)

Same table as rev 1; all behavior targets carry over: window "SCT" 508x540 with icons, animated banner at (4,4) with GIF delays, autoplaying looping XM, tabs SCT/Config/About, SCT group box (Label1/EditBox1/P, Label2/EditBox2/C, GGG), P=paste/C=copy-if-non-empty, Config = music (Play/Pause/Stop) + theme combo (Dark default, Light live, no persistence), About = centered large "SCT", Exit right edge aligned to P/C/G, ESC leaves an edit box first then exits, edit boxes with selection/clipboard/context-menu/Tab navigation, no files read or written at run time, no console on Windows.

## 2. Stack (verified on this machine)

| Piece | Version | All 5 toolchains |
|---|---|---|
| SDL2 | 2.32.x | pkg-config present everywhere; static on all cross/mingw targets, shared-only on native (fine for Linux) |
| Dear ImGui core | 1.92.9 | static everywhere |
| `imgui_impl_sdl2` + `imgui_impl_opengl3` | 1.92.9 | static everywhere; user-confirmed rendering on Windows x86/x64 |
| libxmp-lite | sct-raylib_raygui prebuilts | static everywhere |

Windows: `-static` (fully static incl. SDL2, only system DLLs), GUI subsystem, `.ico` resource. The user's working `sdl2_imgui.exe` uses exactly this link shape.

## 3. What is kept as-is

- `src/ui.{cpp,h}`: the entire ImGui UI (tabs, group boxes, edit boxes with triple-click + context menu, theme, About, Exit). Only the banner image call changes (`rlImGuiImageSize` → `ImGui::Image` with a GL texture name).
- `src/assets_gen.py`, `icons_gen.sh`, `scrub_ident.py`, `sct.rc`, `prebuilt/libxmp-lite/`, embedded assets.
- The build/verify policy: `$CWARN` zero warnings, release strip + scrub, `BUILD=debug`, `SCT_SELFTEST` hook.

## 4. Ports (raylib → SDL2)

| Concern | Old (raylib) | New (SDL2) |
|---|---|---|
| Window/GL | `InitWindow`, rlgl | `SDL_Init(SDL_INIT_VIDEO|AUDIO)`, `SDL_CreateWindow` + `SDL_GL_CreateContext` (GL 3.3 core), `SDL_GL_SwapWindow` |
| ImGui platform | `ImGui_ImplRaylib_*` | `ImGui_ImplSDL2_InitForOpenGL` |
| ImGui renderer | rlImGui's | `ImGui_ImplOpenGL3_Init/RenderDrawData` (the path that already crashed locally under the split-backend attempt will be retested; the sdl2 reference works on the user's machine) |
| GIF decode | raylib `LoadImageAnimFromMemory` + own delay parser | vendored `stb_image.h` (`STBI_ONLY_GIF`): one call returns RGBA frames **and** per-frame delays — `banner.c` shrinks |
| Textures | raylib textures | plain GL: one texture per frame uploaded with `glTexImage2D` (`gl_tex.h` pattern from cross-examples), `GL_CLAMP`+`GL_LINEAR` |
| Music output | raylib `AudioStream` callback | `SDL_OpenAudioDevice` callback: same design — audio-thread state machine, `xmp_play_buffer(..., 0x7fffffff)`; only the callback signature and format negotiation change |
| Window icon | `SetWindowIcons` | decode embedded PNG (stb) → `SDL_CreateRGBSurfaceWithFormatFrom` → `SDL_SetWindowIcon`; Windows exe still uses the `.ico` resource for taskbar/Explorer |
| Main loop / ESC | raylib polling | `SDL_PollEvents`; ESC: if `io.WantTextInput` deactivate the edit box (`ImGui::ClearActiveID`), else quit |
| Time | `GetFrameTime` | `SDL_GetPerformanceCounter` delta (or ImGui's IO delta) |

`xmplayer.c` keeps its audio-thread architecture and libxmp calls; only the sink changes. Everything in `sct-raylib_raygui/AGENTS.md` about libxmp still applies (loop-count arg, sample-frame bytes, `-DLIBXMP_STATIC`).

## 5. build.sh (updated shape)

Same CLI (`./build.sh [targets]`, `BUILD=debug`). Per target: `<prefix>pkg-config --static --cflags --libs sdl2 imgui_impl_sdl2 imgui_impl_opengl3`; Linux adds GL/X11 libs as needed by the link; Windows adds `-static -mwindows`, `windres` resource, `SDL2main`. Release strip/scrub identical to rev 1. No raylib anywhere — `../sct-raylib_raygui` is no longer a build input.

## 6. Decisions (unchanged from rev 1)

Dark theme default with live Light switch (no persistence, no `imgui.ini`); About = plain ImGui large centered "SCT"; ESC leaves an edit box first.

## 7. Execution order

1. **Scaffold swap:** SDL2 window + GL context + official ImGui backends + `ui.cpp` linked in with the banner as a plain color rect; zero warnings on 5 targets; renders on `:0`.
2. **Banner:** stb GIF → GL textures → `ImGui::Image`; delays honored.
3. **Music:** SDL audio callback port of `xmplayer`; autoplay verified via `pactl`.
4. **Icons + ESC + console:** SDL icon, `.ico` resource, `-mwindows`, ESC semantics.
5. **Verification pass:** full scripted Xvfb run (tabs with label-width coordinates, theme flip, edit boxes, ESC), self-test, strip audit, PE validity + icons.
6. **Docs:** `AGENTS.md`/`README.md` rewritten for the SDL2 stack (keep the tab-coordinate and no-trace gotchas).

User tests Windows on real hardware; linux-aarch64 verified live here; x64/x86 under qemu at startup level.

## 8. Rev 3 — window opacity slider (2026-10)

Config tab, theme box, under the combo: "Opacity" `SliderInt` 10–100, default
85, applied live while dragging via `SDL_SetWindowOpacity()` (SDL ≥ 2.0.5;
whole-window compositor opacity: `_NET_WM_WINDOW_OPACITY` on X11, layered
window on Windows, no-op on Wayland). The 10% floor is enforced by the slider
bounds, so the window can never become invisible; nothing persists — every
launch starts at 85%. `ui_draw()` gained the `SDL_Window*` parameter for this.

Verified: default 85% and the 10% floor via `xprop` under Xvfb (no compositor
there — the property is the signal), 100% removes the property (= opaque),
and the user confirmed the live GUI on screen.
