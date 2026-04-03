# TEDIT

![TEDIT Screenshot](assets/screenshot.jpg)

A multi-document text editor for DOS, written entirely in 8086 assembly language.

TEDIT is built on a custom TUI (Text User Interface) framework with support for
up to 20 simultaneous open files, a document side panel, project files, and
disk-based document swapping. It targets real-mode DOS as a `.COM` binary and
runs on IBM PC compatibles, DOSBox-X, and the agent86 emulator.

## Screenshot

Run in DOSBox-X or agent86 with CGA 80-column text mode:

```
 File  Edit  View  Info
The quick brown fox jumps over the lazy dog.
This is line two of the document.

 myfile.txt Line 1 of 3  Col 1
```

## Features

### Editing
- Full keyboard text editing (insert, delete, backspace, enter)
- Insert and overwrite modes (Insert key to toggle, `[OVR]` indicator)
- Tab key inserts literal tab characters with configurable display width
  (`/t 2`, `/t 4`, `/t 8` command-line flag; default 8)
- Horizontal scrolling for lines longer than 80 columns
- Tab-aware rendering with correct tab stop alignment

### Selection
- Shift+Arrow/Home/End/PgUp/PgDn for keyboard selection
- Mouse click and drag selection with auto-scroll
- Selection grace period prevents accidental deselection on Shift release
- Type, Delete, or Backspace replaces selection

### Clipboard
- Cut (Ctrl+X), Copy (Ctrl+C), Paste (Ctrl+V)
- Internal clipboard (16 KB buffer)

### Undo/Redo
- Ctrl+Z to undo, Ctrl+Shift+Z to redo
- 32 KB undo buffer (~3,276 records)
- Grouped undo for consecutive typing, overwrite sequences, and date/time insertion
- Smart dirty tracking (knows when file returns to saved state)

### Find & Replace
- Find (Ctrl+F) with custom dialog featuring checkboxes:
  - **Match case** toggle (case-sensitive or case-insensitive search)
  - **In selection** toggle (restrict search to multi-line selection)
- Find Next (F3), Find Prev (Shift+F3)
- Replace (Ctrl+H) with custom dialog:
  - **Match case**, **In selection**, and **Replace all** checkboxes
  - Interactive replace: per-match Yes/No/All/Cancel confirmation
  - Small batches (50 or fewer matches): fully undoable
  - Large batches (over 50 matches): streaming single-pass replace for
    performance, with permanent save (warned before proceeding)
- Selection pre-fill: single-line selection populates the search field

### Multi-Document Editing
- Up to 20 files open simultaneously
- Disk-based document swapping — only the active document is in memory;
  inactive documents are saved to temporary swap files and restored on switch
- Document switching: Alt+1–0 for documents 1–10, Alt+Shift+1–0 for
  documents 11–20, F6 (next document), Ctrl+PgDn/PgUp (next/prev occupied slot)
- Side panel (F1 or View > Document List): two-tier layout showing all open
  documents with slot number, filename, dirty indicator, and arrow marker.
  Navigate with Up/Down arrows, Enter to switch, Esc to close. Alt+N shortcuts
  also work while the panel is open (closes panel and switches). Drop shadow
  on the right edge
- Duplicate file prevention — opening an already-open file switches to it
- `[N/M]` status bar indicator when multiple documents are open

### Project Files
- Save Project (File > Save Project): writes all open file paths to a `.PRJ`
  file (one path per line, plain text — hand-editable)
- Load Project (File > Load Project): closes all current documents and opens
  every file listed in the `.PRJ` file
- Command-line project loading: `TEDIT myproject.prj` loads the project at
  startup without a dialog
- Project files use `*.PRJ` wildcard filter in the file dialog

### File Operations
- New (Ctrl+N), Open (Ctrl+O) with Turbo Debugger-style file browser:
  - Separate Files and Directories listboxes
  - Filename text field with wildcard support (`*.txt`, `*.asm`, etc.)
  - Wildcard persistence between opens
  - Listbox double-click and Enter to open files
  - Drive selection dropdown and path display
- Save (Ctrl+S), Save As (Ctrl+Shift+S) with overwrite confirmation
- Save All (Ctrl+Alt+S) — saves all dirty, named documents
- Close (Ctrl+W) — single document resets to untitled; multiple documents
  switches to the next
- Close All (Ctrl+Alt+W) — prompts for each dirty document, resets to untitled
- Crash-safe save — writes to a temporary file (`TEDIT.$$$`), then renames
  over the original. If power is lost during save, the original file is intact
- Shell to DOS (File > Shell to DOS, F8) — spawns interactive COMMAND.COM via
  COMSPEC; mouse and cursor fully restored on return; compatible with SHROOM
  utility for memory-efficient shelling
- `/d` flag — dumps all allocated memory (~200 KB) to a swap file before
  shelling, freeing it for the child process. Memory is restored on return.
  Orphan swap files from crashes are detected on startup with a
  Restore/Delete prompt. Usage: `TEDIT myfile.txt /d`
- Word Wrap (Edit > Word Wrap) — permanent hard wrap at 80 columns
- Go to Line (Ctrl+G)
- Date/Time insertion (F5) — inserts `YYYY-MM-DD HH:MM:SS` at cursor
- Exit (Alt+X)
- Working directory restored on exit

### Menu System
- File: New, Open, Close, Close All, Save, Save As, Save All, Load Project,
  Save Project, Shell to DOS, Exit
- Edit: Undo, Redo, Cut, Copy, Paste, Find, Find Next, Find Prev,
  Replace, Goto, Date/Time, Word Wrap
- View: Document List
- Info: About

### Compatibility
- CGA snow prevention for original IBM CGA adapters
- 4DOS and third-party shell compatibility (BSS zeroing, INT 23h/24h handlers)
- Ctrl+C handled as Copy (not break) — INT 23h intercepted

## Building

TEDIT is assembled using **[agent86](https://github.com/cookertron/agent86)**
v0.21.0 or later (required for `SECTION .bss` support), a two-pass 8086
assembler and per-instruction JIT emulator targeting `.COM` binaries. agent86
is a standalone Windows tool designed for agentic AI workflows — all output is
JSON on stdout, enabling automated build-test cycles.

See `manual.md` for the full agent86 reference.

### Assemble only

```
agent86 TEDIT.ASM
```

Produces `TEDIT.COM` (the editor binary) and `TEDIT.DBG` (debug symbols).

### Assemble and run

```
agent86 TEDIT.ASM --build_run --screen CGA80
```

### Run with a file

```
agent86 TEDIT.ASM --build_run --screen CGA80 --args "myfile.txt"
```

### Run with multiple files

```
agent86 TEDIT.ASM --build_run --screen CGA80 --args "main.c header.h notes.txt"
```

### Run with a project file

```
agent86 TEDIT.ASM --build_run --screen CGA80 --args "myproject.prj"
```

### Run with custom tab width

```
agent86 TEDIT.ASM --build_run --screen CGA80 --args "myfile.txt /t 4"
```

### Run with memory-saving shell mode

```
agent86 TEDIT.ASM --build_run --screen CGA80 --args "myfile.txt /d"
```

The `/d` flag dumps all editor memory to disk before Shell to DOS, freeing
~200 KB for compilers and other tools. Memory is restored when you type `EXIT`.

### Run in DOSBox-X

Copy `TEDIT.COM` to your DOSBox-X drive and run:

```
TEDIT myfile.txt
TEDIT main.c header.h notes.txt
TEDIT myproject.prj
TEDIT myfile.txt /t 4
TEDIT myfile.txt /d
```

### Command-Line Flags

| Flag | Description |
|------|-------------|
| `/t N` | Set tab display width (2, 4, or 8; default 8) |
| `/d` | Enable memory dump mode for Shell to DOS |

Flags can use `/` or `-` prefix and appear in any order with filenames.

## Architecture

TEDIT is a `.COM` flat-model program (ORG 100h, all segments equal).
The binary is approximately 43 KB of code with 14 KB of BSS.

### Piece Table Document Model

Text is stored in a piece table — an append-only data structure where the
original file data and new edits live in separate buffers. Insertions and
deletions manipulate piece descriptors (source, offset, length) rather
than moving text. This gives efficient editing regardless of file size.

- Original buffer: file data stays on disk, accessed through a 2-slot LRU
  cache (2 x 32 KB). Only the data being viewed or edited is in memory.
- Add buffer: 64 KB for new text
- Piece table: up to 4,096 piece descriptors (10 bytes each)
- Checkpoint table: 2,048 line-to-piece mappings for fast seeking
- Incremental metadata: typing, Enter, undo/redo, and paste update
  checkpoints in O(1) without a full rebuild

### Memory Layout

| Segment | Size | Purpose |
|---------|------|---------|
| COM segment | 64 KB | Code + BSS (fixed, stays resident during Shell) |
| Cache slot 0 | 32 KB | LRU file cache |
| Cache slot 1 | 32 KB | LRU file cache |
| Piece table | 40 KB | Piece descriptors |
| Add buffer | 64 KB | New text storage |
| Undo buffer | 32 KB | Undo/redo records |
| Clipboard | 16 KB | Cut/copy buffer |
| Shadow buffer | 4 KB | TUI screen compositing |
| Doc table | 3 KB | Multi-document slot metadata |

Total: ~287 KB. With `/d`, all segments except the COM segment are freed
before shelling (~223 KB reclaimed).

### Source Structure

| File | Purpose |
|------|---------|
| `TEDIT.ASM` | Main file: includes, menu data, handlers, BSS |
| `ed_const.inc` | All EQU constants |
| `ed_core.inc` | Piece table engine, LRU cache, cursor, scroll, argument parsing |
| `ed_edit.inc` | Insert, delete, save (temp-file-rename) |
| `ed_undo.inc` | Undo/redo buffer and execution |
| `ed_file.inc` | File loading, compaction |
| `ed_sel.inc` | Selection management |
| `ed_clip.inc` | Clipboard (copy, cut, paste) |
| `ed_find.inc` | Find, replace, streaming replace |
| `ed_draw.inc` | Rendering, tab expansion, cursor, status bar, side panel |
| `ed_keys.inc` | Keyboard dispatch, side panel navigation |
| `ed_mouse.inc` | Mouse click and drag, side panel click |
| `ed_multidoc.inc` | Document table, swap out/in, switching, slot management |
| `ed_shell.inc` | Shell swap: memory dump/restore for Shell to DOS (`/d` flag) |
| `TUI\tui.inc` | TUI framework (master include) |

### TUI Framework

The editor is built on a custom TUI library (`TUI\` directory) that provides:

- Shadow buffer compositing with VRAM blit (segment-based, CGA snow safe)
- Windowing with z-order, borders, titles, move, resize
- Control types: label, button, textbox, checkbox, radio, dropdown,
  listbox, text viewer, editor
- Menu bar with dropdown menus, hotkeys, and accelerators
- Modal dialog system (message box, confirm, input, file selector)
- Mouse support (click, drag, scroll bars)
- Full keyboard navigation (Tab/Shift+Tab focus cycling, Enter activation)

## What TEDIT Is Not

TEDIT is a deliberate exercise in minimalism — a text editor written in
pure 8086 assembly for real-mode DOS. There are things it does well and
things it intentionally does not attempt.

### Limitations

- **20 documents maximum.** Up to 20 files open at once via disk-based swapping.
- **No syntax highlighting.** All text renders in a single colour.
- **No line numbers** in the editing area (line/column shown in status bar).
- **No soft word wrap.** Lines display with horizontal scrolling. The Word
  Wrap feature permanently modifies the file (hard wrap).
- **64 KB add buffer.** Very long editing sessions without saving may fill the
  add buffer, triggering a save-and-reload compaction cycle.
- **4,096 piece limit.** Extremely fragmented documents (thousands of tiny
  edits without saving) may hit this. Save and reopen to consolidate.
- **File dialog shows max 128 files** and **32 directories** per listing.
  Directories with more entries silently truncate.
- **DOS 8.3 filenames.** Filename fields are limited to 13 characters
  (8.3 format). Long filename support is not available.
- **No paste redo.** Undoing a paste works, but redoing it is a no-op.
  This is a deliberate trade-off to prevent LIFO save buffer corruption.
- **No multi-level redo.** Redo is linear — new edits after undo discard the
  redo tail.
- **Replace All for large batches (>50 matches) is permanent.** The streaming
  replace bypasses the undo system for performance. A warning is shown.
- **80-column text mode required.** 40-column and MDA modes are rejected at
  startup. No graphics, no 132-column mode, no colour themes.

### Not a bug, by design

- **No auto-save.** Save explicitly with Ctrl+S or File > Save.
- **No configuration file.** Settings are command-line flags only (`/t`, `/d`).
- **Cursor is a colour block**, not a blinking underscore. This is the
  software cursor implemented via attribute manipulation.
- **Mouse cursor is attribute-inverted**, not a hardware cursor.
- **Undo history cleared on save.** The safe-save process replaces the
  backing file, invalidating piece table references. This is a trade-off
  for crash safety.

## Version History

See `CHANGELOG.md` for the full version history with detailed per-version
changes. The editor has been developed through 68 versions covering:

- Core editing and file I/O with crash-safe temp-file-rename save
- Piece table engine with checkpoint-accelerated seeking and LRU disk cache
- Incremental metadata updates (O(1) typing, Enter, undo/redo, paste)
- Full undo/redo with grouping and smart dirty tracking
- Keyboard and mouse selection
- Range delete with piece table surgery
- Clipboard operations
- Find and replace with case-insensitive and selection-scoped search
- Streaming replace for large batch operations
- Tab support with configurable width
- Insert/overwrite mode toggle
- Turbo Debugger-style file browser with wildcard filtering and double-click
- Word wrap, Go to Line, Date/Time insertion
- Multi-document editing with disk-based swapping (up to 20 files)
- Document side panel with two-tier navigation and drop shadow
- Project file load/save (menu and command line)
- Multi-file command-line loading
- Shell to DOS with `/d` memory dump mode and crash recovery
- CGA snow prevention and 4DOS/third-party shell compatibility
- COM binary size optimization (BSS section, segment migration)

## Credits

- **Directed by** Cookertron
- **Coded by** Claude Opus 4.6
- **Assembler/Emulator:** [agent86](https://github.com/cookertron/agent86) (custom 8086 toolchain)
- **Thanks to** the VOGONS community for support and inspiration
- **Repository:** github.com/cookertron/TEDIT
