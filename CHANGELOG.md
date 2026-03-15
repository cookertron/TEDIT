# TEDIT Changelog

## v0.30.0 — Range Delete Phase 5: Validation & Testing (2026-03-15)

No code changes. Comprehensive validation of the Range Delete feature (Phases 1-4).
Full results in `PHASE5_RESULTS.md`.

### Performance validation
- Select-all-delete on files from 100 to 2000 characters
- Linear O(N) scaling confirmed: ~58 instructions per additional character
- Baseline overhead ~165K instructions (IDLE loop + TUI rendering)

### Edge case tests (9/9 pass)
- Empty selection, single char, entire document, start/end of document
- Delete via Backspace, type-replaces-selection, Enter-replaces-selection
- All boundary conditions handled correctly

### Undo/redo stress tests (3/3 pass)
- Multi-level undo across multiple range deletes
- Redo cycling (undo/redo/undo/redo)
- Redo truncation after new edits

### Multi-line selection tests (4/4 pass)
- Mid-line to mid-line selection + delete + undo
- Backward selection (anchor after cursor)
- Mouse selection and delete

### Wrap mode tests (2/2 pass)
- Short lines (no wrapping triggered)
- 90-char wrapped line: Shift+Down selects first visual row, delete removes 80 chars

### Regression (3/3 pass)
- Phase 1 (line_col_to_offset): `"executed":"OK"` — 2,899 instructions
- Phase 2 (split_piece_at + range_delete_pieces): `"executed":"OK"` — 4,433 instructions
- Phase 3 (UR_DEL_RANGE undo/redo): `"executed":"OK"` — 5,983 instructions

---

## v0.30.0 — Range Delete Phase 4: Wire into sel_delete (2026-03-15)

Replaced the O(N²) character-by-character deletion in `sel_delete` with the
Phase 1-3 range delete infrastructure. The new `sel_delete` (fast path) converts
selection endpoints to byte offsets, calls `range_delete_pieces` for O(P)
deletion, and pushes a single `UR_DEL_RANGE` undo record. On failure (overflow,
too many pieces), it falls back to the original char-by-char loop renamed
`sel_delete_legacy`.

### Bug fix: ES register clobber in `range_delete_pieces`

`split_piece_at` is documented as clobbering ES (sets it to `[pt_seg]`), but
`range_delete_pieces` did not save/restore ES around its two `split_piece_at`
calls. After the call, ES remained set to the piece table segment (e.g. 0x8E00).
The TUI framework's screen buffer fill (`REP STOSW` at ES:DI) then wrote 4KB of
attribute bytes into the adjacent ORIG buffer segment, destroying file content.

Symptoms: loaded file content replaced with 0x17 bytes, `rebuild_meta` counted
0 newlines, `total_lines` dropped to 1, screen displayed garbled CP437 0x17
characters. Only affected ORIG-buffer pieces (file-loaded); ADD-buffer pieces
(typed text) were unaffected because no adjacent ORIG allocation existed.

### Changes to ed_sel.inc
- Added `sel_delete PROC` (fast path, ~120 lines):
  - Normalizes selection via `sel_normalize`
  - Converts start/end to byte offsets via `line_col_to_offset`
  - Calls `range_delete_pieces` for single-operation deletion
  - Pushes `UR_DEL_RANGE` undo record via `undo_push_range`
  - Positions cursor at selection start
  - Cleanup: sel_clear, scroll_to_cursor, FW_DIRTY, last_ins_pi, undo_group_active, meta_dirty, dirty
  - Falls back to `sel_delete_legacy` via JMP on any failure
- Renamed existing `sel_delete` to `sel_delete_legacy`:
  - All internal labels renamed `.sd_*` → `.sdl_*` to avoid collisions
  - Code unchanged — serves as fallback path
- Added `MOV BYTE [dirty], 1` to fast path cleanup (the fast path bypasses
  `delete_at` which normally sets dirty)

### Changes to ed_core.inc
- `range_delete_pieces` step 6 (start split): wrapped `CALL split_piece_at`
  with `PUSH ES` / `POP ES` (3 instructions added)
- `range_delete_pieces` step 7 (end split): wrapped `CALL split_piece_at`
  with `PUSH ES` / `POP ES` (3 instructions added)

### Tests passing
Phase 1-3 regression: 3/3 suites OK (test_phase1, test_phase2, test_phase3)
Smoke tests: 2/2 IDLE (empty doc, file loaded)
Integration tests: 11/11 PASS

| # | Test | Result |
|---|------|--------|
| 1 | Single-line select all, delete (typed) | PASS |
| 2 | Multi-line range delete (typed) | PASS |
| 3 | Delete then Undo (typed) | PASS |
| 4 | Delete, Undo, Redo (typed) | PASS |
| 5 | Mixed undo: range delete + char insert (typed) | PASS |
| 6 | Regression: normal backspace (file loaded) | PASS |
| 7 | Enter after selection delete (typed) | PASS |
| 8 | Multi-line undo (typed) | PASS |
| 9 | Multi-line undo+redo (typed) | PASS |
| 10 | ORIG-buffer 1-char delete (file loaded) | PASS |
| 11 | ORIG-buffer delete+undo (file loaded) | PASS |

### Plan deviations
- `ed_core.inc` modified despite plan saying "do not modify" — the ES clobber
  bug in `range_delete_pieces` had to be fixed at the source; wrapping in
  `sel_delete` would only mask the issue for one caller.
- Added `dirty=1` to fast path cleanup — plan omission since `range_delete_pieces`
  bypasses `delete_at` which normally sets the dirty flag.

---

## v0.29.0 — Range Delete Phase 3: UR_DEL_RANGE Undo/Redo (2026-03-15)

Added undo/redo support for range deletions. A new `UR_DEL_RANGE` record type
captures a range deletion as a single 10-byte undo record. On undo, the saved
piece descriptors are re-inserted into the piece table via `pt_insert`. On redo,
the pieces are saved back to `rd_save_buf` and removed again. A LIFO stack
(`rd_save_used`) tracks multiple outstanding range deletes, flushed on
`undo_discard_half` for safe degradation.

### Changes to ed_const.inc
- Added `UR_DEL_RANGE EQU 4` — range deletion record type

### Changes to ed_core.inc
- Modified `range_delete_pieces`: save destination changed from `rd_save_buf`
  (fixed offset 0) to `rd_save_buf + rd_save_used * PT_DESC_SIZE` (LIFO stack
  top). Required for multiple outstanding range deletes to coexist in the save
  buffer. 4 instructions added (MOV/MOV/MUL/MOV+ADD replacing MOV).

### Changes to ed_undo.inc
- `undo_init`: added `MOV WORD [rd_save_used], 0`
- `undo_clear`: added `MOV WORD [rd_save_used], 0`
- `undo_discard_half`: added `MOV WORD [rd_save_used], 0` in `.done` — flushes
  save buffer when oldest records are discarded
- `undo_execute`: added `UR_DEL_RANGE` dispatch and `.u_del_range` handler
  - Cleans 3 stacked values, checks save buffer validity
  - Positions cursor via `cursor_to_offset`, re-inserts pieces via `pt_insert`
  - Pops descriptors from save stack, sets `meta_dirty = 1`
- `redo_execute`: added `UR_DEL_RANGE` dispatch and `.r_del_range` handler
  - Saves piece count before `cursor_to_offset` (fixes plan bug: ES clobbered)
  - Copies piece descriptors back to `rd_save_buf`, removes via `pt_remove`
  - Capacity check: skips save if buffer full (graceful degradation)
- Added `undo_push_range PROC` — convenience wrapper for pushing `UR_DEL_RANGE`
  records. Input: BX=start_line, CX=start_col, DX=pre_cursor_line,
  SI=pre_cursor_col; reads `[rd_save_count]`. Clobbers AX,BX,CX,DX,SI,DI.

### Changes to TEDIT.ASM
- Added `rd_save_used: RESW 1` in transient BSS section after `rd_same_piece`

### Changes to test_phase2.asm
- Added `rd_save_used: RESW 1` to BSS — required by `range_delete_pieces` change

### New files
- `test_phase3.asm` — standalone unit test harness (3 test cases)
- `PHASE3_RESULTS.md` — test results summary

### UR_DEL_RANGE record layout
```
Byte 0 (UR_TYPE):     UR_DEL_RANGE (4), may include URF_GROUP_CONT
Byte 1 (UR_CHAR):     Piece count (1-102)
Bytes 2-3 (UR_DOC_LINE): Delete start line (re-insertion point for undo)
Bytes 4-5 (UR_DOC_COL):  Delete start column
Bytes 6-7 (UR_CUR_LINE): Pre-delete cursor line (undo restore)
Bytes 8-9 (UR_CUR_COL):  Pre-delete cursor column (undo restore)
```

### Plan deviations
- `range_delete_pieces` modified despite plan saying "do not modify" — the LIFO
  stack design requires writing at the correct offset; without it, a second
  range delete overwrites the first's saved descriptors.
- Redo handler: plan read `ES:[BX + UR_CHAR]` after `cursor_to_offset` which
  clobbers ES. Fixed by saving piece count before the call.

### agent86 observations
- Unresolved symbol `rd_save_used` in test_phase2.asm (before fix) compiled
  without error. May be an assembler bug — unresolved forward references should
  produce a compile error.

### Test results
- 3/3 Phase 3 unit tests passing (test_phase3.asm --build_trace):
  - Test 1: Delete [7,14) + undo → 21 bytes restored, 4 lines
  - Test 2: Redo → 14 bytes, 3 lines
  - Test 3: Two range deletes [0,7) + [7,12) then undo both → 21 bytes, 4 lines
- 5/5 Phase 2 regression passing
- 10/10 Phase 1 regression passing
- TEDIT smoke test (with file): compiled OK, executed IDLE
- TEDIT smoke test (empty): compiled OK, executed IDLE

## v0.28.0 — Range Delete Phase 2: split_piece_at + range_delete_pieces (2026-03-15)

Added core piece table range delete procedures to `ed_core.inc`. Given two byte
offsets `[start_off, end_off)`, `range_delete_pieces` splits pieces at both
boundaries, saves the removed descriptors for future undo restoration, and
removes the pieces in reverse order.

### Changes to ed_const.inc
- Added `RD_MAX_PIECES EQU 102` — max pieces saveable in one range delete

### Changes to ed_core.inc
- Added `split_piece_at PROC` after `line_col_to_offset ENDP`, before `free_all:`
  - Input: BX = piece index, SI = split offset within piece
  - Splits piece into left (length = SI) and right (remainder) at BX+1
  - Preserves BP, DI; clobbers AX, BX, CX, DX, SI, ES
  - Extracted from inline split logic in insert_char / delete_byte
- Added `range_delete_pieces PROC` after `split_piece_at ENDP`
  - Input: AX = start_offset (inclusive), DX = end_offset (exclusive)
  - 10-step algorithm: validate → walk to start → walk to end → detect
    same-piece → bounds check → split at start → split at end → save
    descriptors → remove pieces (reverse order) → set meta_dirty
  - Critical same-piece fix: after start split, adjusts `rd_end_split`
    by subtracting `rd_start_split` to prevent over-deletion
  - Outputs: rd_save_buf[] (saved descriptors), rd_save_count, rd_save_start_pi
  - Preserves BP, DI; clobbers AX, BX, CX, DX, SI

### Changes to TEDIT.ASM
- Added 11 BSS variables in transient section after `lco_off_hi`:
  `rd_save_buf` (1020 bytes), `rd_save_count`, `rd_save_start_pi`,
  `rd_start_off`, `rd_end_off`, `rd_start_pi`, `rd_end_pi`,
  `rd_start_split`, `rd_end_split`, `rd_end_plen`, `rd_same_piece`

### New files
- `test_phase2.asm` — standalone unit test harness (5 test cases)
- `PHASE2_PLAN_REVISED.md` — implementation plan (with same-piece fix)
- `PHASE2_RESULTS.md` — test results summary

### Test results
- 5/5 unit tests passing (test_phase2.asm --build_trace):
  - Test 1: Delete middle line [7,14) — 14 bytes remaining, 3 lines
  - Test 2: Delete first line [0,7) — 14 bytes remaining, 3 lines
  - Test 3: Same-piece critical [2,9) — 14 bytes remaining, 3 lines
  - Test 4: Delete entire document [0,21) — 0 bytes, pt_count=0
  - Test 5: Delete last chars [19,21) — 19 bytes remaining, 4 lines
- TEDIT smoke test: compiled OK (46,450 bytes), executed IDLE — no regressions

### Plan errata
- PHASE2_PLAN_REVISED.md Test 3 expected 12 bytes remaining for [2,9); correct
  value is 14 (exclusive end means 7 bytes deleted, not 9). Corrected in test.

## v0.27.0 — Range Delete Phase 1: line_col_to_offset (2026-03-15)

Added `line_col_to_offset` utility procedure to `ed_core.inc` — converts a
`(line, col)` cursor position to a linear byte offset in the logical document.
This is the foundation for the range delete operation in later phases.

### Changes to ed_core.inc
- Added `line_col_to_offset PROC` (106 lines) after `peek_at_cursor ENDP`,
  before `free_all:`
- Algorithm: seek_to_line → sum piece lengths before rnd_pi (32-bit) → add
  rnd_off → walk visible characters up to target column (stops at CR/LF)
- Preserves BX, BP, DI; clobbers AX, CX, DX, SI, rnd_pi, rnd_off
- Returns DX:AX = byte offset, CF=0 success / CF=1 overflow (DX != 0)

### Changes to TEDIT.ASM
- Added 2 BSS variables: `lco_off_lo`, `lco_off_hi` (32-bit offset
  accumulator scratch) in transient section after `cur_vis_line`

### New files
- `test_phase1.asm` — standalone unit test harness (ed_const.inc + ed_core.inc only)
- `test_rd.txt` — 21-byte test data file (4 lines, CRLF endings)
- `PHASE1_PLAN.md` — implementation plan
- `PHASE1_RESULTS.md` — test results summary

### Test results
- 10/10 unit tests passing (test_phase1.asm --build_trace)
- TEDIT smoke test: compiled OK (44799 bytes), executed IDLE — no regressions

## v0.26.0 — Mouse Drag Auto-Scroll (2026-03-14)

Dragging a mouse selection past the top or bottom of the editor text
area now auto-scrolls the document in that direction. Previously the
cursor clamped to the first/last visible row and the document stayed
still.

### Changes to ed_mouse.inc
- Replaced row-clamping block in `tui_ed_mouse_drag` with above/below
  detection that branches to `.drag_scroll_up` / `.drag_scroll_down`
- **Scroll up** (mouse above editor): sets `cur_line = top_line - 1`
  (clamped to 0). `scroll_to_cursor` scrolls the viewport up. Each
  subsequent frame repeats if mouse stays above.
- **Scroll down** (mouse below editor): sets `cur_line = top_line +
  ED_TEXT_ROWS` (non-wrap) or `_row_to_line[last_row] + 1` (wrap),
  clamped to `total_lines - 1`. `scroll_to_cursor` scrolls down.
- **In-range path**: completely untouched — existing coordinate mapping
  for both wrap and non-wrap modes.
- Added `JMP .drag_done` after `.drag_sel_done` to prevent fall-through
  into the new scroll handler blocks.
- Column computed from mouse X in scroll paths; `left_col` added in
  non-wrap mode. Column clamped via existing `.drag_col_set` path.

### Test results
- 16/16 drag-scroll tests passing (test_drag_scroll.sh)
- 22/22 Phase 1 regression clean
- Phase 2-4 regressions clean (pre-existing stale tests unrelated)

## v0.25.0 — Selection Phase 5: Delete + Undo Grouping (2026-03-14)

Selected text can now be deleted. Backspace and Delete on a selection
remove the selected text. Typing a character or pressing Enter deletes
the selection first, then inserts. All selection deletions are
undoable as a single Ctrl+Z operation via grouped undo records.

Also fixes a pre-existing selection anchor off-by-one bug where
N Shift+arrow presses selected N-1 characters instead of N.

### Changes to ed_sel.inc
- sel_delete: replaced stub with real implementation. Backward walk
  from selection end to start, calling delete_at per character.
  Each deletion recorded as UR_DEL_CHAR or UR_DEL_CRLF with
  URF_GROUP_CONT flag (first record plain, rest flagged). Handles
  collapsed selections (start==end) by clearing and returning CF=1.
  POP AX deferred after housekeeping calls (sel_clear,
  scroll_to_cursor, FW_DIRTY, meta_dirty) so that scroll_to_cursor
  does not clobber the caller's AX (critical for .do_char where AX
  holds the typed character).

### Changes to ed_keys.inc
- .do_char: CALL sel_clear → CALL sel_delete (preserves AX)
- .do_enter: CALL sel_clear → CALL sel_delete
- .do_bksp: CALL sel_clear → CALL sel_delete + JNC .bksp_done
- .do_del: CALL sel_clear → CALL sel_delete + JNC .del_rec_done
- .extended: added anchor pre-capture — calls sel_start_or_extend
  BEFORE any navigation key moves the cursor (5 lines). This fixes
  the off-by-one bug where the anchor was captured at the post-move
  position. Without this fix, Shift+Right from col 0 set anchor to
  col 1 instead of col 0, causing all selection ranges to be 1
  character short.

### Changes to TEDIT.ASM
- Added 3 BSS transient variables: sd_s_line (RESW), sd_s_col (RESW),
  sd_first (RESB) — 5 bytes for sel_delete loop state.

### Known limitation
- Type-replaces-selection requires 2× Ctrl+Z (once for insert, once
  for deletion group). Single-undo deferred to post-POC.

### No changes
ed_draw.inc, ed_mouse.inc, ed_const.inc, ed_core.inc, ed_edit.inc,
ed_undo.inc, ed_file.inc, TUI framework.

## v0.24.0 — Selection Phase 4: Mouse Selection (2026-03-14)

Click-drag in the editor now creates a text selection with real-time
visual highlighting. Clicking without dragging clears any existing
selection (including keyboard selections from Phase 2).

### Changes to ed_mouse.inc
- tui_ed_mouse_press: added CALL sel_clear to dismiss any prior
  selection, then saves (cur_line, cur_col) to sel_anchor_line/col
  as a potential selection starting point. sel_active stays 0 until
  drag moves cursor away from anchor.
- tui_ed_mouse_drag: added selection activation logic after cursor
  position is finalized. If (cur_line, cur_col) differs from anchor,
  sel_active = 1 (highlighting appears). If cursor returns to anchor
  position, sel_active = 0 (highlighting disappears).

### Behavior
- Click without drag: cursor positions, selection clears
- Click + drag: selection from click point to drag position
- Release: selection persists (can be extended with Shift+arrow)
- Click after drag-select: previous selection clears
- Keyboard select → click: keyboard selection clears
- Drag back to start: selection deactivates

### No changes
ed_sel.inc, ed_draw.inc, ed_keys.inc, ed_const.inc, ed_core.inc,
ed_edit.inc, ed_undo.inc, ed_file.inc, TEDIT.ASM, TUI framework.

## v0.23.0 — Selection Phase 3: Selection Rendering (2026-03-13)

Selected text now appears with a distinct visual highlight (blue on
light gray, attribute 71h). The highlight covers all text characters
between the selection anchor and cursor in document order.

### Changes to ed_draw.inc
- Pre-render: normalizes selection bounds into BSS transient vars
  (sel_s_line, sel_s_col, sel_e_line, sel_e_col, sel_rendering)
  once per draw call, before the render_restart loop
- char_loop: per-character attribute check replaces the fixed
  MOV ES:[DI], AX with a 17-instruction selection range test.
  Fast path (no selection): 3 instructions (MOV AH, CMP, JE).
  Uses free registers BX and DX — zero PUSH/POP overhead.
- Wrap mode: added rnd_doc_col tracking — increments per visible
  character, resets on new logical line. Used for selection column
  comparison in wrap mode instead of left_col + CX.
- row_next: resets rnd_doc_col to 0 on logical line advance

### Changes to TEDIT.ASM
- Added 6 BSS transient variables (11 bytes): sel_s_line, sel_s_col,
  sel_e_line, sel_e_col, sel_rendering, rnd_doc_col

### Visual behavior
- Non-selected text: bright white on blue (1Fh) — unchanged
- Selected text: blue on light gray (71h)
- Cursor on selected text: visible (17h nibble-swap of 71h)
- Padding spaces after line content: NOT highlighted (matches Notepad)
- [SEL] status bar indicator: unchanged from Phase 2

### No changes
ed_sel.inc, ed_const.inc, ed_core.inc, ed_edit.inc, ed_undo.inc,
ed_file.inc, ed_keys.inc, ed_mouse.inc, TUI framework files.

## v0.22.0 — Selection Phase 2: Keyboard Selection (2026-03-13)

Shift+arrow keys now create and extend text selections. The selection
anchor is set at the cursor's position when Shift+nav first activates,
and the cursor moves normally as subsequent Shift+nav keys are pressed.
Releasing Shift and pressing a navigation key clears the selection.

### Changes to ed_keys.inc
- Replaced `CALL sel_clear` in `.cursor_moved` with Shift-conditional
  logic: if Shift held (`_key_modifiers` bits 0+1) → `CALL sel_start_or_extend`,
  else → `CALL sel_clear`. Zero changes to any navigation key handler.

### Changes to ed_draw.inc
- Added `[SEL]` indicator to status bar, shown when `sel_active == 1`.
  Appears after `[Modified]` when both are active.

### Changes to TEDIT.ASM
- Added `s_stat_sel` string constant: `'  [SEL]'`

### Keys With Shift Selection Support
Up, Down, Left, Right, Home, End, PgUp, PgDn — all modes including
wrap mode. No-op cases (e.g. Shift+Up at line 0) correctly produce
no selection.

### No changes
ed_sel.inc, ed_const.inc, ed_core.inc, ed_edit.inc, ed_undo.inc,
ed_file.inc, ed_mouse.inc, TUI framework files.

## v0.21.0 — Selection Phase 1: Selection State Infrastructure (2026-03-13)

Zero-visible-change infrastructure phase. Establishes the selection data model, helper procedures, and integration hooks that all subsequent selection phases depend on.

### New Files
- `ed_sel.inc` — 4 selection helper procedures:
  - `sel_clear`: clears selection and requests redraw (no-op if already inactive)
  - `sel_start_or_extend`: snapshots cursor to anchor on first call, no-op on subsequent calls
  - `sel_normalize`: returns selection range in document order (BX/CX=start, DX/SI=end)
  - `sel_delete`: Phase 1 stub — clears selection, returns CF=0 (Phase 5 replaces with real deletion)

### Changes to ed_const.inc
- Added `ED_ATTR_SEL EQU 71h` — blue on light gray (selected text highlight, inverts ED_ATTR_NORM 1Fh)

### Changes to TEDIT.ASM
- Added `INCLUDE ed_sel.inc` between ed_file.inc and ed_draw.inc
- Added `MOV BYTE [sel_active], 0` in `.start_tui` initialization
- Added 3 BSS variables in transient section: `sel_active` (BYTE), `sel_anchor_line` (WORD), `sel_anchor_col` (WORD) — 5 bytes total, outside DOC_CTX

### Changes to ed_keys.inc
- Added `CALL sel_clear` at 5 integration points (first instruction of each handler):
  - `.do_char` — printable character insertion
  - `.do_enter` — Enter key
  - `.do_bksp` — Backspace (after Ctrl+H disambiguation)
  - `.do_del` — Delete key
  - `.cursor_moved` — shared exit for all navigation keys (Up/Down/Left/Right/Home/End/PgUp/PgDn + wrap variants)

### Changes to ed_file.inc
- Added `MOV BYTE [sel_active], 0` in `ed_load_file` (after state reset, before `undo_clear`)
- Added `MOV BYTE [sel_active], 0` in `ed_new_doc` (after state reset, before `undo_clear`)

### Tests Passing (22/22)
**A: Startup** — A1: file load, A2: empty doc
**B: Typing** — B1: type on file, B2: type on empty
**C: Enter** — C1: split line, C2: at start of file
**D: Backspace** — D1: delete char, D2: merge lines
**E: Delete** — E1: remove char, E2: merge lines
**F: Navigation** — F1: right arrow, F2: down, F3: up at top, F4: end, F4b: home, F5: pgdn
**G: Undo/Redo** — G1: Ctrl+Z, G2: Ctrl+Shift+Z
**H: Save** — H1: Ctrl+S clears modified
**I: Menu** — I1: F10 open/Esc close
**J: Mouse** — J1: click in text
**K: Stress** — K1: type/nav/undo

### Design Notes
- DOC_CTX_SIZE unchanged at 12440 (selection state is transient, not part of document context)
- `sel_clear` fast path: CMP + JE (taken 99.9% of the time) + RET — ~30 cycles overhead per keystroke
- Phase 2 will split `.cursor_moved` into `.cursor_moved` (sel_clear) and `.cursor_moved_no_clear` (Shift+arrow)

---

## v0.20.0 — Undo Phase 5: Bug Fixes, Dirty Tracking & Operation Grouping (2026-03-12)

Phase 5 completes the undo/redo system with three sub-phases: bug fixes, smart dirty flag tracking, and operation grouping.

### Sub-Phase 5a: Bug Fixes

- **Sentinel underflow in `undo_discard_half`** (ed_undo.inc): `undo_save_pos = 0FFFFh` (sentinel) was corrupted when `undo_discard_half` subtracted the half-count. Added sentinel guard (`CMP 0FFFFh / JE .done`) before the comparison.
- **Ghost record on failed `insert_char`** (ed_keys.inc `.do_char`): Now checks if `cur_col` advanced after `insert_char`; skips undo recording on failure.
- **Ghost record + cursor desync on failed `insert_crlf`** (ed_keys.inc `.do_enter`): Saves `add_used` before `insert_crlf` and compares after; skips cursor advance and undo recording if unchanged.

### Sub-Phase 5b: Smart Dirty Flag Tracking

Tracks `undo_save_pos` — the `undo_pos` at last save. After undo/redo, compares `undo_pos` to `undo_save_pos`; if equal, `[Modified]` clears.

- **`undo_clear`** (ed_undo.inc): Sets `undo_save_pos = 0` (not `0FFFFh`) — position 0 = clean state for new/loaded docs.
- **`undo_push`** (ed_undo.inc): Invalidates `undo_save_pos` to `0FFFFh` when redo tail is truncated and save_pos falls within the truncated range (timeline divergence).
- **`undo_execute` / `redo_execute`** (ed_undo.inc): Smart dirty flag — sets `dirty=0` when `undo_pos == undo_save_pos`, `dirty=1` otherwise.
- **`menu_save_handler` / `menu_quit_handler`** (TEDIT.ASM): Record `undo_save_pos = undo_pos` after every `save_file` call (4 sites total).

### Sub-Phase 5c: Operation Grouping

Consecutive character inserts on the same line with contiguous columns are grouped. One Ctrl+Z undoes the entire group; one Ctrl+Shift+Z redoes it.

- **New BSS variables**: `undo_group_active` (1 if last op was INS_CHAR), `undo_group_line`, `undo_group_col` (tracking last insert position), `undo_last_type` (scratch for group loop).
- **`.do_char` group flag logic** (ed_keys.inc): Sets `URF_GROUP_CONT` (bit 7 of UR_TYPE) when same line + contiguous column. Updates tracking vars after recording.
- **Group breaks**: `MOV BYTE [undo_group_active], 0` added to `.do_enter`, `.do_bksp`, `.do_del`, `.do_save`, `.do_undo`, `.do_redo`, `.cursor_moved`, `menu_undo_handler`, `menu_redo_handler`, `undo_init`, `undo_clear`.
- **`undo_execute` group loop** (ed_undo.inc): After undoing a record with `URF_GROUP_CONT`, continues to the previous record until one without the flag is reached.
- **`redo_execute` group loop** (ed_undo.inc): After redoing a record, peeks at the NEXT record — if it has `URF_GROUP_CONT`, continues redoing.

### Tests Passing (17/17)

**5a regression:**
1. Type A, undo, redo → "A" visible, Col 2

**5b dirty tracking:**
2. Type ABC, undo → no [Modified] (returns to save point)
3. Type AB, save, type CD, undo → "AB", no [Modified]
4. Type AB, save, undo → empty, [Modified]
5. Type AB, save, undo, redo → "AB", no [Modified]
6. Type AB, save, undo, type C → "C", [Modified] (divergence)

**5c grouping:**
7. Type ABCD, undo → empty (group undo)
8. Type ABCD, undo, redo → "ABCD" (group redo)
9. AB, Enter, CD, undo → AB + empty line 2 (CD group removed)
10. AB, Enter, CD, undo x2 → AB on one line (CRLF undone)
11. AB, Left, Right, CD, undo → AB (cursor move breaks group)
12. AB, Bksp, CD, undo → A (backspace breaks group)
13. Undo/redo on empty → no crash

**Regression:**
14. Bksp undo → AB restored
15. Bksp undo+redo → A
16. Enter undo → AB on one line
17. Full round-trip (undo x3, redo x3) → AB + CD

## v0.19.0 — Undo Phase 4: Single-Step Redo / Ctrl+Shift+Z (2026-03-12)

Ctrl+Shift+Z now re-executes undone edits. Edit > Redo menu entry also wired. Full undo/redo cycle operational.

### New: `redo_execute` procedure (ed_undo.inc)
- Mirrors `undo_execute` structurally but re-executes the forward (original) operation:
  - `UR_INS_CHAR` → `insert_char(UR_CHAR)` (re-insert character)
  - `UR_INS_CRLF` → `insert_crlf` + manual cursor advance to next line col 0
  - `UR_DEL_CHAR` → `delete_at` (re-delete character)
  - `UR_DEL_CRLF` → `delete_at` (re-delete CRLF pair)
- Reads record at `undo_pos` (before increment), increments `undo_pos` after execution
- No cursor restore — uses natural post-operation position (matches original edit behavior)
- Disables `undo_recording` during forward ops to prevent recursive recording
- Sets `dirty=1`, `meta_dirty=1`, `last_ins_pi=FFFFh`, `FW_DIRTY=1`, calls `scroll_to_cursor`
- Safe no-op when `undo_pos >= undo_count` (nothing to redo)

### Changes to `ed_keys.inc`
- `.do_redo` now calls `redo_execute` instead of `menu_edit_stub`

### Changes to `TEDIT.ASM`
- Added `menu_redo_handler` (calls `redo_execute`)
- Edit > Redo menu entry points to `menu_redo_handler` instead of `menu_edit_stub`

### Tests Passing (12/12)
1. Redo single char insert → "A" visible, Col 2
2. Redo 3 char inserts → "ABC", Col 4
3. Redo Enter (CRLF) → Line 2 of 2, Col 1
4. Redo Backspace (char delete) → "A" only, Col 2
5. Redo Backspace (CRLF delete) → Line 1 of 1, Col 2
6. Redo with nothing to redo → no crash
7. Redo invalidation (new edit clears redo tail) → "ABD", Col 4
8. Full round-trip (undo all, redo all) → "A"/"B" on 2 lines
9. Partial undo then redo → "ABC", Col 4
10. Menu Redo (Alt+E, R) → "A" visible, no stub dialog
11. Interleaved undo/redo → "AB", Col 3
12. Redo on loaded file → "XThe Chair", Col 2

---

## v0.18.0 — Undo Phase 3: Single-Step Undo / Ctrl+Z (2026-03-12)

Ctrl+Z now executes the inverse of the most recent edit. Each press undoes one operation (character insert, CRLF insert, character delete, or CRLF delete). Edit > Undo menu entry also wired.

### New: `undo_execute` procedure (ed_undo.inc)
- Reads the top undo record, dispatches the inverse operation:
  - `UR_INS_CHAR` → `delete_at` (undo character insertion)
  - `UR_INS_CRLF` → `delete_at` (undo CRLF insertion / line split)
  - `UR_DEL_CHAR` → `insert_char` (undo character deletion)
  - `UR_DEL_CRLF` → `insert_crlf` (undo CRLF deletion / line merge)
- Saves cursor-restore fields (`UR_CUR_LINE`/`UR_CUR_COL`) and doc position to stack before dispatching (since inverse ops clobber ES:BX)
- Disables `undo_recording` during inverse ops to prevent recursive recording
- Restores cursor to pre-edit position, sets `dirty=1`, `meta_dirty=1`, `last_ins_pi=FFFFh`, `FW_DIRTY=1`, calls `scroll_to_cursor`
- Decrements `undo_pos` but preserves `undo_count` (undone records remain available for redo)
- Safe no-op when `undo_pos == 0` (nothing to undo)

### Changes to `ed_keys.inc`
- `.do_undo` now calls `undo_execute` instead of `menu_edit_stub`

### Changes to `TEDIT.ASM`
- Added `menu_undo_handler` (calls `undo_execute`)
- Edit > Undo menu entry points to `menu_undo_handler` instead of `menu_edit_stub`

### Tests Passing (12/12)
1. Undo single char insert → empty doc, Col 1
2. Undo 3 char inserts → empty doc
3. Undo char + Enter + char → empty doc, Line 1 of 1
4. Undo Backspace (char delete) → "AB" restored, Col 3
5. Undo Backspace (CRLF delete / line merge) → line break restored, Line 2 of 2
6. Undo Delete key → "AB" restored, Col 1
7. Undo with nothing to undo → no crash, empty doc
8. Undo on loaded file → original content restored
9. Multiple undo past all edits → no crash
10. Undo preserves redo tail → "AB" visible after undoing "C"
11. Edit menu Undo (Alt+E, U) → works, no stub dialog
12. Type after undo → "ABD" visible (redo tail truncated by new edit)

---

## v0.17.0 — Undo Phase 2: Recording Edit Actions (2026-03-12)

Every user edit now pushes an undo record into the buffer. Ctrl+Z still shows the placeholder dialog (Phase 3 replaces it).

### Bug Fix: `free_all` no longer frees `undo_seg`
- **Problem**: `free_all` freed `undo_seg` and zeroed it. Since `ed_load_file` and `ed_compact` call `free_all`, any subsequent typing after File>Open would write to segment 0 (IVT), causing a crash.
- **Fix**: Removed `undo_seg` freeing from `free_all`. The undo buffer is now freed only at program exit.

### `delete_at` exports deleted char info
- Two new BSS scratch variables: `undo_del_char` (byte deleted), `undo_del_crlf` (1 if CRLF pair, 0 if single, 0FFh sentinel if no delete).
- All CRLF companion deletion paths in `delete_at` set `undo_del_crlf = 1`.

### Recording hooks in `ed_keys.inc`
- `.do_char`: saves pre-cursor, calls `insert_char`, pushes `UR_INS_CHAR` with original character (AL preserved by `insert_char`).
- `.do_enter`: saves pre-cursor, pushes `UR_INS_CRLF` after cursor advance.
- `.do_bksp`: saves pre-cursor before adjustment, sets 0FFh sentinel, calls `delete_at`, pushes `UR_DEL_CHAR` or `UR_DEL_CRLF` (skips if sentinel unchanged = delete failed). Doc position = post-delete `cur_line`/`cur_col`; restore position = pre-backspace cursor.
- `.do_del`: saves pre-cursor, sets 0FFh sentinel, calls `delete_at`, pushes `UR_DEL_CHAR` or `UR_DEL_CRLF`. Doc and restore positions both = pre-cursor.

### Tests Passing
- A: Type "ABC" → 3 INS_CHAR records ('A' 'B' 'C') at (0,0) (0,1) (0,2)
- B: Type "A", Enter, "B" → INS_CHAR 'A', INS_CRLF, INS_CHAR 'B' at (1,0)
- C: Type "AB", Backspace → INS 'A', INS 'B', DEL_CHAR 'B'
- D: Type "A", Enter, Bksp col-0 → INS 'A', INS_CRLF, DEL_CRLF
- E: Delete key on file → DEL_CHAR 'T' (first char of file)
- F: Ctrl+Z → placeholder dialog, no crash
- G: File menu interaction after typing → no crash (free_all fix)
- H: Basic editing regression → IDLE, correct display

---

## v0.15.1 — Fix: Wrap-Mode Visual Line Navigation (2026-03-12)

**Bug**: In wrap mode, UP/DOWN arrow keys navigated by logical line instead of visual line. With a 120-character line wrapping to 2 visual rows, pressing RETURN then UP skipped the second visual row entirely — cursor jumped from (1, 3) to (1, 1) instead of (1, 2).

**Root cause**: `.key_up` and `.key_down` always moved `cur_line` by 1, which skips over wrapped visual rows within the same logical line.

**Fix in `ed_keys.inc`**: Added wrap-mode branches (`.key_up_wrap`, `.key_down_wrap`) that navigate by visual row:
- **UP within same line**: if `cur_col >= ED_COLS`, subtract `ED_COLS` (move up one visual row)
- **UP to previous line**: if `cur_col < ED_COLS`, decrement `cur_line` and land on the last visual row of the previous line at the same screen column (`last_vis_row_start + screen_col`, clamped to `line_length`)
- **DOWN within same line**: compute `next_vis_row_start`; if within `line_length`, move there at same screen column
- **DOWN to next line**: if `next_vis_row_start > line_length`, increment `cur_line` and set `cur_col = screen_col` (first visual row), clamped to new `line_length`

Non-wrap mode unchanged — still uses original logical-line navigation via `.vert_moved`.

### Tests Passing
1. 120 chars, UP — Line 2, Col 41 (moved up one visual row within line)
2. 120 chars, UP UP — Line 1, Col 41 (top of document, can't go higher)
3. 120 chars, UP DOWN — Line 2, Col 121 (round-trip back)
4. 120 chars, Home, DOWN — Line 2, Col 81 (moved down to second visual row)
5. 120 chars, Home, DOWN, UP — Line 1, Col 1 (round-trip back)
6. 2x120 chars, UP×4 — walks all 4 visual rows correctly
7. Original bug scenario: 120 chars + Enter + UP → Line 2, Col 81 (no longer skips visual row 2)

---

## v0.16.0 — Undo Phase 1: Buffer Infrastructure (2026-03-12)

Added the undo/redo buffer subsystem — a separately allocated 32 KB segment holding fixed-size 10-byte records. Six primitives provide the foundation for Phases 2-5.

### New Files
- `ed_undo.inc` — 6 procedures: `undo_init`, `undo_push`, `undo_discard_half`, `undo_peek_undo`, `undo_peek_redo`, `undo_clear`

### Changes to ed_const.inc
- Added undo constants: `UNDO_SEG_PARA` (0800h = 32 KB), `UR_SIZE` (10), `UNDO_MAX_RECORDS` (3276)
- Record type codes: `UR_INS_CHAR(0)`, `UR_INS_CRLF(1)`, `UR_DEL_CHAR(2)`, `UR_DEL_CRLF(3)`
- Group continuation flag: `URF_GROUP_CONT(80h)` on bit 7 of `UR_TYPE`
- Field offsets: `UR_TYPE(0)`, `UR_CHAR(1)`, `UR_DOC_LINE(2)`, `UR_DOC_COL(4)`, `UR_CUR_LINE(6)`, `UR_CUR_COL(8)`

### Changes to TEDIT.ASM
- Added `INCLUDE ed_undo.inc` between ed_edit.inc and ed_file.inc
- Added undo BSS variables: `undo_seg`, `undo_pos`, `undo_count`, `undo_max`, `undo_recording`, `undo_save_pos`, `undo_pre_line`, `undo_pre_col`
- Added undo segment allocation in `.start_tui` (DOS INT 21h AH=48h) + `undo_init` call
- Added undo segment freeing in `free_all`

### Architecture
- `undo_push`: AL=type, AH=char, BX=doc_line, CX=doc_col, DX=cur_line, SI=cur_col; clobbers AX,BX,CX,DX,SI,DI
- `undo_peek_undo`/`undo_peek_redo`: return ES:BX pointer; clobber AX,BX,ES
- Buffer-full: `undo_discard_half` shifts newest half to position 0, adjusts `save_pos`
- `undo_recording` flag (checked in `undo_push`) suppresses recording during undo/redo replay

---

## v0.15.0 — Wrap-Mode Visual Line Status Bar (2026-03-12)

In wrap mode, the status bar now shows visual line numbers instead of logical line numbers. A 120-character line wrapping to 2 visual rows counts as 2 lines in the status bar.

### Changes to TEDIT.ASM
- Added `total_vis_lines: RESW 1` and `cur_vis_line: RESW 1` to BSS transient section

### Changes to ed_core.inc
- `rebuild_meta`: added dual-counting in wrap-mode byte loop — increments `total_vis_lines` on LF and on column wrap (`stl_col >= ED_COLS`), tracks `cur_vis_line` when logical line count reaches `cur_line`

### Changes to ed_draw.inc
- Status bar: in wrap mode, displays `cur_vis_line`/`total_vis_lines` instead of `cur_line`/`total_lines`

---

## v0.14.3 — Fix: Wrap-Mode Line Counting (2026-03-12)

**Bug**: `rebuild_meta` and `seek_to_line` counted visual wrap boundaries as logical lines in wrap mode. A 120-character line wrapping to 2 visual rows was counted as 2 logical lines, causing `total_lines` to be inflated and navigation to skip lines.

**Fix**: Both `rebuild_meta` and `seek_to_line` now count only LFs as logical line boundaries in all modes. Removed dead wrap-mode byte loops that incorrectly tracked visual wrap points as line breaks.

---

## v0.14.2 — Fix: Wrap-Mode Mouse Click (2026-03-12)

**Bug**: Mouse clicks in wrap mode targeted incorrect lines because the renderer's screen row did not correspond to the logical line the user clicked on.

**Fix**: Added `_row_to_line: RESW ED_TEXT_ROWS` BSS table populated during rendering. Mouse press and drag handlers now use this table for correct line+column mapping in wrap mode.

---

## v0.14.1 — Fix: Empty-Doc Mouse Click Crash (2026-03-12)

**Bug**: Clicking in an empty document caused an unsigned underflow crash. `total_lines` was 0, and mouse clamp code computed `total_lines - 1` which wrapped to 65535.

**Fix**: `rebuild_meta` now always ensures `total_lines >= 1`, preventing the underflow in mouse clamp code.

---

## v0.14.0 — Wrap-Mode Cursor Positioning (2026-03-11)

Fixed cursor visibility in wrap mode. Previously the cursor vanished when `cur_col >= 80` and scroll didn't account for wrapped lines consuming extra visual rows.

### Changes to TEDIT.ASM
- Added `cur_vis_row: RESW 1` and `wrap_retry: RESB 1` to BSS transient section (after `left_col`)

### Changes to ed_draw.inc
- **Renderer init**: added `cur_vis_row = FFFFh` sentinel and `wrap_retry = ED_TEXT_ROWS` guard; added `.render_restart` label after one-time setup (reads `ed_abs_row` from memory instead of DH for re-render safety)
- **Row loop**: added `cur_vis_row` capture — stores `rnd_row` when renderer first encounters `cur_line` (runs in both modes, 3 comparisons per row)
- **Post-render scroll adjustment**: after status bar, computes `cursor_row = cur_vis_row + cur_col / ED_COLS`; if off-screen bottom, increments `top_line` and jumps to `.render_restart`; if `cur_line` not rendered at all, sets `top_line = cur_line` and re-renders; `wrap_retry` counter prevents infinite loops
- **ed_draw_cursor**: rewritten with dual code paths — non-wrap uses `cur_col - left_col` (unchanged from v0.13.0), wrap uses `DIV 80` to compute visual row (`cur_vis_row + quotient`) and screen column (remainder); now saves/restores DX for DIV remainder

### No changes
ed_core.inc, ed_keys.inc, ed_mouse.inc, ed_file.inc, ed_edit.inc, ed_const.inc, TUI framework files.

## v0.13.0 — Horizontal Scroll for Non-Wrap Mode (2026-03-11)

Added `left_col` horizontal scroll offset so the viewport pans when the cursor moves past column 80. In non-wrap mode, the screen displays columns `[left_col, left_col + 80)`. Wrap mode is unaffected (Phase B deferred).

### Changes to TEDIT.ASM
- Added `left_col: RESW 1` BSS variable in transient section (after `ed_abs_row`)
- Added `MOV WORD [left_col], 0` in `.start_tui` initialization

### Changes to ed_core.inc
- Restructured `scroll_to_cursor`: vertical logic now falls through (via JMP `.vert_done`) to a new horizontal clamping block (non-wrap only)
- Scroll right: `left_col = cur_col - ED_COLS + 1` (cursor at right edge)
- Scroll left: `left_col = cur_col` (cursor at left edge)
- Wrap mode skips horizontal logic entirely

### Changes to ed_draw.inc
- Added `ed_hscroll_skip` procedure (~60 lines): walks piece table past `CX` visible characters, updating `rnd_pi`/`rnd_off`. Handles CR skip, LF (short line), piece boundaries.
- Renderer row_loop: inserted hscroll skip call between `total_lines` check and `resolve_piece`, with `PUSH DI`/`POP DI` to preserve shadow_buf write pointer
- `ed_draw_cursor`: subtracts `left_col` from `cur_col` in non-wrap mode for correct screen column

### Changes to ed_file.inc
- Added `MOV WORD [left_col], 0` in `ed_load_file` and `ed_new_doc`
- Added `left_col` save/restore in `ed_compact` (both success and fail-recover paths)

### Changes to ed_mouse.inc
- `tui_ed_mouse_press`: adds `left_col` to screen column for document column (non-wrap only)
- `tui_ed_mouse_drag`: same adjustment

### Bug Found & Fixed During Implementation
- `ed_hscroll_skip` clobbers DI (used as piece base pointer), but the renderer uses DI as the shadow_buf write position. Caused memory corruption and invalid opcode crashes at unrelated addresses. Fixed by wrapping the call with `PUSH DI` / `POP DI`.

### Tests Passing
1. Type 85 X's — IDLE, Col 86, 79 X's visible (left_col=6)
2. Down, End, Home on long line — Col 1, line starts from 'A' (left_col reset)
3. End on 98-char line — Col 99, "EXTRA_TEXT_PAST_80" visible at right
4. Home, Right×5 — Col 6, left_col=0
5. End on long line, Down to short — Col 14, cursor visible
6. Type AB, Backspace — Col 2, normal editing unaffected
7. Mouse click on scrolled view — Col 20 (left_col + screen_col)
8. Wrap mode unchanged — IDLE, left_col stays 0
9. Navigation regression — all keys work on short lines
10. Ctrl+Z regression — edit stub dialog appears

---

## v0.12.0 — Edit Menu with Placeholder Items (2026-03-11)

Added an Edit menu between File and Info with 11 placeholder entries and keyboard shortcuts. Each entry shows a "Not yet implemented" dialog. Shortcuts work directly from the editor without opening the menu.

### Menu Layout
- File | **Edit** | Info
- Edit dropdown: Undo (Ctrl+Z), Redo (Ctrl+Shift+Z), Cut (Ctrl+X), Copy (Ctrl+C), Paste (Ctrl+V), Find... (Ctrl+F), Find Next (F3), Find Prev (Shift+F3), Replace... (Ctrl+H), Goto... (Ctrl+G), Time/Date (F5)

### Changes to TEDIT.ASM
- Added 24 string constants: 12 menu entry labels, 11 accelerator display strings, 1 stub message ("Not yet implemented")
- Added `m_edit_entries` (11 entries, 110 bytes) with dropdown hotkeys and accelerator text
- Inserted Edit item in `m_menu_items` (MI_X=6, MI_W=6, MI_DDW=24, Alt+E = scan 12h)
- Shifted Info MI_X from 6 to 12
- Updated `m_menubar` count from 2 to 3
- Added `menu_edit_stub` handler: shows msgbox with "Not yet implemented" / "Edit" title

### Changes to ed_keys.inc
- Added 7 Ctrl+key checks in normal-key dispatch: Ctrl+Z (1Ah), Ctrl+X (18h), Ctrl+C (03h), Ctrl+V (16h), Ctrl+F (06h), Ctrl+G (07h)
- Added 3 extended-key checks: F3 (3Dh), Shift+F3 (54h), F5 (3Fh)
- Added `.do_undo_or_redo` with `_key_modifiers` Shift-flag test for Ctrl+Z vs Ctrl+Shift+Z
- Added `.do_bksp_or_replace` with `_key_modifiers` Ctrl-flag test for Backspace vs Ctrl+H
- Added 8 stub labels (`.do_cut` through `.do_timedate`) sharing a single fall-through body

### Tests Passing
1. Menu bar shows File / Edit / Info (3 items)
2. Alt+E opens dropdown with all 11 entries
3. Dropdown hotkey 'G' activates Goto stub dialog
4. Down×2 Enter activates Cut stub dialog
5. Ctrl+Z — Undo placeholder dialog
6. Ctrl+Shift+Z — Redo placeholder dialog (Shift-flag disambiguation)
7. Ctrl+X — Cut placeholder dialog
8. Ctrl+C — Copy placeholder dialog
9. Ctrl+V — Paste placeholder dialog
10. Ctrl+F — Find placeholder dialog
11. F3 — Find Next placeholder dialog
12. Shift+F3 — Find Prev placeholder dialog
13. Ctrl+H — Replace placeholder dialog (Ctrl-flag disambiguation from Backspace)
14. Ctrl+G — Goto placeholder dialog
15. F5 — Time/Date placeholder dialog
16. Typing regression — normal character insertion unaffected
17. Backspace regression — plain 08h routes to delete, not Replace
18. Ctrl+S regression — save still works
19. Navigation regression — arrow keys still work
20. Menu bar full cycle — File→Edit→Info→File wraps correctly

---

## v0.11.1 — Shift-Flag Capture Infrastructure (2026-03-11)

Added `_key_modifiers` variable to `tui_event.inc` so `ed_keys.inc` can disambiguate keys that share the same ASCII byte (e.g., Ctrl+Z vs Ctrl+Shift+Z, Backspace vs Ctrl+H).

### Changes to TUI\tui_event.inc
- Added `_key_modifiers: DB 0` — stores BIOS shift flags (INT 16h AH=02h) for the most recent keypress
- In `tui_run` key-received path: added `MOV [_key_modifiers], AL` after existing INT 16h AH=02h call
- In `tui_run` `.no_key:` path: added `MOV BYTE [_key_modifiers], 0` to clear stale flags on idle

### Tests Passing
1. Ctrl+Shift+Z → `_key_modifiers` has Shift+Ctrl bits (07h)
2. Plain Ctrl+Z → no Shift bits in `_key_modifiers`
3. Ctrl+H → Ctrl bit set (04h) in `_key_modifiers`
4. Plain Backspace → no Ctrl bit in `_key_modifiers`
5. TEDIT regression — normal typing unaffected
6. TEDIT regression — Backspace still works
7. TEDIT regression — Ctrl+S still works

---

## v0.11.0 — Buffer Compaction: Add Buffer Full Handler (2026-03-11)

When the 64 KB add buffer fills up, the editor now shows a "Buffer Full" dialog instead of silently dropping keystrokes. The user can accept (save, reload from disk, reset buffer) or cancel (keystroke dropped but editor remains functional).

### Changes to ed_const.inc
- Added `ADD_BUF_BYTES EQU ADD_BUF_PARA * 16` — derived byte count for overflow checks

### Changes to TEDIT.ASM
- Added 3 string constants: `m_str_buf_ttl` ("Buffer Full"), `m_str_buf_ask` ("Save and reload to continue?"), `m_str_buf_err` ("Reload failed - file on disk")

### Changes to ed_file.inc
- **`ed_compact`**: saves cursor state, frees all document memory, reopens saved file, reloads chunks, rebuilds piece table + metadata, restores cursor position with clamping. On failure falls back to empty document with error dialog. Resets dirty, last_ins_pi, meta_dirty.
- **`ed_buffer_full`**: shows confirm dialog, calls `menu_save_handler` (handles Save As if untitled), checks dirty flag to verify save succeeded, then calls `ed_compact`. Returns CF=0 on success, CF=1 on cancel/failure.

### Changes to ed_edit.inc
- **`insert_char`**: replaced `CMP DI, 0FFFFh` / `JAE .ic_done` (silent drop) with `CMP DI, ADD_BUF_BYTES - 1` / `JB .ic_have_space` + call to `ed_buffer_full`. After compact, ES and DI are reloaded from the fresh add buffer.
- **`insert_crlf`**: same pattern — `CMP DI, 0FFFEh` / `JAE .icr_done` replaced with `CMP DI, ADD_BUF_BYTES - 2` / `JB .icr_have_space` + compact logic.

### Tests Passing
1. Normal typing regression — "Hello" at Col 6, no dialog
2. File load + edit + Ctrl+S regression — "XY" prepended, saved
3. Buffer full triggers dialog (tiny 32-byte buffer) — dialog appears at char 32
4. Accept compact — save + reload, typing continues after compact ("More" at Col 37)
5. Cancel compact (Escape) — keystroke dropped, editor remains functional
6. CRLF triggers compact (insert_crlf path) — Enter at near-full buffer compacts, new line created

---

## v0.10.0 — Multi-Doc Removal: Revert to Single-Document Editor (2026-03-11)

Reverted to single-document editor. Deleted `ed_multidoc.inc`, restored 3-entry File menu (Open/Save/Quit), kept filename in status bar and DOC_CTX layout.

---

## v0.5.1 — Fix: Empty Document First-Keystroke Rendering (2026-03-09 ~10:30 UTC)

**Bug**: Typing in a new empty document (or after deleting all text) didn't show characters on screen. Cursor moved but the first line stayed blank. Only pressing Enter made text appear.

**Root cause**: `ed_new_doc` seeds `chkpt_pi[0]=0` as a "start of document" sentinel. When `insert_char` inserts the first piece at index 0 via `pt_insert`, `chkpt_adjust_insert` bumps `chkpt_pi[0]` from 0 to 1 (because `0 >= 0`). But piece 1 doesn't exist — only piece 0 does. So `seek_to_line(0)` sets `rnd_pi=1`, which is `>= pt_count(1)`, and the renderer draws a blank row. Enter works because `ed_keys.inc` sets `meta_dirty=1` after `insert_crlf`, triggering `rebuild_meta` which corrects the checkpoint.

**Fix in `ed_edit.inc`**: Set `meta_dirty=1` in insert_char's `.ic_insert_before` path (taken when `BX >= pt_count`, i.e., cursor past all pieces). This triggers one `rebuild_meta` on the next render, correcting the stale checkpoint. Subsequent keystrokes use the fast path (extending the tracked piece) so no further rebuilds occur.

**Performance impact**: One `rebuild_meta` call per "start typing at a new end-of-document position" — not per keystroke. The fast path avoids repeated rebuilds.

---

## v0.5.0 — Phase 5: Mouse Support + Polish (2026-03-09 ~10:07 UTC)

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

## v0.4.0 — Phase 4: File Operations: Open, Save As, Quit Confirmation (2026-03-08 ~23:29 UTC)

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

## v0.3.1 — Fix: Column Clamping on Vertical Cursor Movement (2026-03-08 ~22:32 UTC)

**Bug**: Moving cursor vertically (Up, Down, PgUp, PgDn) did not clamp `cur_col` to the length of the destination line. E.g., pressing End on a 50-char line then Down to a 5-char line left `cur_col` at 50 — cursor drew on a blank cell and typing inserted at the wrong visual position.

**Fix in `ed_keys.inc`**:
- Added `.vert_moved` label between navigation handlers and `.cursor_moved`
- `.vert_moved` calls `line_length`, clamps `cur_col` to AX if greater, then falls through to `.cursor_moved`
- Changed `.key_up`, `.key_down`, `.pgdn_ok`, `.pgup_ok` to jump to `.vert_moved`
- Left `.key_left`, `.key_right`, `.key_home`, `.key_end` unchanged (still jump to `.cursor_moved`)

---

## v0.3.0 — Phase 3: Text Editing + Save + Simplified Status Bar (2026-03-08 ~21:50 UTC)

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
1. ~~**Checkpoint staleness on insert_before/insert_after**~~: *Reverted* — the original TEXTEDIT.ASM was correct. `chkpt_adjust_insert` bumps checkpoint piece indices >= BX by +1, which keeps checkpoints valid without a full rebuild. Since `insert_char` only handles printable characters (20h-7Eh), line count never changes. The added `meta_dirty=1` was harmless but triggered an unnecessary `rebuild_meta` (full piece-table scan) on every first keystroke at a new cursor position — a performance penalty for large files.
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

## v0.2.0 — Phase 2: File Loading, Editor Display, Navigation (2026-03-08 ~18:00 UTC)

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
- main: shrink_mem -> parse_args -> open_file -> load_file -> pt_init -> tui_run
- Empty document mode: no filename -> pt_count=0, total_lines=1, seeded checkpoint
- BSS: ed_state (top_line + total_lines adjacent), all editor variables

### Key Adaptations from TEXTEDIT.ASM
- `SCREEN_COLS` -> `ED_COLS` (80), `TEXT_ROWS` -> `ED_TEXT_ROWS` (23)
- `ATTR_NORM` (07h) -> `ED_ATTR_NORM` (1Fh), `ATTR_STAT` (70h) -> `ED_ATTR_STAT` (70h)
- `0800h` shrink -> `SHRINK_PARA` (1000h = 64 KB)
- Render target: shadow_buf (ES=CS) instead of VIDEO_SEG (B800h)
- Status bar: null-terminated strings via ed_wstr instead of $-terminated via wstr_stat
- Software cursor (nibble-swap) instead of hardware cursor (INT 10h)

### Tests Passing
1. Load and display file (3-line test.txt)
2. Arrow key navigation (Down x2 + Right x3 -> L3/4 C4)
3. PgDn/PgUp (clamp to document bounds)
4. Home/End (End -> C12, Home -> C1)
5. Empty document (L1/1 C1, no crash)
6. Menu still works (File > Open dialog, Alt+Q exit)

---

## v0.1.0 — Phase 1: TUI Shell with Menu Bar (2026-03-06)

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
