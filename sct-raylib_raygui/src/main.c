/* SCT -- raylib + raygui desktop app with embedded banner and XM music. */
#include "raylib.h"
#define RAYGUI_IMPLEMENTATION
#include "raygui.h"

#include <stdbool.h>
#include <stdio.h>
#include <string.h>

#include "assets.h"
#include "banner.h"
#include "sct_textbox.h"
#include "xmplayer.h"

#define WIN_W 508
#define WIN_H 540

#define BANNER_W 500
#define BANNER_H 281
#define BANNER_X 4
#define BANNER_Y 4

#define TAB_X  12
#define TAB_Y  (BANNER_Y + BANNER_H + 8)
#define TAB_H  32
#define TAB_W  (WIN_W - 2 * TAB_X)

/* group box shared by the SCT and Config tabs */
#define CONTENT_Y (TAB_Y + TAB_H + 8)
#define GRP_X     TAB_X
#define GRP_Y     CONTENT_Y
#define GRP_W     TAB_W
#define GRP_H     152

/* SCT tab rows inside the group box */
#define ROW_H 28
#define ROW1_Y (GRP_Y + 36)
#define ROW2_Y (ROW1_Y + ROW_H + 12)
#define ROW3_Y (ROW2_Y + ROW_H + 12)
#define LBL_X  (GRP_X + 24)
#define LBL_W  80
#define EDT_X  (GRP_X + 98)
#define EDT_W  300
#define BTN_X  (GRP_X + GRP_W - 24 - 48)
#define BTN_W  48
#define EDT_CAP 128

/* Config tab: two side-by-side group boxes over the same footprint as GRP_* */
#define CFG_GAP  12
#define CFG_W    ((GRP_W - CFG_GAP) / 2)
#define MUSIC_X  GRP_X
#define THEME_X  (GRP_X + CFG_W + CFG_GAP)

/* music buttons, centered in the music box */
#define MUS_W   64
#define MUS_H   32
#define MUS_GAP 8
#define MUS_Y   (GRP_Y + 56)
#define MUS_X1  (MUSIC_X + (CFG_W - (3 * MUS_W + 2 * MUS_GAP)) / 2)
#define MUS_X2  (MUS_X1 + MUS_W + MUS_GAP)
#define MUS_X3  (MUS_X2 + MUS_W + MUS_GAP)

/* window opacity slider in the right Config box; floor keeps it visible */
#define OPA_MIN     10.0f
#define OPA_MAX     100.0f
#define OPA_DEFAULT 85.0f

#define BTN_EXIT_W 96
#define BTN_EXIT_H 32
#define BTN_EXIT_X (BTN_X + BTN_W - BTN_EXIT_W)   /* right edge aligned with P/C/G */
#define BTN_EXIT_Y (WIN_H - 12 - BTN_EXIT_H)

static void textbox(Rectangle b, char *buf, int cap, int *edit, int *other, SCTTextBox *tb)
{
    /* left-click enters edit mode (never leaves it: caret clicks/triple-clicks
     * must work while editing); the control itself reports when editing ends */
    if (CheckCollisionPointRec(GetMousePosition(), b) &&
        IsMouseButtonPressed(MOUSE_BUTTON_LEFT) && !*edit) {
        *edit = 1;
        if (other) *other = 0;
    }
    if (SCTGuiTextBox(b, buf, cap, *edit != 0, tb) == 1)
        *edit = 0;                    /* ENTER or click outside ends editing */
}

/* Window icon from the embedded PNGs. On Linux this sets _NET_WM_ICON, which
 * docks (Plank etc.) and taskbars show; on Windows the exe resource
 * (src/sct.rc) already provides it and this keeps small sizes crisp. */
static void set_window_icons(void)
{
    const unsigned char *data[] = { icon_32, icon_64, icon_256 };
    const unsigned int len[] = { icon_32_len, icon_64_len, icon_256_len };
    Image imgs[3];
    for (int i = 0; i < 3; i++) {
        imgs[i] = LoadImageFromMemory(".png", data[i], (int)len[i]);
        ImageFormat(&imgs[i], PIXELFORMAT_UNCOMPRESSED_R8G8B8A8);
    }
    SetWindowIcons(imgs, 3);
    for (int i = 0; i < 3; i++) UnloadImage(imgs[i]);
}

int main(void)
{
    InitWindow(WIN_W, WIN_H, "SCT");
    set_window_icons();
    SetTargetFPS(60);

    Banner banner;
    if (banner_init(&banner, banner_gif, (int)banner_gif_len) != 0)
        TraceLog(LOG_WARNING, "MAIN: banner unavailable");

    InitAudioDevice();
    xmplayer_init(music_xm, (int)music_xm_len);
    xmplayer_play();                        /* autoplay on launch */
    bool selftest = getenv("SCT_SELFTEST") != NULL;
    int frame_no = 0;
    static char edit1[EDT_CAP] = "";
    static char edit2[EDT_CAP] = "";
    static SCTTextBox tb1, tb2;
    SCTTextBoxInit(&tb1);
    SCTTextBoxInit(&tb2);
    int active1 = 0, active2 = 0;
    int tab = 0, hscroll = 0;
    bool quit = false;
    float opacity = OPA_DEFAULT, last_opacity = OPA_DEFAULT;
    SetWindowOpacity(opacity * 0.01f);       /* default opacity at launch */

    while (!quit && !WindowShouldClose()) {
        banner_update(&banner, GetFrameTime());
        if (selftest && ++frame_no == 120)
            SCTTextBoxSelfTest(&tb1, edit1, &tb2, edit2, EDT_CAP);
        /* Tab / Shift+Tab: jump focus between the two edit boxes */
        if (IsKeyPressed(KEY_TAB) && (active1 || active2)) {
            SCTTextBox *to = active1 ? &tb2 : &tb1;
            char *buf = active1 ? edit2 : edit1;
            active1 = !active1;
            active2 = !active2;
            to->dragging = false;
            to->cursor = (int)strlen(buf);
            to->sel_start = to->sel_end = to->cursor;
        }

        BeginDrawing();
        ClearBackground(GetColor(GuiGetStyle(DEFAULT, BACKGROUND_COLOR)));

        banner_draw(&banner, BANNER_X, BANNER_Y);

        GuiTabBar((Rectangle){ TAB_X, TAB_Y, TAB_W, TAB_H },
                  "SCT;Config;About", &hscroll, &tab);

        if (tab == 0) {
            GuiGroupBox((Rectangle){ GRP_X, GRP_Y, GRP_W, GRP_H }, "SCT");

            GuiLabel((Rectangle){ LBL_X, ROW1_Y, LBL_W, ROW_H }, "Label1");
            textbox((Rectangle){ EDT_X, ROW1_Y, EDT_W, ROW_H }, edit1, EDT_CAP, &active1, &active2, &tb1);
            if (GuiButton((Rectangle){ BTN_X, ROW1_Y, BTN_W, ROW_H }, "P")) {
                /* paste clipboard into editbox1 */
                const char *clip = GetClipboardText();
                if (clip) {
                    strncpy(edit1, clip, EDT_CAP - 1);
                    edit1[EDT_CAP - 1] = '\0';
                    tb1.cursor = (int)strlen(edit1);
                    tb1.sel_start = tb1.sel_end = tb1.cursor;
                }
            }

            GuiLabel((Rectangle){ LBL_X, ROW2_Y, LBL_W, ROW_H }, "Label2");
            textbox((Rectangle){ EDT_X, ROW2_Y, EDT_W, ROW_H }, edit2, EDT_CAP, &active2, &active1, &tb2);
            if (GuiButton((Rectangle){ BTN_X, ROW2_Y, BTN_W, ROW_H }, "C")) {
                /* copy editbox2 to clipboard (only if there is text) */
                if (edit2[0] != '\0') SetClipboardText(edit2);
            }

            if (GuiButton((Rectangle){ BTN_X, ROW3_Y, BTN_W, ROW_H }, "GGG")) { }
        } else if (tab == 1) {
            GuiGroupBox((Rectangle){ MUSIC_X, GRP_Y, CFG_W, GRP_H }, "music");
            if (GuiButton((Rectangle){ MUS_X1, MUS_Y, MUS_W, MUS_H }, "Play"))  xmplayer_play();
            if (GuiButton((Rectangle){ MUS_X2, MUS_Y, MUS_W, MUS_H }, "Pause")) xmplayer_pause();
            if (GuiButton((Rectangle){ MUS_X3, MUS_Y, MUS_W, MUS_H }, "Stop"))  xmplayer_stop();

            GuiGroupBox((Rectangle){ THEME_X, GRP_Y, CFG_W, GRP_H }, "window");
            int align = GuiGetStyle(LABEL, TEXT_ALIGNMENT);
            GuiSetStyle(LABEL, TEXT_ALIGNMENT, TEXT_ALIGN_CENTER);
            GuiLabel((Rectangle){ THEME_X, MUS_Y, CFG_W, 24 },
                     TextFormat("Opacity: %d%%", (int)opacity));
            GuiSetStyle(LABEL, TEXT_ALIGNMENT, align);
            GuiSlider((Rectangle){ THEME_X + 24, MUS_Y + 32, CFG_W - 48, 24 },
                      NULL, NULL, &opacity, OPA_MIN, OPA_MAX);
            if (opacity != last_opacity) {
                SetWindowOpacity(opacity * 0.01f);   /* live while dragging */
                last_opacity = opacity;
            }
        } else {
            GuiGroupBox((Rectangle){ GRP_X, GRP_Y, GRP_W, GRP_H }, "About");
            Color fg = GetColor(GuiGetStyle(LABEL, TEXT_COLOR_NORMAL));
            DrawText("SCT",
                     GRP_X + (GRP_W - MeasureText("SCT", 48)) / 2,
                     GRP_Y + (GRP_H - 48) / 2,
                     48, fg);
        }

        if (GuiButton((Rectangle){ BTN_EXIT_X, BTN_EXIT_Y, BTN_EXIT_W, BTN_EXIT_H }, "Exit"))
            quit = true;

        /* context menus on top of everything */
        SCTTextBoxDrawMenu(&tb1, edit1, EDT_CAP);
        SCTTextBoxDrawMenu(&tb2, edit2, EDT_CAP);

        EndDrawing();
    }

    CloseAudioDevice();
    xmplayer_free();
    banner_free(&banner);
    CloseWindow();
    return 0;
}
