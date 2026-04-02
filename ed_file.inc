; ============================================================
; ed_file.inc — File Load/Reset Helpers
; ============================================================

; ============================================================
; ed_load_file — Load the file named in [filename] into editor
; Frees existing document data first.
; Returns: CF=0 success, CF=1 failure
; ============================================================

ed_load_file PROC
    PUSH    AX
    PUSH    BX
    PUSH    CX
    PUSH    DX
    PUSH    SI
    PUSH    DI

    ; ---- Free existing document ----
    CALL    free_all

    ; ---- Open file ----
    CALL    open_file
    JC      .err_open

    ; ---- Load chunks ----
    CALL    load_file
    JC      .err_load

    ; ---- Close file handle ----
    MOV     BX, [file_hnd]
    MOV     AH, 3Eh
    INT     21h

    ; ---- Initialize piece table ----
    CALL    pt_init
    JC      .err_mem

    ; ---- Allocate add buffer ----
    MOV     BX, ADD_BUF_PARA
    MOV     AH, 48h
    INT     21h
    JC      .err_mem
    MOV     [add_seg], AX
    MOV     WORD [add_used], 0

    ; ---- Build metadata ----
    CALL    rebuild_meta
    CALL    clamp_cursor
    MOV     BYTE [meta_dirty], 0

    ; ---- Reset editor state ----
    MOV     WORD [top_line], 0
    MOV     WORD [cur_line], 0
    MOV     WORD [cur_col], 0
    MOV     WORD [left_col], 0
    MOV     BYTE [dirty], 0
    MOV     WORD [last_ins_pi], 0FFFFh
    MOV     BYTE [sel_active], 0
    CALL    undo_clear
    MOV     BYTE [fw_state + FW_DIRTY], 1

    POP     DI
    POP     SI
    POP     DX
    POP     CX
    POP     BX
    POP     AX
    CLC
    RET

.err_open:
    ; Show error dialog — could not open file
    PUSH    SI
    PUSH    DI
    MOV     SI, m_str_err_open
    CALL    show_error_msg
    POP     DI
    POP     SI
    ; Fall through to reinit empty doc
    JMP     .init_empty

.err_load:
    ; Close the file handle we opened
    MOV     BX, [file_hnd]
    MOV     AH, 3Eh
    INT     21h
    ; Fall through

.err_mem:
    ; Show error dialog — out of memory
    PUSH    SI
    PUSH    DI
    MOV     SI, m_str_err_mem
    CALL    show_error_msg
    POP     DI
    POP     SI

.init_empty:
    ; Free partially loaded orig chunks before reinit (load_file may
    ; have allocated some chunks before running out of memory; ed_new_doc
    ; zeroes orig_count without freeing them, leaking the segments)
    CALL    free_all
    ; Reinitialize as empty document so the editor isn't broken
    CALL    ed_new_doc
    MOV     BYTE [filename], 0         ; clear filename since load failed

    POP     DI
    POP     SI
    POP     DX
    POP     CX
    POP     BX
    POP     AX
    STC
    RET
ed_load_file ENDP

; ============================================================
; ed_new_doc — Initialize an empty document
; Allocates pt_seg and add_seg. Resets all editor state.
; Assumes free_all has been called or no prior allocations exist.
; Returns: CF=0 success, CF=1 allocation failure
; ============================================================

ed_new_doc PROC
    PUSH    AX
    PUSH    BX

    MOV     WORD [orig_count], 0

    ; Allocate piece table segment
    MOV     BX, PT_SEG_PARA
    MOV     AH, 48h
    INT     21h
    JC      .fail
    MOV     [pt_seg], AX
    MOV     WORD [pt_count], 0

    ; Allocate add buffer
    MOV     BX, ADD_BUF_PARA
    MOV     AH, 48h
    INT     21h
    JC      .fail
    MOV     [add_seg], AX
    MOV     WORD [add_used], 0

    ; Seed metadata
    MOV     WORD [total_lines], 1
    MOV     WORD [chkpt_num], 1
    MOV     WORD [chkpt_pi], 0
    MOV     WORD [chkpt_po], 0

    ; Reset editor state
    MOV     WORD [top_line], 0
    MOV     WORD [cur_line], 0
    MOV     WORD [cur_col], 0
    MOV     WORD [left_col], 0
    MOV     BYTE [dirty], 0
    MOV     BYTE [meta_dirty], 0
    MOV     WORD [last_ins_pi], 0FFFFh
    MOV     BYTE [sel_active], 0
    CALL    undo_clear
    MOV     BYTE [fw_state + FW_DIRTY], 1

    POP     BX
    POP     AX
    CLC
    RET

.fail:
    POP     BX
    POP     AX
    STC
    RET
ed_new_doc ENDP

; ============================================================
; ed_compact — Save file, reload from disk, reset add buffer.
;
; The file MUST have been saved before calling this (dirty=0).
; Preserves cur_line, cur_col, top_line across the reload.
; Resets: add_used=0, last_ins_pi=FFFFh, meta_dirty=0, dirty=0
;
; Returns: CF=0 success, CF=1 failure (editor left with empty
;          document if reload fails — file is safe on disk)
; ============================================================

ed_compact PROC
    PUSH    AX
    PUSH    BX
    PUSH    CX
    PUSH    DX
    PUSH    SI
    PUSH    DI

    ; ---- Save cursor state on stack (survives free_all) ----
    MOV     AX, [cur_line]
    PUSH    AX
    MOV     AX, [cur_col]
    PUSH    AX
    MOV     AX, [top_line]
    PUSH    AX
    MOV     AX, [left_col]
    PUSH    AX

    ; ---- Free all document memory ----
    CALL    free_all

    ; ---- Reopen the saved file ----
    CALL    open_file
    JC      .cpt_fail_restore

    ; ---- Reload file chunks ----
    CALL    load_file
    JC      .cpt_fail_close

    ; ---- Close file handle ----
    MOV     BX, [file_hnd]
    MOV     AH, 3Eh
    INT     21h

    ; ---- Initialize piece table ----
    CALL    pt_init
    JC      .cpt_fail_recover

    ; ---- Allocate fresh add buffer ----
    MOV     BX, ADD_BUF_PARA
    MOV     AH, 48h
    INT     21h
    JC      .cpt_fail_recover
    MOV     [add_seg], AX
    MOV     WORD [add_used], 0

    ; ---- Rebuild metadata ----
    CALL    rebuild_meta
    MOV     BYTE [meta_dirty], 0

    ; ---- Clear undo (compaction invalidates all undo records) ----
    CALL    undo_clear

    ; ---- Restore cursor state ----
    POP     AX
    MOV     [left_col], AX
    POP     AX
    MOV     [top_line], AX
    POP     AX
    MOV     [cur_col], AX
    POP     AX
    MOV     [cur_line], AX
    CALL    clamp_cursor
    CALL    scroll_to_cursor

    ; ---- Reset tracking state ----
    MOV     BYTE [dirty], 0
    MOV     WORD [last_ins_pi], 0FFFFh
    MOV     BYTE [fw_state + FW_DIRTY], 1

    POP     DI
    POP     SI
    POP     DX
    POP     CX
    POP     BX
    POP     AX
    CLC
    RET

.cpt_fail_close:
    ; load_file failed — close the file handle
    MOV     BX, [file_hnd]
    MOV     AH, 3Eh
    INT     21h
    JMP     .cpt_fail_recover

.cpt_fail_restore:
    ; open_file failed — nothing loaded, fall through to recover

.cpt_fail_recover:
    ; Reload failed. File is safe on disk.
    ; Create empty document so editor doesn't crash.
    CALL    ed_new_doc

    ; Restore cursor (just to pop the stack cleanly; clamp will fix)
    POP     AX
    MOV     [left_col], AX
    POP     AX
    MOV     [top_line], AX
    POP     AX
    MOV     [cur_col], AX
    POP     AX
    MOV     [cur_line], AX
    CALL    clamp_cursor

    ; Show error dialog
    PUSH    SI
    PUSH    DI
    MOV     SI, m_str_buf_err
    CALL    show_error_msg
    POP     DI
    POP     SI

    POP     DI
    POP     SI
    POP     DX
    POP     CX
    POP     BX
    POP     AX
    STC
    RET
ed_compact ENDP

; ============================================================
; ed_buffer_full — Handle add buffer exhaustion.
;
; Shows a confirm dialog. If user accepts, saves the file
; (prompting for filename if untitled) and compacts.
;
; Called from insert_char / insert_crlf when add buffer is full.
; TUI is guaranteed to be initialized at this point.
;
; Returns: CF=0 buffer flushed (editing can continue)
;          CF=1 user cancelled or compact failed
; ============================================================

ed_buffer_full PROC
    PUSH    SI
    PUSH    DI
    PUSH    BX
    PUSH    CX

    ; ---- Ask user ----
    MOV     SI, m_str_buf_ask
    MOV     DI, m_str_buf_ttl
    CALL    tui_dlg_confirm

    CMP     AL, DLG_YES
    JNE     .bf_cancel

    ; ---- Save file (handles Save As if no filename) ----
    CALL    menu_save_handler

    ; ---- Check if save actually succeeded ----
    CMP     BYTE [dirty], 0
    JNE     .bf_cancel         ; save failed or user cancelled Save As

    ; ---- Compact: reload from disk ----
    CALL    ed_compact
    ; CF set by ed_compact

    POP     CX
    POP     BX
    POP     DI
    POP     SI
    RET

.bf_cancel:
    POP     CX
    POP     BX
    POP     DI
    POP     SI
    STC
    RET
ed_buffer_full ENDP
