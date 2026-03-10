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

; ============================================================
; Editor Engine
; ============================================================

INCLUDE ed_core.inc

; ============================================================
; Editor Editing Operations
; ============================================================

INCLUDE ed_edit.inc

; ============================================================
; Editor File Load/Reset Helpers
; ============================================================

INCLUDE ed_file.inc

; ============================================================
; Multi-Document Management
; ============================================================

INCLUDE ed_multidoc.inc

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
m_str_new:      DB 'New', 0
m_str_open:     DB 'Open...', 0
m_str_close:    DB 'Close', 0
m_str_save:     DB 'Save', 0
m_str_next:     DB 'Next Doc', 0
m_str_prev:     DB 'Prev Doc', 0
m_str_quit:     DB 'Quit', 0
m_str_acc_cn:   DB 'Ctrl+N', 0
m_str_acc_cs:   DB 'Ctrl+S', 0
m_str_acc_cw:   DB 'Ctrl+W', 0
m_str_acc_ct:   DB 'Ctrl+Tab', 0
m_str_acc_cpu:  DB 'Ctrl+PgUp', 0
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

; ============================================================
; Menu Data Structures
; ============================================================

m_file_entries:
    ; New
    DW m_str_new, menu_new_handler
    DB 0, 'N'                       ; hotkey index 0 = 'N'
    DW m_str_acc_cn                 ; "Ctrl+N"
    DB 0, 0                         ; accelkey=0 (handled by editor, not menu)

    ; Open...
    DW m_str_open, menu_open_handler
    DB 0, 'O'
    DW 0
    DB 0, 0

    ; Close
    DW m_str_close, menu_close_handler
    DB 0, 'C'
    DW m_str_acc_cw                 ; "Ctrl+W"
    DB 0, 0

    ; Save
    DW m_str_save, menu_save_handler
    DB 0, 'S'
    DW m_str_acc_cs
    DB 0, 0

    ; Next Doc
    DW m_str_next, menu_next_handler
    DB 2, 'x'                       ; hotkey 'x' at index 2 in "Next Doc"
    DW m_str_acc_ct                 ; "Ctrl+Tab"
    DB 0, 0

    ; Prev Doc
    DW m_str_prev, menu_prev_handler
    DB 0, 'P'
    DW m_str_acc_cpu                ; "Ctrl+PgUp"
    DB 0, 0

    ; Quit
    DW m_str_quit, menu_quit_handler
    DB 0, 'Q'
    DW m_str_acc_aq
    DB 10h, 0                       ; accelkey=10h (Alt+Q global dispatch)

m_info_entries:
    DW m_str_about, menu_about_handler
    DB 0, 'A'
    DW 0
    DB 0, 0

m_menu_items:
    DW m_str_file
    DB 0, 6                         ; MI_X=0, MI_W=6
    DW m_file_entries
    DB 7, 22                        ; MI_ECOUNT=7, MI_DDW=22
    DB 0, 21h                       ; MI_HOTIDX=0, MI_ALTKEY=21h (Alt+F)

    DW m_str_info
    DB 6, 6
    DW m_info_entries
    DB 1, 12
    DB 0, 17h

m_menubar:
    DB 2, 0FFh, 0, 0FFh
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

menu_new_handler:
    CALL    doc_new
    ; CF result ignored — doc_new shows nothing (no TUI dialog).
    ; If it failed, user is still on their current document.
    ; Show error dialog here since TUI is active.
    JNC     .new_done
    PUSH    SI
    PUSH    DI
    MOV     SI, m_str_err_mem
    MOV     DI, m_str_error
    CALL    tui_dlg_msgbox
    POP     DI
    POP     SI
.new_done:
    RET

menu_close_handler:
    CALL    doc_close
    RET

menu_next_handler:
    CALL    doc_switch_next
    RET

menu_prev_handler:
    CALL    doc_switch_prev
    RET

menu_quit_handler:
    CALL    doc_quit_all
    ; doc_quit_all sets FW_RUNNING=0 on success,
    ; or returns with FW_DIRTY=1 if user cancelled.
    RET

menu_open_handler:
    PUSH    SI
    PUSH    DI
    PUSH    BX
    PUSH    CX

    ; ---- Show file selector dialog ----
    ; No dirty-save prompt needed — current doc stays open.
    MOV     DI, m_str_open_ttl
    MOV     BX, open_file_buf
    MOV     CL, 13
    CALL    tui_dlg_file
    CMP     AL, DLG_OK
    JNE     .open_cancel

    ; ---- Open selected file in a new document slot ----
    ; Point doc_fname_src to the dialog result buffer.
    ; doc_open will save the current context first, then copy
    ; the filename from open_file_buf into [filename].
    MOV     WORD [doc_fname_src], open_file_buf
    CALL    doc_open
    JNC     .open_cancel           ; success — done

    ; ---- Open failed — show error ----
    PUSH    SI
    PUSH    DI
    MOV     SI, m_str_err_open
    MOV     DI, m_str_error
    CALL    tui_dlg_msgbox
    POP     DI
    POP     SI

.open_cancel:
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
    MOV     BYTE [fw_state + FW_DIRTY], 1

.save_done:
    POP     CX
    POP     BX
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

    ; ---- Initialize multi-doc state ----
    MOV     BYTE [doc_count], 0
    MOV     BYTE [doc_active], 0

    ; ---- Parse command line ----
    CALL    parse_args
    JC      .no_file

    ; ---- Open file into first document slot ----
    ; parse_args already set [filename]; point doc_fname_src to it
    MOV     WORD [doc_fname_src], filename
    CALL    doc_open
    JC      .err_nofile
    JMP     .start_tui

.no_file:
    ; No filename given — create empty document
    CALL    doc_new
    JC      .err_nomem

.start_tui:
    ; ---- Initialize TUI ----
    ; doc_new / doc_open already initialized all editor state
    ; (cursor, scroll, dirty flag, piece table, etc.)
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
doc_fname_src:  RESW 1          ; pointer to filename source for doc_open

; ============================================================
; MULTI-DOC MANAGEMENT — Phase 2+ will use these.
; Zeroed by RESB/RESW. Unused in Phase 1.
; ============================================================

doc_count:      RESB 1          ; number of open documents (0–MAX_DOCS)
doc_active:     RESB 1          ; index of active document (0-based)
doc_segs:       RESW MAX_DOCS   ; save-buffer segment for each doc slot

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
