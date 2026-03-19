; ============================================================
; TEDIT.ASM — TUI Text Editor
;
; Build+Run: agent86 TEDIT.ASM --build_run --screen CGA80 --args "file.txt"
; ============================================================

    ORG     100h
    SCREEN  CGA80

    JMP     main

; ============================================================
; Editor constants (must be before TUI include)
; ============================================================

INCLUDE ed_const.inc

; ============================================================
; TUI Framework
; ============================================================

INCLUDE TUI\tui.inc

; ============================================================
; Editor String Data
; ============================================================

; ---- Error messages (DOS $-terminated, used before TUI init) ----
s_usage:     DB 'Usage: TEDIT [--wrap] <filename>', 0Dh, 0Ah, '$'
s_nofile:    DB 'Error: cannot open file', 0Dh, 0Ah, '$'
s_nomem:     DB 'Error: out of memory', 0Dh, 0Ah, '$'

; ---- Status bar strings (null-terminated, for shadow_buf writing) ----
s_stat_ln:   DB ' Line ', 0
s_stat_of:   DB ' of ', 0
s_stat_col:  DB '  Col ', 0
s_stat_wrap: DB '  [WRAP]', 0
s_stat_mod:  DB '  [Modified]', 0
s_stat_sel:  DB '  [SEL]', 0
s_stat_untitled: DB 'Untitled', 0

; ============================================================
; Editor Engine
; ============================================================

INCLUDE ed_core.inc

; ============================================================
; Editor Editing Operations
; ============================================================

INCLUDE ed_edit.inc

; ============================================================
; Editor Undo/Redo Buffer
; ============================================================

INCLUDE ed_undo.inc

; ============================================================
; Editor File Load/Reset Helpers
; ============================================================

INCLUDE ed_file.inc

; ============================================================
; Editor Selection Helpers
; ============================================================

INCLUDE ed_sel.inc

; ============================================================
; Editor Clipboard (Copy, Cut, Paste)
; ============================================================

INCLUDE ed_clip.inc

; ============================================================
; Editor Rendering
; ============================================================

INCLUDE ed_draw.inc

; ============================================================
; Editor Keyboard Handling
; ============================================================

INCLUDE ed_keys.inc

; ============================================================
; Editor Mouse Handling
; ============================================================

INCLUDE ed_mouse.inc

; ============================================================
; Application Strings (Menu)
; ============================================================

m_str_file:     DB 'File', 0
m_str_open:     DB 'Open...', 0
m_str_save:     DB 'Save', 0
m_str_quit:     DB 'Quit', 0
m_str_acc_cs:   DB 'Ctrl+S', 0
m_str_acc_aq:   DB 'Alt+Q', 0
m_str_info:      DB 'Info', 0
m_str_about:     DB 'About...', 0
m_str_about_msg: DB 'TEDIT - TUI Text Editor', 0
m_str_open_ttl:  DB 'Open File', 0
m_str_save_ttl:  DB 'Save As', 0
m_str_save_pr:   DB 'Filename:', 0
m_str_quit_ttl:  DB 'Quit', 0
m_str_quit_ask:  DB 'Save changes before closing?', 0
m_str_error:     DB 'Error', 0
m_str_err_open:  DB 'Cannot open file', 0
m_str_err_mem:   DB 'Out of memory', 0
m_str_buf_ttl:   DB 'Buffer Full', 0
m_str_buf_ask:   DB 'Save+reload? Undo history cleared.', 0
m_str_buf_err:   DB 'Reload failed - file on disk', 0
m_str_clip_full: DB 'Selection too large to copy', 0

; ---- Error/warning strings for preventive fixes ----
s_err_save_fail:   DB 'Error: Could not save file.', 0
s_err_save_write:  DB 'Error: Write failed (disk full?).', 0
s_err_pt_full:     DB 'Piece table full. Save and reload.', 0
s_err_undo_lost:   DB 'Cannot undo: history too old.', 0
s_err_file_trunc:  DB 'Warning: File exceeds 32MB limit.', 0
s_err_copy_big:    DB 'Selection exceeds copy limit.', 0
s_err_paste_pt:    DB 'Paste failed: table full.', 0
s_err_redo_noop:   DB 'Redo not available for paste.', 0

; ---- Edit menu strings ----
m_str_edit:      DB 'Edit', 0
m_str_undo:      DB 'Undo', 0
m_str_redo:      DB 'Redo', 0
m_str_cut:       DB 'Cut', 0
m_str_copy:      DB 'Copy', 0
m_str_paste:     DB 'Paste', 0
m_str_find:      DB 'Find...', 0
m_str_findnxt:   DB 'Find Next', 0
m_str_findprv:   DB 'Find Prev', 0
m_str_replace:   DB 'Replace...', 0
m_str_goto:      DB 'Goto...', 0
m_str_timedate:  DB 'Time/Date', 0
m_str_not_impl:  DB 'Not yet implemented', 0

; ---- Edit menu accelerator display strings ----
m_str_acc_cz:    DB 'Ctrl+Z', 0
m_str_acc_csz:   DB 'Ctrl+Shift+Z', 0
m_str_acc_cx:    DB 'Ctrl+X', 0
m_str_acc_cc:    DB 'Ctrl+C', 0
m_str_acc_cv:    DB 'Ctrl+V', 0
m_str_acc_cf:    DB 'Ctrl+F', 0
m_str_acc_f3:    DB 'F3', 0
m_str_acc_sf3:   DB 'Shift+F3', 0
m_str_acc_ch:    DB 'Ctrl+H', 0
m_str_acc_cg:    DB 'Ctrl+G', 0
m_str_acc_f5:    DB 'F5', 0

; ============================================================
; Menu Data Structures
; ============================================================

m_file_entries:
    ; Open...
    DW m_str_open, menu_open_handler
    DB 0, 'O'
    DW 0
    DB 0, 0

    ; Save
    DW m_str_save, menu_save_handler
    DB 0, 'S'
    DW m_str_acc_cs
    DB 0, 0

    ; Quit
    DW m_str_quit, menu_quit_handler
    DB 0, 'Q'
    DW m_str_acc_aq
    DB 10h, 0                       ; accelkey=10h (Alt+Q global dispatch)

m_edit_entries:
    ; Undo — hotidx=0, hotkey='U'
    DW m_str_undo, menu_undo_handler
    DB 0, 'U'
    DW m_str_acc_cz
    DB 0, 0

    ; Redo — hotidx=0, hotkey='R'
    DW m_str_redo, menu_redo_handler
    DB 0, 'R'
    DW m_str_acc_csz
    DB 0, 0

    ; Cut — hotidx=2, hotkey='T'
    DW m_str_cut, menu_cut_handler
    DB 2, 'T'
    DW m_str_acc_cx
    DB 0, 0

    ; Copy — hotidx=0, hotkey='C'
    DW m_str_copy, menu_copy_handler
    DB 0, 'C'
    DW m_str_acc_cc
    DB 0, 0

    ; Paste — hotidx=0, hotkey='P'
    DW m_str_paste, menu_paste_handler
    DB 0, 'P'
    DW m_str_acc_cv
    DB 0, 0

    ; Find... — hotidx=0, hotkey='F'
    DW m_str_find, menu_edit_stub
    DB 0, 'F'
    DW m_str_acc_cf
    DB 0, 0

    ; Find Next — hotidx=5, hotkey='N'
    DW m_str_findnxt, menu_edit_stub
    DB 5, 'N'
    DW m_str_acc_f3
    DB 0, 0

    ; Find Prev — hotidx=8, hotkey='V'
    DW m_str_findprv, menu_edit_stub
    DB 8, 'V'
    DW m_str_acc_sf3
    DB 0, 0

    ; Replace... — hotidx=3, hotkey='L'
    DW m_str_replace, menu_edit_stub
    DB 3, 'L'
    DW m_str_acc_ch
    DB 0, 0

    ; Goto... — hotidx=0, hotkey='G'
    DW m_str_goto, menu_edit_stub
    DB 0, 'G'
    DW m_str_acc_cg
    DB 0, 0

    ; Time/Date — hotidx=5, hotkey='D'
    DW m_str_timedate, menu_edit_stub
    DB 5, 'D'
    DW m_str_acc_f5
    DB 0, 0

m_info_entries:
    DW m_str_about, menu_about_handler
    DB 0, 'A'
    DW 0
    DB 0, 0

m_menu_items:
    DW m_str_file
    DB 0, 6                         ; MI_X=0, MI_W=6
    DW m_file_entries
    DB 3, 16                        ; MI_ECOUNT=3, MI_DDW=16
    DB 0, 21h                       ; MI_HOTIDX=0, MI_ALTKEY=21h (Alt+F)

    ; Edit menu (NEW)
    DW m_str_edit
    DB 6, 6                         ; MI_X=6, MI_W=6
    DW m_edit_entries
    DB 11, 24                       ; MI_ECOUNT=11, MI_DDW=24
    DB 0, 12h                       ; MI_HOTIDX=0 ('E'), MI_ALTKEY=12h (Alt+E)

    DW m_str_info
    DB 12, 6                        ; MI_X=12, MI_W=6
    DW m_info_entries
    DB 1, 12
    DB 0, 17h

m_menubar:
    DB 3, 0FFh, 0, 0FFh
    DB CLR_MENUBAR, CLR_MENU_SEL
    DW m_menu_items

; ============================================================
; Editor Control + Window Template
; ============================================================

ed_ctrl:
    DB CTYPE_EDITOR                                     ; CTRL_TYPE
    DB CTRLF_VISIBLE + CTRLF_ENABLED + CTRLF_FOCUSABLE ; CTRL_FLAGS
    DB 0                                                ; CTRL_X
    DB 0                                                ; CTRL_Y
    DB 80                                               ; CTRL_W
    DB 24                                               ; CTRL_H (23 text + 1 status)
    DW 0                                                ; CTRL_NEXT (end of list)
    DW 0                                                ; CTRL_TEXT (unused)
    DB CLR_WIN_BG                                       ; CTRL_ATTR
    DB CLR_WIN_BG                                       ; CTRL_FATTR
    DW 0                                                ; CTRL_HANDLER
    DW ed_state                                         ; CTRL_ED_STATE

ed_win_tmpl:
    DB WINF_VISIBLE                                     ; WIN_FLAGS
    DB 0                                                ; WIN_X
    DB 1                                                ; WIN_Y (below menu bar)
    DB 80                                               ; WIN_W
    DB 24                                               ; WIN_H
    DB CLR_WIN_BG                                       ; WIN_ATTR
    DB CLR_WIN_BDR                                      ; WIN_BATTR
    DB CLR_WIN_TTL                                      ; WIN_TATTR
    DW 0                                                ; WIN_TITLE (none)
    DW ed_ctrl                                          ; WIN_FIRST
    DW ed_ctrl                                          ; WIN_FOCUS
    DW 0                                                ; WIN_HANDLER
    DB 0, 0, 0, 0                                       ; zorder, id, scrollx, scrolly

; ============================================================
; Menu Handlers
; ============================================================

menu_quit_handler:
    PUSH    SI
    PUSH    DI
    PUSH    BX
    PUSH    CX

    CMP     BYTE [dirty], 0
    JE      .quit_now

    MOV     SI, m_str_quit_ask
    MOV     DI, m_str_quit_ttl
    CALL    tui_dlg_confirm

    CMP     AL, DLG_YES
    JE      .quit_save
    CMP     AL, DLG_NO
    JE      .quit_now
    ; Cancel — abort quit
    JMP     .quit_done

.quit_save:
    CMP     BYTE [filename], 0
    JE      .quit_save_as
    CALL    save_file
    MOV     AX, [undo_pos]
    MOV     [undo_save_pos], AX
    JMP     .quit_now

.quit_save_as:
    MOV     SI, m_str_save_pr
    MOV     DI, m_str_save_ttl
    MOV     BX, filename
    MOV     CL, 80
    CALL    tui_dlg_input
    CMP     AL, DLG_OK
    JNE     .quit_done
    CMP     BYTE [filename], 0
    JE      .quit_done
    CALL    save_file
    MOV     AX, [undo_pos]
    MOV     [undo_save_pos], AX

.quit_now:
    MOV     BYTE [fw_state + FW_RUNNING], 0

.quit_done:
    POP     CX
    POP     BX
    POP     DI
    POP     SI
    RET

menu_open_handler:
    PUSH    SI
    PUSH    DI
    PUSH    BX
    PUSH    CX

    ; ---- If dirty, ask to save first ----
    CMP     BYTE [dirty], 0
    JE      .open_dlg

    MOV     SI, m_str_quit_ask
    MOV     DI, m_str_quit_ttl
    CALL    tui_dlg_confirm

    CMP     AL, DLG_YES
    JE      .open_save
    CMP     AL, DLG_NO
    JE      .open_dlg
    JMP     .open_done              ; Cancel — abort open

.open_save:
    CALL    menu_save_handler

.open_dlg:
    ; ---- Show file selector ----
    MOV     DI, m_str_open_ttl
    MOV     BX, open_file_buf
    MOV     CL, 13
    CALL    tui_dlg_file
    CMP     AL, DLG_OK
    JNE     .open_done

    ; ---- Copy filename and load ----
    PUSH    SI
    PUSH    DI
    MOV     SI, open_file_buf
    MOV     DI, filename
.open_copy:
    MOV     AL, [SI]
    MOV     [DI], AL
    INC     SI
    INC     DI
    TEST    AL, AL
    JNZ     .open_copy
    POP     DI
    POP     SI

    CALL    ed_load_file

.open_done:
    POP     CX
    POP     BX
    POP     DI
    POP     SI
    RET

menu_save_handler:
    PUSH    SI
    PUSH    DI
    PUSH    BX
    PUSH    CX

    CMP     BYTE [filename], 0
    JE      .save_as

    ; ---- Direct save ----
    CALL    save_file
    MOV     AX, [undo_pos]
    MOV     [undo_save_pos], AX
    MOV     BYTE [fw_state + FW_DIRTY], 1
    JMP     .save_done

.save_as:
    ; ---- No filename — prompt for one ----
    MOV     SI, m_str_save_pr
    MOV     DI, m_str_save_ttl
    MOV     BX, filename
    MOV     CL, 80
    CALL    tui_dlg_input
    CMP     AL, DLG_OK
    JNE     .save_done

    ; Check that user actually typed something
    CMP     BYTE [filename], 0
    JE      .save_done

    ; Save to the new filename
    CALL    save_file
    MOV     AX, [undo_pos]
    MOV     [undo_save_pos], AX
    MOV     BYTE [fw_state + FW_DIRTY], 1

.save_done:
    POP     CX
    POP     BX
    POP     DI
    POP     SI
    RET

menu_undo_handler:
    MOV     BYTE [undo_group_active], 0
    CALL    undo_execute
    RET

menu_redo_handler:
    MOV     BYTE [undo_group_active], 0
    CALL    redo_execute
    RET

menu_edit_stub:
    PUSH    SI
    PUSH    DI
    MOV     SI, m_str_not_impl
    MOV     DI, m_str_edit
    CALL    tui_dlg_msgbox
    POP     DI
    POP     SI
    RET

menu_about_handler:
    PUSH    SI
    PUSH    DI
    MOV     SI, m_str_about_msg
    MOV     DI, m_str_info
    CALL    tui_dlg_msgbox
    POP     DI
    POP     SI
    RET

; ============================================================
; MAIN
; ============================================================

main:
    CALL    shrink_mem

    ; ---- Parse command line ----
    CALL    parse_args
    JC      .no_file

    ; ---- Open and load file ----
    CALL    open_file
    JC      .err_nofile

    CALL    load_file
    JC      .err_nomem

    ; Close file handle
    MOV     BX, [file_hnd]
    MOV     AH, 3Eh
    INT     21h

    ; ---- Initialize piece table ----
    CALL    pt_init
    JC      .err_nomem

    ; ---- Allocate add buffer ----
    MOV     BX, ADD_BUF_PARA
    MOV     AH, 48h
    INT     21h
    JC      .err_nomem
    MOV     [add_seg], AX
    MOV     WORD [add_used], 0

    ; ---- Build metadata ----
    CALL    rebuild_meta
    CALL    clamp_cursor
    MOV     BYTE [meta_dirty], 0
    JMP     .start_tui

.no_file:
    CALL    ed_new_doc
    JC      .err_nomem

.start_tui:
    ; ---- Allocate undo buffer (if not already allocated) ----
    CMP     WORD [undo_seg], 0
    JNE     .undo_ready
    MOV     BX, UNDO_SEG_PARA
    MOV     AH, 48h
    INT     21h
    JC      .err_nomem
    MOV     [undo_seg], AX
.undo_ready:
    CALL    undo_init

    ; ---- Allocate clipboard buffer (if not already allocated) ----
    CMP     WORD [clip_seg], 0
    JNE     .clip_ready
    MOV     BX, CLIP_SEG_PARA
    MOV     AH, 48h
    INT     21h
    JC      .err_nomem
    MOV     [clip_seg], AX
.clip_ready:
    MOV     WORD [clip_used], 0

    ; ---- Initialize editor state ----
    MOV     WORD [top_line], 0
    MOV     WORD [cur_line], 0
    MOV     WORD [cur_col], 0
    MOV     WORD [left_col], 0
    MOV     BYTE [dirty], 0
    MOV     WORD [last_ins_pi], 0FFFFh
    MOV     BYTE [sel_active], 0

    ; ---- Initialize TUI ----
    CALL    tui_init
    CALL    tui_mouse_init
    MOV     WORD [fw_state + FW_MENUBAR], m_menubar

    ; ---- Create editor window ----
    MOV     SI, ed_win_tmpl
    CALL    tui_win_create

    ; ---- Run event loop ----
    CALL    tui_run

    ; ---- Cleanup and exit ----
    CALL    free_all
    ; Free undo buffer (persistent — not freed by free_all)
    MOV     AX, [undo_seg]
    TEST    AX, AX
    JZ      .free_clip
    MOV     ES, AX
    MOV     AH, 49h
    INT     21h
.free_clip:
    ; Free clipboard buffer (persistent — not freed by free_all)
    MOV     AX, [clip_seg]
    TEST    AX, AX
    JZ      .exit
    MOV     ES, AX
    MOV     AH, 49h
    INT     21h
.exit:
    INT     20h

.err_nofile:
    MOV     DX, s_nofile
    MOV     AH, 09h
    INT     21h
    INT     20h

.err_nomem:
    MOV     DX, s_nomem
    MOV     AH, 09h
    INT     21h
    CALL    free_all
    INT     20h

; ============================================================
; BSS — All uninitialized data at end of file
; ============================================================

; ============================================================
; DOCUMENT CONTEXT — contiguous block, swappable in Phase 2
; DOC_CTX through DOC_CTX_END will be copied as a unit.
;
; CONSTRAINT: top_line (ED_SCROLLY) must be at offset 0 and
; total_lines (ED_LINECOUNT) at offset 2 from ed_state,
; because CTRL_ED_STATE points here and the TUI framework
; reads these fields by offset.
; ============================================================

ed_state:
DOC_CTX:
top_line:       RESW 1          ; +0   ED_SCROLLY
total_lines:    RESW 1          ; +2   ED_LINECOUNT
filename:       RESB 128        ; +4   current file path
orig_count:     RESW 1          ; +132
orig_seg:       RESW MAX_ORIG   ; +134  (2048 bytes)
orig_len:       RESW MAX_ORIG   ; +2182 (2048 bytes)
add_seg:        RESW 1          ; +4230
add_used:       RESW 1          ; +4232
pt_seg:         RESW 1          ; +4234
pt_count:       RESW 1          ; +4236
chkpt_num:      RESW 1          ; +4238
chkpt_pi:       RESW MAX_CHKPT  ; +4240 (4096 bytes)
chkpt_po:       RESW MAX_CHKPT  ; +8336 (4096 bytes)
cur_line:       RESW 1          ; +12432
cur_col:        RESW 1          ; +12434
dirty:          RESB 1          ; +12436
meta_dirty:     RESB 1          ; +12437
last_ins_pi:    RESW 1          ; +12438
DOC_CTX_END:                    ; +12440

; ---- Compile-time verification ----
PRINT "DOC_CTX_SIZE = ", DOC_CTX_END - DOC_CTX

; ============================================================
; TRANSIENT / GLOBAL — not part of document context
; These are scratch variables recomputed on every use, or
; shared global state. Not swapped between documents.
; ============================================================

open_file_buf:  RESB 14         ; temp buffer for file dialog
file_hnd:       RESW 1          ; file handle during load (closed after)
wrap_mode:      RESB 1          ; global --wrap flag
rnd_pi:         RESW 1          ; render/seek scratch
rnd_off:        RESW 1          ; render/seek scratch
rnd_base:       RESW 1          ; render/seek/save scratch
rnd_len:        RESW 1          ; render/save scratch
rnd_seg:        RESW 1          ; render scratch
rnd_row:        RESW 1          ; render scratch
stl_col:        RESW 1          ; seek/rebuild scratch
ed_abs_row:     RESB 1          ; render scratch
left_col:       RESW 1          ; horizontal scroll offset (non-wrap)
cur_vis_row:    RESW 1          ; visual screen row of cur_line (wrap mode)
wrap_retry:     RESB 1          ; scroll-adjust retry counter (wrap mode)
_row_to_line:   RESW ED_TEXT_ROWS ; screen row → logical line mapping (wrap mouse)
total_vis_lines: RESW 1          ; total visual lines (wrap mode status bar)
cur_vis_line:   RESW 1           ; visual line at start of cur_line (wrap mode)

; ---- line_col_to_offset scratch ----
lco_off_lo:     RESW 1          ; byte offset accumulator (low word)
lco_off_hi:     RESW 1          ; byte offset accumulator (high word)

; ---- range_delete scratch ----
rd_save_buf:      RESB RD_MAX_PIECES * PT_DESC_SIZE  ; 1020 bytes
rd_save_count:    RESW 1        ; number of descriptors saved this operation
rd_save_start_pi: RESW 1        ; piece index where deletion began (for undo)
rd_start_off:     RESW 1        ; input start_offset
rd_end_off:       RESW 1        ; input end_offset
rd_start_pi:      RESW 1        ; piece index containing start_off
rd_end_pi:        RESW 1        ; piece index containing end_off
rd_start_split:   RESW 1        ; byte offset within start piece
rd_end_split:     RESW 1        ; byte offset within end piece
rd_end_plen:      RESW 1        ; length of end piece (for split check)
rd_same_piece:    RESW 1        ; 1 if start/end in same original piece
rd_save_used:     RESW 1        ; running count of descriptors in rd_save_buf (LIFO stack top)

; ---- Undo / Redo state ----
undo_seg:       RESW 1          ; segment of undo buffer (DOS alloc)
undo_pos:       RESW 1          ; write cursor - index of next record to write
undo_count:     RESW 1          ; total valid records in buffer
undo_max:       RESW 1          ; capacity (records that fit in segment)
undo_recording: RESB 1          ; 1 = record edits, 0 = suppress during undo/redo
undo_save_pos:  RESW 1          ; undo_pos at last save (Phase 5 dirty tracking)
undo_pre_line:  RESW 1          ; scratch: cursor line before current edit
undo_pre_col:   RESW 1          ; scratch: cursor col before current edit
undo_del_char:  RESB 1          ; scratch: char deleted by last delete_at
undo_del_crlf:  RESB 1          ; scratch: 1 if CRLF pair was deleted, 0FFh=no delete
undo_group_active: RESB 1      ; 1 if last recorded op was INS_CHAR
undo_group_line:   RESW 1      ; doc_line of last INS_CHAR
undo_group_col:    RESW 1      ; doc_col of last INS_CHAR
undo_last_type:    RESB 1      ; scratch: raw UR_TYPE of record being processed

; ---- Selection state ----
sel_active:        RESB 1      ; 0 = no selection, 1 = selection active
sel_anchor_line:   RESW 1      ; line where selection was initiated
sel_anchor_col:    RESW 1      ; column where selection was initiated

sel_shift_tick:    RESW 1      ; BIOS tick of last Shift+nav (grace period)

; ---- Selection rendering transients ----
sel_s_line:        RESW 1      ; normalized start line (pre-render)
sel_s_col:         RESW 1      ; normalized start col
sel_e_line:        RESW 1      ; normalized end line
sel_e_col:         RESW 1      ; normalized end col
sel_rendering:     RESB 1      ; 1 = selection bounds valid for render
rnd_doc_col:       RESW 1      ; document column (wrap-mode rendering)

; ---- sel_delete transients ----
sd_s_line:         RESW 1      ; sel_delete: stop line
sd_s_col:          RESW 1      ; sel_delete: stop column
sd_first:          RESB 1      ; sel_delete: first-record flag (1=first)

; ---- Clipboard state ----
clip_seg:          RESW 1      ; segment of clipboard buffer (DOS alloc)
clip_used:         RESW 1      ; bytes currently in clipboard (0 = empty)
clip_lines:        RESW 1      ; LF count in clipboard content
clip_last_col:     RESW 1      ; visible column width of last line in clipboard

; ---- ed_copy scratch (named to avoid stack-relative addressing) ----
ec_s_line:         RESW 1      ; normalized selection start line
ec_s_col:          RESW 1      ; normalized selection start col
ec_e_line:         RESW 1      ; normalized selection end line
ec_e_col:          RESW 1      ; normalized selection end col
ec_save_line:      RESW 1      ; saved cursor line (restored after copy)
ec_save_col:       RESW 1      ; saved cursor col (restored after copy)
ec_byte_count:     RESW 1      ; total bytes to copy to clipboard

; ---- ed_paste scratch ----
ep_save_add:       RESW 1      ; saved add_used before paste append

; ---- TUI BSS ----
shadow_buf:         RESB 4000
fw_state:           RESB 16
win_table:          RESB 320
z_order:            RESB 16
bdr_scratch:        RESB 7
ctrl_dispatch_addr: RESW 1
mouse_state:        RESB 16
sb_scratch:         RESB 4
dlg_tmpl:           RESB 20
dlg_label:          RESB 14
dlg_btn1:           RESB 14
dlg_btn2:           RESB 14
dlg_textbox:        RESB 18
dlg_label2:         RESB 14
dlg_textbox2:       RESB 18
dlg_save_title:     RESW 1
dlg_save_buf:       RESW 1
dlg_save_maxlen:    RESB 1
dlg_save_prompt2:   RESW 1
dlg_save_buf2:      RESW 1
dlg_save_maxlen2:   RESB 1
dlg_listbox:        RESB 19
dlg_file_dta:       RESB 43
dlg_file_path:      RESB 65
dlg_file_count:     RESB 1
dlg_file_items:     RESB 256
dlg_file_names:     RESB 1792
dlg_drive_dd:       RESB 21
dlg_drive_count:    RESB 1
dlg_drive_items:    RESB 52
dlg_drive_names:    RESB 78
