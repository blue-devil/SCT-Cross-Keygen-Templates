/* SCT -- SDL2 + Dear ImGui desktop app with embedded banner and XM music.
 * Port of the raygui version (sct-raylib_raygui/); see PLAN.md. */
#include <SDL.h>

#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wold-style-cast"
#pragma GCC diagnostic ignored "-Wuseless-cast"
#pragma GCC diagnostic ignored "-Wcast-qual"
#pragma GCC diagnostic ignored "-Wconversion"
#include "imgui.h"
#include "imgui_internal.h"     /* ClearActiveID: leave an edit box on ESC */
#include "backends/imgui_impl_sdl2.h"
#include "backends/imgui_impl_opengl3.h"
#pragma GCC diagnostic pop
#include <GL/gl.h>

#include <cstdint>
#include <cstdlib>

#include "assets.h"
#include "banner.h"
#include "ui.h"
#include "xmplayer.h"

#define WIN_W 508
#define WIN_H 540

/* Window icon from the embedded PNG (decoded with stb via banner.c). */
static void set_window_icon(SDL_Window *win)
{
    int w = 0, h = 0;
    unsigned char *rgba = NULL;
    if (banner_decode_png(icon_64, static_cast<int>(icon_64_len), &w, &h, &rgba) != 0)
        return;
    SDL_Surface *surf = SDL_CreateRGBSurfaceWithFormatFrom(
        rgba, w, h, 32, 4 * w, SDL_PIXELFORMAT_RGBA32);
    if (surf) {
        SDL_SetWindowIcon(win, surf);
        SDL_FreeSurface(surf);
    }
    free(rgba);
}

int main(int argc, char **argv)
{
    (void)argc;
    (void)argv;

    if (SDL_Init(SDL_INIT_VIDEO | SDL_INIT_AUDIO | SDL_INIT_EVENTS) != 0) {
        SDL_Log("SDL_Init failed: %s", SDL_GetError());
        return 1;
    }
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_MAJOR_VERSION, 3);
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_MINOR_VERSION, 3);
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_PROFILE_MASK, SDL_GL_CONTEXT_PROFILE_CORE);

    SDL_Window *win = SDL_CreateWindow("SCT", SDL_WINDOWPOS_CENTERED,
                                       SDL_WINDOWPOS_CENTERED, WIN_W, WIN_H,
                                       SDL_WINDOW_OPENGL);
    if (!win) {
        SDL_Log("SDL_CreateWindow failed: %s", SDL_GetError());
        return 1;
    }
    set_window_icon(win);
    SDL_GLContext gl = SDL_GL_CreateContext(win);
    if (!gl) {
        SDL_Log("SDL_GL_CreateContext failed: %s", SDL_GetError());
        return 1;
    }
    SDL_GL_MakeCurrent(win, gl);
    SDL_GL_SetSwapInterval(1);              /* vsync */

    ImGui::CreateContext();
    ImGui::GetIO().IniFilename = nullptr;   /* no imgui.ini, nothing on disk */
    ImGui::StyleColorsDark();
    ImGui_ImplSDL2_InitForOpenGL(win, gl);
    ImGui_ImplOpenGL3_Init();

    Banner banner;
    if (banner_init(&banner, banner_gif, static_cast<int>(banner_gif_len)) != 0)
        SDL_Log("MAIN: banner unavailable");

    xmplayer_init(music_xm, static_cast<int>(music_xm_len));
    xmplayer_play();                        /* autoplay on launch */

    UiState ui;
    ui_init(&ui);

    /* default window opacity, compositor-level for the whole window */
    if (SDL_SetWindowOpacity(win, static_cast<float>(ui.opacity) * 0.01f) != 0)
        SDL_Log("MAIN: window opacity unsupported: %s", SDL_GetError());

    bool selftest = getenv("SCT_SELFTEST") != nullptr;
    int frame_no = 0;
    bool quit = false;

    while (!quit && !ui.quit) {
        SDL_Event ev;
        while (SDL_PollEvent(&ev)) {
            ImGui_ImplSDL2_ProcessEvent(&ev);
            if (ev.type == SDL_QUIT) quit = true;
            if (ev.type == SDL_KEYDOWN && ev.key.keysym.sym == SDLK_ESCAPE) {
                /* ESC: leave an active edit box first, exit only when idle */
                if (ImGui::GetIO().WantTextInput) ImGui::ClearActiveID();
                else quit = true;
            }
        }

        ImGui_ImplSDL2_NewFrame();
        ImGui_ImplOpenGL3_NewFrame();
        ImGui::NewFrame();

        if (selftest && ++frame_no == 120)
            ui_selftest(&ui);

        ui_draw(&ui, &banner, win);
        banner_update(&banner, ImGui::GetIO().DeltaTime);

        ImGui::Render();
        int dw = 0, dh = 0;
        SDL_GL_GetDrawableSize(win, &dw, &dh);
        glViewport(0, 0, dw, dh);
        glClearColor(0.0f, 0.0f, 0.0f, 1.0f);
        glClear(GL_COLOR_BUFFER_BIT);
        ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());
        SDL_GL_SwapWindow(win);
    }

    xmplayer_free();
    ImGui_ImplOpenGL3_Shutdown();
    ImGui_ImplSDL2_Shutdown();
    ImGui::DestroyContext();
    banner_free(&banner);
    SDL_GL_DeleteContext(gl);
    SDL_DestroyWindow(win);
    SDL_Quit();
    return 0;
}
