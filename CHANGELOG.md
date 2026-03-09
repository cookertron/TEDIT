# TEDIT Changelog

## Phase 5 — Mouse Support + Polish (2026-03-09 ~10:07 UTC)

### Changes to ed_mouse.inc
- Replaced stubs with full implementations:
  - `tui_ed_mouse_press`: computes editor-relative coordinates from click cell, sets `cur_line`/`cur_col` with clamping to line length and total_lines, resets `last_ins_pi`, starts control drag (`MSF_CTRL_DRAG`), requests redraw
  - `tui_ed_mouse_drag`: same coordinate logic with additional negative-row/col clamping, calls `scroll_to_cursor` for auto-scrolling when dragging outside text area

### Changes to ed_draw.inc
- **Wrap mode rendering**: added `.row_done_wrap` / `.row_next_wrap` path — when column count hits ED_COLS in wrap mode, stores piece position, pads row, increments screen row (`rnd_row`) without incrementing logical line counter (BP), continues rendering same logical line on next screen row
- **Status bar polish**: added column display (`Col N`, 1-based) and wrap indicator (`[WRAP]`) between line count and `[Modified]` indicator

### Changes to TEDIT.ASM
- Added `CALL tui_mouse_init` after `tui_init` in `.start_tui` — enables INT 33h mouse driver for menu/dialog/editor mouse interaction
- Added status bar strings: `s_stat_col` ("  Col "), `s_stat_wrap` ("  [WRAP]")
- Added Info menu strings: `m_str_about` ("About..."), `m_str_about_msg` ("TEDIT - TUI Text Editor")
- Added `m_info_entries` dropdown (1 entry: About with hotkey 'A')
- Added Info item to `m_menu_items` (MI_X=6, MI_W=6, Alt+I = scan code 17h)
- Updated `m_menubar` count from 1 to 2
- Added `menu_about_handler`: shows `tui_dlg_msgbox` with title "Info"

### Tests Passing
1. Mouse click cursor positioning — click at cell (5,3) moves to Line 3 Col 6
2. Mouse click + type — click then insert character at correct position
3. Mouse drag cursor tracking — drag updates cursor continuously
4. Mouse menu interaction — click Info opens dropdown, click About shows dialog
5. Status bar Col display — shows `Col N` (1-based) after all navigation
6. Status bar [WRAP] indicator — shown when `--wrap` flag active
7. Wrap mode rendering — 120-char line wraps to 2 screen rows (80+40)
8. Info > About dialog — "TEDIT - TUI Text Editor" with OK button
9. Alt+I keyboard access — opens Info dropdown via keyboard

### Known Limitations
1. Mouse click in wrap mode targets logical lines incorrectly when wrapped lines span multiple screen rows
2. No text selection — drag tracks cursor only
3. No horizontal scroll via mouse
4. Wrap mode cursor past column 79 not visible (matches TEXTEDIT.ASM behavior)

---

## Post-Phase 5 Fix: Empty Document First-Keystroke Rendering (2026-03-09 ~10:30 UTC)

**Bug**: Typing in a new empty document (or after deleting all text) didn't show characters on screen. Cursor moved but the first line stayed blank. Only pressing Enter made text appear.

**Root cause**: `ed_new_doc` seeds `chkpt_pi[0]=0` as a "start of document" sentinel. When `insert_char` inserts the first piece at index 0 via `pt_insert`, `chkpt_adjust_insert` bumps `chkpt_pi[0]` from 0 to 1 (because `0 >= 0`). But piece 1 doesn't exist — only piece 0 does. So `seek_to_line(0)` sets `rnd_pi=1`, which is `>= pt_count(1)`, and the renderer draws a blank row. Enter works because `ed_keys.inc` sets `meta_dirty=1` after `insert_crlf`, triggering `rebuild_meta` which corrects the checkpoint.

**Fix in `ed_edit.inc`**: Set `meta_dirty=1` in insert_char's `.ic_insert_before` path (taken when `BX >= pt_count`, i.e., cursor past all pieces). This triggers one `rebuild_meta` on the next render, correcting the stale checkpoint. Subsequent keystrokes use the fast path (extending the tracked piece) so no further rebuilds occur.

**Performance impact**: One `rebuild_meta` call per "start typing at a new end-of-document position" — not per keystroke. The fast path avoids repeated rebuilds.

---

## Phase 4 — File Operations: Open, Save As, Quit Confirmation (2026-03-08 ~23:29 UTC)

### New Files
- `ed_file.inc` — 2 helper procedures:
  - `ed_load_file`: frees existing document, opens/loads file, inits piece table, allocates add buffer, builds metadata, resets cursor state. On failure shows error dialog and falls back to empty document.
  - `ed_new_doc`: allocates pt_seg and add_seg, seeds metadata for 1-line empty document, resets all editor state.
- `PHASE4_PLAN.md` — Phase 4 implementation plan

### Changes to TEDIT.ASM
- Added `INCLUDE ed_file.inc` between ed_edit.inc and ed_draw.inc
- Replaced menu strings: removed `m_str_not_impl` and `m_str_no_fname`, added `m_str_open_ttl`, `m_str_save_ttl`, `m_str_save_pr`, `m_str_quit_ttl`, `m_str_quit_ask`, `m_str_error`, `m_str_err_open`, `m_str_err_mem`
- `menu_quit_handler`: checks dirty flag, shows `tui_dlg_confirm` ("Save changes before closing?"), supports Save As dialog from quit path when no filename exists
- `menu_open_handler`: checks dirty (prompts to save), opens `tui_dlg_file` file selector, copies result to `filename`, calls `ed_load_file`
- `menu_save_handler`: direct save if filename exists, `tui_dlg_input` Save As dialog if no filename
- Simplified `.no_file` startup path: `CALL ed_new_doc` / `JC .err_nomem`
- Added `open_file_buf: RESB 14` to editor BSS
- Added missing BSS variables: `dlg_save_title`, `dlg_save_buf`, `dlg_save_maxlen`

### Changes to ed_keys.inc
- Ctrl+S (`.do_save`) now delegates to `menu_save_handler` instead of inline save logic, enabling Save As dialog when no filename exists

### Bugs Found & Fixed
1. **Missing BSS variables for `tui_dlg_input`**: `dlg_save_title`, `dlg_save_buf`, `dlg_save_maxlen` were never declared in TEDIT.ASM. agent86 silently resolved these undefined forward references to incorrect addresses (likely 0), causing the dialog to read/write buffer pointers from wrong memory. The Save As dialog appeared to work visually but the filename buffer remained empty after OK. Fix: added the three declarations to the TUI BSS section.
2. **ed_edit.inc meta_dirty regression** (from plan): verified no regression exists — `insert_before` and `insert_after` paths correctly do NOT set `meta_dirty=1`, and the split path correctly does. No changes needed.

### Tests Passing
1. File load — opens and displays file content
2. Save As — Ctrl+S with no filename opens input dialog, saves to entered name
3. Quit+Save — dirty doc, Alt+Q, Yes saves and exits
4. Quit+Discard — dirty doc, Alt+Q, Escape exits without saving
5. Quit+SaveAs — dirty doc with no filename, Alt+Q, Yes opens Save As, then exits
6. Clean quit — unmodified doc, Alt+Q exits immediately
7. File>Open — file selector dialog, select file, content loads
8. Empty doc startup — no args, starts with empty document, editing works
9. Phase 3 regression — typing, Enter, Backspace, navigation all still work

---

## Post-Phase 3 Fixes

### Fix: Column clamping on vertical cursor movement (2026-03-08 ~22:32 UTC)

**Bug**: Moving cursor vertically (Up, Down, PgUp, PgDn) did not clamp `cur_col` to the length of the destination line. E.g., pressing End on a 50-char line then Down to a 5-char line left `cur_col` at 50 — cursor drew on a blank cell and typing inserted at the wrong visual position.

**Fix in `ed_keys.inc`**:
- Added `.vert_moved` label between navigation handlers and `.cursor_moved`
- `.vert_moved` calls `line_length`, clamps `cur_col` to AX if greater, then falls through to `.cursor_moved`
- Changed `.key_up`, `.key_down`, `.pgdn_ok`, `.pgup_ok` to jump to `.vert_moved`
- Left `.key_left`, `.key_right`, `.key_home`, `.key_end` unchanged (still jump to `.cursor_moved`)

---

## Phase 3 — Text Editing + Save + Simplified Status Bar (2026-03-08 ~21:50 UTC)

### New Files
- `ed_edit.inc` — 5 editing procedures ported from TEXTEDIT.ASM:
  insert_char, insert_crlf, delete_byte, delete_at, save_file
- `PHASE3_PLAN.md` — Phase 3 implementation plan

### Changes to ed_keys.inc
- Full rewrite of `tui_ed_handle_key`:
  - Normal key dispatch: Ctrl+S (13h), Enter, Backspace, printable (20h-7Eh), Escape passthrough
  - Extended key: added KEY_DELETE handler alongside existing navigation
  - Enter: calls insert_crlf, advances cur_line, resets cur_col, sets meta_dirty
  - Backspace at col>0: decrements cur_col, calls delete_at
  - Backspace at col 0: merges with previous line (dec cur_line, set cur_col to line_length, delete_at)
  - Ctrl+S: calls save_file if filename exists

### Changes to ed_draw.inc
- Simplified `ed_draw_status_line`: now shows ` Line N of M` + `  [Modified]` when dirty
- Removed: column display, wrap indicator, keyboard shortcut hints

### Changes to TEDIT.ASM
- Replaced old status bar strings (s_stat_hdr/sep/col/wrap/mod/keys) with s_stat_ln/s_stat_of/s_stat_mod
- Added `INCLUDE ed_edit.inc` between ed_core.inc and ed_draw.inc
- Wired `menu_save_handler` to actual save_file (with "No filename" messagebox fallback)
- Added `m_str_no_fname` string
- `.no_file` path now allocates pt_seg and add_seg (required for editing in empty documents)

### Key Adaptations from TEXTEDIT.ASM
- `insert_char`: replaced `CALL render` / `CALL update_cursor` with `MOV BYTE [fw_state + FW_DIRTY], 1` at 4 locations (fast path, split path, insert_before, insert_after)
- `insert_crlf`, `delete_byte`, `delete_at`, `save_file`: verbatim copies (no render/update_cursor calls inside)

### Bugs Found & Fixed During Implementation
1. ~~**Checkpoint staleness on insert_before/insert_after**~~: *Reverted* — the original TEXTEDIT.ASM was correct. `chkpt_adjust_insert` bumps checkpoint piece indices ≥ BX by +1, which keeps checkpoints valid without a full rebuild. Since `insert_char` only handles printable characters (20h–7Eh), line count never changes. The added `meta_dirty=1` was harmless but triggered an unnecessary `rebuild_meta` (full piece-table scan) on every first keystroke at a new cursor position — a performance penalty for large files.
2. **CRLF companion delete meta_dirty race** *(genuine fix)*: In delete_at, after deleting CR, `cursor_to_offset` triggers `ensure_meta` which clears `meta_dirty`. If companion LF is then deleted via `delete_byte`, `meta_dirty` stays 0, leaving stale `total_lines`. Fix: added `MOV BYTE [meta_dirty], 1` after each companion `delete_byte` call (3 locations in delete_at).
   - Root cause: TEXTEDIT.ASM didn't use checkpoints for rendering (its `render` walked pieces linearly). The TUI editor's renderer uses `seek_to_line` which depends on correct checkpoints.

### Tests Passing
1. Type text — "Hello World!" prepended to file content
2. Enter — creates new line, content shifted down
3. Backspace — deletes characters mid-line
4. Backspace at col 0 — merges lines, total_lines decreases
5. Delete key — removes characters at cursor
6. Ctrl+S — saves file, clears [Modified] indicator
7. File > Save menu — saves via Alt+F, S hotkey
8. Empty document — type without crash (pt_seg + add_seg allocated)
9. Status bar — shows ` Line N of M  [Modified]` format only
10. Menu after editing — F10 + Open dialog still works after piece-table modifications

---

## Phase 2 — File Loading, Editor Display, Navigation (2026-03-08 ~18:00 UTC)

### New Files
- `ed_const.inc` — All editor EQU constants (dimensions, attributes, piece table, memory sizing)
- `ed_core.inc` — 19 piece-table engine procedures ported from TEXTEDIT.ASM:
  shrink_mem, parse_args, open_file, load_file, pt_init, resolve_piece,
  rebuild_meta, seek_to_line, ensure_meta, clamp_cursor, scroll_to_cursor,
  cursor_to_offset, line_length, peek_at_cursor, free_all, pt_insert,
  pt_remove, chkpt_adjust_insert, chkpt_adjust_remove
- `ed_draw.inc` — Editor rendering:
  tui_ctrl_draw_editor (23 text rows from piece table into shadow_buf),
  ed_draw_status_line, ed_wstr, ed_wdec, ed_draw_cursor (nibble-swap block cursor)
- `ed_keys.inc` — Keyboard navigation:
  tui_ed_handle_key (Up/Down/Left/Right/PgUp/PgDn/Home/End)
- `ed_mouse.inc` — Mouse handler stubs

### Changes to TEDIT.ASM
- Replaced inline EQU constants with `INCLUDE ed_const.inc`
- Replaced editor stubs with includes for ed_core, ed_draw, ed_keys, ed_mouse
- Added ed_ctrl (CTYPE_EDITOR control with CTRL_ED_STATE pointer)
- Window template now has ed_ctrl as WIN_FIRST and WIN_FOCUS
- main: shrink_mem → parse_args → open_file → load_file → pt_init → tui_run
- Empty document mode: no filename → pt_count=0, total_lines=1, seeded checkpoint
- BSS: ed_state (top_line + total_lines adjacent), all editor variables

### Key Adaptations from TEXTEDIT.ASM
- `SCREEN_COLS` → `ED_COLS` (80), `TEXT_ROWS` → `ED_TEXT_ROWS` (23)
- `ATTR_NORM` (07h) → `ED_ATTR_NORM` (1Fh), `ATTR_STAT` (70h) → `ED_ATTR_STAT` (70h)
- `0800h` shrink → `SHRINK_PARA` (1000h = 64 KB)
- Render target: shadow_buf (ES=CS) instead of VIDEO_SEG (B800h)
- Status bar: null-terminated strings via ed_wstr instead of $-terminated via wstr_stat
- Software cursor (nibble-swap) instead of hardware cursor (INT 10h)

### Tests Passing
1. Load and display file (3-line test.txt)
2. Arrow key navigation (Down×2 + Right×3 → L3/4 C4)
3. PgDn/PgUp (clamp to document bounds)
4. Home/End (End → C12, Home → C1)
5. Empty document (L1/1 C1, no crash)
6. Menu still works (File > Open dialog, Alt+Q exit)

---

## Phase 1 — TUI Shell with Menu Bar (2026-03-06)

### New Files
- `TEDIT.ASM` — Main file with TUI shell
- `PHASE1_PLAN.md` — Phase 1 implementation plan

### Features
- TUI framework integration (menu bar, window, event loop)
- File menu with Open (stub), Save (stub), Quit
- Blue desktop window (80x24, no border)
- Alt+Q global shortcut for Quit
- Editor stubs for TUI dispatch (draw, key, mouse)

### Tests Passing
1. Blue desktop with menu bar
2. Alt+Q quit
3. File > Quit menu
4. File > Open "Not yet implemented" dialog
5. F10 menu activation
