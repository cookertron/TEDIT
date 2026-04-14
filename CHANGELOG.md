# TEDIT Changelog

## v0.71.0 — Mouse Selection, Settings, Clock & Recent Files (2026-04-14)

### New Feature: Double-Click Word Select / Triple-Click Line Select
- **Double-click** on a word selects it (contiguous run of A-Z, a-z, 0-9, _).
  Non-word characters (space, punctuation) get single-character selection.
- **Triple-click** selects the entire line (column 0 to line end).
- Click counting state machine using BIOS tick timing (9-tick / ~500ms window),
  same position required between clicks. Count resets to 0 after triple-click.
- Drag after double/triple-click extends selection at character granularity.
- Drag handler no-motion guard prevents stationary mouse polls from collapsing
  the word/line selection.

### New Procedures (`ed_mouse.inc`)
- `is_word_char` — character classification (CF=0 for word chars, CF=1 otherwise)
- `ed_read_line_buf` — reads current line into 256-byte scratch buffer via piece table
- `ed_select_word` — finds word boundaries around cursor, sets selection
- `ed_select_line` — selects entire line (anchor at col 0, cursor at line end)

### New BSS Variables (`TEDIT.ASM`)
- `ms_ed_click_tick/line/col/count` — click counting state (7 bytes)
- `word_scan_buf/len` — word boundary scan scratch (258 bytes)

### New Constants (`ed_const.inc`)
- `DBLCLICK_TICKS EQU 9` — double-click timing window

### New Feature: TUI Radio Button Control
- **Full radio button support** added to TUI framework (`CTYPE_RADIO = 5`).
- Renders as `(*) Label` (selected) or `( ) Label` (unselected).
- **Keyboard activation** (Enter/Space): clears all radio buttons in the window,
  sets the activated one (mutual exclusion).
- **Mouse click activation**: same mutual exclusion logic on left-button press.

### New Procedures (`TUI/tui_control.inc`, `TUI/tui_mouse.inc`)
- `tui_ctrl_draw_radio` — renders radio button with `(*)` / `( )` indicator
- `.do_radio` in `tui_ctrl_activate` — keyboard mutual exclusion handler
- `.press_radio` in `tui_mouse_on_press` — mouse click mutual exclusion handler

### New Feature: Status Bar Clock
- **HH:MM clock** displayed at the far right of the status bar (columns 74-79).
- Reads time via INT 21h AH=2Ch. Always right-aligned, never conflicts with
  left-side status content.
- Toggled via Settings dialog "Show clock" checkbox. Persists in TEDIT.CFG.

### New Procedures (`ed_draw.inc`)
- `ed_draw_clock` — renders ` HH:MM` at fixed right-side position using
  `.clock_put2` helper for 2-digit formatting

### New Feature: Settings Dialog (`Options > Settings...`)
- Modal dialog with:
  - **Tab Width** radio buttons (2, 4, 8) — changes tab rendering width
  - **Shell memory dump** checkbox — toggles /d flag for Shell to DOS
  - **Show clock** checkbox — toggles status bar clock visibility
- All settings saved to TEDIT.CFG on OK.
- Pre-selects current values when opened.

### New Feature: TEDIT.CFG Configuration File
- **Binary config file** persists settings across sessions.
- Format v3: 7-byte header (magic 'CE', version, tab_width, clock, shell_dump,
  recent_count) + recent file entries (count * 128 bytes).
- **Loaded at startup** after defaults, before command-line args (priority:
  defaults < config < CLI flags).
- **Backward compatible**: v2 configs (from previous sessions) load cleanly
  with an empty recent file list.
- Silently ignores missing or corrupt config files.

### New Feature: Recent Files List (File Menu)
- **Up to 7 recent files** displayed at the bottom of the File menu, below Exit.
- Most recently opened file at position 1; older files shift down.
- **Deduplication**: re-opening a file moves it to the top instead of adding
  a duplicate entry.
- **Persisted in TEDIT.CFG**: recent list survives editor restarts.
- **Click to open**: selecting a recent entry opens the file (handles slot
  reuse for untitled docs, new slot allocation, duplicate detection).
- **Stale entry removal**: if a recent file no longer exists, an error is shown
  and the entry is automatically purged from the list and config.
- **Entry validation on load**: empty or malformed entries in the config are
  filtered out during startup.
- **Exclusion**: files opened via Project Load are NOT added to the recent list.

### New Procedures (`TEDIT.ASM`)
- `recent_add` — add filename to list (dedup, shift, cap at 7, rebuild labels)
- `recent_build_labels` — extract basenames, format "N FILENAME" display strings
- `recent_remove` — remove entry by index (shift, save config)
- `recent_update_menu` — patch File menu MI_ECOUNT and MDE_TEXT pointers
- `recent_open` — open file from recent list (with pre-flight existence check)
- `recent_handler_0..6` — 7 menu handler stubs dispatching to `recent_open`
- `cfg_save` / `cfg_load` — extended for v3 format with recent file data
- `menu_settings_handler` — builds and runs Settings dialog
- `menu_clock_handler` — session clock toggle (View menu, removed in v0.71)

### New BSS Variables (`TEDIT.ASM`)
- `clock_visible` / `clock_on_start` — runtime and startup clock state
- `cfg_buf` (8 bytes) — config file I/O scratch
- `recent_count` / `recent_files` (896 bytes) / `recent_labels` (224 bytes) /
  `recent_clicked_idx` — recent file list storage and menu state
- `_stg_r2/_stg_r4/_stg_r8` — settings dialog radio pre-selection scratch

### New Constants (`ed_const.inc`)
- `RECENT_MAX EQU 7`, `RECENT_SLOT_SZ EQU 128`, `RECENT_LBL_SZ EQU 32`
- `FILE_MENU_BASE EQU 12`

### Menu Reorganisation
- **File menu**: Project Load/Save moved to Options menu. Recent files added
  below Exit.
- **View menu**: Clock entry removed (clock now controlled solely via Settings).
- **Options menu**: Settings moved to top. Divider added. Project Load/Save
  added below Convert line ending entry.

### Bug Fixes
- **ES segment bug in recent_add/recent_remove**: `REP MOVSB` writes to ES:DI,
  but ES pointed to `doc_table_seg` instead of DS when called from menu/startup
  paths. Fixed by saving/setting ES=DS at entry.
- **Stale recent entry not removed**: `ed_load_file` shows its own modal error
  dialog internally, causing IDLE termination before `recent_remove` could run.
  Fixed by adding a pre-flight file existence check (open + close) before
  calling `ed_load_file`.
- **Double-click selection collapsed by drag handler**: stationary mouse polls
  with `MSF_CTRL_DRAG` active caused the drag handler to recompute cursor
  position and clear selection. Fixed by adding an early-out guard in
  `tui_ed_mouse_drag` that skips processing when mouse cell hasn't changed.
- **Word select left-boundary off-by-one**: `is_word_char` left-scan had a
  spurious `INC CX` that shifted the word start one column right. Removed.

### Files Changed
- `TEDIT.ASM` — menu data, handlers, settings dialog, config I/O, recent files,
  BSS variables, startup hooks, About dialog version string
- `ed_const.inc` — new constants for click timing, recent files, tab settings
- `ed_mouse.inc` — click counting state machine, word/line selection procedures,
  drag handler no-motion guard
- `ed_draw.inc` — clock rendering in status bar
- `TUI/tui_control.inc` — radio button draw, activate, dispatch
- `TUI/tui_mouse.inc` — radio button mouse click handler

## v0.70.0 — Swap Directory (2026-04-13)

### New Feature: Hidden `_TEDIT` Swap Directory
- **All swap files now stored in `_TEDIT` subdirectory** under the startup
  working directory, instead of cluttering the working directory itself.
  Applies to both per-document swap files (`TEDT00NN.SWP`) and shell swap
  (`TEDTSHEL.SWP`).
- **Directory created on demand** via INT 21h AH=39h (MKDIR) when any swap
  operation occurs. Hidden attribute (02h) applied via INT 21h AH=43h
  (read-modify-write to preserve existing bits). Hidden attribute works on
  real DOS/FAT; no-op on DOSBox-X with host-mounted directories.
- **Clean exit removes directory**: quit handler, close-all handler, and the
  exit safety net all delete swap files from `_TEDIT`, then RMDIR the
  directory. On clean exit, no trace is left behind.
- **Crash recovery searches `_TEDIT`**: `doc_cleanup_orphans` and
  `shell_check_orphan` now look inside `_TEDIT` for orphaned swap files on
  startup, matching the new location.
- **CWD correctly restored** after all swap I/O operations. Every call to
  `swap_ensure_dir` (which CHDIRs into `_TEDIT`) is paired with a CHDIR
  back to `doc_cwd` or `startup_cwd`. Safety-net CHDIR to `startup_cwd`
  added before EXEC in Shell to DOS handler.

### New Procedures (`ed_multidoc.inc`)
- `swap_ensure_dir` — CHDIRs to `startup_cwd`, MKDIRs `_TEDIT` (ignore
  exists), sets hidden attribute (read-modify-write), CHDIRs into `_TEDIT`.
  Returns CF=1 if directory cannot be entered.
- `swap_remove_dir` — CHDIRs to `startup_cwd`, RMDIRs `_TEDIT` (ignore
  error if non-empty or non-existent).

### New String Constants (`TEDIT.ASM`)
- `s_swap_dirname` — `"_TEDIT"` directory name
- `s_err_path_long` — error message for path length exceeded

### Modified: Startup Ordering (`TEDIT.ASM`)
- `doc_cleanup_orphans` call moved after `startup_cwd` is built (was called
  before `startup_cwd` was initialised, which would pass uninitialised memory
  to `swap_ensure_dir`).

### Bug Fix: CWD Leaks
- `doc_swap_out` now CHDIRs back to `doc_cwd` after writing swap file.
- `shell_check_orphan` now CHDIRs back to `startup_cwd` on all exit paths.
- `menu_close_handler` now CHDIRs back to `doc_cwd` after deleting swap file.
- `menu_shell_handler` CHDIRs to `startup_cwd` before EXEC (safety net).

## v0.69.0 — Line Ending Support (2026-04-04)

### New Feature: CRLF / LF Line Ending Detection & Conversion
- **Auto-detection on load**: scans first piece for the first LF byte. If
  preceded by CR → CRLF mode, bare LF → LF mode, no LF found → defaults to
  CRLF. Works for all load paths (single file, multi-file, ed_load_file).
- **Status bar indicator**: shows "CRLF" or "LF" after the `[OVR]` indicator,
  before the document counter. Updates on file load, document switch, and mode
  toggle.
- **Mode-aware Enter key**: `insert_crlf` checks `[line_ending]` — inserts
  `0Dh+0Ah` (2 bytes) in CRLF mode, `0Ah` only (1 byte) in LF mode. Piece
  descriptor length computed dynamically.
- **Correct undo/redo for bare LF**: `delete_at` now sets `undo_del_crlf=1`
  for any LF deletion (not just CRLF pairs), ensuring undo records use
  `UR_DEL_CRLF` which calls `insert_crlf` (mode-aware) with proper line count
  update. Prevents line count drift on undo of bare LF deletion.
- **Save-time conversion**: `save_file` writes through a 512-byte staging
  buffer with line ending filter. LF mode strips all `0Dh` bytes. CRLF mode
  inserts `0Dh` before any `0Ah` not preceded by `0Dh`. Tracks state across
  piece boundaries via `save_prev_byte`.
- **User conversion toggle**: Options menu → "Convert to LF" / "Convert to
  CRLF" (dynamic label). Toggles mode, marks file dirty, clears undo history
  (existing records have wrong semantics after conversion).
- **Per-document persistence**: `line_ending` saved/restored in doc swap
  (`SWH_LINE_ENDING` at offset 104 in swap header) and shell swap.

### New Menu: Options
- Top-level "Options" menu added between View and Info (Alt+O).
- Currently contains one entry: "Convert to LF" / "Convert to CRLF" (hotkey C).
- Menu bar now has 5 items: File, Edit, View, Options, Info.

### New Constants (`ed_const.inc`)
- `LE_CRLF EQU 0`, `LE_LF EQU 1` — line ending mode values
- `SWH_LINE_ENDING EQU 104` — swap header field (1 byte, was first byte of
  reserved block; reserved shrunk from 24 to 23 bytes)

### New Procedure: `detect_line_ending` (`ed_file.inc`)
- Scans first piece (up to 4 KB) for first `0Ah`. Checks preceding byte for
  `0Dh`. Handles edge case of LF at file position 0.
- Called from `ed_load_file`, main startup single-file path, and multi-file
  load path. `ed_new_doc` sets `LE_CRLF` default.

### New Procedure: `update_le_label` (`TEDIT.ASM`)
- Sets the Options menu label pointer (`m_le_label`) based on `[line_ending]`.
- Called from all paths that change `line_ending`: detection, toggle, doc swap
  restore, shell swap restore.

### Modified: `insert_crlf` (`ed_edit.inc`)
- Branches on `[line_ending]`: CRLF path writes 2 bytes, LF path writes 1
  byte. `.icr_write_piece` uses CX for dynamic byte count.

### Modified: `delete_at` (`ed_edit.inc`)
- `.da_deleted_lf` path now sets `undo_del_crlf=1` immediately after
  `meta_dec_line`, before companion CR check. Ensures bare LF deletion uses
  `UR_DEL_CRLF` record type.

### Modified: `save_file` (`ed_edit.inc`)
- Replaced coalesced bulk-write loop with filtered byte-by-byte write through
  `save_conv_buf` (512 bytes). Removed dead `.sf_break_err` label.

### BSS Additions (`TEDIT.ASM`)
- `line_ending` (RESB 1) — current line ending mode
- `save_conv_buf` (RESB 512) — save conversion staging buffer
- `save_conv_pos` (RESW 1) — staging buffer write position
- `save_prev_byte` (RESB 1) — previous byte for CRLF insertion tracking

### Fix: Document Panel Garbage After Shell Swap Restore
- **`shell_swap_in`** (`ed_shell.inc`): freshly allocated `doc_table_seg` was
  not zeroed before reading back saved slots. Uninitialized memory with
  `DOCF_OCCUPIED` bit set caused `ed_draw_panel` to render garbage filenames.
  Fixed by zeroing entire 3 KB segment after allocation.

## v0.68.0 — Shell Swap & Menu Hotkey Fix (2026-04-03)

### New Feature: Shell Memory Swap (`/d` flag)
- **`/d` command-line flag** enables memory dump mode for Shell to DOS. When
  active, all dynamically allocated segments (~200 KB) are serialized to a
  single swap file (`TEDTSHEL.SWP`) before EXEC, then restored after the user
  types EXIT. This frees memory for compilers, linkers, and other tools.
- **Swap file format**: 128-byte global header (magic `SH`, doc_count,
  active_doc, clipboard state) + doc table + clipboard data + 128-byte per-doc
  header (reuses existing `SWH_*` format) + piece table + add buffer + undo
  records + rd_save_buf. Original file content is NOT saved — it is reloaded
  from disk on restore.
- **Segments freed**: cache_seg (2 × 32 KB), pt_seg (40 KB), add_seg (64 KB),
  undo_seg (32 KB), clip_seg (16 KB), shadow_seg (4 KB), doc_table_seg (3 KB).
- **Error handling**: write failure aborts the shell and leaves editor state
  intact. On restore, if the swap file was deleted during the shell session,
  TEDIT falls back to an empty untitled document and shows a warning dialog
  ("Swap file missing. Unsaved changes lost.") instead of fatally exiting.
  Only a genuine out-of-memory condition is fatal.

### New Feature: Orphan Swap Recovery
- **Crash recovery**: on startup, TEDIT checks for an orphan `TEDTSHEL.SWP` in
  the startup directory. If found, prompts: `Swap file detected. Restore[R] or
  Delete[D]?` Recovery reallocates all segments and restores full editor state.
  Delete also cleans up any orphan per-doc swap files (`TEDT????.SWP`).

### New File: `ed_shell.inc`
- **`shell_swap_out`**: serializes global header, doc table, clipboard, and
  active document state to `TEDTSHEL.SWP`, then frees all 8 segment types.
  Returns CF=0 on success, CF=1 on failure (memory intact).
- **`shell_swap_in`**: reallocates all segments, reads swap file, reloads
  original file from disk, restores piece table, add buffer, undo history,
  cursor/scroll position, dirty flag, tab settings, and find flags. Deletes
  swap file after successful restore. If the swap file is missing (deleted
  during shell), falls back to an empty untitled document with all segments
  allocated, sets `shell_recovering=1` for deferred warning, returns CF=0.
- **`shell_check_orphan`**: startup orphan detection with R/D prompt via DOS
  console I/O (runs before TUI init).
- **`write_seg_data` / `read_seg_data`**: helper procs for cross-segment file
  I/O (DS swap with CS: prefix for handle access).

### New Constants (`ed_const.inc`)
- `SHELL_SWAP_MAGIC EQU 4853h`, `SHELL_SWAP_VERSION EQU 1`,
  `SHELL_HDR_SIZE EQU 128`
- `SHL_MAGIC`, `SHL_VERSION`, `SHL_DOC_COUNT`, `SHL_ACTIVE_DOC`,
  `SHL_CLIP_USED`, `SHL_CLIP_LINES`, `SHL_CLIP_LAST_COL`, `SHL_RESERVED`

### Modified: `parse_args` (`ed_core.inc`)
- Added `/d` and `-d` flag recognition. Sets `shell_dump_mode` BSS byte to 1.

### Modified: `menu_shell_handler` (`TEDIT.ASM`)
- Calls `shell_swap_out` before EXEC and `shell_swap_in` after return when
  `shell_dump_mode=1`. Swap-out failure shows error dialog and aborts shell.
  After TUI restore, checks `shell_recovering` flag — if set, shows
  "Swap file missing. Unsaved changes lost." warning dialog. Only
  out-of-memory failure is fatal.

### Modified: `main:` entry point (`TEDIT.ASM`)
- Orphan swap check inserted after `startup_cwd` setup, before `cache_init`.
  Recovery path calls `shell_swap_in` and jumps to `.init_tui` (skipping
  normal file loading and editor state reset). Deferred warning check added
  before `tui_run` — shows error dialog if `shell_recovering` is set.

### Fix: File Menu Hotkey Duplicates
- **Close All**: changed hotkey from `A` to `E` (hotidx 6→4, highlights 'e'
  in "Clos**e** All"). Eliminates duplicate with Save As.
- **Save All**: fixed hotidx from 5→6 (now highlights 'l' in "Save A**l**l"
  to match hotkey `L`; previously highlighted 'A' misleadingly).
- **Shell to DOS**: fixed hotidx from 0→1 (now highlights 'h' in "S**h**ell
  to DOS" to match hotkey `H`; previously highlighted 'S' misleadingly).
- All 11 File menu items now have unique hotkeys:
  **N**ew, **O**pen, **C**lose, Clos**e** All, **S**ave, Save **A**s,
  Save A**l**l, Loa**d** Project, Save **P**roject, S**h**ell to DOS, E**x**it

### Fix: Document Panel Garbage After Shell Swap Restore
- **`shell_swap_in`** (`ed_shell.inc`): freshly allocated `doc_table_seg` was not
  zeroed before reading back saved slots. Only `doc_count` slots (e.g. 152 bytes
  for 1 document) were restored from the swap file; the remaining ~2900 bytes
  contained uninitialized memory from DOS. If any garbage byte had `DOCF_OCCUPIED`
  (bit 0) set, `ed_draw_panel` rendered it as an occupied slot with a junk
  filename. Fixed by zeroing the entire 3 KB segment (`REP STOSW`,
  `DOC_TBL_SEG_PARA * 8` words) immediately after allocation, matching the
  `doc_table_init` pattern used at normal startup.

### BSS Additions (`TEDIT.ASM`)
- `shell_dump_mode` (RESB 1) — `/d` flag state
- `shell_recovering` (RESB 1) — orphan recovery in progress

### New Feature: Instance Detection (INT 2Fh Multiplex)
- **Prevents running a second TEDIT from a shelled DOS session.** On startup,
  TEDIT calls `INT 2Fh` with AH=C0h (application-range multiplex ID), AL=00h.
  If another TEDIT instance has installed its handler, AL returns FFh and ES:BX
  points to a `"TEDIT"` signature string. The second instance verifies the
  5-byte signature (guards against multiplex ID collisions), prints
  `"TEDIT is already running."`, and exits immediately.
- **Handler installation**: after the instance check passes, TEDIT saves the
  previous INT 2Fh vector via `INT 21h AH=35h` and installs `_int2f_handler`
  via `INT 21h AH=25h`. The handler chains to the previous handler for
  non-matching multiplex IDs via a manually-encoded `JMP FAR CS:[_int2f_prev]`
  (agent86 does not support the `JMP FAR CS:[mem]` syntax natively).
- **Clean exit**: the original INT 2Fh vector is restored at program exit,
  removing the handler from the interrupt chain.
- **New constant**: `TEDIT_MUX_ID EQU 0C0h` in ed_const.inc.
- **Note**: INT 2Fh is not emulated by agent86 (vector get/set are stubs), so
  this feature only works in DOSBox-X and real DOS. It compiles and is
  harmless under agent86.

### Fix: Side Panel DS Clobber (`ed_draw.inc`)
- **`ed_draw_panel`** called `tui_darken_cell` for the right shadow column
  while DS still pointed to `doc_table_seg` (set earlier for slot access).
  `tui_darken_cell` → `calc_vram_offset` reads `row_offsets` and `shadow_seg`
  via DS, so it read garbage from the wrong segment. This corrupted memory and
  caused a hang in DOSBox-X when pressing F1 to open the document panel.
  Fixed by moving `POP DS` (restoring original DS) from `.ep_done` to the
  start of `.ep_shadow`, before the `tui_darken_cell` loop.

### Fix: INT 2Fh Handler Left Resident on Error Exit (`TEDIT.ASM`)
- The early error paths (`.err_nofile`, `.err_nomem`, `.err_mixed`) exited via
  `INT 20h` without restoring the INT 2Fh vector installed at startup. If TEDIT
  failed to open a file (e.g. `TEDIT noexists.txt`), the multiplex handler
  remained resident, causing subsequent runs to print "TEDIT is already running."
  Fixed by routing all three error exits through a new `.err_exit` label that
  restores INT 2Fh before terminating.

### Changed: Untitled Document Naming (`ed_multidoc.inc`)
- Renamed default filename from `unsavedNN.txt` to `nonameNN.txt`. The old name
  had a 9-character basename ("unsaved01") which exceeds the DOS 8.3 limit.
  "noname" (6 chars) + 2-digit number = 8 characters, fully 8.3 compliant.

### About Dialog
- Version bumped from v0.65 to v0.68.

### Test Harness
- **`test_shell_swap.asm`**: standalone round-trip test (5 tests):
  1. `shell_swap_out` — verifies swap file created, all segments freed
  2. `shell_swap_in` — verifies state restored (pt_count, add_used, cur_col,
     undo_count, total_lines, dirty flag)
  3. Swap file deleted after restore
  4. Add buffer content ('X','Y','Z') survives round-trip
  5. Missing swap file fallback — swap out, delete file, swap in: verifies
     graceful recovery (CF=0, `shell_recovering=1`, all segments allocated,
     empty untitled doc with `doc_count=1`, `dirty=0`)

## v0.67.0 — Incremental Metadata Optimization (2026-04-02)

### Architecture: Cache-Based File Access
- **Replaced in-memory `orig_seg[]` chunks with a 2-slot LRU disk cache**. Original
  file data is no longer loaded into memory — it stays on disk and is read on demand
  through two 32 KB cache buffers. Memory usage for original data drops from N × 32 KB
  to a fixed 64 KB regardless of file size.
- **Maximum file size now limited by piece table capacity, not memory**. Previously
  ~416 KB (limited by agent86's ~576 KB allocation budget). Now supports files up to
  32 MB (piece table limit of 4096 pieces × 32 KB chunks).
- **File loads instantly** regardless of size — `load_file` queries file size via
  seek and keeps the handle open. No data is read until rendering or editing needs it.

### New Subsystems (ed_core.inc)
- **`cache_init`**: allocates 2 × 32 KB cache segments at startup (persistent, shared
  across documents). Called once from `main:` before any file loading.
- **`cache_invalidate`**: marks all cache slots empty. Called after save, document
  switch, and compaction.
- **`cache_lookup`**: resolves a 32-bit file offset to a cached segment:offset. Checks
  both slots for a hit (O(1)), falls back to `cache_load_chunk` on miss. Preserves
  BX, DI, BP, DX.
- **`cache_load_chunk`**: evicts the LRU cache slot, seeks to the requested chunk in
  `orig_file_hnd`, reads up to 32 KB. Updates cache metadata.

### Piece Table Changes
- **Start field simplified**: piece descriptors for original-buffer pieces now store a
  plain 32-bit byte offset into the file, replacing the packed chunk_index/chunk_offset
  bit encoding. Cache lookup does `offset >> 15` for chunk index, `offset & 7FFFh` for
  chunk offset.
- **`pt_init` rewritten**: creates chunk-aligned pieces (one per 32 KB of file) with
  simple 32-bit byte offsets. No more multi-chunk loop with packed encoding.
- **`resolve_piece` simplified**: source=0 path replaced 12-instruction chunk decode +
  `orig_seg[]` lookup with a single `CALL cache_lookup`.
- **Inline `resolve_piece` in ed_draw.inc updated**: same cache_lookup call. Fixed CX
  clobber bug where `cache_lookup` destroyed the screen column counter at chunk
  boundaries, causing garbled rendering on files >32 KB. Fix: save/restore CX around
  the call using BX as temp for start_hi.

### Safe Save (Temp-File-Rename)
- **`save_file` rewritten** with 13-step process: write all pieces to `TEDIT.$$$`,
  close old `orig_file_hnd`, delete original, rename temp → original, reopen as new
  backing file, query file size, invalidate cache, reset piece table (chunk-aligned
  pieces over new file), reset add buffer, rebuild metadata, clear undo, mark clean.
- **Crash-safe**: original file is untouched until rename succeeds. If power is lost
  during save, the original is intact and `TEDIT.$$$` is incomplete but harmless.
- **Undo history cleared on save**: necessary because the backing file changes and
  piece table references to old file offsets become invalid.
- **Error recovery**: on write failure, temp file is deleted; on rename failure, editor
  state is cleared gracefully with error dialog.

### File Loading Changes
- **`load_file` rewritten**: no longer allocates memory segments. Queries file size via
  `INT 21h AH=42h` (seek to end), stores in `orig_file_size_lo/hi`, transfers
  `file_hnd` → `orig_file_hnd`, calls `cache_invalidate`.
- **`ed_load_file`**: removed file handle close after `load_file` — handle is now owned
  by `orig_file_hnd` for the document's lifetime.
- **`ed_new_doc`**: zeroes `orig_file_hnd`/`orig_file_size_lo/hi` instead of `orig_count`.
- **All startup load paths fixed**: main single-file path, multi-file `.lsf_open` path,
  and `doc_swap_in` path all updated to not close the file handle after `load_file`.
- **`cache_init` moved before file loading** in `main:` — critical fix. Previously
  called in `.start_tui` (after loading), causing `cache_load_chunk` to read file data
  into uninitialized segment 0 (the COM code segment), corrupting the program.

### Resource Management
- **`free_all` rewritten**: closes `orig_file_hnd` + `cache_invalidate` instead of
  freeing `orig_seg[]` chunks. Zeroes `pt_seg`/`add_seg` after freeing.
- **`ed_compact` simplified**: reduced from 120+ lines to 5. `save_file` now handles
  the full reset cycle; `ed_compact` just clamps cursor and refreshes display.
- **Program exit**: added `.free_cache` loop to free both cache segments alongside
  undo/clip/shadow/doc_table cleanup.

### Multi-Document Support
- **`doc_swap_out` `.free_doc_segs`**: closes `orig_file_hnd` + `cache_invalidate`
  instead of freeing `orig_seg[]` chunks. `SWH_ORIG_COUNT` header field now writes 0
  (no longer used).
- **`doc_swap_in`**: updated error path to close `orig_file_hnd` instead of `file_hnd`
  on `load_file` failure. Main flow unchanged — `pt_init` creates throwaway pieces,
  then swap restore overwrites descriptors from swap file.

### BSS Cleanup
- **Removed `orig_count`, `orig_seg`, `orig_len`** from DOC_CTX (4098 bytes freed).
- **Removed `MAX_ORIG` constant** from ed_const.inc.
- **Added to DOC_CTX**: `orig_file_hnd` (WORD), `orig_file_size_lo` (WORD),
  `orig_file_size_hi` (WORD) — 6 bytes total.
- **DOC_CTX_SIZE**: 12446 → 8348 (net savings: 4098 bytes).
- **Added to transient section**: `cache_seg` (2 WORDs), `cache_chunk_id` (2 WORDs),
  `cache_lru` (2 WORDs), `cache_lru_clock` (WORD) — 14 bytes.

### New Constants (ed_const.inc)
- `CACHE_SLOTS EQU 2`, `CACHE_CHUNK_PARA EQU CHUNK_PARA`,
  `CACHE_CHUNK_BYTES EQU CHUNK_BYTES`, `CACHE_EMPTY EQU 0FFFFh`

### Test Harness
- **`test_cache.asm`**: 9-test standalone harness — creates a 3-chunk (96 KB) test file,
  exercises cache hits, misses, LRU eviction, invalidation, and post-invalidation reload.

## v0.66.0 — File-Backed Original Buffer (2026-04-02)

### Performance: Lazy Checkpoint Invalidation
- **Eliminated `rebuild_meta` from character typing**: removed `meta_dirty = 1` from
  7 call sites in `insert_char` (split/before paths), `delete_byte` (remove/split
  paths), and `delete_at` (bare CR, companion CR paths). Piece index adjustments
  via `chkpt_adjust_insert/remove` are sufficient — no full rebuild needed.
- **Eliminated `rebuild_meta` from Enter/line-delete**: new `meta_inc_line` and
  `meta_dec_line` procs incrementally update `total_lines` and truncate `chkpt_num`
  to discard stale checkpoints past the edit point. Replaces the blanket
  `meta_dirty = 1` that previously forced an O(file_size) rebuild.
- **Eliminated `rebuild_meta` from undo/redo of typed characters**: removed
  unconditional `meta_dirty = 1` from `undo_execute` `.u_finish` and
  `redo_execute` `.r_finish`. Individual handlers now manage metadata:
  char insert/delete needs no rebuild; CRLF undo/redo calls `meta_inc/dec_line`;
  range operations still set `meta_dirty = 1`.
- **Eliminated `rebuild_meta` from paste**: new `meta_inc_n_lines` proc handles
  multi-line paste. Single-line paste needs no metadata update at all.

### Lazy Rebuild in `seek_to_line`
- When `seek_to_line` needs a checkpoint that was truncated away (e.g., user edits
  near the top then scrolls far down), it calls `rebuild_meta` on the spot before
  proceeding. This ensures: local edits are O(1), first distant seek pays one
  rebuild, all subsequent seeks are fast.

### Checkpoint 0 Invariant
- `seek_to_line` now always resets `chkpt_pi[0]` and `chkpt_po[0]` to (0, 0).
  Line 0 always starts at the document beginning; `chkpt_adjust_insert/remove`
  can corrupt this during incremental updates. The 2-instruction reset enforces
  correctness without a full rebuild.

### What Still Triggers Full Rebuild
- `range_delete_pieces` (bulk selection delete)
- Undo/redo of range operations (`UR_DEL_RANGE`, `UR_INS_RANGE`)
- `save_file` (reloads from disk)
- Document swap (`ed_multidoc` restore)

## v0.65.0 — Menu Shortcuts & Quit Dialog Fix (2026-04-02)

### Menu Shortcuts
- **Shell to DOS**: assigned F8 shortcut (scan code 42h via TUI accelerator dispatch)
- **Exit (renamed from Quit)**: changed shortcut from Alt+Q to Alt+X (scan code 2Dh)
- Verified all 21 menu shortcuts work correctly via agent86 emulation

### DOSBox-X Ctrl+Alt Key Fix
- **Ctrl+Alt+W (Close All)** and **Ctrl+Alt+S (Save All)**: added extended key
  fallback handlers in ed_keys.inc. DOSBox-X sends Ctrl+Alt+letter as an extended
  key (ASCII=0, scancode=letter) rather than as a control code with Alt in the
  modifier flags. New handlers check for W (scan 11h) and S (scan 1Fh) with
  Ctrl modifier bit, jumping to existing `.do_closeall` / `.do_saveall`.

### Quit Dialog Fix
- **"Other docs have unsaved changes."** replaced with **"Discard unsaved changes?"**
  — the old message wasn't a question, making the Yes/No buttons meaningless
- **No button** now aborts quit (previously both Yes and No would quit, making
  the dialog pointless). Only Yes discards and exits; No/Cancel return to the editor.

### Open File OOM Crash Fix
- **Memory leak in ed_load_file**: when `load_file` failed partway (out of memory),
  partially allocated orig_seg chunks were not freed before `ed_new_doc` zeroed
  `orig_count`. This leaked segments, preventing `ed_new_doc` from allocating
  `pt_seg`/`add_seg`, leaving them at segment 0 (interrupt vector table). Typing
  then corrupted the IVT → smiley faces on screen → DOSBox-X crash.
  Fixed by adding `CALL free_all` before `ed_new_doc` in the `.init_empty` path.
- **Ghost slot in menu_open_handler**: `ed_load_file` return code (CF) was not
  checked after loading into a new slot. On failure, the slot remained occupied
  with an empty filename, appearing as a blank entry in the side panel. Closing
  it left the editor in a broken state with no valid document.
  Fixed by adding `JC .open_load_failed` — on failure, the handler frees segments,
  clears the slot (`doc_slot_clear`), finds the previous document
  (`doc_next_occupied`), and swaps it back in. User is returned to their
  original file with no ghost entries.

## v0.64.0 — BSS/Stack Overflow Fix, Segment Migration (2026-04-01)

### Shell-to-DOS JemmEx Crash Fix
- **Root cause**: BSS grew to 65,530 bytes (0xFFFA), leaving only 4 bytes
  before the stack (SP=0xFFFE). The EXEC parameter block at 0xFFE8 was
  overwritten by stack frames from `INT 10h`/`INT 21h` calls, causing JemmEx
  to GPF (exception 0Dh) on corrupted pointers during `REP MOVSW`
- Reported on VOGONS forum: v0.63 shells to DOS caused JemmEx exception, v0.60
  did not

### BSS-to-Segment Migration
- **`shadow_buf` (4,000 bytes)** moved to a separately-allocated DOS segment
  (`shadow_seg`, 256 paragraphs / 4 KB via INT 21h/48h), matching the existing
  pattern for `undo_seg` and `clip_seg`
  - `tui_clear_shadow`: ES = `[shadow_seg]`, DI = 0
  - `tui_blit`: DS temporarily set to `[shadow_seg]` for REP MOVSW source;
    CGA snow path uses `CS:` prefix for `tui_cga_snow` BSS access
  - `calc_vram_offset`: now sets ES = `[shadow_seg]` and returns offset from 0
    (no `shadow_buf` base); clobbers ES
  - All TUI drawing primitives (`tui_putchar`, `tui_putstr`, `tui_hline`,
    `tui_fill_rect`, `tui_darken_cell`, `tui_darken_hline`): save/restore ES,
    write via `ES:[DI]` instead of `[DI]`
  - `tui_ctrl_draw_editor`: render setup sets `ES = [shadow_seg]` instead of
    `ES = CS`; five `ADD DI, shadow_buf` removed; `ed_draw_cursor` uses
    `ES:[DI+1]` for attribute write
  - `ed_draw_panel`: sets `DS = CS:[doc_table_seg]` for slot iteration
    (doc_table also migrated); all BSS access via `CS:` prefix
- **`doc_table` (3,040 bytes)** moved to a separately-allocated DOS segment
  (`doc_table_seg`, 192 paragraphs / 3 KB)
  - `doc_get_slot`: now returns ES:DI (ES = `[doc_table_seg]`, DI = offset);
    clobbers ES
  - All doc_table field accesses (`[DI + DOC_FLAGS]`, `[DI + DOC_FILENAME]`,
    etc.) converted to `ES:` prefix across ed_multidoc.inc, TEDIT.ASM,
    ed_keys.inc, ed_mouse.inc, ed_draw.inc
  - File I/O paths (swap file create/open/delete) temporarily set
    `DS = doc_table_seg` via `PUSH DS / PUSH ES / POP DS` for INT 21h calls
  - Filename copy loops between doc_table and BSS use DS/ES swaps with `CS:`
    prefix for BSS writes
  - `doc_table_init`, `doc_find_free_slot`, `doc_find_by_name`: iterate via
    ES:DI with offset 0
- **Early allocation**: `doc_table_seg` and `shadow_seg` allocated immediately
  after `shrink_mem`, before `doc_table_init` — prevents writing to segment 0
  (IVT) when BSS is uninitialized
- **Allocation guards**: `.alloc_early` now skips `shadow_seg` and
  `doc_table_seg` if already allocated (prevents leaking the early allocation
  when the multi-file or PRJ startup paths call `.alloc_early`)
- **ES restore after `doc_table_init`**: `doc_table_init` sets ES to
  `doc_table_seg`; startup code now restores ES=DS immediately after, preventing
  the subsequent `REP MOVSB` (CWD copy) from writing into the doc_table segment
  instead of BSS
- **BSS reclaimed**: 7,040 bytes (4,000 + 3,040) moved out of the 64 KB COM
  segment
- **Stack space**: 4 bytes → 6,668 bytes (6.5 KB); growth margin ~6.1 KB

### Changes
- **ed_const.inc** — `SHADOW_SEG_PARA` (0x100), `DOC_TBL_SEG_PARA` (0xC0)
- **TEDIT.ASM** — `shadow_buf` RESB 4000 → `shadow_seg` RESW 1; `doc_table`
  RESB 3040 → `doc_table_seg` RESW 1; early segment allocation after
  `shrink_mem` with ES restore; allocation guards in `.alloc_early` for
  shadow_seg and doc_table_seg; deallocation at exit (`.free_shadow`,
  `.free_doctbl`); all doc_table field accesses converted to `ES:` prefix;
  version bump to v0.64
- **TUI/tui_video.inc** — all 8 drawing functions rewritten for segment-based
  shadow buffer (`calc_vram_offset` sets ES, primitives use `ES:[DI]`)
- **ed_draw.inc** — render setup, cursor draw, status bar, and panel overlay
  updated for `shadow_seg` and `doc_table_seg`
- **ed_multidoc.inc** — all 10 doc_table functions converted to ES:DI access
  pattern; file I/O paths use DS swaps for INT 21h
- **ed_keys.inc** — panel up/down slot checks use `ES:[DI + DOC_FLAGS]`
- **ed_mouse.inc** — panel click iteration uses `ES:[SI + DOC_FLAGS]`

### 4DOS / Third-Party Shell Compatibility
- **BSS zeroing at startup**: added `CLD / MOV DI, DOC_CTX / MOV CX,
  BSS_END - DOC_CTX / XOR AL, AL / REP STOSB` as the first code in `main:`.
  agent86 zeroes BSS automatically, but real DOS does not — under 4DOS the
  BSS region overlaps 4DOS's freed transient code, filling it with non-zero
  garbage. This broke conditional allocation guards (`CMP WORD [undo_seg], 0 /
  JNE .skip`) causing `undo_seg` and `clip_seg` allocation to be skipped, and
  left flag variables like `panel_visible` non-zero (intercepting all keyboard
  input for panel navigation). `BSS_END` label added after last RESB.
- **INT 23h handler installed** (`_int23_handler: IRET`): prevents 4DOS (or any
  parent shell) from terminating TEDIT on Ctrl+C. TEDIT uses Ctrl+C for Copy,
  not break. DOS restores the original vector automatically on INT 20h exit.
- **INT 24h handler installed** (`_int24_handler: MOV AL, 3 / IRET`): returns
  FAIL to caller, preventing 4DOS's critical-error handler from displaying its
  own Abort/Retry/Fail dialog during TEDIT file I/O.
- Handler code placed before `SECTION .bss`; installation via `INT 21h AH=25h`
  after `shrink_mem`

Binary: 40,988 bytes code, BSS ends at 58,897.

## v0.63.0 — Multi-File Command Line, Shell Fix, Shift+Tab (2026-03-31)

### Shell-to-DOS Bug Fix
- **Mouse restored after shell**: replaced bare `INT 33h AX=0000h` with
  `CALL tui_mouse_init` — re-establishes horizontal/vertical ranges and shows
  the hardware cursor (previously mouse was dead after returning from shell)
- **Text cursor re-hidden**: added `INT 10h AH=01h, CH=20h` after the mode 3
  reset — video mode reset makes the BIOS cursor visible again

### Shift+Tab Focus Navigation
- **KEY_SHIFT_TAB (0Fh)**: new constant in `tui_const.inc`
- **Normal dispatch**: Shift+Tab cycles focus backward via `tui_ctrl_focus_prev`,
  falls back to reverse window cycling (mirrors Tab behavior)
- **Modal dispatch**: Shift+Tab routes to existing `.mdl_left` handler (focus prev)

### Side Panel Alt+N Shortcuts
- **Alt+1..0 / Alt+Shift+1..0 while panel open**: closes panel and switches to
  the selected document, matching mouse-click-on-panel behavior
- Previously these shortcuts were eaten by the panel's catch-all key intercept

### Multi-File Command Line
- **Multiple files**: `TEDIT a.txt b.txt c.asm` loads all files into separate
  document slots — last file is active, earlier files are swapped to disk
- **Project files**: `TEDIT myproject.prj` loads the project at startup, same
  as File > Load Project but without the dialog
- **Mixed detection**: specifying both .PRJ and regular files prints
  "Cannot mix .PRJ and documents" and exits
- **parse_args rewrite**: counts all filename tokens, detects .PRJ extension
  (case-insensitive), saves PSP offsets in `rd_save_buf` for later loading
- **Early undo/clip allocation**: `doc_swap_out` reads undo state from BSS,
  so undo_seg must be allocated and `undo_init` called before the loading loop
- **doc_slot_init AL clobber fix**: PUSH/POP AX around `doc_slot_init` call —
  it uses DIV internally for swap path digits, destroying the slot index in AL

### Changes
- **TEDIT.ASM** — shell handler: `tui_mouse_init` + cursor hide; new `.alloc_early`,
  `.load_startup_file`, `.load_multi_cmdline`, `.load_prj_cmdline`, `.err_mixed`
  startup paths; `s_err_mixed` string; `arg_file_count`/`arg_has_prj` BSS vars;
  removed unused `s_usage` string to reclaim code space
- **ed_core.inc** — `parse_args` rewritten for multi-file + .PRJ detection
- **ed_keys.inc** — side panel intercept: Alt+1..0 / Alt+Shift+1..0 close panel
  and switch document
- **TUI/tui_const.inc** — `KEY_SHIFT_TAB EQU 0Fh`
- **TUI/tui_event.inc** — `.shift_tab` handler in normal dispatch;
  `KEY_SHIFT_TAB` check in modal dispatch

Binary: 40,530 bytes code, BSS ends at 65,529.

## v0.62.0 — COM Binary Size Optimization (2026-03-31)

### BSS Section Support
- **SECTION .bss**: all zero-initialized buffers (DOC_CTX, shadow_buf, doc_table,
  dialog buffers, find/replace state, etc.) moved to a true BSS section using
  agent86's new `SECTION .bss` directive — these 24,687 bytes of zeros are no
  longer emitted to the .COM file

### Subroutine Factoring
- **mul_bx_10**: extracted the 6-instruction `DI = BX * PT_DESC_SIZE` pattern
  (12 bytes inline) into a shared subroutine, replaced 29 call sites across
  ed_core.inc, ed_edit.inc, ed_clip.inc, ed_find.inc, ed_undo.inc, ed_draw.inc
- **show_error_msg**: extracted the `MOV DI, m_str_error / CALL tui_dlg_msgbox`
  pair (6 bytes inline) into a tail-call helper, replaced 30 call sites across
  10 source files

### Tail-Call Elimination
- Converted 10 `CALL X / RET` sequences to `JMP X` in menu handler wrappers
  (ed_clip.inc: copy/cut/paste, TEDIT.ASM: undo/redo, TUI/tui_event.inc:
  window move/cycle)

### String Deduplication & Shortening
- `m_str_save_warn` and `m_str_quit_ttl` changed to EQU aliases of existing
  identical strings (`m_str_save_ttl`, `m_str_quit`)
- Removed redundant "Error: " and "Warning: " prefixes from 6 error messages
  (dialog title already says "Error")
- Shortened 7 verbose messages (e.g., "Piece table full" → "Table full",
  "Selection too large to copy" → "Selection too large.")
- Trimmed MDA error from 51 to 29 characters

### Dead Code Removal
- Removed 13 redundant `PUSH SI / PUSH DI` + `POP DI / POP SI` pairs around
  error dialog calls where the function epilogue already restores registers
  (ed_edit.inc: 9 sites, ed_clip.inc: 2 sites, ed_find.inc: 1 site)

### Changes
- **ed_core.inc** — new `mul_bx_10` and `show_error_msg` helper procs
- **ed_edit.inc** — 14× mul_bx_10 refactor, 9× show_error_msg, 9× PUSH/POP removal
- **ed_clip.inc** — 6× mul_bx_10, 3× show_error_msg, 2× PUSH/POP removal,
  3× tail-call elimination
- **ed_find.inc** — 2× mul_bx_10, 1× show_error_msg, 1× PUSH/POP removal
- **ed_undo.inc** — 1× mul_bx_10, 3× show_error_msg
- **ed_draw.inc** — 1× mul_bx_10 (SI variant with DI save/restore)
- **ed_file.inc** — 3× show_error_msg
- **ed_sel.inc** — 1× show_error_msg
- **ed_multidoc.inc** — 2× show_error_msg
- **TEDIT.ASM** — 7× show_error_msg, 2× tail-call, string dedup/shortening,
  `SECTION .bss` before ed_state
- **TUI/tui_event.inc** — 5× tail-call elimination

Binary size: 39,789 bytes (down from 64,986, -25,197 bytes / -38.8%).
Headroom to 65,280-byte COM limit: 25,491 bytes (was 294).

## v0.61.0 — 20-Document Multi-Doc Expansion (2026-03-30)

### Multi-Document Scaling (8 → 20)
- **MAX_DOCS raised to 20**: two-tier keyboard switching scheme
  - **Alt+1..0**: switch to documents 1–10 (slots 0–9)
  - **Alt+Shift+1..0**: switch to documents 11–20 (slots 10–19)
  - Detection uses BIOS scancode range 78h–81h with shift-flag check
- **Side panel two-tier layout**: "ALT+n:" header (always shown) with tier 1
  slots below; "ALT+SHFT+n:" header appears only when tier 2 has documents
  - Header rows use distinct PANEL_ATTR_HDR (30h, black on cyan)
  - Border bars (│) always use PANEL_ATTR_NORM (bright white) regardless of
    row type
  - Digit labels: 1–9, 0 per tier (matching the keyboard shortcut)
- **Swap filenames**: TEDT00NN.SWP (2-digit slot index, was single digit)
- **Untitled filenames**: unsavedNN.txt (2-digit, 01–20)
- **Slot allocation**: lower gaps filled first (doc_find_free_slot scans from 0)
- **Status bar [N/M]**: now renders 2-digit values correctly (was overflowing
  for counts ≥ 10)
- **Error message**: "Maximum 20 documents open."
- **Panel click handler**: accounts for header rows in click-to-slot mapping

### Changes
- **ed_const.inc** — MAX_DOCS=20, PANEL_ATTR_HDR constant, ASSERT updated
- **ed_multidoc.inc** — 2-digit swap/untitled filename generation via DIV;
  comment updates for expanded slot ranges
- **ed_keys.inc** — Alt+N dispatch extended to scancodes 78h–81h; shift-flag
  check adds +10 for tier 2
- **ed_draw.inc** — ed_draw_panel rewritten with .ep_draw_hdr/.ep_draw_slot
  helpers; .ed_write_num helper for 2-digit status bar counter
- **ed_mouse.inc** — panel click handler split into tier 1/tier 2 scan with
  header-row skip logic
- **TEDIT.ASM** — error message and BSS comments updated

Binary size: 64,986 bytes (up from 62,799, +2,187 bytes / +3.5%).

## v0.60.0 — File Menu Overhaul, Projects & Shell (2026-03-30)

### File Menu Enhancements
- **Ctrl+O**: new shortcut for File > Open
- **Ctrl+Shift+S**: new shortcut for File > Save As, with overwrite confirmation
  dialog ("File exists. Overwrite?") when target file already exists
- **Save All (Ctrl+Alt+S)**: saves all dirty, named documents by switching to each
  in turn; skips untitled documents (no filename to save to)
- **Close All (Ctrl+Alt+W)**: prompts to save each dirty document (Yes/No/Cancel);
  Cancel aborts entirely; resets to a single fresh untitled document after closing
- **Close behavior fix**: closing the last document no longer quits TEDIT; untitled
  + clean = no-op; dirty or named = prompt/reset to untitled

### Project Files
- **Save Project** (File menu, hotkey P): writes all open file paths to a `.PRJ`
  file (one absolute path per line, CRLF-terminated). Skips untitled documents.
  Shows overwrite confirmation if the target file exists
- **Load Project** (File menu, hotkey D): opens a `.PRJ` file, closes all current
  documents (with save prompts for dirty docs), then loads each listed file into
  its own slot. Warns if the project exceeds the document limit
- Both dialogs use `*.PRJ` wildcard filter, with save/restore of the normal
  `dlg_file_pattern` so regular Open dialogs remain `*.txt`
- PRJ format: plain text, one path per line — hand-editable, no metadata

### Shell to DOS
- **File > Shell to DOS** (hotkey H): spawns an interactive COMMAND.COM shell via
  INT 21h AH=4Bh (EXEC), using COMSPEC from the DOS environment
- Clears screen, prints "Type EXIT to return to TEDIT." before shelling
- On return: restores SS:SP, resets video mode 3, reinitializes mouse driver,
  triggers full TUI screen redraw
- Compatible with SHROOM utility for memory-efficient shelling (SHROOM intercepts
  EXEC and swaps TEDIT's allocated memory to disk transparently)
- Shows error if COMSPEC environment variable is not found

### Save-Before-Open Prompt Removed
- Opening a file no longer prompts to save the current document — the multi-doc
  system swaps documents to disk, so nothing is lost. Removed leftover
  single-document-era save prompt from `menu_open_handler`

### 40-Column Display Guard
- Startup now rejects text modes with fewer than 80 columns (e.g. CGA 40-column
  mode) in addition to the existing MDA check, with a clear error message

### Changes
- **ed_keys.inc** — Ctrl+O dispatch; Ctrl+Shift+S → Save As; Ctrl+Alt+S → Save
  All; Ctrl+Alt+W → Close All
- **TEDIT.ASM** — File menu expanded to 15 entries (11 items + 4 dividers), DDW
  widened to 28; `menu_saveall_handler`, `menu_closeall_handler`,
  `menu_saveprj_handler`, `menu_loadprj_handler`, `menu_shell_handler` added;
  close handler rewritten for single-doc reset; overwrite confirmation on Save As
  and Save Project; 40-column startup guard; version bump to v0.60

Binary size: 62,799 bytes (up from 60,651, +2,148 bytes / +3.5%).

## v0.59.0 — Side Panel & File Dialog Polish (2026-03-30)

### Side Panel Improvements
- **Arrow indicator**: `»` (CP437 0xAF) marks the highlighted entry; space for
  others. Layout: `│»N FILENAME.EXT*│`
- **Drop shadow**: right edge of panel darkened using `tui_darken_cell`, preserving
  underlying text — same effect as dialog box shadows
- **Panel highlight sync**: opening or creating a document now updates `panel_sel`
  to match `active_doc`, so the newly loaded file is always highlighted. Fixed in
  `doc_swap_in`, `menu_open_handler`, and `menu_new_handler`
- **View menu**: new "View" menu (Alt+V) between Edit and Info with "Document List"
  entry (hotkey D) that toggles the side panel. Displays F1 as the accelerator
- `PANEL_WIDTH` widened from 17 to 18 columns to accommodate the arrow indicator

### File Dialog Enhancements
- **Wildcard persistence**: default `*.txt` shown in textbox on first open;
  pattern persists between opens (e.g. change to `*.asm`, open a file, next open
  shows `*.asm`). Cancel restores the previous wildcard
- **Textbox gets initial focus**: cursor starts in the filename textbox — no
  tabbing required to start typing
- **Enter key dispatch**: Enter on textbox with wildcard re-filters the file list
  and moves focus to the listbox; Enter with a plain filename opens it directly
- **Listbox Enter opens file**: pressing Enter on a selected file in the listbox
  now closes the dialog and opens the file (previously only copied the filename)
- **Smart OK button priority**: (1) plain filename in textbox → open file,
  (2) wildcard from focused textbox → re-filter + focus listbox,
  (3) OK button with file selected → open from list, (4) fallback → re-filter

### Listbox Double-Click
- Single click selects item and focuses the control; double click selects and
  calls the handler (opens file / navigates directory)
- New `mouse_state` fields: `MS_LB_TICK` (WORD), `MS_LB_CTRL` (WORD),
  `MS_LB_SEL` (BYTE); `MS_SIZE` expanded from 16 to 21 bytes
- Double-click window: 9 BIOS ticks (~500ms), same control and same item

### Dead Code Removal
- Removed unused `doc_draw_tabs` proc (138 lines) from `ed_draw.inc`
- Removed stale PRINT debug line from `ed_multidoc.inc`

### Changes
- **ed_draw.inc** — removed `doc_draw_tabs`; added panel arrow indicator and
  drop shadow in `ed_draw_panel`
- **ed_multidoc.inc** — removed debug PRINT; `doc_swap_in` syncs `panel_sel`
- **ed_const.inc** — `PANEL_WIDTH` 17→18
- **ed_keys.inc** — F1 handler unchanged (toggle still works via panel intercept)
- **TUI/tui_dialog.inc** — wildcard persistence (`dlg_file_pat_save`), textbox
  initial focus, Enter dispatch, listbox-Enter-opens, smart OK priority
- **TUI/tui_const.inc** — `MS_LB_TICK`, `MS_LB_CTRL`, `MS_LB_SEL`, `MS_SIZE`
  16→21
- **TUI/tui_mouse.inc** — listbox double-click detection
- **TEDIT.ASM** — View menu (strings, entries, `menu_doclist_handler`); Info menu
  shifted to X=18; menubar count 3→4; `panel_sel` sync in `menu_open_handler` and
  `menu_new_handler`; `open_file_buf` clear; `dlg_file_pat_save` BSS;
  `mouse_state` 16→21; version bump to v0.59

Binary size: 60,651 bytes (up from 60,411, +240 bytes / +0.4%).

## v0.58.0 — Multi-Document Editing (2026-03-27)

7-phase project adding full multi-document support with up to 8 simultaneous
open files, disk-based document swapping, tab bar UI, and robustness features.

### Document Table and Swap Mechanism (Phases 1-2)
- 8-slot document table with 152-byte slot descriptors tracking flags, filename,
  swap path, cached display state, and slot ID
- Disk-based swap: documents not currently active are written to temporary
  `TEDTnnnn.SWP` files and their memory segments freed, then restored on switch
- Swap file format: 64-byte header (magic, version, segment sizes, piece table
  state, cursor position, dirty flag) followed by raw segment data (original
  buffer, add buffer, piece table)
- 12,440-byte contiguous document context block (`DOC_CTX`) swapped as a unit
  via `REP MOVSW` between BSS and save segments

### Document Switching (Phases 3-4)
- `doc_switch`: saves active document to disk, loads target from swap file,
  restores all editor state (cursor, scroll, dirty flag, undo buffer)
- Keyboard shortcuts: Alt+1 through Alt+8 (direct slot), F6 (next doc),
  Ctrl+PgDn/PgUp (next/prev occupied slot)
- `doc_next_occupied` / `doc_prev_occupied`: wrapping search through occupied
  slots for F6 and Ctrl+PgDn/PgUp navigation

### File Menu Operations (Phase 5)
- **File > New (Ctrl+N)**: allocates a free slot, swaps out active document,
  initializes empty untitled document in new slot
- **File > Open**: reuses current slot if untitled and clean; otherwise opens in
  new slot with automatic swap-out of active document. Shows error when all 8
  slots are occupied
- **File > Close (Ctrl+W)**: prompts to save if dirty; single-document close
  triggers quit; last-document close auto-creates empty untitled document
- **Quit (Alt+Q)**: prompts for active document if dirty, warns about unsaved
  swapped documents, deletes all swap files on exit
- **Save As**: updates slot filename after save so tab bar reflects new name

### Tab Bar UI (Phase 6)
- Menu bar displays document tabs after "Info" menu: `1:basename 2:basename ...`
- Active tab highlighted bright white on blue; inactive tabs use menu bar
  attribute; dirty documents marked with `*` suffix
- Basenames extracted from full path with extension stripped (8-char max display)
- Tab bar hidden when only one document is open (no visual clutter)
- `[N/M]` status bar indicator shows document position when multiple docs open

### Robustness (Phase 7)
- **Orphan swap file cleanup**: on startup, `doc_cleanup_orphans` scans for
  `TEDT????.SWP` via FindFirst/FindNext and deletes them (crash recovery)
- **Exit safety net**: walk all slots on program exit and delete any remaining
  swap files, independent of quit handler
- **Duplicate file prevention**: `doc_find_by_name` performs case-insensitive
  filename comparison across all occupied slots; opening an already-open file
  silently switches to the existing slot instead of creating a duplicate

### TUI Framework Trimming
- Removed ~3,800 bytes of dead TUI code (unused dialog types, unreachable
  control paths) to reclaim headroom for multi-document features

### Changes
- **ed_multidoc.inc** (new) — 1,080 lines: `doc_table_init`, `doc_get_slot`,
  `doc_find_free_slot`, `doc_slot_init`, `doc_slot_clear`, `doc_swap_out`,
  `doc_swap_in`, `doc_switch`, `doc_next_occupied`, `doc_prev_occupied`,
  `doc_cleanup_orphans`, `doc_find_by_name`
- **ed_const.inc** — Multi-doc constants: `MAX_DOCS`, `DOC_SLOT_SIZE`,
  `DOCF_OCCUPIED/SWAPPED/DIRTY/UNTITLED`, field offsets, swap header layout
- **ed_draw.inc** — `doc_draw_tabs` proc; `[N/M]` status bar indicator
- **ed_keys.inc** — Ctrl+N, Ctrl+W, Alt+1-8, F6, Ctrl+PgDn/PgUp dispatch
- **TEDIT.ASM** — File menu expanded (New, Open, Close, Save, Save As, Quit);
  `menu_new_handler`, `menu_close_handler` rewritten; `menu_open_handler` with
  reuse/new-slot logic and duplicate check; `menu_quit_handler` multi-doc aware;
  startup orphan cleanup; exit swap cleanup loop; version bump to v0.58
- **TUI/tui_window.inc** — Tab bar rendering call after menu bar
- **TUI/** — Dead code removal (~3,800 bytes reclaimed)

Binary size: 59,380 bytes (up from 58,965, +415 bytes / +0.7%).
Multi-doc internal growth was ~4,200 bytes offset by ~3,800 bytes of TUI trimming.

## v0.57.0 — Scrollbar Track Hold-to-Scroll + Display Compatibility (2026-03-26)

### Scrollbar Track Hold-to-Scroll
- Click and hold on the scrollbar track (above or below the thumb) to continuously
  scroll toward the mouse position, stopping when the thumb reaches the click point
- Scrolls by page (ED_TEXT_ROWS lines) per step for fast navigation; arrows remain
  line-by-line
- Reuses existing `MSF_SB_ARROW` repeat mechanism with new direction values
  (4 = track-up, 5 = track-down) to distinguish from arrow repeat (0/1)
- Repeat handler recomputes thumb position each frame using the same formula as the
  renderer, compares with current mouse row via `ed_abs_row`, and stops when the
  thumb reaches or passes the mouse

### CGA Snow Prevention
- Detects CGA adapter at startup via INT 10h AH=12h (EGA/VGA info query): if BL
  returns unchanged (10h), no EGA/VGA is present — CGA assumed
- `tui_blit` now has two paths: fast `REP MOVSW` for EGA/VGA, and a vertical
  retrace-synchronized path for CGA that waits for vretrace (port 3DAh bit 3)
  then blasts with `REP MOVSW` — CPU writes top-to-bottom faster than the raster
  beam scans (~285μs/row vs ~508μs/row), staying ahead and avoiding snow
- New BSS flag `tui_cga_snow` (0 = fast path, 1 = snow-free path)
- EGA/VGA systems are completely unaffected — zero overhead on the fast path

### MDA Display Guard
- TEDIT now detects MDA display (video mode 7) at startup and exits with a clear
  error message instead of writing to the wrong video segment
- Check runs before any memory allocation or initialization (INT 10h AH=0Fh)

### Changes
- **ed_mouse.inc** — `.sb_page_up` / `.sb_page_down` replaced: page-scroll with
  clamping + track-hold repeat setup
- **TUI/tui_mouse.inc** — `.sba_editor` extended: dir 0/1 unchanged (arrows);
  dir > 1 routes to new `.sba_ed_track` handler with page-scroll per frame and
  thumb vs mouse position check
- **TUI/tui_video.inc** — `tui_blit` gains CGA snow-free copy path (retrace sync)
- **TEDIT.ASM** — MDA check + CGA detection at top of `main:`; new `tui_cga_snow`
  BSS flag; new `s_err_mda` string; version bump to v0.57

Binary size: 58965 bytes (up from 58620, +345 bytes / +0.6%).

## v0.56.0 — Scrollbar Arrow Auto-Repeat (2026-03-25)

### Scrollbar Arrow Auto-Repeat
- Holding the mouse button on a scrollbar arrow now continuously scrolls
- Works for all control types: listbox, textview (vertical + horizontal), and editor
- New `MSF_SB_ARROW` (0x40) flag tracks arrow-held state across frames
- New `tui_mouse_on_sb_arrow` handler dispatches repeat scroll by control type
- Event loop now dispatches mouse handler when continuous-action flags are active,
  even with no position/button change (fixes held-button-at-same-position case)

### Changes
- **TUI/tui_const.inc** — New `MSF_SB_ARROW` flag
- **TUI/tui_event.inc** — Mouse poll also dispatches on active continuous-action flags
- **TUI/tui_mouse.inc** — `MSF_SB_ARROW` dispatch in chain; `tui_mouse_on_sb_arrow`
  proc; listbox + textview arrow click sites set repeat flag
- **ed_mouse.inc** — Editor scrollbar arrow clicks set repeat flag
- **TEDIT.ASM** — Version bump to v0.56

Binary size: 58620 bytes (up from 58200, +420 bytes / +0.7%).

## v0.55.0 — Menu Dividers + Scrollbar Hscroll Fix (2026-03-25)

### Menu Dividers
- Dropdown menus now support horizontal divider/separator lines
- Divider entry = `MDE_TEXT` is NULL (0) — no struct size change needed
- Divider renders as `├──────────────────┤` using CP437 T-junction characters
- Arrow Up/Down navigation skips dividers automatically (wrapping works)
- Mouse click on divider is ignored; hover doesn't highlight divider rows
- Hotkey dispatch naturally skips dividers (MDE_HOTKEY=0)
- New constants: `BOX_LT` (0xC3 ├) and `BOX_RT` (0xB4 ┤) in tui_const.inc

### Menu Layout
- **File menu** (5 entries): Open, Save, Save As | Quit
- **Edit menu** (15 entries): Undo, Redo | Cut, Copy, Paste | Find, Find Next,
  Find Prev, Replace | Goto, Date/Time, Word Wrap

### Scrollbar Horizontal Scroll Fix
- Fixed critical buffer overflow in `ed_draw.inc` tab-overshoot check
- When blank/short lines were rendered with `left_col > 0`, unsigned subtraction
  `rnd_visual_col - left_col` underflowed to 0xFFFF, causing `REP STOSW` to write
  65535 words — corrupting all memory (caused DOSBox-X "Illegal Callback" crash)
- Fix uses `CMP`/`JA` with explicit `XOR CX, CX` for the no-overshoot path
- Also made `scroll_to_cursor` compute visible width on-the-fly from `total_lines`
  instead of reading stale `ed_text_cols` BSS variable

### Changes
- **TUI/tui_const.inc** — New `BOX_LT`/`BOX_RT` T-junction constants
- **TUI/tui_menu.inc** — Divider check in entry/accel/hotkey rendering passes;
  Up/Down skip dividers with wrap
- **TUI/tui_mouse.inc** — Divider check in dropdown click and hover handlers
- **TEDIT.ASM** — 4 divider entries (1 File + 3 Edit); updated entry counts;
  version bump to v0.55
- **ed_draw.inc** — Fixed tab-overshoot underflow bug (critical crash fix)
- **ed_core.inc** — `scroll_to_cursor` computes visible width on-the-fly

Binary size: 58200 bytes (up from 57948, +252 bytes / +0.4%).

## v0.54.0 — Scrollbar + Dialog Navigation + Replace All Fix (2026-03-25)

### Vertical Scrollbar
- Auto-showing vertical scrollbar on the right edge of the text area
- Appears when document exceeds 23 visible lines (ED_TEXT_ROWS); hidden otherwise
- Text area shrinks from 80 to 79 columns when scrollbar is visible
- 16-bit safe thumb position/size math — works correctly for files with 255+ lines
- Scrollbar drawn per-row during editor render, using pre-computed thumb bounds

### Scrollbar Mouse Interaction
- Up/down arrow clicks: scroll 1 line
- Track click above thumb: page up; below thumb: page down
- Thumb drag: continuous scroll via existing `tui_mouse_on_sb_drag` → `.sbd_v_editor`
- Scrollbar only interactive when content overflows (short files ignore clicks on col 79)

### Dialog Arrow Key Navigation
- Left/Right arrow keys now cycle focus between buttons in modal dialogs
- New `tui_ctrl_focus_prev` procedure walks singly-linked control list backward
- Right arrow reuses Tab (forward cycle); Left arrow calls focus_prev (backward cycle)

### Replace All Fix
- Replace All now starts from beginning of document (or selection start), not cursor
- Previously skipped matches between BOF and cursor position
- Re-finds first match from BOF after counting, before entering batch loop

### Changes
- **ed_draw.inc** — Scrollbar visibility check, thumb pre-computation, per-row scrollbar
  character rendering; `ED_COLS` → `[ed_text_cols]` with `CS:` segment overrides;
  cursor bounds check uses `ed_text_cols`
- **ed_mouse.inc** — Scrollbar column click detection and dispatch (up/down arrows,
  page up/down, thumb drag setup)
- **TEDIT.ASM** — New BSS: `ed_sb_active` (BYTE), `ed_text_cols` (WORD);
  version bump to v0.54
- **TUI/tui_event.inc** — `.mdl_ext` handles KEY_LEFT/KEY_RIGHT for button focus cycling
- **TUI/tui_control.inc** — New `tui_ctrl_focus_prev` proc (backward focus walk with wrap)
- **ed_find.inc** — Replace All re-searches from BOF/sel_start before batch loop

Binary size: 57948 bytes (up from 55531, +2417 bytes / +4.4%).

## v0.53.0 — Word Wrap + Save As (2026-03-25)

### Word Wrap (Edit > Word Wrap)
- Permanent hard word wrap at 80 columns — inserts CRLF at word boundaries
- Confirm dialog warns: "Save and wrap to 80 cols? Cannot undo."
- Streaming single-pass implementation (`_wrap_save_file`): walks piece table,
  accumulates line in rd_save_buf, breaks at last space when column exceeds 80
- Handles tabs (tab-stop-aware column tracking), long words without spaces
  (hard break), and lines already under 80 columns (passed through)
- After wrapping, file is saved and reloaded via `ed_compact`

### Changes
- **TEDIT.ASM** — New `menu_wordwrap_handler` + `_wrap_save_file` proc (~200 lines);
  added `m_str_wordwrap`, `s_wrap_confirm` strings; Edit menu: 11→12 entries
- Also includes Save As and undo group perf fix from v0.52.0

Binary size: 55531 bytes (up from 54859, +672 bytes / +1.2%).

## v0.52.0 — Save As + undo group performance fix (2026-03-25)

### Save As (File > Save As...)
- New File menu entry between Save and Quit
- Uses the same file browser dialog as Open (drive selection, directory
  navigation, file listing)
- Copies the selected filename and saves the document to it
- Updates the editor's filename — subsequent Ctrl+S saves to the new location

### Undo/Redo group performance fix
- Suppresses `rebuild_meta` during grouped undo/redo loops (overwrite mode,
  datetime insertion, sel_delete, etc.)
- Previously each record in a group triggered a full metadata rebuild (O(n)
  per record × group size). Now: one rebuild at the end of the group.
- Fixes significant lag when undoing overwrite mode edits on large files

### Changes
- **TEDIT.ASM** — New `menu_saveas_handler` proc; added `m_str_saveas` string;
  File menu: 3→4 entries, dropdown width 16→18
- **ed_undo.inc** — Added `MOV BYTE [meta_dirty], 0` before group continuation
  jumps in both `undo_execute` and `redo_execute`

Binary size: 54859 bytes (up from 54759, +100 bytes / +0.2%).

## v0.51.0 — Date/Time insertion + Go to Line (2026-03-25)

### Date/Time (F5 / Edit > Date/Time)
- Inserts current date and time at cursor position in `YYYY-MM-DD HH:MM:SS`
  format (19 characters)
- Uses DOS INT 21h AH=2Ah (date) and AH=2Ch (time)
- Fully undoable as a single grouped operation (one Ctrl+Z removes all 19 chars)
- Replaces the "Toggle Case" menu entry (case toggle is now handled by the
  Match case checkbox in Find/Replace dialogs)
- Removed `[Aa]` status bar indicator (no longer needed)

Binary size: 54759 bytes (up from 54519, +240 bytes / +0.4%).

## v0.50.0 — Go to Line (2026-03-25)

### Go to Line (Ctrl+G / Edit > Goto)
- Input dialog accepts a line number (1-based, max 5 digits)
- Validates: non-numeric input → "Invalid line number"; out of range → "Line
  number out of range"
- On success: moves cursor to start of target line, clears selection, scrolls
  into view

### Changes
- **ed_keys.inc** — `.do_goto` now calls `menu_goto_handler` (was `menu_edit_stub`)
- **TEDIT.ASM** — New `menu_goto_handler` proc (~60 lines, decimal parser with
  overflow detection + range validation); wired into Edit menu; added strings
  `s_goto_title/prompt/invalid/range`; added BSS `goto_buf` (6 bytes)

Binary size: 54519 bytes (up from 54260, +259 bytes / +0.5%).

## v0.49.0 — Permanent Replace All for large batches (2026-03-25)

### Large Replace All — streaming single-pass rewrite
- Replace All counts matches first. If count exceeds 50
  (`REPL_PERM_THRESHOLD`), warns: "Cannot undo. Save and replace all?"
- Permanent path uses new `_repl_save_replaced` proc — a single O(n) pass
  that walks the piece table byte-by-byte, scanning for the needle and
  writing matches as replacement text directly to the output file
- Completely bypasses: `rebuild_meta` (the lockup root cause — called once
  per replacement, each walking the entire 72KB file), `sel_delete`,
  `_repl_insert`, undo buffer, `rd_save_buf`, and piece table manipulation
- Output is buffered (16KB flushes) for efficient disk I/O
- After streaming, `ed_compact` reloads the clean file
- Small Replace All (≤50 matches) remains fully undoable (existing behavior)
- Tested: 102 replacements in 72KB file completes in ~6M instructions
  (previously froze at 500M+)

### Changes
- **ed_const.inc** — Added `REPL_PERM_THRESHOLD EQU 50`
- **ed_find.inc** — New `_find_count_matches` proc; new `_repl_save_replaced`
  proc (~140 lines, streaming piece-table-to-file with inline search/replace,
  16KB write buffer, partial match handling with re-comparison); replaced
  `.mr_perm_*` loop-based path with single `_repl_save_replaced` call
- **TEDIT.ASM** — Added strings `s_repl_perm`, `s_repl_perm_ttl`

Binary size: 54260 bytes (up from 53643, +617 bytes / +1.2%).

## v0.48.0 — Replace dialog with checkboxes + Shift+F3 fix (2026-03-25)

### Custom Replace Dialog
- Replaced `tui_dlg_input2` with a custom dialog featuring:
  - Find: label + textbox
  - Replace with: label + textbox
  - **[X] Match case** checkbox
  - **[ ] In selection** checkbox (pre-checked when multi-line selection exists)
  - **[ ] Replace all** checkbox (new — skips interactive Yes/No/All/Cancel loop)
- Tab order: find textbox → replace textbox → Match case → In selection →
  Replace all → OK → Cancel
- When "Replace all" is checked: batch replace from first match, grouped undo
  (single Ctrl+Z undoes all). When unchecked: interactive per-match confirmation
  (existing Yes/No/All/Cancel flow preserved)
- Selection-only bounds respected in both interactive and batch replace modes

### Shift+F3 Fix
- Corrected Shift+F3 scan code from 54h (Shift+F1) to **56h** (actual Shift+F3)
- Added disambiguation for scan code 52h: Shift held → Find Prev (DOSBox-X
  Shift+F3), no Shift → Insert toggle (overwrite mode)
- F3 + `_key_modifiers` Shift bit also triggers Find Prev (agent86 compatibility)

### Changes
- **ed_const.inc** — No changes
- **ed_find.inc** — New `_repl_show_dialog` proc (~200 lines) builds custom
  Replace dialog with 2 labels, 2 textboxes, 3 checkboxes, 2 buttons; reads
  checkbox states back into `find_flags` + `dlg_chk3` on OK.
  `menu_replace_handler`: detect multi-line selection, call `_repl_show_dialog`,
  check "Replace all" checkbox → jump to batch path or interactive confirm.
  New `.mr_replace_all_from_first` entry point sets selection before batch loop.
- **ed_keys.inc** — Fixed Shift+F3 scan code (54h→56h); added 52h+Shift
  disambiguation for DOSBox-X compatibility
- **TEDIT.ASM** — Added BSS: `dlg_chk3` (15 bytes); added strings
  `s_repl_all`, `s_repl_done_sfx` (already existed)

Binary size: 53643 bytes (up from 52710, +933 bytes / +1.8%).

## v0.47.0 — Find dialog with checkboxes (2026-03-24)

### Custom Find Dialog
- Replaced the plain text-input Find dialog with a custom dialog featuring:
  - TextBox for search term
  - **[X] Match case** checkbox (checked = case-sensitive, unchecked = ignore case)
  - **[ ] In selection** checkbox (enabled only when multi-line selection exists;
    pre-checked when available)
  - OK / Cancel buttons
- Tab order: textbox → Match case → In selection → OK → Cancel
- Enter on textbox auto-activates OK (via TUI framework's built-in behavior)

### Find in Selection
- Multi-line selection scopes the search: Ctrl+F with a multi-line selection
  pre-checks the "In selection" checkbox
- `find_forward` gains bounds checking: matches whose end extends past the
  saved selection boundary are rejected
- F3/Shift+F3 respect selection scope (wrap within selection bounds)
- Single-line selections still pre-fill the search term (existing behavior)

### Dynamic Titles
- "Not found" and "No search term" message boxes show dynamic title based on
  find_flags: "Find", "Find [Aa]", "Find in Selection", or
  "Find in Selection [Aa]"

### Changes
- **ed_const.inc** — Added `FIND_FLAG_CI EQU 01h`, `FIND_FLAG_SEL EQU 02h`
- **ed_find.inc** — New `_find_show_dialog` proc (~130 lines) builds custom dialog
  with label, textbox, 2 checkboxes, 2 buttons; reads checkbox states back into
  `find_flags` on OK. `find_forward`: added `.ff_found_past_sel` bounds check.
  New `_find_select_title` helper. Updated `menu_find_handler` to detect
  multi-line selection and call `_find_show_dialog`. Updated findnxt/findprv
  to wrap within selection bounds and use dynamic titles.
- **TEDIT.ASM** — Added BSS: `find_sel_s_line/col`, `find_sel_e_line/col` (8 bytes),
  `dlg_chk1` (15 bytes), `dlg_chk2` (15 bytes); added title strings
  (`s_find_title_ci`, `s_find_title_sel`, `s_find_title_sel_ci`),
  checkbox label strings (`s_find_matchcase`, `s_find_insel`)

Binary size: 52710 bytes (up from 51866, +844 bytes / +1.6%).

## v0.46.0 — Tab support and Insert/Overwrite toggle (2026-03-24)

### Tab Key Support
- Tab key (09h) inserts a literal tab character into the document
- Tabs render as spaces at tab-stop positions (every N columns)
- Default tab width is 8; override with `/t 2`, `/t 4`, or `/t 8` flag (also `-t`)
- Full horizontal scroll support: partial tab overshoot at left edge renders correctly
- New `byte_to_visual_col` procedure converts byte column to visual column for cursor/scroll

### Insert/Overwrite Toggle
- Insert key (scan 52h) toggles between insert and overwrite mode
- In overwrite mode, typing replaces the character under the cursor (except at EOL, where it inserts)
- Tab in overwrite mode always inserts (no overwrite)
- Grouped undo: one Ctrl+Z undoes an entire sequence of consecutive overwrites
- Status bar shows `[OVR]` when overwrite mode is active

### Changes
- **ed_const.inc** — Added `ED_TAB_DEFAULT EQU 8`, `KEY_INSERT EQU 52h`
- **ed_core.inc** — Rewrote `parse_args` to scan all tokens and handle `/t`/`-t` flags;
  added `byte_to_visual_col` procedure (~70 lines); modified `scroll_to_cursor` to use
  visual columns for horizontal scroll (left_col is now a visual column offset)
- **ed_draw.inc** — Rewrote `ed_hscroll_skip` for tab-aware visual column skipping;
  added tab expansion in `char_loop` with per-tab selection attribute; added overshoot
  fill for partial tabs at left screen edge; `ed_draw_cursor` uses `byte_to_visual_col`;
  added `rnd_byte_col`/`rnd_visual_col` tracking; added `[OVR]` status bar indicator
- **ed_keys.inc** — Route Tab (09h) to `.do_char`; added Insert key toggle handler;
  added overwrite-mode logic in `.do_char` (delete-before-insert with grouped undo)
- **TEDIT.ASM** — Added BSS: `tab_width`, `tab_mask`, `rnd_byte_col`, `rnd_visual_col`,
  `overwrite_mode`; added `s_stat_ovr` string; initialization in `main`

Binary size: 51866 bytes (up from 50956, +910 bytes / +1.8%).

## v0.45.0 — Remove wrap mode (2026-03-24)

Completely removed the `--wrap` text wrapping system. Horizontal scrolling is
now the only display mode. The `--wrap` command-line flag is no longer
recognized.

### Changes

- **ed_edit.inc** — Removed 2 wrap-boundary `total_vis_lines` increment blocks
- **ed_keys.inc** — Removed wrap-mode UP/DOWN visual-row handlers, PgUp/PgDn
  delta walk, `cvl_old_len`/`cvl_old_line` scratch saves (~90 lines)
- **ed_mouse.inc** — Removed wrap-mode click, drag, and auto-scroll coordinate
  mapping using `_row_to_line` (~70 lines)
- **ed_core.inc** — Removed `--wrap` flag parsing, wrap-mode `rebuild_meta`
  (byte-by-byte loop), wrap-mode `line_length`, and `scroll_to_cursor` hscroll
  guard. `rebuild_meta` and `line_length` are now single-path REPNE SCASB
- **ed_file.inc** — Removed `total_vis_lines`/`cur_vis_line` initialization
- **ed_draw.inc** — Removed wrap rendering, scroll-adjust retry loop,
  `cur_vis_row` capture, `_row_to_line` mapping, visual-line status bar,
  `[WRAP]` indicator, and wrap cursor positioning (~150 lines)
- **TEDIT.ASM** — Removed 9 BSS variables (60 bytes: `wrap_mode`, `cur_vis_row`,
  `wrap_retry`, `_row_to_line`, `total_vis_lines`, `cur_vis_line`, `rnd_doc_col`,
  `cvl_old_len`, `cvl_old_line`), `s_stat_wrap` string, updated `s_usage`

Binary size: 50956 bytes (down from 52347, -1391 bytes / -2.7%).
~420 lines of wrap-specific code removed across 7 files.

## v0.44.0 — Wrap-mode performance fix (2026-03-24) [superseded by v0.45.0]

Eliminated O(document_size) `rebuild_meta` calls from the five most common
wrap-mode operations — typing characters, UP/DOWN arrows crossing lines, and
PgUp/PgDn. These now use O(1) incremental updates for the status bar's visual
line counters (`cur_vis_line` and `total_vis_lines`), leaving checkpoints and
piece table structure untouched.

Structural edits (Enter, Backspace, Delete, Cut, Paste, Undo, Redo, Replace,
file load) still trigger full rebuilds as before — only the visual-line-only
sites were changed.

### Changes

- **ed_edit.inc** — `insert_char` fast path (extend piece) and insert_after
  path: replaced `meta_dirty=1` with O(1) wrap boundary check. When `cur_col`
  crosses a multiple of `ED_COLS` (80), increments `total_vis_lines` by 1.
  `cur_vis_line` is unchanged (typing stays on the same logical line).

- **ed_keys.inc** — `.up_prev_line`: removed `meta_dirty=1`. After the existing
  `DIV CX` that computes `line_length / ED_COLS`, subtracts `quotient + 1`
  (the target line's visual height) from `cur_vis_line`. 4 extra instructions.

- **ed_keys.inc** — `.key_down_wrap` / `.down_next_line`: saves old line length
  in `cvl_old_len` BSS scratch. At `.down_next_line`, computes the old line's
  visual height via `old_len / ED_COLS + 1` and adds it to `cur_vis_line`.

- **ed_keys.inc** — `.key_pgdn` / `.key_pgup`: saves old `cur_line` in
  `cvl_old_line` BSS scratch.

- **ed_keys.inc** — `.vert_moved`: replaced `meta_dirty=1` with a bounded walk.
  Iterates from old to new `cur_line`, calling `line_length` for each
  intermediate line and summing visual heights. Adds (PgDn) or subtracts (PgUp)
  the total from `cur_vis_line`. Cost: O(ED_TEXT_ROWS) ≈ 23 line_length calls,
  each using fast checkpoint seeks (meta_dirty=0).

- **TEDIT.ASM** — BSS: added `cvl_old_len` (RESW) and `cvl_old_line` (RESW)

### Performance

Per-keystroke cost on the fast path (2nd+ character typed) is now ~46K-55K
instructions regardless of file size — identical to non-wrap mode. Previously,
every keystroke triggered `rebuild_meta` which walked the entire document
(~800K instructions on a 900-line file).

## v0.43.0 — Cursor styling + fix copy/paste offset bug (2026-03-23)

Two changes: a visual enhancement to the text cursor, and a fix for clipboard
operations copying from the wrong document position.

### Cursor styling

The software block cursor was a nibble-swap (`ROL AL, 4`) which on normal text
(`1Fh`) produced `F1h` — bright white background but with bit 7 set, causing
the cursor character to blink in CGA text mode. Now uses a fixed `0F1h`
attribute (bright white background, blue foreground) and disables blink mode at
startup via `INT 10h AX=1003h BL=0`, reinterpreting bit 7 as bright background
intensity instead of blink.

### Copy/paste offset bug

`line_col_to_offset` in ed_core.inc did not update `rnd_off` after walking
columns in step 4. The `.lco_col_found` and `.lco_col_eol` exit paths left
`rnd_off` pointing at the start of the line rather than the target column.
`ed_copy` saved these stale `rnd_pi`/`rnd_off` values and used them as the
copy-loop starting position, causing the clipboard to capture bytes shifted left
by the column offset — extra content at the start, missing content at the end.

### Changes

- **ed_core.inc** — `line_col_to_offset`: `.lco_col_found` and `.lco_col_eol`
  now compute `rnd_off = SI - DI` (piece-relative offset) before returning
- **ed_draw.inc** — `ed_draw_cursor`: replaced `ROL AL, 4` nibble-swap with
  fixed `MOV BYTE [DI+1], 0F1h` attribute
- **TEDIT.ASM** — `main`: added `INT 10h AX=1003h BL=0` after `tui_init` to
  disable blink attribute (enables bright backgrounds via bit 7)

## v0.42.1 — Fix ES clobber causing MCB corruption on exit (2026-03-20)

`tui_clear_shadow` uses `REP STOSW` to `ES:DI` but never set ES explicitly. When
`_find_prefill_from_sel` or `find_forward` called `resolve_piece` (which clobbers
ES to pt_seg), the next redraw cycle wrote 4000 bytes of desktop fill (`0x17 0x20`
= CLR_DESKTOP + space) to `pt_seg:shadow_buf_offset`, overflowing the piece table
segment and corrupting MCB headers. The corruption was silent during operation
(screen rendered correctly via DS-based writes) but detected on exit when
`free_all` walked the MCB chain.

### Changes

- **TUI/tui_video.inc** — `tui_clear_shadow` now does `PUSH ES / MOV AX,DS /
  MOV ES,AX` before `REP STOSW` and `POP ES` after (defensive root-cause fix)
- **ed_find.inc** — `_find_prefill_from_sel` now preserves ES via `PUSH ES` /
  `POP ES` around `resolve_piece` calls (good practice)

## v0.42.0 — Find Phase 5: Replace All, Case-Insensitive, Selection Pre-fill (2026-03-20)

Four features completing the Find & Replace project:

**Feature A: 4-button Replace confirm dialog** — New `tui_dlg_confirm4` procedure
with [Yes] [No] [All] [Cancel] buttons. Added `dlg_handler_repl_all` handler,
`DLG_REPL_ALL` changed from 2→3 (collision fix), `dlg_btn3`/`dlg_btn4` BSS slots.

**Feature B: Replace All** — Clicking [All] enters batch loop replacing all
remaining matches without further confirmation. Undo grouping via post-patching:
first replacement's DEL_RANGE is group head, all subsequent records get
`URF_GROUP_CONT`. Single Ctrl+Z undoes entire Replace All. No wrap-around in
batch (prevents infinite cycling when replacement contains search term).

**Feature C: Case-insensitive search** — `find_flags` bit 0 toggles via F5 key
(and Edit menu "Toggle Case" entry). `find_forward` uppercases text bytes when
flag set. `_find_upcase_needle` helper pre-uppercases `find_buf` before each
search (called from all 4 handler procs). Status bar shows `[Aa]` indicator.

**Feature D: Selection pre-fill** — `_find_prefill_from_sel` copies single-line
selections ≤64 chars into `find_buf`. Called at start of `menu_find_handler` and
`menu_replace_handler`. Multi-line or oversized selections silently skip pre-fill.

### Changes

- **TUI/tui_const.inc** — `DLG_REPL_ALL` 2→3, added `DLG_BTN_ALL_W EQU 8`
- **TUI/tui_data.inc** — added `dlg_str_all: DB 'All', 0`
- **TUI/tui_dialog.inc** — added `dlg_handler_repl_all`, `tui_dlg_confirm4` proc
- **TEDIT.ASM** — added `dlg_btn3`/`dlg_btn4` (28 bytes BSS), `s_stat_ci` string,
  `m_str_tglcase` menu string; Edit menu entry 11 rewired to `menu_case_toggle`
- **ed_find.inc** — CI compare in `find_forward`, `_find_upcase_needle`,
  `menu_case_toggle`, `_find_prefill_from_sel`, Replace All batch loop with
  undo post-patching, `tui_dlg_confirm4` call in `menu_replace_handler`
- **ed_draw.inc** — `[Aa]` status bar indicator after `[SEL]`
- **ed_keys.inc** — F5 rewired from stub to `menu_case_toggle`

## v0.41.0 — Find Phase 4: Interactive Replace (2026-03-20)

Ctrl+H and Edit > Replace now open a dual-input dialog (Find/Replace with),
then loop through matches with a Yes/No confirmation per match. Replacement
text is bulk-inserted via a new `_repl_insert` helper following the ed_paste
pattern. Each replacement generates 1-2 undo records (UR_DEL_RANGE + UR_INS_RANGE).
The loop terminates when no more matches are found, showing an "N replacements
made." summary. Empty replacement = delete-only. Wrap-around on first search.

### Changes

- **TEDIT.ASM** — added 4 replace dialog string constants (`s_repl_title`,
  `s_repl_prompt`, `s_repl_confirm`, `s_repl_done_sfx`); added 95 bytes BSS
  (`replace_buf`, `replace_len`, `replace_count`, `repl_msg_buf`); changed
  Replace menu entry handler from `menu_edit_stub` to `menu_replace_handler`

- **ed_find.inc** — added 3 new procedures:
  - `_repl_format_count` (~25 lines): 16-bit itoa + suffix append
  - `_repl_insert` (~130 lines): bulk insert from BSS following ed_paste's
    7-stage pattern (capacity check, add buffer append, cursor_to_offset,
    3-case piece insert, cursor advance, UR_INS_RANGE undo push)
  - `menu_replace_handler` (~100 lines): dual-input dialog, interactive
    confirm loop, wrap-around first search, sel_delete + _repl_insert per
    replacement, summary msgbox

- **ed_keys.inc** — `.do_replace` stub replaced with `undo_group_active=0`
  + `CALL menu_replace_handler`

## v0.40.0 — Find Phase 3: Find Next/Prev (2026-03-20)

F3 and Shift+F3 (and Edit > Find Next / Find Prev) repeat the last search
forward or backward without re-prompting. `find_backward` uses a forward-scan
O(n) algorithm tracking the last match before the target position. Guard shows
"No search term." if no previous search exists.

### Changes

- **ed_find.inc** — added 3 new procedures:
  - `find_backward` (~100 lines): forward-scan from BOF, tracks last match
    before target using DI/BP (preserved by find_forward). `find_prev_*` BSS
    scratch for candidate tracking. Sentinel `0FFFFh` for no-prev-found.
  - `menu_findnxt_handler` (~60 lines): F3 — searches from cursor, wraps to
    BOF on miss, sets selection to match
  - `menu_findprv_handler` (~65 lines): Shift+F3 — targets sel_anchor when
    selection active (guarantees backward progress), else cursor

- **ed_keys.inc** — split `.do_findnxt` and `.do_findprv` from stub block,
  wired to handlers with `undo_group_active=0` break

- **TEDIT.ASM** — added `s_find_noterm` string constant; changed Find Next
  and Find Prev menu entry handlers from `menu_edit_stub` to
  `menu_findnxt_handler` / `menu_findprv_handler`; added `find_prev_line/col/
  eline/ecol` BSS variables (8 bytes)

### Tests (12/12 passing)

---

## v0.39.0 — Find Phase 2: Find dialog & forward search (2026-03-20)

Ctrl+F and Edit > Find now show a text input dialog, perform forward search
with wrap-around, and select + scroll to the result. On no match, a "not found"
message box is shown. Selection pre-fill deferred to Phase 5.

### Changes

- **TEDIT.ASM** — added 3 find dialog string constants (`s_find_title`,
  `s_find_prompt`, `s_find_notfound`); changed Find menu entry handler from
  `menu_edit_stub` to `menu_find_handler`

- **ed_find.inc** — added `menu_find_handler` procedure (~65 lines):
  - Shows `tui_dlg_input` with "Find:" prompt, pre-populated with previous
    search term (find_buf persists between calls)
  - Computes `find_len` from null-terminated buffer after dialog returns
  - Calls `find_forward(cur_line, cur_col)` for forward search from cursor
  - On miss, wraps via `find_forward(0, 0)` from document start
  - On no match, shows "Search term not found." via `tui_dlg_msgbox`
  - On match, sets selection (anchor=match start, cursor=exclusive end),
    calls `scroll_to_cursor`, sets `FW_DIRTY`
  - Breaks undo group on entry

- **ed_keys.inc** — split `.do_find` from the remaining stub block
  (`.do_findnxt`/`.do_findprv`/`.do_goto`/`.do_timedate`), wired to
  `menu_find_handler` with `undo_group_active=0` break

### Tests (7/7 passing)

| # | Test | Result |
|---|------|--------|
| 1 | Find existing term "test" | Line 2, Col 15, `[SEL]` |
| 2 | Find non-existent "zzzzz" | Msgbox shown, no `[SEL]` |
| 3 | Find with wrap-around (from line 4) | Wraps to Line 1, Col 6, `[SEL]` |
| 4 | Cancel dialog (Escape) | No state change |
| 5 | Empty find term (Enter) | Silent abort, no crash |
| 6 | Persistent search term | Second Ctrl+F reuses "Hello", finds next match |
| 7 | Edit > Find menu (Alt+E, F) | Same behavior as Ctrl+F |

---

## v0.38.0 — Find Phase 1: Search engine core (2026-03-20)

New `ed_find.inc` module with `find_forward` — a piece-table-walking forward
search procedure. This is the foundation for Ctrl+F, F3, Shift+F3, and
Ctrl+H (Phases 2–5). No UI changes in this phase; the search engine is tested
via a standalone harness.

### Changes

- **ed_const.inc** — added `FIND_MAX_LEN EQU 64` (max search term length)

- **ed_find.inc** — new file with `find_forward` procedure:
  - Walks pieces sequentially via `resolve_piece`, comparing each byte against
    the needle in `find_buf`. `find_needle_pos` persists across piece
    boundaries for cross-piece matching.
  - **Compare-then-track** byte ordering: needle comparison happens BEFORE
    position tracking. Start position captured pre-update (position OF the
    matched char), end position captured post-update (exclusive end).
  - Mismatch restart: on partial match failure, resets `find_needle_pos` to 0
    and re-compares the same byte against `needle[0]` without advancing.
  - Segment discipline follows `ed_copy` pattern: DS=text segment in inner
    loop, all BSS via CS: prefix.

- **ed_find.inc** — piece-finding loop after `line_col_to_offset`:
  - **Bug discovered**: `line_col_to_offset` sets `rnd_pi`/`rnd_off` to the
    LINE START, not the column position. The column walk in step 4 updates
    `rnd_pi` on piece transitions but never updates `rnd_off`.
  - Fix: after `line_col_to_offset` returns the 32-bit byte offset in DX:AX,
    walk pieces from index 0, subtracting each piece's length, to find the
    correct `rnd_pi` and `rnd_off` for the target byte.

- **TEDIT.ASM** — added `INCLUDE ed_find.inc` (between ed_clip.inc and
  ed_draw.inc) and 86 bytes of find BSS variables:
  - `find_buf` (65 bytes), `find_len` (WORD), `find_flags` (BYTE)
  - `find_match_line/col/end_line/end_col` (4 WORDs — match result)
  - `find_cur_line/col`, `find_needle_pos`, `find_start_line/col` (5 WORDs —
    engine scratch)

### Tests
- 12/12 new tests passing (`test_find.asm` + `test_find.txt`):
  - T1: Match at document start ("Hello" at (0,0)→(0,5))
  - T2: Match at document end, no trailing CRLF ("XY" at (4,0)→(4,2))
  - T3: Match mid-line ("World" at (0,6)→(0,11))
  - T4: Needle not found (CF=1)
  - T5–T7: Multiple matches — first, second, and no third ("ABC" in "ABCABC")
  - T8: Empty document guard (pt_count=0, CF=1, no crash)
  - T9: Single-character needle ("e" at (0,1)→(0,2))
  - T10: Search starting mid-document ("line" from (2,5)→(2,9))
  - T11: Search starting past match (CF=1)
  - T12: Cross-line needle ("d\r\nX" spanning lines 3→4)

## v0.37.0 — Fix copy/cut in files larger than 64KB (2026-03-19)

Copy and cut operations failed with "Selection exceeds copy limit" for any
selection near the end of files larger than 65,535 bytes, regardless of
selection size. Root cause: `line_col_to_offset` and `ed_copy` rejected
byte offsets that exceeded 16 bits. A 73,728-byte file has positions past
64K, triggering the error even for a 16-byte selection.

### Changes

- **ed_core.inc** — `line_col_to_offset`: removed the 16-bit overflow guard
  (`TEST DX, DX / JNZ .lco_overflow`). The function now always returns the
  full 32-bit byte offset in DX:AX with CF=0. Callers that need 16-bit
  offsets perform their own checks.

- **ed_clip.inc** — `ed_copy` stages 2–4 rewritten:
  - Stage 2 saves `rnd_pi`/`rnd_off` (piece position) from the first
    `line_col_to_offset` call into repurposed `ec_s_line`/`ec_s_col`, plus
    the 32-bit start offset on the stack.
  - Stage 3 computes byte count via 32-bit subtraction (end − start). Only
    the *count* must fit in 16 bits (clipboard is 16 KB), not the absolute
    offsets.
  - Stage 4 (piece-finding walk loop) eliminated — `line_col_to_offset`
    already sets `rnd_pi`/`rnd_off` as a side effect, making the redundant
    linear walk unnecessary.

- **ed_sel.inc** — `sel_delete` fast path: replaced now-dead `JC .sd_fallback`
  with `TEST DX, DX / JNZ .sd_fallback` (two sites), so range-delete
  correctly falls back to the legacy char-by-char path for offsets > 64K.

- **ed_undo.inc** — `undo_execute` UR_INS_RANGE path: removed dead
  `JC .u_restore` (the subsequent `TEST DX, DX / JNZ .u_restore` already
  handled the > 64K case).

### Tests
- 8/8 new tests passing (`test_big_copy.sh`):
  - A1–A2: 78 KB file (2001 lines) loads and navigates to end
  - B1–B2: Shift+Right and Shift+End copy at > 64K offset — no error dialog
  - C1–C2: Copy + paste round-trip at > 64K offset
  - D1: Multi-line select + copy near end of > 64K file
  - E1: Small-file copy + paste regression check

## v0.36.0 — Preventive fixes: 18 bug fixes across 9 phases (2026-03-19)

Comprehensive preventive fix pass addressing 18 confirmed issues from the
`TEDIT_PREVENTIVE_FIXES.md` audit. All fixes follow the same pattern: validate
BEFORE modifying state, show an error dialog if the operation would fail, and
abort cleanly. ~191 new instructions + ~300 bytes of strings.

### Phase 1: Shared infrastructure
- 8 new NUL-terminated error/warning strings added to `TEDIT.ASM`:
  `s_err_save_fail`, `s_err_save_write`, `s_err_pt_full`, `s_err_undo_lost`,
  `s_err_file_trunc`, `s_err_copy_big`, `s_err_paste_pt`, `s_err_redo_noop`
- **M4 fix**: `m_str_buf_ask` updated from "Save and reload to continue?" to
  "Save+reload? Undo history cleared." — warns user that undo history is lost

### Phase 2: Save file error dialogs (H1)
- File creation failure (INT 21h AH=3Ch CF=1): shows `s_err_save_fail` dialog,
  `dirty` flag preserved, jumps to `.sf_done`
- Partial write detection at 2 write sites (`.sf_break`, `.sf_flush`): checks
  `CMP AX, CX` after INT 21h AH=40h, shows `s_err_save_write` dialog on
  mismatch, closes file handle, `dirty` flag preserved

### Phase 3: Piece table split pre-checks (C1, C4, H6)
- 7 pre-check sites across 3 procedures in `ed_edit.inc`:
  - S1: `insert_char` split path — 2-slot check, dialog, `JMP .ic_done`
  - S2: `insert_char` insert_before — 1-slot check, dialog, `JMP .ic_done`
  - S3: `insert_char` insert_after — 1-slot check, dialog, `JMP .ic_done`
  - S4: `insert_crlf` split path — 2-slot check, dialog, `JMP .icr_done`
  - S5: `insert_crlf` insert_before — 1-slot check, dialog, `JMP .icr_done`
  - S6: `insert_crlf` insert_after — 1-slot check, dialog, `JMP .icr_done`
  - S7: `delete_byte` split path — 1-slot check, `STC; RET` (no dialog, internal proc)
- All checks fire BEFORE any piece descriptor is modified, preventing the
  corruption where the left piece is shrunk but the right piece is never created

### Phase 4: Range-delete save buffer overflow (C2)
- Cumulative bounds check in `range_delete_pieces` Step 8: verifies
  `rd_save_used + rd_save_count <= RD_MAX_PIECES` before copying descriptors
  to `rd_save_buf`. Returns CF=1 on overflow instead of silently overwriting
  past the buffer

### Phase 5: Undo/redo robustness (C3, H4, H5, M5)
- **C3**: `undo_execute` UR_DEL_RANGE handler shows `s_err_undo_lost` dialog
  when `rd_save_used` has insufficient descriptors (piece count > 0). Records
  with piece count = 0 (M5-invalidated) skip silently — cursor-restore only
- **H4**: `redo_execute` UR_INS_RANGE handler shows `s_err_redo_noop` dialog
  instead of silently advancing `undo_pos`. Record is consumed to prevent
  stuck-in-loop on repeated Ctrl+Shift+Z
- **H5**: `redo_execute` UR_DEL_RANGE handler refuses redo when
  `rd_save_used + piece_count > RD_MAX_PIECES`. Shows `s_err_undo_lost` dialog,
  skips deletion, advances `undo_pos`. Prevents creating irreversible operations
- **M5**: `undo_discard_half` now scans surviving records after flushing
  `rd_save_used = 0`. All surviving UR_DEL_RANGE records get `UR_CHAR = 0`
  (piece count zeroed), preventing stale piece-count references. Uses
  `TEST CX, CX / JZ` instead of `JCXZ` (avoids short-range limitation in
  large binaries)

### Phase 6: Paste failure handling (H2)
- `ep_save_add` snapshot moved to top of `ed_paste` (before Stage 1), ensuring
  rollback value is always valid — previously set only in Stage 3, leaving
  `.ep_fail` paths from Stage 2 with a stale value
- `.ep_fail` now rolls back `add_used` to `ep_save_add` and shows
  `s_err_paste_pt` dialog before returning CF=1

### Phase 7: Copy overflow dialog (M2, M4)
- **M2**: New `.ec_offset_overflow` handler in `ed_copy` — when
  `line_col_to_offset` returns CF=1 or DX != 0 (offset > 64K), shows
  `s_err_copy_big` dialog instead of silently failing
- **M4**: Buffer-full dialog text updated (done in Phase 1)

### Phase 8: File truncation warning (H3)
- `load_file` in `ed_core.inc`: after the loading loop, checks
  `CMP BP, MAX_ORIG`. If the file filled all 1024 chunks (~32MB), shows
  `s_err_file_trunc` warning dialog. File is still loaded (truncated) —
  the dialog is informational

### Phase 9: sel_delete_legacy pre-check (M3)
- Before the character-by-character deletion loop, checks
  `pt_count + 100 <= MAX_PT_PIECES`. If the piece table is within 100 slots
  of capacity, shows `s_err_pt_full` dialog, calls `sel_clear`, returns CF=1.
  This is a safety net for the rare case where the fast-path `sel_delete`
  (which uses `range_delete_pieces`) falls through to the legacy path

### Files changed
- `TEDIT.ASM` — 8 error strings, `m_str_buf_ask` text update
- `ed_edit.inc` — Phase 2 (save dialogs) + Phase 3 (7 pre-check sites)
- `ed_core.inc` — Phase 4 (rd_save_buf bounds check) + Phase 8 (truncation warning)
- `ed_undo.inc` — Phase 5 (C3, H4, H5, M5 — 4 code sites)
- `ed_clip.inc` — Phase 6 (paste rollback) + Phase 7 (copy overflow dialog)
- `ed_sel.inc` — Phase 9 (legacy delete capacity check)

### New test harnesses
- `test_pt_full.asm` — 6 tests: split refused (1 slot), boundary insert success,
  boundary insert refused (0 slots), CRLF split refused, delete_byte CF=1,
  normal operation
- `test_undo_robust.asm` — 5 tests: redo paste dialog (H4), undo range-del
  dialog (C3), invalidated record silent skip (C3+M5), redo refused (H5),
  discard_half zeroes UR_CHAR (M5)

### Test results
- 3/3 existing regression tests pass (test_phase1, test_phase2, test_phase3)
- 6/6 test_pt_full tests pass
- 5/5 test_undo_robust tests pass
- 7/7 smoke + integration tests pass (empty doc, file load, typing, save,
  split insert, undo/redo cycle, select+delete+undo)
- DOC_CTX_SIZE unchanged at 12440

## v0.35.0 — Selection grace period (2026-03-18)

### New features
- **Selection grace period**: When holding Shift+Arrow to select text, releasing Shift
  while still holding the arrow key now freezes both the selection and cursor for ~500ms.
  This prevents accidental selection loss when Shift is released slightly before the
  arrow key. During the grace window, nav keys are eaten entirely — no cursor movement,
  no selection change. After ~500ms, the selection clears and cursor movement resumes
  normally. Re-pressing Shift before the window expires resumes selection extension.

### Implementation details
- Grace period check at `.ext_dispatch` in `ed_keys.inc`, before nav key dispatch —
  intercepts nav keys when no Shift + active selection + within tick window
- Within grace: `CLC; RET` (key consumed, nothing happens)
- Grace expired: `sel_clear` then normal nav dispatch
- Shift held path records BIOS tick into `sel_shift_tick` for timer baseline
- `SEL_GRACE_TICKS EQU 9` (~500ms at 18.2 ticks/sec)
- Simplified `.cursor_moved` — removed redundant grace logic (now handled upstream)

### Files changed
- `ed_const.inc` — `SEL_GRACE_TICKS` changed from 5 to 9
- `ed_keys.inc` — grace period check at `.ext_dispatch`, simplified `.cursor_moved`

## v0.34.0 — INT 16h key polling fix (2026-03-17 10:40 UTC)

### Bug fixes
- **DOSBox-X nav-cluster key loss**: Arrow keys, Home, End, Delete, PgUp, PgDn on the
  navigation cluster (E0h-prefixed) were intermittently lost in DOSBox-X. The root cause
  is a race condition in DOSBox-X's `INT 21h AH=06h` two-byte protocol for E0h-prefixed
  extended keys — between the two INT 21h calls, a keyboard interrupt can corrupt the
  "extended pending" state, causing the second call to fail or return wrong data. Numpad
  keys (00h prefix) were unaffected.
- Replaced `tui_poll_key` implementation: switched from `INT 21h AH=06h` (two-call
  protocol for extended keys) to `INT 16h AH=01h` (non-blocking peek) + `AH=00h`
  (consume). INT 16h returns the complete keystroke (scan code + ASCII) in a single call,
  eliminating the two-byte protocol entirely. Agent86 idle detection counts INT 16h
  AH=01h polls, so no behavioral change there.

### Files changed
- `TUI/tui_event.inc` — `tui_poll_key` rewritten (INT 21h → INT 16h)

## v0.33.0 — Clipboard Phase 3: Paste (2026-03-17)

Paste inserts clipboard contents at the cursor position. If a selection is active,
it is deleted first (paste-replaces-selection). Full Cut/Copy/Paste cycle now works
end-to-end with undo support.

### New features
- `ed_paste` proc in `ed_clip.inc` — 5-stage pipeline: check clipboard → delete selection → check/compact add buffer → bulk copy + piece table insert (3 cases: before/after/split) → cursor advance + undo push
- `menu_paste_handler` — Edit > Paste menu entry handler
- Ctrl+V keyboard shortcut wired to `ed_paste`
- Edit > Paste menu entry wired to `menu_paste_handler` (was stub)
- Clipboard overflow dialog — `ed_copy` now shows "Selection too large to copy" msgbox when selection exceeds 16 KB (was silent failure)
- `UR_INS_RANGE` (type 5) — new undo record type for paste operations

### Bug fixes
- Add buffer capacity check uses `JNC` (carry flag) instead of `CMP AX, ADD_BUF_BYTES`. `ADD_BUF_BYTES = 0x10000` wraps to 0 in 16-bit, causing every paste to trigger `ed_buffer_full` unnecessarily. Existing `insert_char` avoids this with `ADD_BUF_BYTES - 1` (= 0xFFFF).

### Architecture
- `ed_paste` creates a single piece in the piece table pointing to the add buffer, covering 3 insertion cases (before, after, split) matching `insert_char`'s existing patterns
- Bulk copy: `clip_seg:0 → add_seg:add_used` via REP MOVSB with cross-segment discipline (DS=clip, ES=add, CS: for BSS)
- Cursor advancement: single-line paste adds `clip_last_col` to `cur_col`; multi-line paste advances `cur_line` by `clip_lines` and sets `cur_col` to `clip_last_col`
- Undo of paste calls `range_delete_pieces` to remove the pasted byte range
- Redo of paste is a deliberate no-op — prevents LIFO `rd_save_buf` corruption in the paste-replaces-selection case
- `.do_paste` in `ed_keys.inc` sets `undo_group_active = 0` to break typing group continuity

### New constants (`ed_const.inc`)
- `UR_INS_RANGE EQU 5` — range insertion (paste) undo record type

### BSS additions (`TEDIT.ASM`, 2 bytes)
- Paste scratch: `ep_save_add` (RESW 1) — saved `add_used` before paste append

### New strings (`TEDIT.ASM`)
- `m_str_clip_full` — "Selection too large to copy"

### Known limitations
- Redo of paste is a no-op (Ctrl+Shift+Z after undoing a paste does nothing)
- Paste-replaces-selection requires 2x Ctrl+Z (consistent with type-over-selection)

### Test results
- 2/2 smoke tests passing
- 10/10 integration tests passing (1 deferred: Ctrl+Home/End not in key dispatch)

### Audits applied
- Gemini audit: 1 rejected, 2 accepted (SI leak guard, capacity re-check)
- Claude Code audit: 4 accepted (register calling convention fix, LIFO corruption prevention, group break, test deferral)

## v0.32.0 — Clipboard Phase 2: Cut (2026-03-16)

Cut = Copy + Delete. Ctrl+X copies selected text to clipboard, then deletes the
selection. Single Ctrl+Z undoes the entire cut. No-op when no selection.

### New features
- `ed_cut` proc in `ed_clip.inc` — composes `ed_copy` + `sel_delete`
- `menu_cut_handler` — Edit > Cut menu entry handler
- Ctrl+X keyboard shortcut wired to `ed_cut`
- Edit > Cut menu entry wired to `menu_cut_handler` (was stub)

### Architecture
- `ed_cut` is 5 instructions of glue: call ed_copy, check CF, call sel_delete, CLC, RET
- No new BSS variables, constants, or undo record types
- Undo: sel_delete pushes one UR_DEL_RANGE record; clipboard unaffected by undo

## v0.31.0 — Clipboard Phase 1: Copy (2026-03-16)

Clipboard infrastructure and `ed_copy` implementation. Ctrl+C copies selected
text to a 16 KB clipboard buffer. Selection persists after copy. Cut and Paste
remain stubs for future phases.

### New features
- `ed_clip.inc` — new include file with `ed_copy` proc and `menu_copy_handler`
- 16 KB clipboard segment allocated at startup, freed at exit (`CLIP_SEG_PARA = 0400h`)
- Ctrl+C copies selected text (silent no-op when no selection — previously showed "Not yet implemented" dialog)
- Pre-computed clipboard metrics: `clip_lines` (LF count), `clip_last_col` (last line column width)
- Edit > Copy menu entry wired to `menu_copy_handler`

### Bug fixes
- `ed_compact` now calls `undo_clear` after reloading from disk — previously left dangling undo records referencing freed segments

### Architecture
- `ed_copy`: 6-stage pipeline (preconditions → normalize → start offset → end offset → find piece → copy loop)
- All values surviving across CALLs in named BSS scratch (`ec_*`), zero stack-relative addressing
- Copy loop: DS=text segment (LODSB), ES=clip segment (STOSB), CS: prefix for BSS access
- Relies on `resolve_piece` preserving DI (clipboard write offset) and BP (remaining byte count)

### New constants (`ed_const.inc`)
- `CLIP_SEG_PARA EQU 0400h` (1024 paragraphs = 16 KB)
- `CLIP_BUF_BYTES EQU CLIP_SEG_PARA * 16` (16384)

### BSS additions (`TEDIT.ASM`, 30 bytes)
- Clipboard state: `clip_seg`, `clip_used`, `clip_lines`, `clip_last_col` (8 bytes)
- Copy scratch: `ec_s_line`, `ec_s_col`, `ec_e_line`, `ec_e_col`, `ec_save_line`, `ec_save_col`, `ec_byte_count` (14 bytes)
- Reuses existing `rd_start_off` for start byte offset

### Test results
- Unit test (`test_copy.asm`): 8/8 assertions pass
- Smoke tests: 2/2 IDLE (empty doc + file loaded)
- Integration tests: 6/6 pass (no-selection silent, selection persists, multi-line, loaded file, typing after copy, undo after copy)
- Full report: `phase1_report.md`

### Files changed
| File | Change |
|------|--------|
| `ed_const.inc` | +3 lines: clipboard constants |
| `ed_file.inc` | +2 lines: `CALL undo_clear` in `ed_compact` |
| `ed_clip.inc` | **New** (~230 lines): `ed_copy` + `menu_copy_handler` |
| `TEDIT.ASM` | +~30 lines: BSS, include, alloc/free, menu wiring |
| `ed_keys.inc` | +5 lines: separate `.do_copy` handler |
| `create_test_file.asm` | **New**: test data generator |
| `test_copy.asm` | **New** (~310 lines): 8-case unit test |
| `check_test.js` | **New**: test output checker utility |

---

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
