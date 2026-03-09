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

.err_open:
    ; Show error dialog — could not open file
    PUSH    SI
    PUSH    DI
    MOV     SI, m_str_err_open
    MOV     DI, m_str_error
    CALL    tui_dlg_msgbox
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
    MOV     DI, m_str_error
    CALL    tui_dlg_msgbox
    POP     DI
    POP     SI

.init_empty:
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
    MOV     BYTE [dirty], 0
    MOV     BYTE [meta_dirty], 0
    MOV     WORD [last_ins_pi], 0FFFFh
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
