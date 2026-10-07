#ifndef SCT_TEXTBOX_H
#define SCT_TEXTBOX_H

#include "raylib.h"

/* SCTGuiTextBox: raygui GuiTextBox look-and-feel plus a real selection model.
 *
 * Extra features over raygui's GuiTextBox:
 *   - selection with highlight (click places cursor, triple-click selects all)
 *   - Ctrl+A select all, Ctrl+C copy, Ctrl+X cut, Ctrl+V paste-over-selection
 *   - Backspace/Delete/type replaces the selection
 *   - right-click context menu (Cut / Copy / Paste / Select All)
 *
 * Kept from raygui: single-line editing, codepoint input (UTF-8), arrows,
 * Ctrl+arrows word skip, Home/End, Ctrl+Backspace/Delete word delete,
 * key auto-repeat, text scrolling offset, ENTER/click-outside ends editing.
 */

typedef struct SCTTextBox {
    int cursor;              /* byte index of caret */
    int sel_start;           /* selection range (bytes); equal -> no selection */
    int sel_end;
    bool dragging;           /* left button held: selection follows pointer */
    int offset;              /* first visible byte (scroll) */
    int autorepeat_counter;  /* held-key repeat counter */
    double last_click;       /* timestamp of last left click (triple-click) */
    int click_count;
    bool menu_open;          /* right-click context menu */
    Vector2 menu_pos;
} SCTTextBox;

void SCTTextBoxInit(SCTTextBox *tb);

/* Draw + update one textbox. Returns 1 when editing should end
 * (ENTER pressed or click outside), mirroring raygui's RESULT_PRESSED. */
int SCTGuiTextBox(Rectangle bounds, char *text, int cap, bool editMode, SCTTextBox *tb);

/* Logic self-test hook (main.c runs it when SCT_SELFTEST is set): exercises
 * the selection/clipboard paths and logs "SELFTEST: n/m passed". */
void SCTTextBoxSelfTest(SCTTextBox *a, char *ea, SCTTextBox *b, char *eb, int cap);

/* Draw/handle the context menu overlay for a box, if open.
 * Call after all controls, before EndDrawing(). */
void SCTTextBoxDrawMenu(SCTTextBox *tb, char *text, int cap);

#endif /* SCT_TEXTBOX_H */
