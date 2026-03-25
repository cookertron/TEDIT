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
s_usage:     DB 'Usage: TEDIT <filename>', 0Dh, 0Ah, '$'
s_nofile:    DB 'Error: cannot open file', 0Dh, 0Ah, '$'
s_nomem:     DB 'Error: out of memory', 0Dh, 0Ah, '$'

; ---- Status bar strings (null-terminated, for shadow_buf writing) ----
s_stat_ln:   DB ' Line ', 0
s_stat_of:   DB ' of ', 0
s_stat_col:  DB '  Col ', 0
s_stat_mod:  DB '  [Modified]', 0
s_stat_sel:  DB '  [SEL]', 0
s_stat_ci:   DB '  [Aa]', 0
s_stat_ovr:  DB '  [OVR]', 0
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
; Editor Find / Search Engine
; ============================================================

INCLUDE ed_find.inc

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
m_str_saveas:   DB 'Save As...', 0
m_str_quit:     DB 'Quit', 0
m_str_acc_cs:   DB 'Ctrl+S', 0
m_str_acc_aq:   DB 'Alt+Q', 0
m_str_info:      DB 'Info', 0
m_str_about:     DB 'About...', 0
m_str_about_1:   DB 'TEDIT v0.53', 0
m_str_about_2:   DB 'Directed by Cookertron', 0
m_str_about_3:   DB 'Coded by Claude Opus 4.6', 0
m_str_about_4:   DB 'Thanks to the VOGONS community', 0
m_str_about_5:   DB 'github: cookertron/TEDIT', 0
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

; ---- Find dialog strings ----
s_find_title:        DB 'Find', 0
s_find_title_ci:     DB 'Find [Aa]', 0
s_find_title_sel:    DB 'Find in Selection', 0
s_find_title_sel_ci: DB 'Find in Selection [Aa]', 0
s_find_prompt:   DB 'Find:', 0
s_find_notfound: DB 'Search term not found.', 0
s_find_noterm:   DB 'No search term.', 0
s_find_matchcase: DB 'Match case', 0
s_find_insel:    DB 'In selection', 0

; ---- Replace dialog strings ----
s_repl_title:    DB 'Replace', 0
s_repl_prompt:   DB 'Replace with:', 0
s_repl_confirm:  DB 'Replace this match?', 0
s_repl_done_sfx: DB ' replacements made.', 0
s_repl_all:      DB 'Replace all', 0
s_repl_perm:     DB 'Cannot undo. Save and replace all?', 0
s_repl_perm_ttl: DB 'Warning', 0

; ---- Goto dialog strings ----
s_goto_title:    DB 'Go to Line', 0
s_goto_prompt:   DB 'Line number:', 0
s_goto_invalid:  DB 'Invalid line number.', 0
s_goto_range:    DB 'Line number out of range.', 0

; ---- Word Wrap dialog strings ----
s_wrap_confirm:  DB 'Save and wrap to 80 cols? Cannot undo.', 0

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
m_str_datetime:  DB 'Date/Time', 0
m_str_wordwrap:  DB 'Word Wrap', 0
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

    ; Save As...
    DW m_str_saveas, menu_saveas_handler
    DB 5, 'A'
    DW 0
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
    DW m_str_find, menu_find_handler
    DB 0, 'F'
    DW m_str_acc_cf
    DB 0, 0

    ; Find Next — hotidx=5, hotkey='N'
    DW m_str_findnxt, menu_findnxt_handler
    DB 5, 'N'
    DW m_str_acc_f3
    DB 0, 0

    ; Find Prev — hotidx=8, hotkey='V'
    DW m_str_findprv, menu_findprv_handler
    DB 8, 'V'
    DW m_str_acc_sf3
    DB 0, 0

    ; Replace... — hotidx=3, hotkey='L'
    DW m_str_replace, menu_replace_handler
    DB 3, 'L'
    DW m_str_acc_ch
    DB 0, 0

    ; Goto... — hotidx=0, hotkey='G'
    DW m_str_goto, menu_goto_handler
    DB 0, 'G'
    DW m_str_acc_cg
    DB 0, 0

    ; Date/Time — hotidx=0, hotkey='D'
    DW m_str_datetime, menu_datetime_handler
    DB 0, 'D'
    DW m_str_acc_f5
    DB 0, 0

    ; Word Wrap — hotidx=0, hotkey='W'
    DW m_str_wordwrap, menu_wordwrap_handler
    DB 0, 'W'
    DW 0
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
    DB 4, 18                        ; MI_ECOUNT=4, MI_DDW=18
    DB 0, 21h                       ; MI_HOTIDX=0, MI_ALTKEY=21h (Alt+F)

    ; Edit menu (NEW)
    DW m_str_edit
    DB 6, 6                         ; MI_X=6, MI_W=6
    DW m_edit_entries
    DB 12, 24                       ; MI_ECOUNT=12, MI_DDW=24
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

; ============================================================
; menu_saveas_handler — Save As with file browser + text field
; ============================================================
menu_saveas_handler:
    PUSH    SI
    PUSH    DI
    PUSH    BX
    PUSH    CX

    ; Pre-fill open_file_buf with current filename basename (if any)
    MOV     BYTE [open_file_buf], 0
    CMP     BYTE [filename], 0
    JE      .sa_show

    ; Extract basename from filename (after last '\')
    MOV     SI, filename
    MOV     BX, SI
.sa_find_bs:
    CMP     BYTE [SI], 0
    JZ      .sa_copy_base
    CMP     BYTE [SI], '\'
    JNE     .sa_next_bs
    MOV     BX, SI
    INC     BX
.sa_next_bs:
    INC     SI
    JMP     .sa_find_bs
.sa_copy_base:
    MOV     SI, BX
    MOV     DI, open_file_buf
.sa_cpb:
    LODSB
    MOV     [DI], AL
    INC     DI
    TEST    AL, AL
    JNZ     .sa_cpb

.sa_show:
    ; Show file dialog (now includes textbox for filename editing)
    MOV     DI, m_str_save_ttl          ; "Save As"
    MOV     BX, open_file_buf
    MOV     CL, 13
    CALL    tui_dlg_file
    CMP     AL, DLG_OK
    JNE     .sa_done

    ; Copy result to filename buffer
    MOV     SI, open_file_buf
    MOV     DI, filename
.sa_copy:
    LODSB
    MOV     [DI], AL
    INC     DI
    TEST    AL, AL
    JNZ     .sa_copy

    ; Save to the new filename
    CALL    save_file
    MOV     AX, [undo_pos]
    MOV     [undo_save_pos], AX
    MOV     BYTE [fw_state + FW_DIRTY], 1

.sa_done:
    POP     CX
    POP     BX
    POP     DI
    POP     SI
    RET

; ============================================================
; menu_wordwrap_handler — Hard word wrap to 80 columns
; ============================================================
menu_wordwrap_handler:
    PUSH    SI
    PUSH    DI
    PUSH    BX
    PUSH    CX

    ; ---- Ensure file has a name ----
    CMP     BYTE [filename], 0
    JNE     .ww_have_name
    CALL    menu_save_handler
    CMP     BYTE [dirty], 0
    JNE     .ww_done
.ww_have_name:

    ; ---- Confirm dialog ----
    MOV     SI, s_wrap_confirm
    MOV     DI, s_repl_perm_ttl         ; "Warning"
    CALL    tui_dlg_confirm
    CMP     AL, DLG_YES
    JNE     .ww_done

    ; ---- Stream piece table to file with wrapping applied ----
    CALL    _wrap_save_file
    JC      .ww_fail

    ; ---- Reload from disk ----
    CALL    ed_compact
    CALL    sel_clear
    MOV     BYTE [fw_state + FW_DIRTY], 1
    JMP     .ww_done

.ww_fail:
    MOV     SI, s_err_save_fail
    MOV     DI, m_str_error
    CALL    tui_dlg_msgbox

.ww_done:
    POP     CX
    POP     BX
    POP     DI
    POP     SI
    RET

; ============================================================
; _wrap_save_file — Stream piece table to file with word wrap
;
; Walks byte-by-byte, tracking column position. When a line
; exceeds 80 columns, breaks at the last space (word boundary).
; Uses rd_save_buf as a line accumulator (~1020 bytes).
; Output buffered through add_seg (16KB flushes).
;
; Input:  [filename], [tab_width], [tab_mask]
; Output: CF=0 success, CF=1 failure.
; Clobbers: AX, BX, CX, DX, SI, DI, BP, ES
; ============================================================

_wrap_save_file PROC
    ; ---- Create/truncate output file ----
    MOV     DX, filename
    XOR     CX, CX
    MOV     AH, 3Ch
    INT     21h
    JC      .wf_fail
    MOV     [file_hnd], AX

    ; ---- Init output buffer position (stored in stl_col) ----
    MOV     WORD [stl_col], 0           ; output write position in add_seg

    ; ---- Init line state ----
    MOV     WORD [rnd_base], 0          ; line_len = 0
    MOV     WORD [rnd_len], 0           ; line_col = 0
    MOV     WORD [rnd_seg], 0FFFFh      ; last_space = -1

    ; ---- Walk pieces ----
    XOR     BP, BP
.wf_piece_loop:
    CMP     BP, [pt_count]
    JAE     .wf_done_pieces

    MOV     BX, BP
    CALL    resolve_piece               ; AX=seg, SI=base, DX=piece_len
    JC      .wf_done_pieces

    PUSH    DS
    MOV     DS, AX
    ADD     DX, SI                      ; DX = piece end

.wf_byte:
    CMP     SI, DX
    JAE     .wf_next_piece
    LODSB

    CMP     AL, 0Dh
    JE      .wf_byte                    ; skip CR

    CMP     AL, 0Ah
    JE      .wf_newline

    ; ---- Accumulate byte into line buffer ----
    MOV     BX, CS:[rnd_base]
    CMP     BX, 1016
    JAE     .wf_force_break
    MOV     CS:[BX + rd_save_buf], AL
    INC     BX
    MOV     CS:[rnd_base], BX

    CMP     AL, ' '
    JE      .wf_is_space
    CMP     AL, 09h
    JE      .wf_is_tab
    INC     WORD CS:[rnd_len]
    JMP     .wf_check_wrap

.wf_is_space:
    DEC     BX
    MOV     CS:[rnd_seg], BX            ; last_space = index of this space
    INC     WORD CS:[rnd_len]
    JMP     .wf_check_wrap

.wf_is_tab:
    PUSH    AX
    PUSH    CX
    MOV     AX, CS:[rnd_len]
    AND     AX, CS:[tab_mask]
    MOV     CX, CS:[tab_width]
    SUB     CX, AX
    ADD     CS:[rnd_len], CX
    POP     CX
    POP     AX
    JMP     .wf_check_wrap

.wf_check_wrap:
    CMP     WORD CS:[rnd_len], 80
    JB      .wf_byte

    ; ---- Wrap needed ----
    CMP     WORD CS:[rnd_seg], 0FFFFh
    JE      .wf_force_break

    ; ---- Word break: flush [0..last_space-1], CRLF, keep remainder ----
    PUSH    SI
    PUSH    DX
    PUSH    DS
    PUSH    CS
    POP     DS

    ; Write bytes before the space
    MOV     CX, [rnd_seg]
    MOV     SI, rd_save_buf
    JCXZ    .wf_brk_no_pre
.wf_brk_pre:
    LODSB
    CALL    .wf_write_byte
    LOOP    .wf_brk_pre
.wf_brk_no_pre:
    CALL    .wf_write_crlf

    ; Compute remaining: line_len - space_idx - 1
    MOV     CX, [rnd_base]
    SUB     CX, [rnd_seg]
    DEC     CX                          ; CX = remaining bytes after space

    ; Move remaining to start of rd_save_buf
    MOV     SI, rd_save_buf
    ADD     SI, [rnd_seg]
    INC     SI                          ; skip the space byte
    MOV     DI, rd_save_buf
    PUSH    ES
    PUSH    CS
    POP     ES
    PUSH    CX
    JCXZ    .wf_brk_no_move
    REP     MOVSB
.wf_brk_no_move:
    POP     CX
    POP     ES

    ; Update line state
    MOV     [rnd_base], CX              ; new line_len = remaining
    MOV     WORD [rnd_seg], 0FFFFh      ; reset last_space

    ; Recompute visual column for remaining bytes
    ; (scan rd_save_buf[0..CX-1] counting columns)
    PUSH    BX
    XOR     BX, BX                      ; BX = new visual col
    MOV     SI, rd_save_buf
    PUSH    CX
    JCXZ    .wf_brk_recol_done
.wf_brk_recol:
    MOV     AL, [SI]
    INC     SI
    CMP     AL, 09h
    JE      .wf_brk_recol_tab
    CMP     AL, ' '
    JE      .wf_brk_recol_sp
    INC     BX
    LOOP    .wf_brk_recol
    JMP     .wf_brk_recol_done
.wf_brk_recol_sp:
    ; Re-record this space as potential break point
    PUSH    AX
    MOV     AX, SI
    SUB     AX, rd_save_buf
    DEC     AX
    MOV     [rnd_seg], AX
    POP     AX
    INC     BX
    LOOP    .wf_brk_recol
    JMP     .wf_brk_recol_done
.wf_brk_recol_tab:
    PUSH    AX
    MOV     AX, BX
    AND     AX, [tab_mask]
    PUSH    CX
    MOV     CX, [tab_width]
    SUB     CX, AX
    ADD     BX, CX
    POP     CX
    POP     AX
    LOOP    .wf_brk_recol
.wf_brk_recol_done:
    POP     CX
    MOV     [rnd_len], BX               ; visual col for remaining
    POP     BX

    POP     DS
    POP     DX
    POP     SI
    JMP     .wf_byte

.wf_force_break:
    ; Hard break: flush entire line buffer + CRLF
    CALL    .wf_flush_line
    CALL    .wf_write_crlf
    JMP     .wf_byte

.wf_newline:
    ; Explicit newline in source: flush line + CRLF
    CALL    .wf_flush_line
    CALL    .wf_write_crlf
    JMP     .wf_byte

.wf_next_piece:
    POP     DS
    INC     BP
    JMP     .wf_piece_loop

.wf_done_pieces:
    ; Flush any remaining line content (last line without trailing LF)
    CALL    .wf_flush_line

    ; Flush output buffer
    MOV     CX, [stl_col]
    JCXZ    .wf_close
    PUSH    DS
    MOV     DS, [add_seg]
    XOR     DX, DX
    MOV     BX, CS:[file_hnd]
    MOV     AH, 40h
    INT     21h
    POP     DS

.wf_close:
    MOV     BX, [file_hnd]
    MOV     AH, 3Eh
    INT     21h
    MOV     BYTE [dirty], 0
    CLC
    RET

.wf_fail:
    STC
    RET

    ; ---- Helper: flush line buffer (rd_save_buf[0..rnd_base-1]) to output ----
.wf_flush_line:
    PUSH    SI
    PUSH    CX
    PUSH    DS
    PUSH    CS
    POP     DS
    MOV     CX, [rnd_base]
    MOV     SI, rd_save_buf
    JCXZ    .wf_fl_done
.wf_fl_loop:
    LODSB
    CALL    .wf_write_byte
    LOOP    .wf_fl_loop
.wf_fl_done:
    MOV     WORD [rnd_base], 0
    MOV     WORD [rnd_len], 0
    MOV     WORD [rnd_seg], 0FFFFh
    POP     DS
    POP     CX
    POP     SI
    RET

    ; ---- Helper: write CRLF to output ----
.wf_write_crlf:
    PUSH    AX
    MOV     AL, 0Dh
    CALL    .wf_write_byte
    MOV     AL, 0Ah
    CALL    .wf_write_byte
    POP     AX
    RET

    ; ---- Helper: write AL to output buffer, flush at 16KB ----
.wf_write_byte:
    PUSH    ES
    PUSH    DI
    MOV     ES, CS:[add_seg]
    MOV     DI, CS:[stl_col]
    STOSB
    MOV     CS:[stl_col], DI
    CMP     DI, 16384
    JB      .wf_wb_done
    ; Flush
    PUSH    AX
    PUSH    BX
    PUSH    CX
    PUSH    DX
    PUSH    DS
    MOV     DS, CS:[add_seg]
    XOR     DX, DX
    MOV     CX, DI
    MOV     BX, CS:[file_hnd]
    MOV     AH, 40h
    INT     21h
    POP     DS
    MOV     WORD CS:[stl_col], 0
    POP     DX
    POP     CX
    POP     BX
    POP     AX
.wf_wb_done:
    POP     DI
    POP     ES
    RET

_wrap_save_file ENDP
    PUSH    SI
    PUSH    DI
    MOV     SI, m_str_not_impl
    MOV     DI, m_str_edit
    CALL    tui_dlg_msgbox
    POP     DI
    POP     SI
    RET

; ============================================================
; menu_goto_handler — Go to line number
; ============================================================
menu_goto_handler:
    PUSH    SI
    PUSH    DI
    PUSH    BX
    PUSH    CX

    ; Clear input buffer
    MOV     BYTE [goto_buf], 0

    ; Show input dialog
    MOV     SI, s_goto_prompt
    MOV     DI, s_goto_title
    MOV     BX, goto_buf
    MOV     CL, 5                       ; max 5 digits
    CALL    tui_dlg_input
    CMP     AL, DLG_OK
    JNE     .gt_done

    ; ---- Parse decimal number from goto_buf ----
    MOV     SI, goto_buf
    XOR     AX, AX                      ; AX = accumulated number
    XOR     CX, CX                      ; CX = digit count

.gt_parse:
    MOV     BL, [SI]
    TEST    BL, BL
    JZ      .gt_parse_done
    CMP     BL, '0'
    JB      .gt_invalid
    CMP     BL, '9'
    JA      .gt_invalid
    ; AX = AX * 10 + digit
    MOV     DX, 10
    MUL     DX                          ; DX:AX = AX * 10
    TEST    DX, DX
    JNZ     .gt_range                   ; overflow (> 65535)
    SUB     BL, '0'
    XOR     BH, BH
    ADD     AX, BX
    JC      .gt_range                   ; overflow
    INC     CX
    INC     SI
    JMP     .gt_parse

.gt_parse_done:
    ; Must have at least one digit
    TEST    CX, CX
    JZ      .gt_invalid

    ; AX = line number (1-based). Validate range.
    TEST    AX, AX
    JZ      .gt_range                   ; 0 is invalid (1-based)
    CMP     AX, [total_lines]
    JA      .gt_range                   ; > total_lines

    ; ---- Go to line (convert 1-based to 0-based) ----
    DEC     AX
    MOV     [cur_line], AX
    MOV     WORD [cur_col], 0
    MOV     WORD [left_col], 0
    CALL    sel_clear
    CALL    scroll_to_cursor
    MOV     BYTE [fw_state + FW_DIRTY], 1
    MOV     WORD [last_ins_pi], 0FFFFh
    MOV     BYTE [undo_group_active], 0
    JMP     .gt_done

.gt_invalid:
    MOV     SI, s_goto_invalid
    MOV     DI, s_goto_title
    CALL    tui_dlg_msgbox
    JMP     .gt_done

.gt_range:
    MOV     SI, s_goto_range
    MOV     DI, s_goto_title
    CALL    tui_dlg_msgbox

.gt_done:
    POP     CX
    POP     BX
    POP     DI
    POP     SI
    RET

; ============================================================
; menu_datetime_handler — Insert date/time at cursor
; Format: YYYY-MM-DD HH:MM:SS
; ============================================================
menu_datetime_handler:
    PUSH    SI
    PUSH    DI
    PUSH    BX
    PUSH    CX

    ; Break undo group (typed chars should not merge with datetime)
    MOV     WORD [last_ins_pi], 0FFFFh
    MOV     BYTE [undo_group_active], 0

    ; ---- Get DOS date: CX=year, DH=month, DL=day ----
    MOV     AH, 2Ah
    INT     21h
    ; Build "YYYY-MM-DD " into goto_buf (reuse as scratch)
    MOV     DI, goto_buf            ; actually need more space, use repl_msg_buf
    ; Actually repl_msg_buf is 26 bytes — enough for "YYYY-MM-DD HH:MM:SS" (19 chars)
    MOV     DI, repl_msg_buf
    PUSH    DX                      ; save month/day
    MOV     AX, CX                  ; AX = year
    CALL    .dt_put4
    MOV     BYTE [DI], '-'
    INC     DI
    POP     DX
    MOV     AL, DH                  ; month
    CALL    .dt_put2
    MOV     BYTE [DI], '-'
    INC     DI
    MOV     AL, DL                  ; day
    CALL    .dt_put2
    MOV     BYTE [DI], ' '
    INC     DI

    ; ---- Get DOS time: CH=hour, CL=minute, DH=second ----
    MOV     AH, 2Ch
    INT     21h
    MOV     AL, CH                  ; hour
    CALL    .dt_put2
    MOV     BYTE [DI], ':'
    INC     DI
    MOV     AL, CL                  ; minute
    CALL    .dt_put2
    MOV     BYTE [DI], ':'
    INC     DI
    MOV     AL, DH                  ; second
    CALL    .dt_put2
    MOV     BYTE [DI], 0           ; null-terminate

    ; ---- Insert each character at cursor ----
    MOV     SI, repl_msg_buf
.dt_insert:
    LODSB
    TEST    AL, AL
    JZ      .dt_done
    ; Save pre-cursor for undo
    PUSH    AX
    MOV     AX, [cur_line]
    MOV     [undo_pre_line], AX
    MOV     AX, [cur_col]
    MOV     [undo_pre_col], AX
    POP     AX
    CALL    insert_char

    ; Record undo
    PUSH    AX
    PUSH    SI
    MOV     AH, AL                  ; AH = char
    MOV     AL, UR_INS_CHAR
    CMP     BYTE [undo_group_active], 0
    JE      .dt_no_group
    OR      AL, URF_GROUP_CONT
.dt_no_group:
    MOV     BX, [undo_pre_line]
    MOV     CX, [undo_pre_col]
    MOV     DX, [undo_pre_line]
    MOV     SI, [undo_pre_col]
    CALL    undo_push
    POP     SI
    POP     AX

    ; Update group tracking
    MOV     BYTE [undo_group_active], 1
    PUSH    AX
    MOV     AX, [undo_pre_line]
    MOV     [undo_group_line], AX
    MOV     AX, [undo_pre_col]
    MOV     [undo_group_col], AX
    POP     AX
    JMP     .dt_insert

.dt_done:
    MOV     BYTE [fw_state + FW_DIRTY], 1
    POP     CX
    POP     BX
    POP     DI
    POP     SI
    RET

    ; ---- Helpers: write 2-digit and 4-digit decimal to [DI] ----
.dt_put2:
    ; AL = value (0-99), writes 2 ASCII digits to [DI], advances DI by 2
    XOR     AH, AH
    MOV     BL, 10
    DIV     BL                      ; AL = tens, AH = ones
    ADD     AL, '0'
    MOV     [DI], AL
    INC     DI
    MOV     AL, AH
    ADD     AL, '0'
    MOV     [DI], AL
    INC     DI
    RET

.dt_put4:
    ; AX = value (0-9999), writes 4 ASCII digits to [DI], advances DI by 4
    PUSH    CX
    MOV     CX, 1000
    XOR     DX, DX
    DIV     CX                      ; AX = thousands, DX = remainder
    ADD     AL, '0'
    MOV     [DI], AL
    INC     DI
    MOV     AX, DX
    MOV     CL, 100
    XOR     AH, AH
    DIV     CL                      ; AL = hundreds, AH = remainder
    ADD     AL, '0'
    MOV     [DI], AL
    INC     DI
    MOV     AL, AH
    MOV     BL, 10
    XOR     AH, AH
    DIV     BL                      ; AL = tens, AH = ones
    ADD     AL, '0'
    MOV     [DI], AL
    INC     DI
    MOV     AL, AH
    ADD     AL, '0'
    MOV     [DI], AL
    INC     DI
    POP     CX
    RET

menu_about_handler:
    PushAll

    ; Interior: 34 wide, 9 rows (5 lines + 1 blank + 1 blank + 1 button + 1 pad)
    ; WIN_W = 36, WIN_H = 11

    ; ---- Line 1: "TEDIT v0.53" (centered label) ----
    MOV     DI, dlg_label
    MOV     BYTE [DI + CTRL_TYPE], CTYPE_LABEL
    MOV     BYTE [DI + CTRL_FLAGS], CTRLF_VISIBLE
    MOV     BYTE [DI + CTRL_X], 1
    MOV     BYTE [DI + CTRL_Y], 1
    MOV     BYTE [DI + CTRL_W], 32
    MOV     BYTE [DI + CTRL_H], 1
    MOV     WORD [DI + CTRL_NEXT], dlg_label2
    MOV     WORD [DI + CTRL_TEXT], m_str_about_1
    MOV     BYTE [DI + CTRL_ATTR], CLR_WIN_BG
    MOV     BYTE [DI + CTRL_FATTR], CLR_WIN_BG
    MOV     WORD [DI + CTRL_HANDLER], 0

    ; ---- Line 2: "Directed by Cookertron" ----
    MOV     DI, dlg_label2
    MOV     BYTE [DI + CTRL_TYPE], CTYPE_LABEL
    MOV     BYTE [DI + CTRL_FLAGS], CTRLF_VISIBLE
    MOV     BYTE [DI + CTRL_X], 1
    MOV     BYTE [DI + CTRL_Y], 3
    MOV     BYTE [DI + CTRL_W], 32
    MOV     BYTE [DI + CTRL_H], 1
    MOV     WORD [DI + CTRL_NEXT], dlg_chk1
    MOV     WORD [DI + CTRL_TEXT], m_str_about_2
    MOV     BYTE [DI + CTRL_ATTR], CLR_WIN_BG
    MOV     BYTE [DI + CTRL_FATTR], CLR_WIN_BG
    MOV     WORD [DI + CTRL_HANDLER], 0

    ; ---- Line 3: "Coded by Claude Opus 4.6" (reuse dlg_chk1 as label) ----
    MOV     DI, dlg_chk1
    MOV     BYTE [DI + CTRL_TYPE], CTYPE_LABEL
    MOV     BYTE [DI + CTRL_FLAGS], CTRLF_VISIBLE
    MOV     BYTE [DI + CTRL_X], 1
    MOV     BYTE [DI + CTRL_Y], 4
    MOV     BYTE [DI + CTRL_W], 32
    MOV     BYTE [DI + CTRL_H], 1
    MOV     WORD [DI + CTRL_NEXT], dlg_chk2
    MOV     WORD [DI + CTRL_TEXT], m_str_about_3
    MOV     BYTE [DI + CTRL_ATTR], CLR_WIN_BG
    MOV     BYTE [DI + CTRL_FATTR], CLR_WIN_BG
    MOV     WORD [DI + CTRL_HANDLER], 0

    ; ---- Line 4: "Thanks to the VOGONS community" (reuse dlg_chk2 as label) ----
    MOV     DI, dlg_chk2
    MOV     BYTE [DI + CTRL_TYPE], CTYPE_LABEL
    MOV     BYTE [DI + CTRL_FLAGS], CTRLF_VISIBLE
    MOV     BYTE [DI + CTRL_X], 1
    MOV     BYTE [DI + CTRL_Y], 5
    MOV     BYTE [DI + CTRL_W], 32
    MOV     BYTE [DI + CTRL_H], 1
    MOV     WORD [DI + CTRL_NEXT], dlg_chk3
    MOV     WORD [DI + CTRL_TEXT], m_str_about_4
    MOV     BYTE [DI + CTRL_ATTR], CLR_WIN_BG
    MOV     BYTE [DI + CTRL_FATTR], CLR_WIN_BG
    MOV     WORD [DI + CTRL_HANDLER], 0

    ; ---- Line 5: "github: cookertron/TEDIT" (reuse dlg_chk3 as label) ----
    MOV     DI, dlg_chk3
    MOV     BYTE [DI + CTRL_TYPE], CTYPE_LABEL
    MOV     BYTE [DI + CTRL_FLAGS], CTRLF_VISIBLE
    MOV     BYTE [DI + CTRL_X], 1
    MOV     BYTE [DI + CTRL_Y], 7
    MOV     BYTE [DI + CTRL_W], 32
    MOV     BYTE [DI + CTRL_H], 1
    MOV     WORD [DI + CTRL_NEXT], dlg_btn1
    MOV     WORD [DI + CTRL_TEXT], m_str_about_5
    MOV     BYTE [DI + CTRL_ATTR], CLR_WIN_BG
    MOV     BYTE [DI + CTRL_FATTR], CLR_WIN_BG
    MOV     WORD [DI + CTRL_HANDLER], 0

    ; ---- OK button ----
    MOV     DI, dlg_btn1
    MOV     BYTE [DI + CTRL_TYPE], CTYPE_BUTTON
    MOV     BYTE [DI + CTRL_FLAGS], CTRLF_VISIBLE + CTRLF_ENABLED + CTRLF_FOCUSABLE
    MOV     BYTE [DI + CTRL_X], 13
    MOV     BYTE [DI + CTRL_Y], 9
    MOV     BYTE [DI + CTRL_W], DLG_BTN_OK_W
    MOV     BYTE [DI + CTRL_H], 1
    MOV     WORD [DI + CTRL_NEXT], 0
    MOV     WORD [DI + CTRL_TEXT], dlg_str_ok
    MOV     BYTE [DI + CTRL_ATTR], CLR_BTN
    MOV     BYTE [DI + CTRL_FATTR], CLR_BTN_FOCUS
    MOV     WORD [DI + CTRL_HANDLER], dlg_handler_ok

    ; ---- Window template ----
    MOV     DI, dlg_tmpl
    MOV     BYTE [DI + WIN_FLAGS], WINF_VISIBLE + WINF_BORDER + WINF_TITLE + WINF_SHADOW
    MOV     BYTE [DI + WIN_X], (SCR_W - 36) / 2
    MOV     BYTE [DI + WIN_Y], (SCR_H - 12) / 2
    MOV     BYTE [DI + WIN_W], 36
    MOV     BYTE [DI + WIN_H], 12
    MOV     BYTE [DI + WIN_ATTR], CLR_WIN_BG
    MOV     BYTE [DI + WIN_BATTR], CLR_WIN_BDR
    MOV     BYTE [DI + WIN_TATTR], CLR_WIN_TTL
    MOV     WORD [DI + WIN_TITLE], m_str_info
    MOV     WORD [DI + WIN_FIRST], dlg_label
    MOV     WORD [DI + WIN_FOCUS], dlg_btn1
    MOV     WORD [DI + WIN_HANDLER], 0
    MOV     BYTE [DI + WIN_ZORDER], 0
    MOV     BYTE [DI + WIN_ID], 0
    MOV     BYTE [DI + WIN_SCROLLX], 0
    MOV     BYTE [DI + WIN_SCROLLY], 0

    CALL    tui_dlg_run_modal

    PopAll
    RET

; ============================================================
; MAIN
; ============================================================

main:
    CALL    shrink_mem

    ; ---- Save startup working directory (restored on exit) ----
    MOV     AH, 19h
    INT     21h                         ; AL = current drive (0=A)
    MOV     [startup_drive], AL
    MOV     DL, AL
    INC     DL                          ; DL = drive (1=A, 2=B, ...)
    MOV     SI, startup_path
    MOV     AH, 47h
    INT     21h                         ; DS:SI filled with CWD (no leading \)

    ; ---- Initialize tab defaults (before parse_args may override) ----
    MOV     WORD [tab_width], ED_TAB_DEFAULT
    MOV     WORD [tab_mask], ED_TAB_DEFAULT - 1
    MOV     BYTE [filename], 0          ; ensure clean for parse_args flag check

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
    MOV     BYTE [overwrite_mode], 0

    ; ---- Initialize TUI ----
    CALL    tui_init

    ; ---- Disable blink attribute, enable bright backgrounds ----
    MOV     AX, 1003h
    XOR     BL, BL              ; BL=0: bright bg instead of blink
    INT     10h
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
    ; ---- Restore startup working directory ----
    MOV     DL, [startup_drive]
    MOV     AH, 0Eh
    INT     21h                         ; set drive
    ; Build "\path" from startup_path (AH=47h returns without leading \)
    MOV     BYTE [open_file_buf], '\'
    MOV     SI, startup_path
    MOV     DI, open_file_buf + 1
.exit_cp:
    LODSB
    MOV     [DI], AL
    INC     DI
    TEST    AL, AL
    JNZ     .exit_cp
    MOV     DX, open_file_buf
    MOV     AH, 3Bh
    INT     21h                         ; CHDIR

    ; ---- Restore 80x25 text mode (clears screen) ----
    MOV     AX, 0003h
    INT     10h
    ; ---- Cursor to (0,0) ----
    MOV     AH, 02h
    XOR     BH, BH
    XOR     DX, DX
    INT     10h
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
rnd_pi:         RESW 1          ; render/seek scratch
rnd_off:        RESW 1          ; render/seek scratch
rnd_base:       RESW 1          ; render/seek/save scratch
rnd_len:        RESW 1          ; render/save scratch
rnd_seg:        RESW 1          ; render scratch
rnd_row:        RESW 1          ; render scratch
stl_col:        RESW 1          ; seek/rebuild scratch
ed_abs_row:     RESB 1          ; render scratch
left_col:       RESW 1          ; horizontal scroll offset

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

; ---- Find state ----
find_buf:            RESB 65     ; null-terminated search term (max 64 + null)
find_len:            RESW 1      ; needle byte length
find_flags:          RESB 1      ; bit 0: case-insensitive (Phase 5)

; ---- Find result (set by find_forward) ----
find_match_line:     RESW 1      ; match start line
find_match_col:      RESW 1      ; match start column
find_match_end_line: RESW 1      ; match end line (exclusive)
find_match_end_col:  RESW 1      ; match end column (exclusive)

; ---- Find engine scratch (internal) ----
find_cur_line:       RESW 1      ; current line during piece walk
find_cur_col:        RESW 1      ; current col during piece walk
find_needle_pos:     RESW 1      ; bytes matched so far
find_start_line:     RESW 1      ; line where current partial match started
find_start_col:      RESW 1      ; col where current partial match started

; ---- Find-in-selection bounds (saved when entering selection-only mode) ----
find_sel_s_line:     RESW 1      ; selection scope start line
find_sel_s_col:      RESW 1      ; selection scope start col
find_sel_e_line:     RESW 1      ; selection scope end line
find_sel_e_col:      RESW 1      ; selection scope end col

; ---- Find backward scratch ----
find_prev_line:      RESW 1      ; last good match start line
find_prev_col:       RESW 1      ; last good match start col
find_prev_eline:     RESW 1      ; last good match end line
find_prev_ecol:      RESW 1      ; last good match end col

; ---- Replace state ----
replace_buf:        RESB 65     ; null-terminated replacement (max 64 + null)
replace_len:        RESW 1      ; replacement length (WORD)
replace_count:      RESW 1      ; replacements-made counter
repl_msg_buf:       RESB 26     ; scratch for "NNN replacements made.\0"

; ---- Tab settings ----
tab_width:          RESW 1      ; tab display width (2, 4, or 8)
tab_mask:           RESW 1      ; tab_width - 1 (bitmask for modulo)

; ---- Render scratch for tab support ----
rnd_byte_col:       RESW 1      ; byte column during render (for selection)
rnd_visual_col:     RESW 1      ; visual column during render (for tab stops)

; ---- Overwrite mode ----
overwrite_mode:     RESB 1      ; 0 = insert, 1 = overwrite

; ---- Goto ----
goto_buf:           RESB 6      ; line number input (max 5 digits + null)

; ---- Startup CWD (restored on exit) ----
startup_drive:      RESB 1      ; drive number (0=A, 1=B, 2=C...)
startup_path:       RESB 65     ; path returned by INT 21h AH=47h

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
dlg_btn3:           RESB 14
dlg_btn4:           RESB 14
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
dlg_chk1:           RESB 15         ; dialog checkbox 1 (match case)
dlg_chk2:           RESB 15         ; dialog checkbox 2 (in selection)
dlg_chk3:           RESB 15         ; dialog checkbox 3 (replace all)
dlg_listbox2:       RESB 19         ; directory listbox for file dialog
dlg_drive_dd:       RESB 21
dlg_drive_count:    RESB 1
dlg_drive_items:    RESB 52
dlg_drive_names:    RESB 78
dlg_dir_count:      RESB 1          ; directory entry count
dlg_dir_items:      RESB 64         ; 32 dir pointers (32 × 2)
dlg_dir_names:      RESB 448        ; 32 dir names (32 × 14)
