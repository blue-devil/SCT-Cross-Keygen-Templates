#ifndef SCT_UI_H
#define SCT_UI_H

/* All Dear ImGui widgets for SCT: tabs, group boxes, text boxes, buttons,
 * theme combo, window opacity slider, About. Layout constants mirror the
 * raygui version (cau/src/main.c): group boxes share one footprint, the
 * Exit button's right edge aligns with the P/C/G button column. */

#include "banner.h"

#define UI_EDIT_CAP 128
#define UI_OPACITY_MIN 10     /* % floor: the window is never invisible */
#define UI_OPACITY_DEFAULT 85
#define UI_OPACITY_MAX 100

/* One text box: buffer + the small amount of state ImGui needs from us
 * (click counting for triple-click, edit requests routed through the
 * InputText callback while the item is active). */
struct TBState {
    char buf[UI_EDIT_CAP];
    int sel_start;        /* cached from the callback (ordered) */
    int sel_end;
    int cursor;
    int clicks;
    double last_click;
    bool select_all_req;  /* apply in the next callback pass */
    bool cut_req;
    bool paste_req;
};

struct UiState {
    TBState tb1, tb2;
    int theme;            /* 0 = Dark (default), 1 = Light */
    int opacity;          /* window opacity % (UI_OPACITY_*), applied live */
    bool quit;
};

struct SDL_Window;

void ui_init(UiState *ui);
void ui_draw(UiState *ui, const Banner *banner, SDL_Window *win);

/* Logic self-test for the text-box helpers (run when SCT_SELFTEST is set). */
void ui_selftest(UiState *ui);

#endif /* SCT_UI_H */
