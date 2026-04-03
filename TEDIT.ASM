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
s_nofile:    DB 'Cannot open file', 0Dh, 0Ah, '$'
s_nomem:     DB 'Out of memory', 0Dh, 0Ah, '$'
s_err_mda:   DB 'Requires 80-column text mode', 0Dh, 0Ah, '$'
s_err_mixed: DB 'Cannot mix .PRJ and documents', 0Dh, 0Ah, '$'

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
; Editor Multi-Document Support
; ============================================================

INCLUDE ed_multidoc.inc
INCLUDE ed_shell.inc

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
m_str_saveas:   DB 'Save As...', 0
m_str_closeall: DB 'Close All', 0
m_str_saveall:  DB 'Save All', 0
m_str_loadprj:  DB 'Load Project...', 0
m_str_saveprj:  DB 'Save Project...', 0
m_str_lprj_ttl: DB 'Load Project', 0
m_str_sprj_ttl: DB 'Save Project', 0
s_prj_pattern:  DB '*.PRJ', 0
s_prj_crlf:     DB 0Dh, 0Ah
s_err_prj_open: DB 'Cannot open project file.', 0
s_prj_truncated: DB 'Project exceeds 8 file limit.', 0
m_str_shell:    DB 'Shell to DOS', 0
s_shell_msg:    DB 'Type EXIT to return to TEDIT.', 0Dh, 0Ah, '$'
s_comspec_key:  DB 'COMSPEC='
S_COMSPEC_LEN   EQU 8
s_err_no_comspec: DB 'COMSPEC not found.', 0
s_shell_swap_name: DB 'TEDTSHEL.SWP', 0
s_swap_prompt:  DB 'Swap file detected. Restore[R] or Delete[D]? $'
s_err_shl_swap:  DB 'Shell swap failed: disk error. Shell aborted.', 0
s_err_shl_lost:  DB 'Swap file missing. Unsaved changes lost.', 0
s_err_shl_fatal: DB 0Dh, 0Ah, 'FATAL: Out of memory restoring editor.', 0Dh, 0Ah, '$'
m_str_quit:     DB 'Exit', 0
m_str_acc_cn:   DB 'Ctrl+N', 0
m_str_acc_co:   DB 'Ctrl+O', 0
m_str_acc_cw:   DB 'Ctrl+W', 0
m_str_acc_cs:   DB 'Ctrl+S', 0
m_str_acc_css:  DB 'Ctrl+Shift+S', 0
m_str_acc_caw:  DB 'Ctrl+Alt+W', 0
m_str_acc_cas:  DB 'Ctrl+Alt+S', 0
m_str_acc_ax:   DB 'Alt+X', 0
m_str_acc_f8:   DB 'F8', 0
m_str_view:      DB 'View', 0
m_str_doclist:   DB 'Document List', 0
m_str_acc_f1:    DB 'F1', 0
m_str_info:      DB 'Info', 0
m_str_about:     DB 'About...', 0
m_str_about_1:   DB 'TEDIT v0.68', 0
m_str_about_2:   DB 'Directed by Cookertron', 0
m_str_about_3:   DB 'Coded by Claude Opus 4.6', 0
m_str_about_4:   DB 'Thanks to the VOGONS community', 0
m_str_about_5:   DB 'github: cookertron/TEDIT', 0
m_str_open_ttl:  DB 'Open File', 0
m_str_save_ttl:  DB 'Save As', 0
m_str_save_pr:   DB 'Filename:', 0
m_str_overwrite: DB 'File exists. Overwrite?', 0
m_str_save_warn  EQU m_str_save_ttl
m_str_quit_ttl   EQU m_str_quit
m_str_quit_ask:  DB 'Save changes before closing?', 0
m_str_error:     DB 'Error', 0
m_str_err_open:  DB 'Cannot open file', 0
m_str_err_mem:   DB 'Out of memory', 0
m_str_buf_ttl:   DB 'Buffer Full', 0
m_str_buf_ask:   DB 'Save+reload? Undo cleared.', 0
m_str_buf_err:   DB 'Reload failed - file on disk', 0
m_str_clip_full: DB 'Selection too large.', 0

; ---- Error/warning strings for preventive fixes ----
s_err_save_fail:   DB 'Could not save file.', 0
s_err_save_write:  DB 'Write failed (disk full?).', 0
s_err_save_rename: DB 'Rename failed after save.', 0
s_temp_file:       DB 'TEDIT.$$$', 0
s_err_pt_full:     DB 'Table full. Save and reload.', 0
s_err_undo_lost:   DB 'Cannot undo: history too old.', 0
s_err_file_trunc:  DB 'File exceeds 32MB limit.', 0
s_err_copy_big:    DB 'Selection exceeds copy limit.', 0
s_err_paste_pt:    DB 'Paste failed: table full.', 0
s_err_redo_noop:   DB 'Redo not available for paste.', 0
s_err_swap_out:    DB 'Cannot swap document to disk.', 0
s_err_swap_in:     DB 'Cannot load target document.', 0
s_err_max_docs:    DB 'Maximum 20 documents open.', 0
s_quit_unsaved:    DB 'Discard unsaved changes?', 0

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
s_repl_perm:     DB 'Save and replace all?', 0
s_repl_perm_ttl: DB 'Warning', 0

; ---- Goto dialog strings ----
s_goto_title:    DB 'Go to Line', 0
s_goto_prompt:   DB 'Line number:', 0
s_goto_invalid:  DB 'Invalid line number.', 0
s_goto_range:    DB 'Line number out of range.', 0

; ---- Word Wrap dialog strings ----
s_wrap_confirm:  DB 'Save and wrap to 80 cols?', 0

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
    ; New
    DW m_str_new, menu_new_handler
    DB 0, 'N'
    DW m_str_acc_cn
    DB 0, 0

    ; Open...
    DW m_str_open, menu_open_handler
    DB 0, 'O'
    DW m_str_acc_co
    DB 0, 0

    ; Close
    DW m_str_close, menu_close_handler
    DB 0, 'C'
    DW m_str_acc_cw
    DB 0, 0

    ; Close All
    DW m_str_closeall, menu_closeall_handler
    DB 4, 'E'
    DW m_str_acc_caw
    DB 0, 0

    ; ---- divider ----
    DW 0, 0
    DB 0, 0
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
    DW m_str_acc_css
    DB 0, 0

    ; Save All
    DW m_str_saveall, menu_saveall_handler
    DB 6, 'L'
    DW m_str_acc_cas
    DB 0, 0

    ; ---- divider ----
    DW 0, 0
    DB 0, 0
    DW 0
    DB 0, 0

    ; Load Project...
    DW m_str_loadprj, menu_loadprj_handler
    DB 3, 'D'
    DW 0
    DB 0, 0

    ; Save Project...
    DW m_str_saveprj, menu_saveprj_handler
    DB 5, 'P'
    DW 0
    DB 0, 0

    ; ---- divider ----
    DW 0, 0
    DB 0, 0
    DW 0
    DB 0, 0

    ; Shell to DOS
    DW m_str_shell, menu_shell_handler
    DB 1, 'H'
    DW m_str_acc_f8
    DB 42h, 0                       ; accelkey=42h (F8 global dispatch)

    ; ---- divider ----
    DW 0, 0
    DB 0, 0
    DW 0
    DB 0, 0

    ; Exit
    DW m_str_quit, menu_quit_handler
    DB 1, 'X'
    DW m_str_acc_ax
    DB 2Dh, 0                       ; accelkey=2Dh (Alt+X global dispatch)

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

    ; ---- divider ----
    DW 0, 0
    DB 0, 0
    DW 0
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

    ; ---- divider ----
    DW 0, 0
    DB 0, 0
    DW 0
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

    ; ---- divider ----
    DW 0, 0
    DB 0, 0
    DW 0
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

m_view_entries:
    ; Document List
    DW m_str_doclist, menu_doclist_handler
    DB 0, 'D'
    DW m_str_acc_f1
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
    DB 15, 28                       ; MI_ECOUNT=15 (11+4 dividers), MI_DDW=28
    DB 0, 21h                       ; MI_HOTIDX=0, MI_ALTKEY=21h (Alt+F)

    ; Edit menu (NEW)
    DW m_str_edit
    DB 6, 6                         ; MI_X=6, MI_W=6
    DW m_edit_entries
    DB 15, 24                       ; MI_ECOUNT=15 (12+3 dividers), MI_DDW=24
    DB 0, 12h                       ; MI_HOTIDX=0 ('E'), MI_ALTKEY=12h (Alt+E)

    ; View menu
    DW m_str_view
    DB 12, 6                        ; MI_X=12, MI_W=6
    DW m_view_entries
    DB 1, 20                        ; MI_ECOUNT=1, MI_DDW=20
    DB 0, 2Fh                       ; MI_HOTIDX=0 ('V'), MI_ALTKEY=2Fh (Alt+V)

    DW m_str_info
    DB 18, 6                        ; MI_X=18, MI_W=6
    DW m_info_entries
    DB 1, 12
    DB 0, 17h

m_menubar:
    DB 4, 0FFh, 0, 0FFh
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

; ============================================================
; menu_quit_handler — Multi-document quit
; Prompts for active doc if dirty, warns about other dirty
; swapped docs, deletes all swap files on exit.
; ============================================================
menu_quit_handler:
    PushAll

    ; ---- Check active document dirty ----
    CMP     BYTE [dirty], 0
    JE      .quit_check_others

    MOV     SI, m_str_quit_ask
    MOV     DI, m_str_quit_ttl
    CALL    tui_dlg_confirm

    CMP     AL, DLG_YES
    JE      .quit_save
    CMP     AL, DLG_NO
    JE      .quit_check_others
    JMP     .quit_done              ; Cancel — abort quit

.quit_save:
    MOV     AL, [active_doc]
    CALL    doc_get_slot
    TEST    BYTE ES:[DI + DOC_FLAGS], DOCF_UNTITLED
    JNZ     .quit_save_as
    CALL    save_file
    MOV     AX, [undo_pos]
    MOV     [undo_save_pos], AX
    JMP     .quit_check_others

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

.quit_check_others:
    ; ---- Check for other dirty (swapped) documents ----
    MOV     ES, [doc_table_seg]
    XOR     DI, DI
    XOR     CX, CX                  ; CX = slot counter
    XOR     BL, BL                  ; BL = dirty-swapped count
.quit_walk:
    CMP     CL, MAX_DOCS
    JAE     .quit_walk_done
    CMP     CL, [active_doc]
    JE      .quit_walk_next
    TEST    BYTE ES:[DI + DOC_FLAGS], DOCF_OCCUPIED
    JZ      .quit_walk_next
    TEST    BYTE ES:[DI + DOC_FLAGS], DOCF_DIRTY
    JZ      .quit_walk_next
    INC     BL
.quit_walk_next:
    ADD     DI, DOC_SLOT_SIZE
    INC     CL
    JMP     .quit_walk

.quit_walk_done:
    TEST    BL, BL
    JZ      .quit_now

    ; ---- Warn about unsaved swapped documents ----
    MOV     SI, s_quit_unsaved
    MOV     DI, m_str_quit_ttl
    CALL    tui_dlg_confirm
    CMP     AL, DLG_YES
    JE      .quit_now
    JMP     .quit_done              ; No or Cancel = abort quit

.quit_now:
    ; ---- Delete all swap files ----
    MOV     ES, [doc_table_seg]
    XOR     DI, DI
    XOR     CX, CX
.quit_del:
    CMP     CL, MAX_DOCS
    JAE     .quit_del_done
    TEST    BYTE ES:[DI + DOC_FLAGS], DOCF_OCCUPIED
    JZ      .quit_del_next
    TEST    BYTE ES:[DI + DOC_FLAGS], DOCF_SWAPPED
    JZ      .quit_del_next
    PUSH    CX
    PUSH    DI
    MOV     DX, DI
    ADD     DX, DOC_SWAP_PATH
    PUSH    DS
    PUSH    ES
    POP     DS                      ; DS = doc_table_seg for INT 21h
    MOV     AH, 41h
    INT     21h                     ; delete swap file, ignore error
    POP     DS
    POP     DI
    POP     CX
.quit_del_next:
    ADD     DI, DOC_SLOT_SIZE
    INC     CL
    JMP     .quit_del
.quit_del_done:

    MOV     BYTE [fw_state + FW_RUNNING], 0

.quit_done:
    PopAll
    RET

; ============================================================
; menu_new_handler — Create a new untitled document in a new slot
; ============================================================
menu_new_handler:
    PushAll

    ; ---- Find a free slot ----
    CALL    doc_find_free_slot
    JNC     .new_have_slot

    MOV     SI, s_err_max_docs
    CALL    show_error_msg
    JMP     .new_done

.new_have_slot:
    PUSH    AX                      ; save free slot index

    ; ---- Swap out active document ----
    CALL    doc_swap_out
    JC      .new_swap_fail
    ; doc_swap_out CHDIRs to startup dir — restore to doc_cwd
    ; New untitled doc inherits the previous doc's working directory
    PUSH    SI
    MOV     SI, doc_cwd
    CALL    swap_chdir
    POP     SI

    ; ---- Initialize new slot ----
    POP     AX
    PUSH    AX
    XOR     SI, SI                  ; SI=0 = untitled
    CALL    doc_slot_init
    INC     BYTE [doc_count]

    ; ---- Copy generated "unsavedN.txt" to BSS filename ----
    POP     AX
    PUSH    AX                      ; AL = new slot index (peek from stack)
    CALL    doc_get_slot            ; ES:DI = slot
    MOV     SI, DI
    ADD     SI, DOC_FILENAME        ; SI = offset within doc_table_seg
    PUSH    DS
    PUSH    ES
    POP     DS                      ; DS = doc_table_seg for LODSB
    MOV     DI, filename
.new_cp_fn:
    LODSB
    MOV     CS:[DI], AL             ; filename is in original DS (=CS for .COM)
    INC     DI
    TEST    AL, AL
    JNZ     .new_cp_fn
    POP     DS

    ; ---- Create empty document structures ----
    CALL    ed_new_doc
    JC      .new_alloc_fail

    ; ---- Set as active document ----
    POP     AX
    MOV     [active_doc], AL
    MOV     [panel_sel], AL

    ; ---- Initialize editor state for new doc ----
    CALL    rebuild_meta
    MOV     BYTE [meta_dirty], 0
    MOV     BYTE [dirty], 0
    MOV     WORD [cur_line], 0
    MOV     WORD [cur_col], 0
    MOV     WORD [top_line], 0
    MOV     WORD [left_col], 0
    MOV     WORD [last_ins_pi], 0FFFFh
    MOV     BYTE [undo_group_active], 0
    MOV     BYTE [sel_active], 0
    MOV     BYTE [overwrite_mode], 0
    CALL    undo_init
    MOV     BYTE [fw_state + FW_DIRTY], 1
    JMP     .new_done

.new_swap_fail:
    POP     AX
    JMP     .new_done

.new_alloc_fail:
    POP     AX
    MOV     AL, [active_doc]
    CALL    doc_swap_in
    JMP     .new_done

.new_done:
    PopAll
    RET

; ============================================================
; menu_open_handler — Open file (multi-document aware)
; Reuses current slot if untitled+clean, else opens in new slot.
; ============================================================
menu_open_handler:
    PushAll

    ; ---- Determine if we need a new slot or can reuse current ----
    MOV     AL, [active_doc]
    CALL    doc_get_slot
    TEST    BYTE ES:[DI + DOC_FLAGS], DOCF_UNTITLED
    JZ      .open_need_new
    CMP     BYTE [dirty], 0
    JNE     .open_need_new
    MOV     BYTE [open_reuse_flag], 1
    JMP     .open_show_dlg

.open_need_new:
    MOV     BYTE [open_reuse_flag], 0
    CALL    doc_find_free_slot
    JNC     .open_show_dlg
    MOV     SI, s_err_max_docs
    CALL    show_error_msg
    JMP     .open_done

.open_show_dlg:
.open_dlg:
    ; ---- Show file selector ----
    MOV     BYTE [open_file_buf], 0     ; clear so wildcard pattern pre-fills
    MOV     DI, m_str_open_ttl
    MOV     BX, open_file_buf
    MOV     CL, 13
    CALL    tui_dlg_file
    CMP     AL, DLG_OK
    JNE     .open_done

    ; ---- Copy selected filename to BSS filename ----
    MOV     SI, open_file_buf
    MOV     DI, filename
.open_copy:
    MOV     AL, [SI]
    MOV     [DI], AL
    INC     SI
    INC     DI
    TEST    AL, AL
    JNZ     .open_copy

    ; ---- Save new file's CWD (dialog may have changed it) ----
    PUSH    DI
    MOV     DI, rd_save_buf         ; temp: reuse range delete scratch
    CALL    swap_get_cwd
    POP     DI

    ; ---- Check for duplicate: is this file already open? ----
    MOV     SI, filename
    CALL    doc_find_by_name
    JC      .open_not_dup

    ; File is already open in slot AL — switch to it
    CALL    doc_switch
    JMP     .open_done

.open_not_dup:
    ; ---- Reuse or new slot? ----
    CMP     BYTE [open_reuse_flag], 1
    JE      .open_load_current

    ; ---- New slot path: swap out active, init new slot ----
    CALL    doc_find_free_slot
    JC      .open_done
    PUSH    AX

    CALL    doc_swap_out
    JC      .open_swap_fail

    POP     AX
    PUSH    AX
    MOV     SI, filename
    CALL    doc_slot_init
    INC     BYTE [doc_count]

    POP     AX
    MOV     [active_doc], AL
    MOV     [panel_sel], AL

.open_load_current:
    ; ---- Update doc_cwd to new file's directory ----
    PUSH    ES
    MOV     AX, DS
    MOV     ES, AX
    MOV     SI, rd_save_buf
    MOV     DI, doc_cwd
    MOV     CX, SWH_DOC_CWD_SIZE
    REP     MOVSB
    POP     ES
    ; ---- CHDIR to new file's dir (no-op if already there) ----
    PUSH    SI
    MOV     SI, doc_cwd
    CALL    swap_chdir
    POP     SI

    ; ---- Load file into current slot ----
    CALL    ed_load_file
    JC      .open_load_failed

    ; ---- Update document table slot with new filename ----
    MOV     AL, [active_doc]
    CALL    doc_get_slot            ; ES:DI = slot in doc_table_seg
    PUSH    DI
    MOV     SI, filename
    ADD     DI, DOC_FILENAME        ; ES:DI = DOC_FILENAME within slot
    MOV     CX, 127
.open_slot_copy:
    LODSB                           ; DS:SI (BSS filename)
    STOSB                           ; ES:DI (doc_table_seg slot)
    TEST    AL, AL
    JZ      .open_slot_done
    DEC     CX
    JNZ     .open_slot_copy
    MOV     BYTE ES:[DI-1], 0
.open_slot_done:
    POP     DI
    AND     BYTE ES:[DI + DOC_FLAGS], ~DOCF_UNTITLED

    JMP     .open_done

.open_load_failed:
    ; ---- Load failed: undo slot allocation, restore previous doc ----
    CMP     BYTE [open_reuse_flag], 1
    JE      .open_done              ; reuse path: slot unchanged, empty doc OK

    ; New slot path: clean up the failed slot
    CALL    free_all                ; free any segments ed_new_doc allocated
    MOV     AL, [active_doc]
    CALL    doc_slot_clear          ; clear OCCUPIED flag, dec doc_count

    ; Switch back to the previous document (still swapped to disk)
    CALL    doc_next_occupied       ; AL = next occupied slot
    JC      .open_done              ; shouldn't happen — guard
    MOV     [active_doc], AL
    MOV     [panel_sel], AL
    CALL    doc_swap_in
    JMP     .open_done

.open_swap_fail:
    POP     AX

.open_done:
    PopAll
    RET

; ============================================================
; menu_close_handler — Close the active document
; Single doc: untitled+clean = no-op; else reset to fresh untitled.
; Multiple docs: close + switch to next.
; ============================================================
menu_close_handler:
    PushAll

    ; ---- If only one document open, handle specially ----
    CMP     BYTE [doc_count], 1
    JA      .close_multi

    ; Single doc — if untitled and clean, do nothing
    MOV     AL, [active_doc]
    CALL    doc_get_slot
    TEST    BYTE ES:[DI + DOC_FLAGS], DOCF_UNTITLED
    JZ      .close_single_prompt
    CMP     BYTE [dirty], 0
    JE      .close_done

.close_single_prompt:
    ; Dirty or named file — ask to save
    CMP     BYTE [dirty], 0
    JE      .close_single_reset

    MOV     SI, m_str_quit_ask
    MOV     DI, m_str_quit_ttl
    CALL    tui_dlg_confirm

    CMP     AL, DLG_YES
    JE      .close_single_save
    CMP     AL, DLG_NO
    JE      .close_single_reset
    JMP     .close_done             ; Cancel — abort

.close_single_save:
    CALL    menu_save_handler

.close_single_reset:
    ; Reset to fresh untitled document
    CALL    free_all
    CALL    ed_new_doc
    JC      .close_done

    ; Update slot to untitled
    MOV     AL, [active_doc]
    XOR     SI, SI
    CALL    doc_slot_init

    ; Copy generated filename to BSS
    MOV     AL, [active_doc]
    CALL    doc_get_slot            ; ES:DI = slot in doc_table_seg
    MOV     SI, DI
    ADD     SI, DOC_FILENAME        ; SI = offset within doc_table_seg
    PUSH    DS
    PUSH    ES
    POP     DS                      ; DS = doc_table_seg for LODSB
    MOV     DI, filename
.close_cp_fn:
    LODSB
    MOV     CS:[DI], AL             ; filename in original DS (=CS for .COM)
    INC     DI
    TEST    AL, AL
    JNZ     .close_cp_fn
    POP     DS

    ; Reset editor state
    CALL    rebuild_meta
    MOV     BYTE [meta_dirty], 0
    MOV     BYTE [dirty], 0
    MOV     WORD [cur_line], 0
    MOV     WORD [cur_col], 0
    MOV     WORD [top_line], 0
    MOV     WORD [left_col], 0
    MOV     WORD [last_ins_pi], 0FFFFh
    MOV     BYTE [undo_group_active], 0
    MOV     BYTE [sel_active], 0
    MOV     BYTE [overwrite_mode], 0
    CALL    undo_init
    MOV     BYTE [fw_state + FW_DIRTY], 1
    JMP     .close_done

.close_multi:
    ; ---- If dirty, ask to save ----
    CMP     BYTE [dirty], 0
    JE      .close_do_it

    MOV     SI, m_str_quit_ask
    MOV     DI, m_str_quit_ttl
    CALL    tui_dlg_confirm

    CMP     AL, DLG_YES
    JE      .close_save
    CMP     AL, DLG_NO
    JE      .close_do_it
    JMP     .close_done             ; Cancel — abort close

.close_save:
    CALL    menu_save_handler

.close_do_it:
    ; ---- Free active document memory ----
    CALL    free_all

    ; ---- Delete swap file if it exists ----
    MOV     AL, [active_doc]
    CALL    doc_get_slot            ; ES:DI = slot in doc_table_seg
    MOV     DX, DI
    ADD     DX, DOC_SWAP_PATH
    PUSH    DS
    PUSH    ES
    POP     DS                      ; DS:DX = swap path in doc_table_seg
    MOV     AH, 41h
    INT     21h                     ; ignore error
    POP     DS

    ; ---- Clear the slot ----
    MOV     AL, [active_doc]
    CALL    doc_slot_clear

    ; ---- Switch to next occupied slot ----
    CALL    doc_next_occupied
    JC      .close_done

    CALL    doc_swap_in
    JC      .close_swap_fail

    MOV     BYTE [fw_state + FW_DIRTY], 1
    JMP     .close_done

.close_swap_fail:
    MOV     BYTE [filename], 0
    CALL    ed_new_doc
    CALL    rebuild_meta
    MOV     BYTE [meta_dirty], 0
    MOV     BYTE [fw_state + FW_DIRTY], 1

.close_done:
    PopAll
    RET

menu_save_handler:
    PUSH    SI
    PUSH    DI
    PUSH    BX
    PUSH    CX

    ; Check DOCF_UNTITLED flag — unsaved docs need Save As
    MOV     AL, [active_doc]
    CALL    doc_get_slot
    TEST    BYTE ES:[DI + DOC_FLAGS], DOCF_UNTITLED
    JNZ     .save_as

    ; ---- Direct save ----
    CALL    save_file
    MOV     AX, [undo_pos]
    MOV     [undo_save_pos], AX
    MOV     BYTE [fw_state + FW_DIRTY], 1
    JMP     .save_done

.save_as:
    ; ---- Untitled doc — delegate to Save As (file browser) ----
    POP     CX
    POP     BX
    POP     DI
    POP     SI
    JMP     menu_saveas_handler     ; tail call (handles everything)

.save_done:
    POP     CX
    POP     BX
    POP     DI
    POP     SI
    RET

; ============================================================
; menu_closeall_handler — Close all documents, reset to untitled
; Prompts to save each dirty document. Cancel aborts entirely.
; ============================================================
menu_closeall_handler:
    PushAll

    ; ---- Prompt for each dirty document ----
    ; First: active doc
    CMP     BYTE [dirty], 0
    JE      .ca_walk

    MOV     SI, m_str_quit_ask
    MOV     DI, m_str_quit_ttl
    CALL    tui_dlg_confirm
    CMP     AL, DLG_YES
    JE      .ca_save_active
    CMP     AL, DLG_NO
    JE      .ca_walk
    JMP     .ca_abort               ; Cancel — abort close all

.ca_save_active:
    CALL    menu_save_handler

.ca_walk:
    ; Walk non-active slots for dirty docs
    XOR     CL, CL
.ca_prompt_loop:
    CMP     CL, MAX_DOCS
    JAE     .ca_do_close

    CMP     CL, [active_doc]
    JE      .ca_prompt_next

    MOV     AL, CL
    CALL    doc_get_slot
    TEST    BYTE ES:[DI + DOC_FLAGS], DOCF_OCCUPIED
    JZ      .ca_prompt_next
    TEST    BYTE ES:[DI + DOC_FLAGS], DOCF_DIRTY
    JZ      .ca_prompt_next

    ; Dirty swapped doc — switch to it and prompt
    MOV     AL, CL
    PUSH    CX
    CALL    doc_switch

    MOV     SI, m_str_quit_ask
    MOV     DI, m_str_quit_ttl
    CALL    tui_dlg_confirm
    CMP     AL, DLG_YES
    JE      .ca_save_switched
    CMP     AL, DLG_NO
    JE      .ca_switched_skip
    POP     CX
    JMP     .ca_abort               ; Cancel — abort

.ca_save_switched:
    CALL    menu_save_handler
.ca_switched_skip:
    POP     CX

.ca_prompt_next:
    INC     CL
    JMP     .ca_prompt_loop

.ca_do_close:
    ; ---- Free all documents and their swap files ----
    XOR     CL, CL
.ca_free_loop:
    CMP     CL, MAX_DOCS
    JAE     .ca_reset

    MOV     AL, CL
    CALL    doc_get_slot
    TEST    BYTE ES:[DI + DOC_FLAGS], DOCF_OCCUPIED
    JZ      .ca_free_next

    ; Delete swap file if it exists
    CMP     CL, [active_doc]
    JE      .ca_free_clear          ; active doc has no swap file
    PUSH    DX
    MOV     DX, DI
    ADD     DX, DOC_SWAP_PATH
    PUSH    DS
    PUSH    ES
    POP     DS                      ; DS:DX = swap path in doc_table_seg
    MOV     AH, 41h
    INT     21h                     ; ignore error
    POP     DS
    POP     DX

.ca_free_clear:
    MOV     AL, CL
    PUSH    CX
    CALL    doc_slot_clear
    POP     CX

.ca_free_next:
    INC     CL
    JMP     .ca_free_loop

.ca_reset:
    ; ---- Free active doc memory and create fresh untitled ----
    CALL    free_all
    CALL    ed_new_doc

    ; Re-init slot 0 as untitled
    MOV     BYTE [active_doc], 0
    MOV     BYTE [panel_sel], 0
    MOV     BYTE [doc_count], 1
    XOR     AL, AL
    XOR     SI, SI
    CALL    doc_slot_init

    ; Copy generated filename to BSS
    XOR     AL, AL
    CALL    doc_get_slot            ; ES:DI = slot in doc_table_seg
    MOV     SI, DI
    ADD     SI, DOC_FILENAME        ; SI = offset within doc_table_seg
    PUSH    DS
    PUSH    ES
    POP     DS                      ; DS = doc_table_seg for LODSB
    MOV     DI, filename
.ca_cp_fn:
    LODSB
    MOV     CS:[DI], AL             ; filename in original DS (=CS for .COM)
    INC     DI
    TEST    AL, AL
    JNZ     .ca_cp_fn
    POP     DS

    ; Reset editor state
    CALL    rebuild_meta
    MOV     BYTE [meta_dirty], 0
    MOV     BYTE [dirty], 0
    MOV     WORD [cur_line], 0
    MOV     WORD [cur_col], 0
    MOV     WORD [top_line], 0
    MOV     WORD [left_col], 0
    MOV     WORD [last_ins_pi], 0FFFFh
    MOV     BYTE [undo_group_active], 0
    MOV     BYTE [sel_active], 0
    MOV     BYTE [overwrite_mode], 0
    CALL    undo_init
    MOV     BYTE [fw_state + FW_DIRTY], 1

.ca_abort:
    PopAll
    RET

; ============================================================
; menu_saveall_handler — Save all dirty, named documents
; Skips untitled documents (no filename to save to).
; ============================================================
menu_saveall_handler:
    PushAll

    ; Remember which doc was active so we can return to it
    MOV     AL, [active_doc]
    PUSH    AX                      ; stack: original active_doc

    ; ---- Save active doc first (if dirty + named) ----
    MOV     AL, [active_doc]
    CALL    doc_get_slot
    TEST    BYTE ES:[DI + DOC_FLAGS], DOCF_UNTITLED
    JNZ     .sal_walk
    CMP     BYTE [dirty], 0
    JE      .sal_walk
    CALL    save_file
    MOV     AX, [undo_pos]
    MOV     [undo_save_pos], AX

.sal_walk:
    ; ---- Walk all other slots ----
    XOR     CL, CL
.sal_loop:
    CMP     CL, MAX_DOCS
    JAE     .sal_restore

    ; Skip active doc (already handled)
    CMP     CL, [active_doc]
    JE      .sal_next

    ; Get slot
    MOV     AL, CL
    CALL    doc_get_slot

    ; Skip unoccupied
    TEST    BYTE ES:[DI + DOC_FLAGS], DOCF_OCCUPIED
    JZ      .sal_next

    ; Skip untitled
    TEST    BYTE ES:[DI + DOC_FLAGS], DOCF_UNTITLED
    JNZ     .sal_next

    ; Skip clean
    TEST    BYTE ES:[DI + DOC_FLAGS], DOCF_DIRTY
    JZ      .sal_next

    ; Switch to this doc, save it
    MOV     AL, CL
    PUSH    CX
    CALL    doc_switch
    CALL    save_file
    MOV     AX, [undo_pos]
    MOV     [undo_save_pos], AX
    POP     CX

.sal_next:
    INC     CL
    JMP     .sal_loop

.sal_restore:
    ; Switch back to original doc
    POP     AX                      ; AL = original active_doc
    CMP     AL, [active_doc]
    JE      .sal_done
    CALL    doc_switch

.sal_done:
    MOV     BYTE [fw_state + FW_DIRTY], 1
    PopAll
    RET

; ============================================================
; menu_saveprj_handler — Save all open file paths to a .PRJ file
; ============================================================
menu_saveprj_handler:
    PushAll
    MOV     AX, DS
    MOV     ES, AX

    ; ---- Save and set dlg_file_pattern to *.PRJ ----
    MOV     SI, dlg_file_pattern
    MOV     DI, prj_pat_save
    MOV     CX, 14
    REP     MOVSB
    MOV     DI, dlg_file_pattern
    MOV     SI, s_prj_pattern
    MOV     CX, 6
    REP     MOVSB
    ; zero-pad remaining 8 bytes
    XOR     AL, AL
    MOV     CX, 8
    REP     STOSB

    ; ---- Show file dialog ----
    MOV     BYTE [open_file_buf], 0
    MOV     DI, m_str_sprj_ttl
    MOV     BX, open_file_buf
    MOV     CL, 13
    CALL    tui_dlg_file

    PUSH    AX                      ; save dialog result
    ; ---- Restore dlg_file_pattern ----
    MOV     SI, prj_pat_save
    MOV     DI, dlg_file_pattern
    MOV     CX, 14
    REP     MOVSB
    POP     AX

    CMP     AL, DLG_OK
    JNE     .sprj_done

    ; ---- Check if file exists ----
    MOV     DX, open_file_buf
    MOV     AL, 0
    MOV     AH, 3Dh
    INT     21h
    JC      .sprj_create

    MOV     BX, AX
    MOV     AH, 3Eh
    INT     21h
    MOV     SI, m_str_overwrite
    MOV     DI, m_str_save_warn
    CALL    tui_dlg_confirm
    CMP     AL, DLG_YES
    JNE     .sprj_done

.sprj_create:
    ; ---- Create the PRJ file ----
    MOV     DX, open_file_buf
    XOR     CX, CX
    MOV     AH, 3Ch
    INT     21h
    JC      .sprj_err
    MOV     [prj_file_hnd], AX

    ; ---- Walk all slots, write occupied non-untitled paths ----
    XOR     CL, CL
.sprj_walk:
    CMP     CL, MAX_DOCS
    JAE     .sprj_close

    MOV     AL, CL
    PUSH    CX
    CALL    doc_get_slot
    POP     CX
    TEST    BYTE ES:[DI + DOC_FLAGS], DOCF_OCCUPIED
    JZ      .sprj_next
    TEST    BYTE ES:[DI + DOC_FLAGS], DOCF_UNTITLED
    JNZ     .sprj_next

    ; ---- For active doc use BSS filename; others use slot ----
    PUSH    CX
    CMP     CL, [active_doc]
    JNE     .sprj_use_slot
    MOV     DX, filename
    PUSH    DS                      ; save DS (points to BSS)
    JMP     .sprj_write
.sprj_use_slot:
    MOV     DX, DI
    ADD     DX, DOC_FILENAME
    PUSH    DS
    PUSH    ES
    POP     DS                      ; DS = doc_table_seg for slot filename
.sprj_write:
    ; Compute string length
    PUSH    DI
    MOV     DI, DX
    XOR     CX, CX
.sprj_strlen:
    CMP     BYTE [DI], 0
    JE      .sprj_strlen_done
    INC     DI
    INC     CX
    CMP     CX, 128
    JB      .sprj_strlen
.sprj_strlen_done:
    POP     DI

    ; Write filename (DS:DX = filename string)
    MOV     BX, CS:[prj_file_hnd]
    MOV     AH, 40h
    INT     21h
    POP     DS                      ; restore DS to BSS segment

    ; Write CRLF
    MOV     DX, s_prj_crlf
    MOV     CX, 2
    MOV     AH, 40h
    INT     21h

    POP     CX

.sprj_next:
    INC     CL
    JMP     .sprj_walk

.sprj_close:
    MOV     BX, [prj_file_hnd]
    MOV     AH, 3Eh
    INT     21h

.sprj_done:
    PopAll
    RET

.sprj_err:
    MOV     SI, s_err_prj_open
    CALL    show_error_msg
    PopAll
    RET

; ============================================================
; menu_loadprj_handler — Load a .PRJ file and open all listed files
; ============================================================
menu_loadprj_handler:
    PushAll
    MOV     AX, DS
    MOV     ES, AX

    ; ---- Save and set dlg_file_pattern to *.PRJ ----
    MOV     SI, dlg_file_pattern
    MOV     DI, prj_pat_save
    MOV     CX, 14
    REP     MOVSB
    MOV     DI, dlg_file_pattern
    MOV     SI, s_prj_pattern
    MOV     CX, 6
    REP     MOVSB
    XOR     AL, AL
    MOV     CX, 8
    REP     STOSB

    ; ---- Show file dialog ----
    MOV     BYTE [open_file_buf], 0
    MOV     DI, m_str_lprj_ttl
    MOV     BX, open_file_buf
    MOV     CL, 13
    CALL    tui_dlg_file

    PUSH    AX
    ; ---- Restore dlg_file_pattern ----
    MOV     SI, prj_pat_save
    MOV     DI, dlg_file_pattern
    MOV     CX, 14
    REP     MOVSB
    POP     AX

    CMP     AL, DLG_OK
    JNE     .lprj_done

    ; ---- Open and read the PRJ file into rd_save_buf ----
    MOV     DX, open_file_buf
    MOV     AL, 0
    MOV     AH, 3Dh
    INT     21h
    JC      .lprj_err_open
    MOV     BX, AX

    MOV     DX, rd_save_buf
    MOV     CX, 1020
    MOV     AH, 3Fh
    INT     21h
    PUSH    AX                      ; save bytes read
    ; Close PRJ file
    MOV     AH, 3Eh
    INT     21h
    POP     AX
    MOV     [prj_bytes_read], AX

    CMP     WORD [prj_bytes_read], 0
    JE      .lprj_done              ; empty file

    ; ---- Close all existing documents ----
    CALL    menu_closeall_handler

    ; After closeall: active_doc=0, doc_count=1, slot 0 = untitled+clean
    MOV     BYTE [prj_first_file], 1

    ; ---- Parse rd_save_buf line by line ----
    MOV     SI, rd_save_buf
    MOV     CX, [prj_bytes_read]

.lprj_parse_line:
    ; Skip leading CR/LF/spaces
    OR      CX, CX
    JZ      .lprj_final
.lprj_skip_ws:
    CMP     BYTE [SI], 0Dh
    JE      .lprj_skip1
    CMP     BYTE [SI], 0Ah
    JE      .lprj_skip1
    CMP     BYTE [SI], ' '
    JE      .lprj_skip1
    JMP     .lprj_got_line
.lprj_skip1:
    INC     SI
    DEC     CX
    JNZ     .lprj_skip_ws
    JMP     .lprj_final

.lprj_got_line:
    ; Copy chars until CR/LF/end into filename BSS
    MOV     DI, filename
    XOR     BL, BL                  ; BL = char count
.lprj_copy_ch:
    OR      CX, CX
    JZ      .lprj_line_end
    MOV     AL, [SI]
    CMP     AL, 0Dh
    JE      .lprj_line_end
    CMP     AL, 0Ah
    JE      .lprj_line_end
    MOV     [DI], AL
    INC     SI
    INC     DI
    INC     BL
    DEC     CX
    CMP     BL, 127
    JB      .lprj_copy_ch

.lprj_line_end:
    MOV     BYTE [DI], 0           ; null-terminate
    TEST    BL, BL
    JZ      .lprj_parse_line        ; empty line — skip

    ; ---- Load this file ----
    PUSH    CX
    PUSH    SI                      ; save parse state

    CMP     BYTE [prj_first_file], 1
    JNE     .lprj_new_slot

    ; First file: reuse untitled slot 0
    MOV     BYTE [prj_first_file], 0
    MOV     AL, 0
    MOV     SI, filename
    CALL    doc_slot_init

    CALL    ed_load_file
    JMP     .lprj_update_slot

.lprj_new_slot:
    ; Check 8-file limit
    CMP     BYTE [doc_count], MAX_DOCS
    JAE     .lprj_too_many

    CALL    doc_find_free_slot
    JC      .lprj_too_many
    PUSH    AX

    CALL    doc_swap_out
    JC      .lprj_swap_fail

    POP     AX
    PUSH    AX
    MOV     SI, filename
    CALL    doc_slot_init
    INC     BYTE [doc_count]

    POP     AX
    MOV     [active_doc], AL
    MOV     [panel_sel], AL

    CALL    ed_load_file

.lprj_update_slot:
    ; Update slot filename + clear untitled flag
    MOV     AL, [active_doc]
    CALL    doc_get_slot            ; ES:DI = slot in doc_table_seg
    PUSH    DI
    MOV     SI, filename
    ADD     DI, DOC_FILENAME        ; ES:DI = DOC_FILENAME within slot
    MOV     CX, 127
.lprj_scopy:
    LODSB                           ; DS:SI (BSS filename)
    STOSB                           ; ES:DI (doc_table_seg slot)
    TEST    AL, AL
    JZ      .lprj_scopy_done
    DEC     CX
    JNZ     .lprj_scopy
    MOV     BYTE ES:[DI-1], 0
.lprj_scopy_done:
    POP     DI
    AND     BYTE ES:[DI + DOC_FLAGS], ~DOCF_UNTITLED

    POP     SI
    POP     CX
    JMP     .lprj_parse_line

.lprj_too_many:
    POP     SI
    POP     CX
    MOV     SI, s_prj_truncated
    CALL    show_error_msg
    JMP     .lprj_final

.lprj_swap_fail:
    POP     AX                      ; discard slot index
    POP     SI
    POP     CX

.lprj_final:
    MOV     BYTE [fw_state + FW_DIRTY], 1

.lprj_done:
    PopAll
    RET

.lprj_err_open:
    MOV     SI, s_err_prj_open
    CALL    show_error_msg
    PopAll
    RET

; ============================================================
; menu_shell_handler — Shell to DOS via COMSPEC
; Swaps out the screen, runs COMMAND.COM, restores on return.
; ============================================================
menu_shell_handler:
    PushAll

    ; ---- Find COMSPEC in environment ----
    MOV     AX, [002Ch]             ; environment segment from PSP
    TEST    AX, AX
    JZ      .sh_no_comspec
    MOV     ES, AX

    XOR     DI, DI                  ; ES:DI = start of environment
.sh_env_loop:
    CMP     BYTE ES:[DI], 0         ; end of environment?
    JE      .sh_no_comspec

    ; Compare "COMSPEC=" against ES:DI
    MOV     SI, s_comspec_key
    MOV     CX, S_COMSPEC_LEN
.sh_cmp:
    MOV     AL, ES:[DI]
    CMP     AL, [SI]
    JNE     .sh_skip_var
    INC     DI
    INC     SI
    DEC     CX
    JNZ     .sh_cmp

    ; Match — ES:DI now points to the value after "COMSPEC="
    ; Copy to shell_comspec
    MOV     SI, DI                  ; SI = offset in env segment
    MOV     DI, shell_comspec
    PUSH    DS
    PUSH    ES
    POP     DS                      ; DS = environment segment
    MOV     AX, CS
    MOV     ES, AX                  ; ES = our segment
.sh_copy_cs:
    LODSB
    STOSB
    TEST    AL, AL
    JNZ     .sh_copy_cs
    POP     DS                      ; restore DS
    MOV     AX, DS
    MOV     ES, AX                  ; restore ES = DS
    JMP     .sh_found

.sh_skip_var:
    ; Skip to end of this env string (find the null)
    CMP     BYTE ES:[DI], 0
    JE      .sh_next_var
    INC     DI
    JMP     .sh_skip_var
.sh_next_var:
    INC     DI                      ; skip past the null
    JMP     .sh_env_loop

.sh_no_comspec:
    MOV     AX, DS
    MOV     ES, AX
    MOV     SI, s_err_no_comspec
    CALL    show_error_msg
    JMP     .sh_done

.sh_found:
    ; ---- Build EXEC parameter block ----
    MOV     BYTE [shell_cmdtail], 0
    MOV     BYTE [shell_cmdtail+1], 0Dh

    ; Zero out FCB
    MOV     DI, shell_fcb
    XOR     AL, AL
    MOV     CX, 37
    REP     STOSB

    ; Fill exec parameter block
    MOV     DI, shell_exec_pb
    XOR     AX, AX
    STOSW                           ; environment = 0 (inherit)
    MOV     AX, shell_cmdtail
    STOSW                           ; command tail offset
    MOV     AX, DS
    STOSW                           ; command tail segment
    MOV     AX, shell_fcb
    STOSW                           ; FCB1 offset
    MOV     AX, DS
    STOSW                           ; FCB1 segment
    MOV     AX, shell_fcb
    STOSW                           ; FCB2 offset
    MOV     AX, DS
    STOSW                           ; FCB2 segment

    ; ---- Shell swap: dump memory if /d active ----
    CMP     BYTE [shell_dump_mode], 0
    JE      .sh_no_dump
    CALL    shell_swap_out
    JC      .sh_dump_failed
.sh_no_dump:

    ; ---- Clear screen and print shell message ----
    MOV     AX, 0003h               ; INT 10h: set mode 3 (80x25 text, clears screen)
    INT     10h
    MOV     AH, 09h
    MOV     DX, s_shell_msg
    INT     21h

    ; ---- Save SS:SP and call EXEC ----
    MOV     [shell_save_ss], SS
    MOV     [shell_save_sp], SP

    MOV     DX, shell_comspec       ; DS:DX = program path
    MOV     BX, shell_exec_pb       ; ES:BX = parameter block (ES=DS)
    MOV     AX, 4B00h               ; EXEC: load and execute
    INT     21h

    ; ---- Restore SS:SP (EXEC clobbers all registers) ----
    CLI
    MOV     SS, CS:[shell_save_ss]
    MOV     SP, CS:[shell_save_sp]
    STI

    ; ---- Restore DS and ES ----
    MOV     AX, CS
    MOV     DS, AX
    MOV     ES, AX

    ; ---- Shell swap: restore memory if /d was active ----
    CMP     BYTE [shell_dump_mode], 0
    JE      .sh_no_restore
    CALL    shell_swap_in
    JC      .sh_restore_failed
.sh_no_restore:

    ; ---- Restore video mode and TUI screen ----
    MOV     AX, 0003h               ; reset mode 3
    INT     10h

    ; Hide BIOS text cursor (mode reset makes it visible again)
    MOV     AH, 01h
    MOV     CH, 20h                 ; start line > max = hidden
    INT     10h

    ; Reinitialize mouse driver (reset + set ranges + show cursor)
    CALL    tui_mouse_init

    MOV     BYTE [fw_state + FW_DIRTY], 1

    ; ---- Show deferred warning if swap file was missing ----
    CMP     BYTE [shell_recovering], 0
    JE      .sh_done
    MOV     BYTE [shell_recovering], 0
    MOV     SI, s_err_shl_lost
    CALL    show_error_msg

.sh_done:
    PopAll
    RET

.sh_dump_failed:
    ; Swap-out failed — show error, abort shell (memory still intact)
    MOV     SI, s_err_shl_swap
    CALL    show_error_msg
    JMP     .sh_done

.sh_restore_failed:
    ; Fatal — cannot restore. Print message and exit.
    MOV     AH, 09h
    MOV     DX, s_err_shl_fatal
    INT     21h
    MOV     AX, 4C01h
    INT     21h

menu_undo_handler:
    MOV     BYTE [undo_group_active], 0
    JMP     undo_execute

menu_redo_handler:
    MOV     BYTE [undo_group_active], 0
    JMP     redo_execute

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

    ; ---- Check if file exists (try open for read) ----
    MOV     DX, open_file_buf
    MOV     AL, 0                       ; read-only
    MOV     AH, 3Dh
    INT     21h
    JC      .sa_no_exist                ; CF=1 = doesn't exist, proceed

    ; File exists — close handle and prompt
    MOV     BX, AX
    MOV     AH, 3Eh
    INT     21h

    MOV     SI, m_str_overwrite
    MOV     DI, m_str_save_warn
    CALL    tui_dlg_confirm
    CMP     AL, DLG_YES
    JNE     .sa_done                    ; No or Cancel — abort

.sa_no_exist:
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

    ; ---- Update document table slot with new filename ----
    MOV     AL, [active_doc]
    CALL    doc_get_slot            ; ES:DI = slot in doc_table_seg
    PUSH    DI
    MOV     SI, filename
    ADD     DI, DOC_FILENAME        ; ES:DI = DOC_FILENAME within slot
    MOV     CX, 127
.sa_slot_copy:
    LODSB                           ; DS:SI (BSS filename)
    STOSB                           ; ES:DI (doc_table_seg slot)
    TEST    AL, AL
    JZ      .sa_slot_done
    DEC     CX
    JNZ     .sa_slot_copy
    MOV     BYTE ES:[DI-1], 0
.sa_slot_done:
    POP     DI
    AND     BYTE ES:[DI + DOC_FLAGS], ~DOCF_UNTITLED

    ; ---- Update doc_cwd (Save As may have changed directory) ----
    PUSH    DI
    MOV     DI, doc_cwd
    CALL    swap_get_cwd
    POP     DI

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
    CALL    show_error_msg

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

; ============================================================
; menu_doclist_handler — Toggle document side panel
; ============================================================
menu_doclist_handler:
    XOR     BYTE [panel_visible], 1
    CMP     BYTE [panel_visible], 0
    JE      .vdl_done
    MOV     AL, [active_doc]
    MOV     [panel_sel], AL
.vdl_done:
    MOV     BYTE [fw_state + FW_DIRTY], 1
    RET

menu_about_handler:
    PushAll

    ; Interior: 34 wide, 9 rows (5 lines + 1 blank + 1 blank + 1 button + 1 pad)
    ; WIN_W = 36, WIN_H = 11

    ; ---- Line 1: "TEDIT v0.56" (centered label) ----
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
    ; ---- Zero BSS (real DOS does not zero memory beyond the COM file) ----
    ; agent86 guarantees zero-filled BSS; real DOS / 4DOS does not.
    ; Without this, BSS contains garbage from the previous occupant of
    ; this memory (e.g. 4DOS transient code), breaking conditional
    ; allocation guards (undo_seg, clip_seg) and flag variables
    ; (panel_visible, find_flags, etc.).
    CLD
    MOV     DI, DOC_CTX
    MOV     CX, BSS_END - DOC_CTX
    XOR     AL, AL
    REP     STOSB

    ; ---- Reject MDA display (we write directly to B800h) ----
    MOV     AH, 0Fh
    INT     10h                         ; AL = mode, AH = columns
    CMP     AL, 07h                     ; mode 7 = MDA monochrome
    JE      .video_fail
    ; ---- Reject 40-column modes (need 80x25 minimum) ----
    CMP     AH, 80
    JAE     .video_ok
.video_fail:
    MOV     DX, s_err_mda
    MOV     AH, 09h
    INT     21h
    INT     20h
.video_ok:

    ; ---- Detect CGA for snow prevention ----
    MOV     BYTE [tui_cga_snow], 0
    MOV     AH, 12h
    MOV     BL, 10h
    INT     10h                         ; BL unchanged if no EGA/VGA
    CMP     BL, 10h
    JNE     .snow_done
    MOV     BYTE [tui_cga_snow], 1      ; CGA — enable snow prevention
.snow_done:

    CALL    shrink_mem

    ; ---- Install INT 23h handler (ignore Ctrl+C at DOS level) ----
    ; Prevents 4DOS (or any parent shell) from terminating TEDIT when
    ; Ctrl+C is pressed.  TEDIT uses Ctrl+C for Copy, not break.
    ; DOS automatically restores the original vector on INT 20h exit.
    MOV     AX, 2523h
    MOV     DX, _int23_handler
    INT     21h

    ; ---- Install INT 24h handler (return FAIL on critical errors) ----
    ; Prevents 4DOS's critical-error handler from displaying its own
    ; Abort/Retry/Fail dialog during TEDIT file I/O.
    MOV     AX, 2524h
    MOV     DX, _int24_handler
    INT     21h

    ; ---- Allocate persistent segments needed before doc_table_init ----
    MOV     BX, DOC_TBL_SEG_PARA
    MOV     AH, 48h
    INT     21h
    JC      .err_nomem
    MOV     [doc_table_seg], AX

    MOV     BX, SHADOW_SEG_PARA
    MOV     AH, 48h
    INT     21h
    JC      .err_nomem
    MOV     [shadow_seg], AX

    ; ---- Clean up orphaned swap files from previous crashes ----
    CALL    doc_cleanup_orphans

    ; ---- Initialize document table ----
    CALL    doc_table_init
    MOV     AX, DS
    MOV     ES, AX                      ; restore ES=DS (doc_table_init sets ES=doc_table_seg)

    ; ---- Save startup working directory (restored on exit) ----
    MOV     AH, 19h
    INT     21h                         ; AL = current drive (0=A)
    MOV     [startup_drive], AL
    MOV     DL, AL
    INC     DL                          ; DL = drive (1=A, 2=B, ...)
    MOV     SI, startup_path
    MOV     AH, 47h
    INT     21h                         ; DS:SI filled with CWD (no leading \)

    ; ---- Build startup_cwd in "D:\path" format ----
    MOV     DI, startup_cwd
    CALL    swap_get_cwd
    ; ---- Initialize doc_cwd = startup_cwd ----
    MOV     SI, startup_cwd
    MOV     DI, doc_cwd
    MOV     CX, 68
    REP     MOVSB

    ; ---- Initialize tab defaults (before parse_args may override) ----
    MOV     WORD [tab_width], ED_TAB_DEFAULT
    MOV     WORD [tab_mask], ED_TAB_DEFAULT - 1
    MOV     WORD [ed_text_cols], ED_COLS
    MOV     BYTE [filename], 0          ; ensure clean for parse_args flag check

    ; ---- Check for orphan shell swap file (crash recovery) ----
    CALL    shell_check_orphan
    JC      .do_recovery

    ; ---- Allocate cache buffers (needed before any file loading) ----
    CALL    cache_init
    JC      .err_nomem

    ; ---- Parse command line ----
    CALL    parse_args
    JC      .no_file

    ; ---- Check for project file or multiple files ----
    CMP     BYTE [arg_has_prj], 0
    JZ      .not_prj_cmdline
    CMP     BYTE [arg_file_count], 1
    JA      .err_mixed
    JMP     .load_prj_cmdline
.not_prj_cmdline:
    CMP     BYTE [arg_file_count], 1
    JA      .load_multi_cmdline

    ; ---- Single file (existing code) ----
    CALL    open_file
    JC      .err_nofile

    CALL    load_file
    JC      .err_nomem

    ; (file handle now owned by orig_file_hnd — do NOT close)

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

    ; ---- Register in document table slot 0 ----
    MOV     AL, 0
    MOV     SI, filename
    CALL    doc_slot_init
    MOV     BYTE [active_doc], 0
    MOV     BYTE [doc_count], 1

    ; ---- Runtime verification (--build_trace only) ----
    ASSERT_EQ BYTE [active_doc], 0
    ASSERT_EQ BYTE [doc_count], 1
    ; doc_table moved to separate segment — debug assertions commented out
    ;ASSERT_EQ BYTE [doc_table + DOC_FLAGS], DOCF_OCCUPIED
    ;ASSERT_EQ BYTE [doc_table + DOC_SLOT_ID], 0
    ;ASSERT_EQ BYTE [doc_table + DOC_SWAP_PATH], 'T'
    ;ASSERT_EQ BYTE [doc_table + DOC_SWAP_PATH + 7], '0'
    ;LOG "slot0_flags", BYTE [doc_table + DOC_FLAGS]
    ;LOG "slot0_swap_0", BYTE [doc_table + DOC_SWAP_PATH]
    LOG "active_doc", BYTE [active_doc]
    LOG "doc_count", BYTE [doc_count]

    ; doc_table moved to separate segment — debug assertions commented out
    ;ASSERT_EQ BYTE [doc_table + DOC_SLOT_SIZE + DOC_FLAGS], 0
    ;ASSERT_EQ BYTE [doc_table + DOC_SLOT_SIZE * 2 + DOC_FLAGS], 0
    ;ASSERT_EQ BYTE [doc_table + DOC_SLOT_SIZE * 7 + DOC_FLAGS], 0

    ; doc_table moved to separate segment — snapshot commented out
    ;MEM_SNAPSHOT doc_tbl_snap, DS, doc_table, MAX_DOCS * DOC_SLOT_SIZE

    JMP     .start_tui

; ====================================================================
; .alloc_early — Allocate undo + clipboard before multi-file loading
; doc_swap_out reads undo_pos/undo_count from BSS, so undo_init must
; run first to zero them.  Returns: CF=1 if alloc fails.
; ====================================================================
.alloc_early:
    MOV     BX, UNDO_SEG_PARA
    MOV     AH, 48h
    INT     21h
    JC      .ae_fail
    MOV     [undo_seg], AX
    CALL    undo_init
    MOV     BX, CLIP_SEG_PARA
    MOV     AH, 48h
    INT     21h
    JC      .ae_fail
    MOV     [clip_seg], AX
    MOV     WORD [clip_used], 0
    CMP     WORD [shadow_seg], 0
    JNE     .ae_shadow_ok
    MOV     BX, SHADOW_SEG_PARA
    MOV     AH, 48h
    INT     21h
    JC      .ae_fail
    MOV     [shadow_seg], AX
.ae_shadow_ok:
    CMP     WORD [doc_table_seg], 0
    JNE     .ae_doctbl_ok
    MOV     BX, DOC_TBL_SEG_PARA
    MOV     AH, 48h
    INT     21h
    JC      .ae_fail
    MOV     [doc_table_seg], AX
.ae_doctbl_ok:
    CLC
    RET
.ae_fail:
    STC
    RET

; ====================================================================
; .load_startup_file — Load [filename] into a document slot at startup
; Uses [prj_first_file] to decide slot 0 vs new slot.
; Returns: CF=0 success, CF=1 failure
; ====================================================================
.load_startup_file:
    CMP     BYTE [prj_first_file], 1
    JNE     .lsf_subsequent

    ; ---- First file → slot 0 ----
    MOV     BYTE [prj_first_file], 0
    XOR     AL, AL
    MOV     SI, filename
    CALL    doc_slot_init
    MOV     BYTE [active_doc], 0
    MOV     BYTE [doc_count], 1
    JMP     .lsf_open

.lsf_subsequent:
    ; ---- Subsequent → swap out current, allocate new slot ----
    CALL    doc_swap_out
    JC      .lsf_fail
    CALL    doc_find_free_slot
    JC      .lsf_fail
    PUSH    AX                          ; save slot index (doc_slot_init clobbers AL)
    MOV     SI, filename
    CALL    doc_slot_init
    POP     AX                          ; restore slot index
    MOV     [active_doc], AL
    MOV     [panel_sel], AL
    INC     BYTE [doc_count]

.lsf_open:
    CALL    open_file
    JC      .lsf_fail
    CALL    load_file
    JC      .lsf_fail
    ; (file handle now owned by orig_file_hnd — do NOT close)
    CALL    pt_init
    JC      .lsf_fail
    MOV     BX, ADD_BUF_PARA
    MOV     AH, 48h
    INT     21h
    JC      .lsf_fail
    MOV     [add_seg], AX
    MOV     WORD [add_used], 0
    CALL    rebuild_meta
    CALL    clamp_cursor
    MOV     BYTE [meta_dirty], 0
    MOV     WORD [top_line], 0
    MOV     WORD [cur_line], 0
    MOV     WORD [cur_col], 0
    MOV     BYTE [dirty], 0
    MOV     WORD [last_ins_pi], 0FFFFh
    CALL    undo_init
    CLC
    RET
.lsf_fail:
    STC
    RET

; ====================================================================
; .load_multi_cmdline — Load multiple files from command line
; Iterates arg_offsets[], loading each file into a separate slot.
; ====================================================================
.load_multi_cmdline:
    CALL    .alloc_early
    JC      .err_nomem

    MOV     BYTE [prj_first_file], 1
    XOR     BX, BX                      ; BX = file index (BL used)

.lm_loop:
    CMP     BL, [arg_file_count]
    JAE     .lm_done
    CMP     BL, MAX_DOCS
    JAE     .lm_done

    ; ---- Get filename from saved PSP offset ----
    PUSH    BX
    XOR     BH, BH
    SHL     BX, 1
    MOV     SI, [BX + rd_save_buf]

    ; Copy to [filename]
    MOV     DI, filename
.lm_copy:
    MOV     AL, [SI]
    CMP     AL, ' '
    JE      .lm_copy_end
    CMP     AL, 0Dh
    JE      .lm_copy_end
    CMP     AL, 0Ah
    JE      .lm_copy_end
    MOV     [DI], AL
    INC     SI
    INC     DI
    JMP     .lm_copy
.lm_copy_end:
    MOV     BYTE [DI], 0

    CALL    .load_startup_file
    JC      .lm_fail

    POP     BX
    INC     BL
    JMP     .lm_loop

.lm_fail:
    POP     BX
    JMP     .err_nofile

.lm_done:
    JMP     .start_tui

; ====================================================================
; .load_prj_cmdline — Load a .PRJ project file from command line
; Opens the PRJ, reads filenames, loads each into a slot.
; ====================================================================
.load_prj_cmdline:
    CALL    .alloc_early
    JC      .err_nomem

    ; ---- Open and read the .PRJ file ----
    CALL    open_file
    JC      .err_nofile

    MOV     BX, [file_hnd]
    MOV     DX, rd_save_buf
    MOV     CX, 1020
    MOV     AH, 3Fh
    INT     21h
    PUSH    AX                          ; save bytes read
    MOV     BX, [file_hnd]
    MOV     AH, 3Eh
    INT     21h                         ; close PRJ file
    POP     AX
    MOV     [prj_bytes_read], AX

    CMP     WORD [prj_bytes_read], 0
    JE      .no_file                    ; empty PRJ → untitled doc

    ; ---- Parse lines from rd_save_buf ----
    MOV     BYTE [prj_first_file], 1
    MOV     SI, rd_save_buf
    MOV     CX, [prj_bytes_read]

.lp_parse:
    OR      CX, CX
    JZ      .lp_done
.lp_skip_ws:
    CMP     BYTE [SI], 0Dh
    JE      .lp_skip1
    CMP     BYTE [SI], 0Ah
    JE      .lp_skip1
    CMP     BYTE [SI], ' '
    JE      .lp_skip1
    JMP     .lp_got_line
.lp_skip1:
    INC     SI
    DEC     CX
    JNZ     .lp_skip_ws
    JMP     .lp_done

.lp_got_line:
    ; Copy line to [filename]
    MOV     DI, filename
    XOR     DL, DL                      ; DL = char count
.lp_copy:
    OR      CX, CX
    JZ      .lp_line_end
    MOV     AL, [SI]
    CMP     AL, 0Dh
    JE      .lp_line_end
    CMP     AL, 0Ah
    JE      .lp_line_end
    MOV     [DI], AL
    INC     SI
    INC     DI
    INC     DL
    DEC     CX
    CMP     DL, 127
    JB      .lp_copy
.lp_line_end:
    MOV     BYTE [DI], 0
    TEST    DL, DL
    JZ      .lp_parse                   ; empty line → skip

    PUSH    CX
    PUSH    SI

    CALL    .load_startup_file
    JC      .lp_fail

    POP     SI
    POP     CX
    JMP     .lp_parse

.lp_fail:
    POP     SI
    POP     CX
    JMP     .err_nofile

.lp_done:
    CMP     BYTE [doc_count], 0
    JE      .no_file
    JMP     .start_tui

; ====================================================================
; .err_mixed — User specified both .PRJ and regular files
; ====================================================================
.err_mixed:
    MOV     DX, s_err_mixed
    MOV     AH, 09h
    INT     21h
    INT     20h

.no_file:
    CALL    ed_new_doc
    JC      .err_nomem

    ; ---- Register untitled doc in slot 0 ----
    MOV     AL, 0
    XOR     SI, SI
    CALL    doc_slot_init
    MOV     BYTE [active_doc], 0
    MOV     BYTE [doc_count], 1

    ; ---- Copy generated "unsaved1.txt" to BSS filename ----
    MOV     ES, [doc_table_seg]
    MOV     SI, DOC_FILENAME        ; SI = offset within doc_table_seg
    PUSH    DS
    PUSH    ES
    POP     DS                      ; DS = doc_table_seg for LODSB
    MOV     DI, filename
.nf_cp:
    LODSB
    MOV     CS:[DI], AL             ; filename in original DS (=CS for .COM)
    INC     DI
    TEST    AL, AL
    JNZ     .nf_cp
    POP     DS

    ; ---- Runtime verification (--build_trace only) ----
    ASSERT_EQ BYTE [active_doc], 0
    ASSERT_EQ BYTE [doc_count], 1
    ; doc_table moved to separate segment — debug assertions commented out
    ;ASSERT_EQ BYTE [doc_table + DOC_FLAGS], DOCF_OCCUPIED | DOCF_UNTITLED
    ;ASSERT_EQ BYTE [doc_table + DOC_SLOT_ID], 0
    ;ASSERT_EQ BYTE [doc_table + DOC_FILENAME], 'u'
    ;ASSERT_EQ BYTE [doc_table + DOC_SWAP_PATH], 'T'
    ;ASSERT_EQ BYTE [doc_table + DOC_SWAP_PATH + 7], '0'
    ;LOG "untitled_flags", BYTE [doc_table + DOC_FLAGS]

    ; doc_table moved to separate segment — snapshot commented out
    ;MEM_SNAPSHOT doc_tbl_snap, DS, doc_table, MAX_DOCS * DOC_SLOT_SIZE

    JMP     .start_tui

.do_recovery:
    ; ---- Orphan swap file: user chose Restore ----
    ; shell_swap_in allocates ALL segments and restores full state
    CALL    shell_swap_in
    JC      .recovery_failed
    JMP     .init_tui               ; skip state reset, go straight to TUI

.recovery_failed:
    MOV     AH, 09h
    MOV     DX, s_err_shl_fatal
    INT     21h
    MOV     AX, 4C01h
    INT     21h

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

    ; ---- Allocate cache buffers (if not already allocated) ----
    CMP     WORD [cache_seg], 0
    JNE     .cache_ready
    CALL    cache_init
    JC      .err_nomem
.cache_ready:

    ; ---- Allocate shadow buffer segment (if not already allocated) ----
    CMP     WORD [shadow_seg], 0
    JNE     .shadow_ready
    MOV     BX, SHADOW_SEG_PARA
    MOV     AH, 48h
    INT     21h
    JC      .err_nomem
    MOV     [shadow_seg], AX
.shadow_ready:

    ; ---- Allocate document table segment (if not already allocated) ----
    CMP     WORD [doc_table_seg], 0
    JNE     .doctbl_ready
    MOV     BX, DOC_TBL_SEG_PARA
    MOV     AH, 48h
    INT     21h
    JC      .err_nomem
    MOV     [doc_table_seg], AX
.doctbl_ready:

    ; ---- Initialize editor state ----
    MOV     WORD [top_line], 0
    MOV     WORD [cur_line], 0
    MOV     WORD [cur_col], 0
    MOV     WORD [left_col], 0
    MOV     BYTE [dirty], 0
    MOV     WORD [last_ins_pi], 0FFFFh
    MOV     BYTE [sel_active], 0
    MOV     BYTE [overwrite_mode], 0

.init_tui:
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

    ; doc_table moved to separate segment — assertion commented out
    ;MEM_ASSERT doc_tbl_snap, DS, doc_table, MAX_DOCS * DOC_SLOT_SIZE

    ; ---- Phase 1 trace-mode checkpoint (all init complete) ----
    BREAKPOINT p1_ready, 999 : VRAMOUT

    ; Multi-document: slots are populated via File > New / File > Open.
    ; Switching via Alt+1-8, F6, Ctrl+PgDn/PgUp.

    ; ---- Show deferred warning if startup recovery lost data ----
    CMP     BYTE [shell_recovering], 0
    JE      .no_deferred_warn
    MOV     BYTE [shell_recovering], 0
    MOV     SI, s_err_shl_lost
    CALL    show_error_msg
.no_deferred_warn:

    ; ---- Run event loop ----
    CALL    tui_run

    ; ---- Delete all swap files (safety net) ----
    MOV     ES, [doc_table_seg]
    XOR     DI, DI
    XOR     CX, CX
.exit_del_swp:
    CMP     CL, MAX_DOCS
    JAE     .exit_del_done
    TEST    BYTE ES:[DI + DOC_FLAGS], DOCF_OCCUPIED
    JZ      .exit_del_next
    TEST    BYTE ES:[DI + DOC_FLAGS], DOCF_SWAPPED
    JZ      .exit_del_next
    MOV     DX, DI
    ADD     DX, DOC_SWAP_PATH
    PUSH    CX
    PUSH    DI
    PUSH    DS
    PUSH    ES
    POP     DS                      ; DS:DX = swap path in doc_table_seg
    MOV     AH, 41h
    INT     21h                     ; ignore errors
    POP     DS
    POP     DI
    POP     CX
.exit_del_next:
    ADD     DI, DOC_SLOT_SIZE
    INC     CL
    JMP     .exit_del_swp
.exit_del_done:

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
    JZ      .free_shadow
    MOV     ES, AX
    MOV     AH, 49h
    INT     21h
.free_shadow:
    ; Free shadow buffer segment (persistent — not freed by free_all)
    MOV     AX, [shadow_seg]
    TEST    AX, AX
    JZ      .free_doctbl
    MOV     ES, AX
    MOV     AH, 49h
    INT     21h
.free_cache:
    ; Free cache segments (persistent — not freed by free_all)
    XOR     SI, SI
.free_cache_loop:
    CMP     SI, CACHE_SLOTS * 2
    JAE     .free_doctbl
    MOV     AX, [SI + cache_seg]
    TEST    AX, AX
    JZ      .free_cache_next
    MOV     ES, AX
    MOV     AH, 49h
    INT     21h
.free_cache_next:
    ADD     SI, 2
    JMP     .free_cache_loop

.free_doctbl:
    ; Free document table segment (persistent — not freed by free_all)
    MOV     AX, [doc_table_seg]
    TEST    AX, AX
    JZ      .exit
    MOV     ES, AX
    MOV     AH, 49h
    INT     21h
.exit:
    ; ---- Restore startup working directory ----
    MOV     SI, startup_cwd
    CALL    swap_chdir

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
; Interrupt handlers (must be in code section, before BSS)
; ============================================================

; INT 23h — Ctrl+C/Break handler: ignore (TEDIT uses Ctrl+C for Copy)
_int23_handler:
    IRET

; INT 24h — Critical error handler: return FAIL to caller
_int24_handler:
    MOV     AL, 3
    IRET

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

SECTION .bss

ed_state:
DOC_CTX:
top_line:       RESW 1          ; +0   ED_SCROLLY
total_lines:    RESW 1          ; +2   ED_LINECOUNT
filename:       RESB 128        ; +4   current file path
orig_file_hnd:  RESW 1          ; +132  DOS file handle (0 = no backing file)
orig_file_size_lo: RESW 1       ; +134  file size low word
orig_file_size_hi: RESW 1       ; +136  file size high word
add_seg:        RESW 1          ; +138
add_used:       RESW 1          ; +140
pt_seg:         RESW 1          ; +142
pt_count:       RESW 1          ; +144
chkpt_num:      RESW 1          ; +146
chkpt_pi:       RESW MAX_CHKPT  ; +148  (4096 bytes)
chkpt_po:       RESW MAX_CHKPT  ; +4244 (4096 bytes)
cur_line:       RESW 1          ; +8340
cur_col:        RESW 1          ; +8342
dirty:          RESB 1          ; +8344
meta_dirty:     RESB 1          ; +8345
last_ins_pi:    RESW 1          ; +8346
DOC_CTX_END:                    ; +8348

; ---- Compile-time verification ----
PRINT "DOC_CTX_SIZE = ", DOC_CTX_END - DOC_CTX

; ============================================================
; TRANSIENT / GLOBAL — not part of document context
; These are scratch variables recomputed on every use, or
; shared global state. Not swapped between documents.
; ============================================================

open_file_buf:  RESB 14         ; temp buffer for file dialog
file_hnd:       RESW 1          ; file handle during load (transient)

; ---- LRU chunk cache (file-backed original buffer) ----
cache_seg:      RESW CACHE_SLOTS    ; segment address of each cache buffer
cache_chunk_id: RESW CACHE_SLOTS    ; file chunk loaded (CACHE_EMPTY = empty)
cache_lru:      RESW CACHE_SLOTS    ; access counter for LRU eviction
cache_lru_clock: RESW 1             ; monotonically increasing counter

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

; ---- Editor scrollbar ----
ed_text_cols:       RESW 1      ; effective text width (ED_COLS or ED_COLS-1)
ed_sb_active:       RESB 1      ; 1 if vertical scrollbar visible

; ---- Overwrite mode ----
overwrite_mode:     RESB 1      ; 0 = insert, 1 = overwrite

; ---- Document side panel ----
panel_visible:      RESB 1      ; 0 = hidden, 1 = shown
panel_sel:          RESB 1      ; highlighted slot index (0-19)

; ---- Goto ----
goto_buf:           RESB 6      ; line number input (max 5 digits + null)

; ---- Startup CWD (restored on exit) ----
startup_drive:      RESB 1      ; drive number (0=A, 1=B, 2=C...)
startup_path:       RESB 65     ; path returned by INT 21h AH=47h
startup_cwd:        RESB 68     ; "D:\path\0" format, filled at startup
doc_cwd:            RESB 68     ; active document's CWD, "D:\path\0" format
swap_hnd:           RESW 1      ; swap file handle during swap-in

; ---- Multi-document state ----
active_doc:     RESB 1          ; index of currently active document slot (0-19)
doc_count:      RESB 1          ; number of occupied slots (1-20)
doc_table_seg:  RESW 1          ; segment for doc_table (DOS alloc, 3 KB)

; ---- Open handler scratch ----
open_reuse_flag:  RESB 1          ; 1 = reuse current slot, 0 = new slot

; ---- Swap I/O buffer ----
swap_hdr_buf:   RESB SWAP_HEADER_SIZE   ; 64-byte swap file header staging

; ---- TUI BSS ----
tui_cga_snow:       RESB 1          ; 1 = CGA snow prevention in tui_blit
shadow_seg:         RESW 1          ; segment for shadow buffer (DOS alloc, 4 KB)
fw_state:           RESB 16
win_table:          RESB 320
z_order:            RESB 16
bdr_scratch:        RESB 7
ctrl_dispatch_addr: RESW 1
mouse_state:        RESB 21
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
dlg_dir_count:      RESB 1          ; directory entry count
dlg_dir_items:      RESB 64         ; 32 dir pointers (32 × 2)
dlg_dir_names:      RESB 448        ; 32 dir names (32 × 14)
dlg_file_pat_save:  RESB 14         ; saved pattern for Cancel restore
prj_file_hnd:       RESW 1          ; PRJ file handle during load/save
prj_bytes_read:     RESW 1          ; bytes read from PRJ file
prj_first_file:     RESB 1          ; flag: first file reuses slot 0
prj_pat_save:       RESB 14         ; saved dlg_file_pattern for project dialogs
arg_file_count:     RESB 1          ; number of filename args from command line
arg_has_prj:        RESB 1          ; 1 if a .PRJ file was in command-line args
; NOTE: arg offsets stored in rd_save_buf (reused — mutually exclusive with PRJ content)
shell_comspec:      RESB 128        ; COMSPEC path from environment
shell_cmdtail:      RESB 2          ; empty command tail (00h, 0Dh)
shell_fcb:          RESB 37         ; dummy FCB for EXEC
shell_exec_pb:      RESB 14         ; EXEC parameter block
shell_save_ss:      RESW 1          ; saved SS before EXEC
shell_save_sp:      RESW 1          ; saved SP before EXEC
shell_dump_mode:    RESB 1          ; 1 = /d flag: dump memory to disk on shell
shell_recovering:   RESB 1          ; 1 = orphan swap recovery in progress
BSS_END:
