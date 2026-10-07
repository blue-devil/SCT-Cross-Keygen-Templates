#include "sct_textbox.h"

#include <ctype.h>
#include <stdlib.h>
#include <string.h>

#include "raygui.h"

#define AUTO_REPEAT_COOLDOWN 20   /* frames before held keys repeat (raygui value) */
#define TRIPLE_CLICK_S       0.5  /* seconds between clicks counting as triple */

/* ---------- small helpers ---------- */

/* Minimal UTF-8 encoder (raylib 6.0 has no public EncodeCodepoint). */
static int utf8_encode(int cp, char *out)
{
    if (cp < 0x80) { out[0] = (char)cp; return 1; }
    if (cp < 0x800) {
        out[0] = (char)(0xc0 | (cp >> 6));
        out[1] = (char)(0x80 | (cp & 0x3f));
        return 2;
    }
    if (cp < 0x10000) {
        out[0] = (char)(0xe0 | (cp >> 12));
        out[1] = (char)(0x80 | ((cp >> 6) & 0x3f));
        out[2] = (char)(0x80 | (cp & 0x3f));
        return 3;
    }
    out[0] = (char)(0xf0 | (cp >> 18));
    out[1] = (char)(0x80 | ((cp >> 12) & 0x3f));
    out[2] = (char)(0x80 | ((cp >> 6) & 0x3f));
    out[3] = (char)(0x80 | (cp & 0x3f));
    return 4;
}

/* raygui 5.x flat property layout: per-state triplets stride 3. */
static int border_prop(int state) { return BORDER_COLOR_NORMAL + state * 3; }
static int text_prop(int state)   { return TEXT_COLOR_NORMAL + state * 3; }

static int text_len(const char *text) { return (int)strlen(text); }

static bool has_selection(const SCTTextBox *tb)
{
    return tb->sel_start != tb->sel_end;
}

static int sel_lo(const SCTTextBox *tb)
{
    return (tb->sel_start < tb->sel_end) ? tb->sel_start : tb->sel_end;
}

static int sel_hi(const SCTTextBox *tb)
{
    return (tb->sel_start > tb->sel_end) ? tb->sel_start : tb->sel_end;
}

static void clear_selection(SCTTextBox *tb)
{
    tb->sel_start = tb->sel_end = tb->cursor;
}

/* Pixel width of text[from..to), matching DrawTextEx exactly:
 * glyph advances + spacing between glyphs (n-1 spacings for n glyphs). */
static float range_width(const char *text, int from, int to)
{
    float w = 0.0f;
    int m = 0;
    Font f = GuiGetFont();
    float spacing = (float)GuiGetStyle(DEFAULT, TEXT_SPACING);
    float fs = (float)GuiGetStyle(DEFAULT, TEXT_SIZE);
    int i = from;
    while (i < to) {
        int sz = 0;
        int cp = GetCodepoint(text + i, &sz);
        char tmp[5] = { 0 };
        utf8_encode(cp, tmp);
        w += MeasureTextEx(f, tmp, fs, 0).x;    /* pure advance, no spacing */
        m++;
        i += sz > 0 ? sz : 1;
    }
    return w + (m > 0 ? (float)(m - 1) * spacing : 0.0f);
}

static float inner_pad(void) { return (float)GuiGetStyle(TEXTBOX, TEXT_PADDING); }

static float inner_width(Rectangle b) { return b.width - 2.0f * inner_pad(); }

/* Advance a byte index by one UTF-8 codepoint. */
static void advance_codepoint(const char *text, int *idx)
{
    int sz = 0;
    GetCodepoint(text + *idx, &sz);
    *idx += sz > 0 ? sz : 1;
}

/* Clamp the scroll offset so the caret stays visible. */
static void update_offset(SCTTextBox *tb, const char *text, Rectangle b)
{
    float iw = inner_width(b);
    if (range_width(text, 0, text_len(text)) <= iw) { tb->offset = 0; return; }
    while (tb->offset < tb->cursor && range_width(text, tb->offset, tb->cursor) > iw)
        advance_codepoint(text, &tb->offset);
    while (tb->offset > tb->cursor) {
        int i = tb->offset - 1;
        while (i > 0 && (text[i] & 0xc0) == 0x80) i--;   /* walk back over UTF-8 */
        tb->offset = i;
    }
}

/* Delete the current selection (if any). */
static void delete_selection(char *text, SCTTextBox *tb)
{
    if (!has_selection(tb)) return;
    int lo = sel_lo(tb), hi = sel_hi(tb);
    memmove(text + lo, text + hi, strlen(text + hi) + 1);
    tb->cursor = lo;
    clear_selection(tb);
}

/* Insert bytes at caret, respecting capacity. Deletes selection first. */
static void insert_text(char *text, int cap, SCTTextBox *tb, const char *ins)
{
    int ilen = (int)strlen(ins);
    delete_selection(text, tb);
    int len = text_len(text);
    if (ilen > cap - 1 - len) ilen = cap - 1 - len;
    if (ilen <= 0) return;
    memmove(text + tb->cursor + ilen, text + tb->cursor, len - tb->cursor + 1);
    memcpy(text + tb->cursor, ins, ilen);
    tb->cursor += ilen;
    clear_selection(tb);
}

/* ---------- clipboard/selection actions (shared by hotkeys and menu) ---------- */

static void do_select_all(SCTTextBox *tb, const char *text)
{
    tb->sel_start = 0;
    tb->sel_end = text_len(text);
    tb->cursor = tb->sel_end;
}

static void do_copy(const SCTTextBox *tb, const char *text)
{
    if (!has_selection(tb)) return;
    int lo = sel_lo(tb), hi = sel_hi(tb);
    char *tmp = malloc((size_t)(hi - lo) + 1);
    if (!tmp) return;
    memcpy(tmp, text + lo, hi - lo);
    tmp[hi - lo] = '\0';
    SetClipboardText(tmp);
    free(tmp);
}

static void do_cut(char *text, SCTTextBox *tb)
{
    if (!has_selection(tb)) return;
    do_copy(tb, text);
    delete_selection(text, tb);
}

static void do_paste(char *text, int cap, SCTTextBox *tb)
{
    const char *clip = GetClipboardText();
    if (clip && clip[0]) insert_text(text, cap, tb, clip);
}

/* ---------- word skipping, ported from raygui ---------- */

static int prev_word_boundary(const char *text, int from)
{
    int offset = from, cp = 0, sz = 0;
    while (offset > 0) {
        cp = GetCodepointPrevious(text + offset, &sz);
        if (!isspace(cp & 0xff)) break;
        offset -= sz;
    }
    bool punct = (cp != 0) && (ispunct(cp & 0xff) != 0);
    while (offset > 0) {
        cp = GetCodepointPrevious(text + offset, &sz);
        if ((punct && !ispunct(cp & 0xff)) ||
            (!punct && (isspace(cp & 0xff) || ispunct(cp & 0xff)))) break;
        offset -= sz;
    }
    return offset < 0 ? 0 : offset;
}

static int next_word_boundary(const char *text, int from, int len)
{
    int offset = from, cp = 0, sz = 0;
    while (offset < len) {
        cp = GetCodepoint(text + offset, &sz);
        if (!isspace(cp & 0xff)) break;
        offset += sz;
    }
    bool punct = (cp != 0) && (ispunct(cp & 0xff) != 0);
    while (offset < len) {
        cp = GetCodepoint(text + offset, &sz);
        if ((punct && !ispunct(cp & 0xff)) ||
            (!punct && (isspace(cp & 0xff) || ispunct(cp & 0xff)))) break;
        offset += sz;
    }
    return offset > len ? len : offset;
}

static void delete_forward_words(char *text, SCTTextBox *tb)
{
    int to = next_word_boundary(text, tb->cursor, text_len(text));
    if (to > tb->cursor) {
        memmove(text + tb->cursor, text + to, strlen(text + to) + 1);
        clear_selection(tb);
    }
}

static void delete_backward_words(char *text, SCTTextBox *tb)
{
    int from = prev_word_boundary(text, tb->cursor);
    if (from < tb->cursor) {
        memmove(text + from, text + tb->cursor, strlen(text + tb->cursor) + 1);
        tb->cursor = from;
        clear_selection(tb);
    }
}

/* Byte index under a mouse position, clamped to the visible text range. */
static int index_at(SCTTextBox *tb, const char *text, Rectangle bounds, Vector2 mouse)
{
    int len = text_len(text);
    float px = bounds.x + inner_pad();
    if (mouse.x <= px) return tb->offset;
    if (mouse.x >= px + range_width(text, tb->offset, len)) return len;

    float spacing = (float)GuiGetStyle(DEFAULT, TEXT_SPACING);
    float fs = (float)GuiGetStyle(DEFAULT, TEXT_SIZE);
    Font f = GuiGetFont();
    float w = 0.0f;
    int i = tb->offset;
    while (i < len) {
        int sz = 0;
        int c = GetCodepoint(text + i, &sz);
        char tmp[5] = { 0 };
        utf8_encode(c, tmp);
        float gw = MeasureTextEx(f, tmp, fs, 0).x;   /* pure advance */
        if (mouse.x <= px + w + gw / 2) break;
        w += gw + spacing;
        i += sz > 0 ? sz : 1;
    }
    return i;
}

/* ---------- the control ---------- */

void SCTTextBoxInit(SCTTextBox *tb)
{
    memset(tb, 0, sizeof(*tb));
}

int SCTGuiTextBox(Rectangle bounds, char *text, int cap, bool editMode, SCTTextBox *tb)
{
    int result = 0;
    int len = text_len(text);
    Vector2 mouse = GetMousePosition();
    bool hover = CheckCollisionPointRec(mouse, bounds);
    int state = editMode ? STATE_PRESSED : (hover ? STATE_FOCUSED : STATE_NORMAL);

    /* right-click opens the context menu (also while editing) */
    if (hover && IsMouseButtonPressed(MOUSE_BUTTON_RIGHT)) {
        tb->menu_open = true;
        tb->menu_pos = mouse;
    }

    if (editMode && !tb->menu_open) {
        bool ctrl = IsKeyDown(KEY_LEFT_CONTROL) || IsKeyDown(KEY_RIGHT_CONTROL);
        bool anyNavKey = IsKeyDown(KEY_LEFT) || IsKeyDown(KEY_RIGHT) ||
                         IsKeyDown(KEY_BACKSPACE) || IsKeyDown(KEY_DELETE);

        if (anyNavKey) tb->autorepeat_counter++;
        else tb->autorepeat_counter = 0;
        bool rpt = tb->autorepeat_counter > AUTO_REPEAT_COOLDOWN;

        /* selection-aware clipboard hotkeys */
        if (IsKeyPressed(KEY_A) && ctrl) do_select_all(tb, text);
        else if (IsKeyPressed(KEY_C) && ctrl) do_copy(tb, text);
        else if (IsKeyPressed(KEY_X) && ctrl) do_cut(text, tb);
        else if (IsKeyPressed(KEY_V) && ctrl) do_paste(text, cap, tb);

        /* typed codepoints replace the selection */
        int cp = GetCharPressed();
        while (cp > 0) {
            char enc[5] = { 0 };
            utf8_encode(cp, enc);
            insert_text(text, cap, tb, enc);
            cp = GetCharPressed();
        }

        /* caret movement (Shift extends the selection, Ctrl skips words) */
        len = text_len(text);
        bool shift = IsKeyDown(KEY_LEFT_SHIFT) || IsKeyDown(KEY_RIGHT_SHIFT);

        /* move caret to target and optionally extend selection */
        #define MOVE_TO(target) do { \
            if (shift) { \
                if (!has_selection(tb)) tb->sel_start = tb->cursor; \
                tb->cursor = (target); \
                tb->sel_end = tb->cursor; \
            } else { \
                tb->cursor = (target); \
                clear_selection(tb); \
            } \
        } while (0)

        if (len > 0 && IsKeyPressed(KEY_HOME)) MOVE_TO(0);
        if (len > tb->cursor && IsKeyPressed(KEY_END)) MOVE_TO(len);

        if (tb->cursor > 0 && (IsKeyPressed(KEY_LEFT) || (IsKeyDown(KEY_LEFT) && rpt))) {
            int target;
            if (ctrl) target = prev_word_boundary(text, tb->cursor);
            else { int sz = 0; GetCodepointPrevious(text + tb->cursor, &sz); target = tb->cursor - sz; }
            MOVE_TO(target);
        } else if (len > tb->cursor &&
                   (IsKeyPressed(KEY_RIGHT) || (IsKeyDown(KEY_RIGHT) && rpt))) {
            int target;
            if (ctrl) target = next_word_boundary(text, tb->cursor, len);
            else {
                int sz = 0;
                GetCodepoint(text + tb->cursor, &sz);
                if (sz <= 0) sz = 1;
                target = tb->cursor + sz;
            }
            MOVE_TO(target);
        }
        #undef MOVE_TO

        /* deletion (selection first, then raygui-style single/word deletes) */
        if (IsKeyPressed(KEY_BACKSPACE) || IsKeyPressed(KEY_DELETE)) delete_selection(text, tb);
        len = text_len(text);
        if (!has_selection(tb)) {
            if (len > tb->cursor &&
                (IsKeyPressed(KEY_DELETE) || (IsKeyDown(KEY_DELETE) && rpt))) {
                if (ctrl) delete_forward_words(text, tb);
                else {
                    int sz = 0;
                    GetCodepoint(text + tb->cursor, &sz);
                    if (sz <= 0) sz = 1;
                    memmove(text + tb->cursor, text + tb->cursor + sz,
                            strlen(text + tb->cursor + sz) + 1);
                }
            }
            if (tb->cursor > 0 &&
                (IsKeyPressed(KEY_BACKSPACE) || (IsKeyDown(KEY_BACKSPACE) && rpt))) {
                if (ctrl) delete_backward_words(text, tb);
                else {
                    int sz = 0;
                    GetCodepointPrevious(text + tb->cursor, &sz);
                    memmove(text + tb->cursor - sz, text + tb->cursor,
                            strlen(text + tb->cursor) + 1);
                    tb->cursor -= sz;
                }
            }
        }

        len = text_len(text);

        /* mouse: caret / drag-select / triple-click select-all */
        Rectangle textBounds = {
            bounds.x + inner_pad(), bounds.y,
            bounds.width - 2 * inner_pad(), bounds.height
        };
        if (CheckCollisionPointRec(mouse, textBounds) && IsMouseButtonPressed(MOUSE_BUTTON_LEFT)) {
            double now = GetTime();
            tb->click_count = (now - tb->last_click <= TRIPLE_CLICK_S) ? tb->click_count + 1 : 1;
            tb->last_click = now;
            if (tb->click_count >= 3) {
                do_select_all(tb, text);
                tb->click_count = 0;
                tb->dragging = false;
            } else {
                tb->cursor = index_at(tb, text, bounds, mouse);
                tb->sel_start = tb->cursor;   /* drag anchor */
                tb->sel_end = tb->cursor;
                tb->dragging = true;
            }
        } else if (IsMouseButtonPressed(MOUSE_BUTTON_LEFT)) {
            tb->autorepeat_counter = 0;
            tb->dragging = false;
            result = 1;                 /* click outside ends editing */
        }

        /* while the button is held, the selection follows the pointer */
        if (tb->dragging) {
            if (IsMouseButtonDown(MOUSE_BUTTON_LEFT)) {
                tb->cursor = index_at(tb, text, bounds, mouse);
                tb->sel_end = tb->cursor;
            } else {
                tb->dragging = false;   /* release: selection stays */
            }
        }
        if (IsKeyPressed(KEY_ENTER)) {
            tb->autorepeat_counter = 0;
            result = 1;                 /* ENTER ends editing */
        }
    } else if (!editMode) {
        if (hover && IsMouseButtonPressed(MOUSE_BUTTON_LEFT)) {
            tb->cursor = text_len(text);   /* caret to end, raygui behavior */
            clear_selection(tb);
        }
    }

    /* keep caret within text and visible */
    if (tb->cursor > text_len(text)) tb->cursor = text_len(text);
    update_offset(tb, text, bounds);

    /* ---------- draw (raylib primitives, raygui styles) ---------- */
    int bw = GuiGetStyle(TEXTBOX, BORDER_WIDTH);
    Color border = GetColor(GuiGetStyle(TEXTBOX, border_prop(state)));
    Color txtcol = GetColor(GuiGetStyle(TEXTBOX, text_prop(state)));
    Color base = editMode ? GetColor(GuiGetStyle(TEXTBOX, BASE_COLOR_PRESSED)) : BLANK;

    DrawRectangleRec(bounds, base);
    DrawRectangleLinesEx((Rectangle){ bounds.x + 0.5f, bounds.y + 0.5f,
                                      bounds.width - 1.0f, bounds.height - 1.0f },
                         (float)bw, border);

    float fs = (float)GuiGetStyle(DEFAULT, TEXT_SIZE);
    float sp = (float)GuiGetStyle(DEFAULT, TEXT_SPACING);
    float ty = bounds.y + (bounds.height - fs) / 2.0f;

    /* selection highlight beneath the text */
    if (has_selection(tb)) {
        float x0 = bounds.x + inner_pad() + range_width(text, tb->offset, sel_lo(tb));
        float x1 = bounds.x + inner_pad() + range_width(text, tb->offset, sel_hi(tb));
        if (x1 > x0)
            DrawRectangle((int)x0, (int)(bounds.y + 1), (int)(x1 - x0),
                          (int)(bounds.height - 2), (Color){ 51, 153, 255, 90 });
    }

    DrawTextEx(GuiGetFont(), text + tb->offset,
               (Vector2){ bounds.x + inner_pad(), ty }, fs, sp, txtcol);

    /* caret */
    if (editMode) {
        float cx = bounds.x + inner_pad() + range_width(text, tb->offset, tb->cursor);
        DrawRectangle((int)cx, (int)(bounds.y + 2), 1, (int)(bounds.height - 4),
                      GetColor(GuiGetStyle(TEXTBOX, BORDER_COLOR_PRESSED)));
    }

    return result;
}

/* ---------- right-click context menu ---------- */

void SCTTextBoxDrawMenu(SCTTextBox *tb, char *text, int cap)
{
    if (!tb->menu_open) return;

    static const char *items[] = { "Cut", "Copy", "Paste", "Select All" };
    const int n = 4;
    const float iw = 130.0f, ih = 26.0f;
    const float sw = (float)GetScreenWidth(), sh = (float)GetScreenHeight();
    Rectangle menu = { tb->menu_pos.x, tb->menu_pos.y, iw, (float)n * ih + 6.0f };

    /* keep the menu inside the window */
    if (menu.x + menu.width > sw) menu.x = sw - menu.width;
    if (menu.y + menu.height > sh) menu.y = sh - menu.height;
    if (menu.x < 0) menu.x = 0;
    if (menu.y < 0) menu.y = 0;

    Vector2 mouse = GetMousePosition();

    /* click outside or ESC closes */
    if (IsKeyPressed(KEY_ESCAPE) ||
        ((IsMouseButtonPressed(MOUSE_BUTTON_LEFT) || IsMouseButtonPressed(MOUSE_BUTTON_RIGHT)) &&
         !CheckCollisionPointRec(mouse, menu))) {
        tb->menu_open = false;
        return;
    }

    /* panel */
    DrawRectangleRec(menu, GetColor(GuiGetStyle(DEFAULT, BACKGROUND_COLOR)));
    DrawRectangleLinesEx(menu, 1, GetColor(GuiGetStyle(DEFAULT, LINE_COLOR)));

    Font f = GuiGetFont();
    float fs = (float)GuiGetStyle(DEFAULT, TEXT_SIZE);
    Color txt = GetColor(GuiGetStyle(LABEL, TEXT_COLOR_NORMAL));

    for (int i = 0; i < n; i++) {
        Rectangle r = { menu.x + 3, menu.y + 3 + (float)i * ih, iw - 6, ih - 2 };
        bool hot = CheckCollisionPointRec(mouse, r);
        if (hot) DrawRectangleRec(r, GetColor(GuiGetStyle(BUTTON, BASE_COLOR_FOCUSED)));
        DrawTextEx(f, items[i], (Vector2){ r.x + 8, r.y + (r.height - fs) / 2 }, fs, 1, txt);

        if (hot && IsMouseButtonReleased(MOUSE_BUTTON_LEFT)) {
            tb->menu_open = false;
            switch (i) {
                case 0: do_cut(text, tb); break;
                case 1: do_copy(tb, text); break;
                case 2: do_paste(text, cap, tb); break;
                case 3: do_select_all(tb, text); break;
            }
        }
    }
}

/* Temporary self-test: exercises the selection/clipboard logic directly and
 * leaves a selection + open menu on screen for visual verification. */
void SCTTextBoxSelfTest(SCTTextBox *a, char *ea, SCTTextBox *b, char *eb, int cap)
{
    int pass = 0, total = 0;
    #define CHECK(cond, name) do { total++; if (cond) { pass++; \
        TraceLog(LOG_INFO, "SELFTEST PASS: %s", name); } \
        else TraceLog(LOG_WARNING, "SELFTEST FAIL: %s", name); } while (0)

    a->cursor = 0; a->sel_start = a->sel_end = 0; a->offset = 0; ea[0] = 0;
    b->cursor = 0; b->sel_start = b->sel_end = 0; b->offset = 0; eb[0] = 0;

    insert_text(eb, cap, b, "HELLO");
    CHECK(strcmp(eb, "HELLO") == 0, "type inserts text");

    do_select_all(b, eb);
    CHECK(has_selection(b) && sel_lo(b) == 0 && sel_hi(b) == 5, "select all covers whole text");

    do_copy(b, eb);
    const char *cl = GetClipboardText();
    CHECK(cl && strcmp(cl, "HELLO") == 0, "copy puts selection on clipboard");

    do_paste(ea, cap, a);
    CHECK(strcmp(ea, "HELLO") == 0, "paste fills empty box");

    insert_text(ea, cap, a, "XY");
    CHECK(strcmp(ea, "HELLOXY") == 0, "typing appends at caret");

    do_select_all(a, ea);
    insert_text(ea, cap, a, "Z");
    CHECK(strcmp(ea, "Z") == 0, "typing replaces selection");

    do_select_all(a, ea);
    delete_selection(ea, a);
    CHECK(strcmp(ea, "") == 0, "delete removes selection");

    insert_text(ea, cap, a, "CUTME");
    do_select_all(a, ea);
    do_cut(ea, a);
    CHECK(strcmp(ea, "") == 0 && strcmp(GetClipboardText(), "CUTME") == 0, "cut empties box and copies");

    /* leave visual state: selection in edit2, menu open over edit1 */
    insert_text(eb, cap, b, "WORLD");
    do_select_all(b, eb);
    a->menu_open = true;
    a->menu_pos = (Vector2){ 20, 470 };

    TraceLog(LOG_INFO, "SELFTEST: %d/%d passed", pass, total);
}
