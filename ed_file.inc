; ============================================================
; ed_file.inc — File Load/Reset Helpers
; ============================================================

; ============================================================
; open_ensure_drive — SelectDrive to match filename's drive prefix
;
; If [filename] begins with "X:" (A..Z / a..z + colon), calls
; INT 21h AH=0Eh to set the DOS default drive. Defends against
; divergence between TEDIT's tracked CWD and DOS's current drive
; after prior CHDIR activity (P1-#2 workaround).
; ============================================================
open_ensure_drive PROC
    PUSH    AX
    PUSH    DX
    MOV     AL, [filename]
    CMP     BYTE [filename + 1], ':'
    JNE     .oed_done
    OR      AL, 20h                 ; to lowercase
    CMP     AL, 'a'
    JB      .oed_done
    CMP     AL, 'z'
    JA      .oed_done
    SUB     AL, 'a'
    MOV     DL, AL
    MOV     AH, 0Eh
    INT     21h
.oed_done:
    POP     DX
    POP     AX
    RET
open_ensure_drive ENDP

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

    ; ---- Align DOS default drive with filename's drive prefix ----
    CALL    open_ensure_drive

    ; ---- Open file ----
    CALL    open_file
    JC      .err_open

    ; ---- Query file size, keep handle open for cache ----
    CALL    load_file
    JC      .err_load

    ; (file handle now owned by orig_file_hnd — do NOT close)

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

    ; ---- Detect line ending mode ----
    CALL    detect_line_ending
    CALL    update_le_label

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
    ; Close any open file handle and free allocated segments
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

    MOV     WORD [orig_file_hnd], 0
    MOV     WORD [orig_file_size_lo], 0
    MOV     WORD [orig_file_size_hi], 0

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
    MOV     BYTE [line_ending], LE_CRLF
    CALL    update_le_label
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
    ; save_file now handles the full cycle: write temp, rename,
    ; reopen, reset PT, rebuild meta, clear undo, reset add_used.
    ; ed_compact just needs to clamp cursor and update display state.
    CALL    clamp_cursor
    CALL    scroll_to_cursor
    MOV     BYTE [fw_state + FW_DIRTY], 1
    CLC
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

; ============================================================
; detect_line_ending — Detect CRLF vs LF from file content
;
; Scans the first piece for the first LF (0Ah). If preceded by
; CR (0Dh), sets line_ending = LE_CRLF. If bare LF, sets
; line_ending = LE_LF. No LF found → defaults to LE_CRLF.
;
; Must be called after pt_init / rebuild_meta (piece table ready).
; Output: [line_ending] set
; Clobbers: AX, BX, CX, DX, SI, DI, ES
; ============================================================
detect_line_ending PROC
    MOV     BYTE [line_ending], LE_CRLF  ; default

    CMP     WORD [pt_count], 0
    JE      .dle_done                    ; empty file → default

    ; Resolve first piece
    XOR     BX, BX                       ; piece index 0
    CALL    resolve_piece                ; AX=seg, SI=base, DX=length
    JC      .dle_done

    MOV     ES, AX
    MOV     DI, SI                       ; ES:DI = start of piece data
    MOV     CX, DX                       ; CX = piece length
    CMP     CX, 4096
    JBE     .dle_scan
    MOV     CX, 4096                     ; cap scan at 4 KB

.dle_scan:
    CLD
    MOV     AL, 0Ah
    REPNE   SCASB
    JNZ     .dle_done                    ; no LF found → default CRLF

    ; Found LF at ES:[DI-1]. SCASB incremented DI past it.
    ; Check if LF was the first byte of the piece (no preceding byte).
    MOV     AX, DI
    SUB     AX, SI                       ; AX = bytes consumed (1 if LF was first)
    CMP     AX, 1
    JE      .dle_bare_lf                 ; LF at position 0 → bare LF

    ; Check byte before the LF
    CMP     BYTE ES:[DI-2], 0Dh
    JE      .dle_done                    ; preceded by CR → CRLF (default)

.dle_bare_lf:
    MOV     BYTE [line_ending], LE_LF

.dle_done:
    RET
detect_line_ending ENDP
