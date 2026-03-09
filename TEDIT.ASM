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

; ============================================================
; Menu Data Structures
; ============================================================

m_file_entries:
    DW m_str_open, menu_open_handler
    DB 0, 'O'
    DW 0
    DB 0, 0

    DW m_str_save, menu_save_handler
    DB 0, 'S'
    DW m_str_acc_cs
    DB 0, 0

    DW m_str_quit, menu_quit_handler
    DB 0, 'Q'
    DW m_str_acc_aq
    DB 10h, 0

m_info_entries:
    DW m_str_about, menu_about_handler
    DB 0, 'A'
    DW 0
    DB 0, 0

m_menu_items:
    DW m_str_file
    DB 0, 6
    DW m_file_entries
    DB 3, 20
    DB 0, 21h

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

menu_quit_handler:
    ; ---- If not dirty, just quit ----
    CMP     BYTE [dirty], 0
    JE      .quit_now

    ; ---- Dirty — ask user ----
    PUSH    SI
    PUSH    DI
    MOV     SI, m_str_quit_ask
    MOV     DI, m_str_quit_ttl
    CALL    tui_dlg_confirm
    POP     DI
    POP     SI

    CMP     AL, DLG_YES
    JE      .quit_save
    JMP     .quit_now

.quit_save:
    ; Try to save first
    CMP     BYTE [filename], 0
    JE      .quit_save_as
    CALL    save_file
    JMP     .quit_now

.quit_save_as:
    ; No filename — need Save As dialog
    PUSH    SI
    PUSH    DI
    PUSH    BX
    PUSH    CX
    MOV     SI, m_str_save_pr
    MOV     DI, m_str_save_ttl
    MOV     BX, filename
    MOV     CL, 80
    CALL    tui_dlg_input
    POP     CX
    POP     BX
    POP     DI
    POP     SI

    CMP     AL, DLG_OK
    JNE     .quit_now
    CMP     BYTE [filename], 0
    JE      .quit_now
    CALL    save_file

.quit_now:
    MOV     BYTE [fw_state + FW_RUNNING], 0
    RET

menu_open_handler:
    PUSH    SI
    PUSH    DI
    PUSH    BX
    PUSH    CX

    ; ---- If document is dirty, prompt to save first ----
    CMP     BYTE [dirty], 0
    JE      .open_no_save

    MOV     SI, m_str_quit_ask
    MOV     DI, m_str_open_ttl
    CALL    tui_dlg_confirm
    CMP     AL, DLG_YES
    JNE     .open_no_save

    ; User chose Yes — save current file first
    CMP     BYTE [filename], 0
    JE      .open_no_save
    CALL    save_file

.open_no_save:
    ; ---- Show file selector dialog ----
    MOV     DI, m_str_open_ttl
    MOV     BX, open_file_buf
    MOV     CL, 13
    CALL    tui_dlg_file
    CMP     AL, DLG_OK
    JNE     .open_cancel

    ; ---- User selected a file ----
    ; Copy filename from open_file_buf to filename
    PUSH    SI
    PUSH    DI
    MOV     SI, open_file_buf
    MOV     DI, filename
.copy_fname:
    MOV     AL, [SI]
    MOV     [DI], AL
    INC     SI
    INC     DI
    TEST    AL, AL
    JNZ     .copy_fname
    POP     DI
    POP     SI

    ; ---- Load the file ----
    CALL    ed_load_file

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

    ; ---- Parse command line ----
    CALL    parse_args
    JC      .no_file

    ; ---- Open and load file ----
    CALL    open_file
    JC      .err_nofile

    CALL    load_file
    JC      .err_nomem

    ; Close file handle (data is in memory now)
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
    ; No filename given — start with empty document
    CALL    ed_new_doc
    JC      .err_nomem
    JMP     .start_tui

.start_tui:
    ; ---- Initialize cursor / editor state ----
    MOV     WORD [top_line], 0
    MOV     WORD [cur_line], 0
    MOV     WORD [cur_col], 0
    MOV     BYTE [dirty], 0
    MOV     WORD [last_ins_pi], 0FFFFh

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

; ---- Editor state (ed_state struct: top_line then total_lines) ----
; These two MUST be adjacent — CTRL_ED_STATE points here
ed_state:
top_line:       RESW 1          ; ED_SCROLLY  (offset 0)
total_lines:    RESW 1          ; ED_LINECOUNT (offset 2)

; ---- Editor BSS ----
filename:       RESB 128
open_file_buf:  RESB 14
wrap_mode:      RESB 1
file_hnd:       RESW 1
orig_count:     RESW 1
orig_seg:       RESW MAX_ORIG   ; 2048 bytes
orig_len:       RESW MAX_ORIG   ; 2048 bytes
add_seg:        RESW 1
add_used:       RESW 1
pt_seg:         RESW 1
pt_count:       RESW 1
chkpt_num:      RESW 1
chkpt_pi:       RESW MAX_CHKPT  ; 4096 bytes
chkpt_po:       RESW MAX_CHKPT  ; 4096 bytes
cur_line:       RESW 1
cur_col:        RESW 1
dirty:          RESB 1
meta_dirty:     RESB 1
rnd_pi:         RESW 1
rnd_off:        RESW 1
rnd_base:       RESW 1
rnd_len:        RESW 1
rnd_seg:        RESW 1
rnd_row:        RESW 1
stl_col:        RESW 1
last_ins_pi:    RESW 1
ed_abs_row:     RESB 1

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
