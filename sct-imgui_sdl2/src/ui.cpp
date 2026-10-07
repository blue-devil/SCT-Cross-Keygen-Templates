#include "ui.h"

#include <cstdint>
#include <cstdlib>
#include <string.h>

#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wold-style-cast"
#pragma GCC diagnostic ignored "-Wuseless-cast"
#pragma GCC diagnostic ignored "-Wcast-qual"
#include <SDL.h>

#include "imgui.h"
#pragma GCC diagnostic pop

#include "xmplayer.h"

#define WIN_W 508.0f
#define WIN_H 540.0f

#define BANNER_X 4.0f
#define BANNER_Y 4.0f
#define BANNER_W 500.0f
#define BANNER_H 281.0f

#define TAB_X 12.0f
#define TAB_Y 293.0f
#define TAB_W 484.0f

#define GRP_X 12.0f
#define GRP_Y 333.0f
#define GRP_W 484.0f
#define GRP_H 152.0f

/* SCT tab rows, relative to the group box. Row height is ImGui's natural
 * frame height (see row_height()); rows are spaced height + ROW_GAP. */
#define ROW1_Y 36.0f
#define ROW_GAP 12.0f
#define LBL_X 24.0f
#define LBL_W 72.0f
#define EDT_X 98.0f
#define EDT_W 300.0f
#define BTN_X 412.0f
#define BTN_W 48.0f

/* Config tab: two boxes over the SCT box footprint */
#define CFG_GAP 12.0f
#define CFG_W 236.0f
#define THEME_X (CFG_W + CFG_GAP)

#define MUS_W 64.0f
#define MUS_H 32.0f
#define MUS_GAP 8.0f
#define MUS_Y 56.0f

#define BTN_EXIT_W 96.0f
#define BTN_EXIT_H 32.0f
#define BTN_EXIT_X (GRP_X + BTN_X + BTN_W - BTN_EXIT_W) /* right edge = P/C/G */
#define BTN_EXIT_Y 496.0f

#define TRIPLE_CLICK_S 0.5

/* ---------- group box: bordered child + title on the top border ---------- */

static void group_begin(const char *id, ImVec2 pos, ImVec2 size)
{
    ImGui::SetCursorPos(pos);
    ImGui::BeginChild(id, size, ImGuiChildFlags_Borders);
    ImGui::SetCursorPos(ImVec2(0, 0));
}

static void group_end(const char *title)
{
    ImGui::EndChild();
    /* title over the top border, raygui-style: a window-colored chip behind
     * the text breaks the border line */
    ImVec2 rmin = ImGui::GetItemRectMin();
    ImDrawList *dl = ImGui::GetWindowDrawList();
    ImVec2 ts = ImGui::CalcTextSize(title);
    ImVec2 a(rmin.x + 8.0f, rmin.y - ts.y * 0.5f);
    ImVec2 b(a.x + ts.x + 8.0f, rmin.y + ts.y * 0.5f + 1.0f);
    dl->AddRectFilled(a, b, ImGui::GetColorU32(ImGuiCol_WindowBg));
    dl->AddText(ImVec2(a.x + 4.0f, a.y), ImGui::GetColorU32(ImGuiCol_Text), title);
}

/* ---------- text-box helpers ---------- */

static int tb_len(const TBState *s) { return static_cast<int>(strlen(s->buf)); }

static void tb_select_all(TBState *s)
{
    s->sel_start = 0;
    s->sel_end = tb_len(s);
    s->cursor = s->sel_end;
    s->select_all_req = true;
}

static void tb_copy(const TBState *s)
{
    if (s->sel_end > s->sel_start) {
        char tmp[UI_EDIT_CAP];
        int n = s->sel_end - s->sel_start;
        memcpy(tmp, s->buf + s->sel_start, static_cast<size_t>(n));
        tmp[n] = '\0';
        ImGui::SetClipboardText(tmp);
    }
}

static void tb_cut(TBState *s)
{
    if (s->sel_end <= s->sel_start) return;
    tb_copy(s);
    memmove(s->buf + s->sel_start, s->buf + s->sel_end,
            strlen(s->buf + s->sel_end) + 1);
    s->cursor = s->sel_start;
    s->sel_end = s->sel_start;
}

static void tb_paste(TBState *s)
{
    /* snapshot the clipboard first: tb_cut() below calls SetClipboardText,
     * which invalidates the pointer GetClipboardText returned */
    char clip[UI_EDIT_CAP];
    const char *p = ImGui::GetClipboardText();
    if (!p || !p[0]) return;
    int ilen = static_cast<int>(strlen(p));
    if (ilen > UI_EDIT_CAP - 1) ilen = UI_EDIT_CAP - 1;
    memcpy(clip, p, static_cast<size_t>(ilen));
    clip[ilen] = '\0';

    if (s->sel_end > s->sel_start) tb_cut(s);
    int len = tb_len(s);
    if (ilen > UI_EDIT_CAP - 1 - len) ilen = UI_EDIT_CAP - 1 - len;
    if (ilen <= 0) return;
    memmove(s->buf + s->cursor + ilen, s->buf + s->cursor,
            static_cast<size_t>(len - s->cursor) + 1);
    memcpy(s->buf + s->cursor, clip, static_cast<size_t>(ilen));
    s->cursor += ilen;
    s->sel_start = s->sel_end = s->cursor;
}

static void tb_clear(TBState *s)
{
    memset(s, 0, sizeof(*s));
}

/* ImGui InputText callback (CallbackAlways): caches selection/caret and
 * applies edit requests through the supported data API while active. */
static int tb_callback(ImGuiInputTextCallbackData *d)
{
    TBState *s = static_cast<TBState *>(d->UserData);
    s->sel_start = d->SelectionStart;
    s->sel_end = d->SelectionEnd;
    s->cursor = d->CursorPos;

    if (s->select_all_req) {
        d->SelectionStart = 0;
        d->SelectionEnd = d->BufTextLen;
        s->select_all_req = false;
    }
    if (s->cut_req) {
        s->cut_req = false;
        if (d->SelectionEnd > d->SelectionStart) {
            char tmp[UI_EDIT_CAP];
            int n = d->SelectionEnd - d->SelectionStart;
            memcpy(tmp, d->Buf + d->SelectionStart, static_cast<size_t>(n));
            tmp[n] = '\0';
            ImGui::SetClipboardText(tmp);
            d->DeleteChars(d->SelectionStart, n);
        }
    }
    if (s->paste_req) {
        s->paste_req = false;
        const char *clip = ImGui::GetClipboardText();
        if (clip && clip[0]) {
            if (d->SelectionEnd > d->SelectionStart)
                d->DeleteChars(d->SelectionStart, d->SelectionEnd - d->SelectionStart);
            d->InsertChars(d->SelectionStart, clip);
        }
    }
    return 0;
}

/* One edit box: label + InputText + triple-click select-all + context menu.
 * Positions are relative to the current group child. The InputText uses
 * ImGui's natural frame height; labels and buttons are matched to it so a
 * row has one uniform height. */
static float row_height() { return ImGui::GetFrameHeight(); }

static void edit_box(const char *id, const char *label, TBState *s, float row_y)
{
    float h = row_height();
    ImVec2 ts = ImGui::CalcTextSize(label);
    ImGui::SetCursorPos(ImVec2(LBL_X, row_y + (h - ts.y) * 0.5f));
    ImGui::TextUnformatted(label);
    ImGui::SetCursorPos(ImVec2(EDT_X, row_y));
    ImGui::SetNextItemWidth(EDT_W);
    ImGui::InputText(id, s->buf, UI_EDIT_CAP, ImGuiInputTextFlags_CallbackAlways,
                     tb_callback, s);
    bool active = ImGui::IsItemActive();

    /* triple-click select-all */
    if (ImGui::IsItemHovered() && ImGui::IsMouseClicked(0)) {
        double t = ImGui::GetTime();
        s->clicks = (t - s->last_click <= TRIPLE_CLICK_S) ? s->clicks + 1 : 1;
        s->last_click = t;
        if (s->clicks >= 3) {
            tb_select_all(s);
            s->clicks = 0;
        }
    }

    if (ImGui::BeginPopupContextItem()) {
        if (ImGui::MenuItem("Cut")) {
            if (active) s->cut_req = true;
            else tb_cut(s);
        }
        if (ImGui::MenuItem("Copy")) tb_copy(s);
        if (ImGui::MenuItem("Paste")) {
            if (active) s->paste_req = true;
            else tb_paste(s);
        }
        if (ImGui::MenuItem("Select All")) tb_select_all(s);
        ImGui::EndPopup();
    }
}

/* ---------- theme ---------- */

static void apply_theme(int theme)
{
    if (theme == 0) ImGui::StyleColorsDark();
    else ImGui::StyleColorsLight();
}

/* ---------- tab bodies ---------- */

static void draw_tab_sct(UiState *ui)
{
    group_begin("grp_sct", ImVec2(GRP_X, GRP_Y), ImVec2(GRP_W, GRP_H));

    float h = row_height();
    float row2 = ROW1_Y + h + ROW_GAP;
    float row3 = row2 + h + ROW_GAP;
    edit_box("##edit1", "Label1", &ui->tb1, ROW1_Y);
    ImGui::SetCursorPos(ImVec2(BTN_X, ROW1_Y));
    if (ImGui::Button("P", ImVec2(BTN_W, h))) {
        /* paste clipboard into EditBox1 */
        const char *clip = ImGui::GetClipboardText();
        if (clip) {
            strncpy(ui->tb1.buf, clip, UI_EDIT_CAP - 1);
            ui->tb1.buf[UI_EDIT_CAP - 1] = '\0';
            ui->tb1.cursor = tb_len(&ui->tb1);
            ui->tb1.sel_start = ui->tb1.sel_end = ui->tb1.cursor;
        }
    }

    edit_box("##edit2", "Label2", &ui->tb2, row2);
    ImGui::SetCursorPos(ImVec2(BTN_X, row2));
    if (ImGui::Button("C", ImVec2(BTN_W, h))) {
        /* copy EditBox2 to clipboard, only when non-empty */
        if (ui->tb2.buf[0] != '\0') ImGui::SetClipboardText(ui->tb2.buf);
    }

    ImGui::SetCursorPos(ImVec2(BTN_X, row3));
    ImGui::Button("GGG", ImVec2(BTN_W, h));

    group_end("SCT");
}

static void draw_tab_config(UiState *ui, SDL_Window *win)
{
    /* music box, left */
    group_begin("grp_music", ImVec2(GRP_X, GRP_Y), ImVec2(CFG_W, GRP_H));
    float bx = (CFG_W - (3.0f * MUS_W + 2.0f * MUS_GAP)) * 0.5f;
    ImGui::SetCursorPos(ImVec2(bx, MUS_Y));
    if (ImGui::Button("Play", ImVec2(MUS_W, MUS_H))) xmplayer_play();
    ImGui::SetCursorPos(ImVec2(bx + MUS_W + MUS_GAP, MUS_Y));
    if (ImGui::Button("Pause", ImVec2(MUS_W, MUS_H))) xmplayer_pause();
    ImGui::SetCursorPos(ImVec2(bx + 2.0f * (MUS_W + MUS_GAP), MUS_Y));
    if (ImGui::Button("Stop", ImVec2(MUS_W, MUS_H))) xmplayer_stop();
    group_end("music");

    /* theme box, right */
    ImGui::SameLine();
    group_begin("grp_theme", ImVec2(GRP_X + THEME_X, GRP_Y), ImVec2(CFG_W, GRP_H));
    static const char *themes[] = { "Dark", "Light" };
    ImGui::SetCursorPos(ImVec2(24.0f, MUS_Y + 4.0f));
    ImGui::SetNextItemWidth(CFG_W - 48.0f);
    if (ImGui::BeginCombo("##theme", themes[ui->theme])) {
        for (int i = 0; i < 2; i++) {
            if (ImGui::Selectable(themes[i]) && ui->theme != i) {
                ui->theme = i;
                apply_theme(i);
            }
        }
        ImGui::EndCombo();
    }

    /* window opacity under the theme combo: compositor-level alpha for the
     * whole window; slider bounds enforce the floor so it is never invisible */
    ImGui::SetCursorPos(ImVec2(24.0f, MUS_Y + 4.0f + ImGui::GetFrameHeight() + 12.0f));
    ImGui::SetNextItemWidth(CFG_W - 48.0f);
    if (ImGui::SliderInt("##opacity", &ui->opacity, UI_OPACITY_MIN, UI_OPACITY_MAX, "%d%%"))
        SDL_SetWindowOpacity(win, static_cast<float>(ui->opacity) * 0.01f);
    group_end("theme");
}

static void draw_tab_about()
{
    group_begin("grp_about", ImVec2(GRP_X, GRP_Y), ImVec2(GRP_W, GRP_H));
    ImGui::PushFont(ImGui::GetFont(), 48.0f);
    ImVec2 ts = ImGui::CalcTextSize("SCT");
    ImGui::SetCursorPos(ImVec2((GRP_W - ts.x) * 0.5f, (GRP_H - ts.y) * 0.5f));
    ImGui::TextUnformatted("SCT");
    ImGui::PopFont();
    group_end("About");
}

/* ---------- self-test (SCT_SELFTEST=1): pure helper logic ---------- */

void ui_selftest(UiState *ui)
{
    int pass = 0, total = 0;
    TBState a;
    tb_clear(&a);

#define CHECK(cond, name) do { total++; if (cond) { pass++; \
    SDL_Log( "SELFTEST PASS: %s", name); } \
    else SDL_Log( "SELFTEST FAIL: %s", name); } while (0)

    ImGui::SetClipboardText("HELLO");
    tb_paste(&a);
    CHECK(strcmp(a.buf, "HELLO") == 0, "paste fills empty box");

    tb_select_all(&a);
    CHECK(a.sel_start == 0 && a.sel_end == 5, "select all covers text");

    tb_copy(&a);
    CHECK(strcmp(ImGui::GetClipboardText(), "HELLO") == 0, "copy selection");

    tb_cut(&a);
    CHECK(a.buf[0] == '\0' && strcmp(ImGui::GetClipboardText(), "HELLO") == 0,
          "cut empties and copies");

    tb_clear(&a);
    strcpy(a.buf, "XY");
    a.cursor = 2;
    ImGui::SetClipboardText("ZW");
    tb_paste(&a);
    CHECK(strcmp(a.buf, "XYZW") == 0, "paste appends at caret");

    strcpy(a.buf, "ABC");
    a.sel_start = 0; a.sel_end = 3; a.cursor = 3;
    ImGui::SetClipboardText("Z");
    tb_paste(&a);
    CHECK(strcmp(a.buf, "Z") == 0, "paste replaces selection");

    SDL_Log( "SELFTEST: %d/%d passed", pass, total);
#undef CHECK
    (void)ui;
}

/* ---------- the whole UI ---------- */

void ui_init(UiState *ui)
{
    memset(ui, 0, sizeof(*ui));
    ui->theme = 0; /* Dark at every start; nothing persisted */
    ui->opacity = UI_OPACITY_DEFAULT;
}

void ui_draw(UiState *ui, const Banner *banner, SDL_Window *win)
{
    ImGuiIO &io = ImGui::GetIO();
    ImGui::SetNextWindowPos(ImVec2(0, 0));
    ImGui::SetNextWindowSize(io.DisplaySize);
    ImGui::Begin("##sct", nullptr,
                 ImGuiWindowFlags_NoDecoration | ImGuiWindowFlags_NoMove |
                 ImGuiWindowFlags_NoSavedSettings | ImGuiWindowFlags_NoBringToFrontOnFocus);

    /* banner */
    if (banner->count > 0) {
        ImGui::SetCursorPos(ImVec2(BANNER_X, BANNER_Y));
        ImGui::Image(static_cast<ImTextureID>(banner_tex(banner)),
                     ImVec2(static_cast<float>(banner->width),
                            static_cast<float>(banner->height)));
    }

    /* tabs */
    ImGui::SetCursorPos(ImVec2(TAB_X, TAB_Y));
    if (ImGui::BeginTabBar("##tabs", ImGuiTabBarFlags_None)) {
        if (ImGui::BeginTabItem("SCT")) {
            draw_tab_sct(ui);
            ImGui::EndTabItem();
        }
        if (ImGui::BeginTabItem("Config")) {
            draw_tab_config(ui, win);
            ImGui::EndTabItem();
        }
        if (ImGui::BeginTabItem("About")) {
            draw_tab_about();
            ImGui::EndTabItem();
        }
        ImGui::EndTabBar();
    }

    /* exit, bottom-right, right edge aligned with the P/C/G column */
    ImGui::SetCursorPos(ImVec2(BTN_EXIT_X, BTN_EXIT_Y));
    if (ImGui::Button("Exit", ImVec2(BTN_EXIT_W, BTN_EXIT_H))) ui->quit = true;

    ImGui::End();
}
