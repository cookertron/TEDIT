; ============================================================
; ed_multidoc.inc — Multi-Document Management
;
; Procedures for creating, switching, and managing multiple
; open documents. Uses the DOC_CTX contiguous block defined
; in TEDIT.ASM BSS for save/restore via REP MOVSW.
;
; These procedures do NOT show error dialogs. They return
; CF=0 success or CF=1 failure. The caller handles errors.
; ============================================================

; ============================================================
; doc_save_active — Copy live BSS context to active doc's
; save segment. Preserves all registers.
; ============================================================

doc_save_active PROC
    PUSH    AX
    PUSH    BX
    PUSH    CX
    PUSH    SI
    PUSH    DI
    PUSH    ES

    MOV     BL, [doc_active]
    XOR     BH, BH
    SHL     BX, 1
    MOV     ES, [BX + doc_segs]    ; ES = active doc's save segment
    MOV     SI, DOC_CTX            ; DS:SI = live context (DS=CS)
    XOR     DI, DI                 ; ES:DI = save segment offset 0
    MOV     CX, DOC_CTX_SIZE / 2   ; 6220 words
    CLD
    REP     MOVSW

    POP     ES
    POP     DI
    POP     SI
    POP     CX
    POP     BX
    POP     AX
    RET
doc_save_active ENDP

; ============================================================
; doc_load_into_bss — Copy a doc's save segment into live BSS.
; Input: AL = document index to load
; Preserves all registers except flags.
; ============================================================

doc_load_into_bss PROC
    PUSH    AX
    PUSH    BX
    PUSH    CX
    PUSH    SI
    PUSH    DI
    PUSH    DS
    PUSH    ES

    MOV     BL, AL
    XOR     BH, BH
    SHL     BX, 1
    MOV     AX, [BX + doc_segs]   ; AX = save segment
    MOV     DS, AX                 ; DS = save segment
    PUSH    CS
    POP     ES                     ; ES = COM segment
    XOR     SI, SI                 ; DS:SI = save segment offset 0
    MOV     DI, DOC_CTX            ; ES:DI = live context
    MOV     CX, DOC_CTX_SIZE / 2   ; 6220 words
    CLD
    REP     MOVSW

    PUSH    CS
    POP     DS                     ; restore DS = CS

    POP     ES
    POP     DS
    POP     DI
    POP     SI
    POP     CX
    POP     BX
    POP     AX
    RET
doc_load_into_bss ENDP

; ============================================================
; doc_swap — Switch from active document to document AL.
; Input: AL = target document index (0-based)
; Caller must verify AL != doc_active.
; ============================================================

doc_swap PROC
    PUSH    AX

    ; Save current context to active doc's save segment
    CALL    doc_save_active

    ; Load target context from its save segment
    ; (AL preserved by doc_save_active)
    CALL    doc_load_into_bss

    ; Update active index and trigger redraw
    MOV     [doc_active], AL
    MOV     BYTE [fw_state + FW_DIRTY], 1

    POP     AX
    RET
doc_swap ENDP

; ============================================================
; doc_new — Create a new empty document in the next slot.
; Returns: CF=0 success, CF=1 failure
; On failure, the previously active document is restored.
; ============================================================

doc_new PROC
    PUSH    AX
    PUSH    BX
    PUSH    ES

    ; ---- Check document limit ----
    CMP     BYTE [doc_count], MAX_DOCS
    JB      .dn_limit_ok
    JMP     .dn_fail

.dn_limit_ok:
    ; ---- Allocate save-buffer segment for new doc ----
    MOV     BX, DOC_SAVE_PARA
    MOV     AH, 48h
    INT     21h
    JC      .dn_fail

    ; Store segment in doc_segs[doc_count]
    PUSH    AX                     ; save segment address
    MOV     BL, [doc_count]
    XOR     BH, BH
    SHL     BX, 1
    POP     AX
    MOV     [BX + doc_segs], AX

    ; ---- If there are existing docs, save current context ----
    CMP     BYTE [doc_count], 0
    JE      .dn_no_save
    CALL    doc_save_active
.dn_no_save:

    ; ---- Zero segment fields so error cleanup is unambiguous ----
    MOV     WORD [pt_seg], 0
    MOV     WORD [add_seg], 0
    MOV     BYTE [filename], 0

    ; ---- Initialize empty document in live BSS ----
    CALL    ed_new_doc
    JC      .dn_init_fail

    ; ---- Success: update management state ----
    MOV     AL, [doc_count]
    MOV     [doc_active], AL
    INC     BYTE [doc_count]
    MOV     BYTE [fw_state + FW_DIRTY], 1

    POP     ES
    POP     BX
    POP     AX
    CLC
    RET

.dn_init_fail:
    ; ed_new_doc failed — free any segment it managed to allocate.
    ; pt_seg may be non-zero (allocated before add_seg failed).
    ; add_seg is 0 (we zeroed it; ed_new_doc only writes on success).
    CMP     WORD [pt_seg], 0
    JZ      .dn_skip_free_pt
    PUSH    ES
    MOV     ES, [pt_seg]
    MOV     AH, 49h
    INT     21h
    POP     ES
.dn_skip_free_pt:

    ; Free the save-buffer segment we allocated for the new slot.
    MOV     BL, [doc_count]
    XOR     BH, BH
    SHL     BX, 1
    PUSH    ES
    MOV     ES, [BX + doc_segs]
    MOV     AH, 49h
    INT     21h
    POP     ES

    ; Restore previous document's context if there was one.
    CMP     BYTE [doc_count], 0
    JE      .dn_fail
    MOV     AL, [doc_active]
    CALL    doc_load_into_bss

.dn_fail:
    POP     ES
    POP     BX
    POP     AX
    STC
    RET
doc_new ENDP

; ============================================================
; doc_open — Open [filename] into a new document slot.
;
; ASSUMES: [filename] already contains the file path to open.
; For startup (doc_count==0): parse_args sets [filename].
;
; Returns: CF=0 success, CF=1 failure
; On failure, the previously active document is restored.
; ============================================================

doc_open PROC
    PUSH    AX
    PUSH    BX
    PUSH    CX
    PUSH    DX
    PUSH    SI
    PUSH    DI
    PUSH    ES

    ; ---- Check document limit ----
    CMP     BYTE [doc_count], MAX_DOCS
    JB      .do_limit_ok
    JMP     .do_fail

.do_limit_ok:
    ; ---- Allocate save-buffer segment ----
    MOV     BX, DOC_SAVE_PARA
    MOV     AH, 48h
    INT     21h
    JC      .do_fail

    ; Store in doc_segs[doc_count]
    PUSH    AX
    MOV     BL, [doc_count]
    XOR     BH, BH
    SHL     BX, 1
    POP     AX
    MOV     [BX + doc_segs], AX

    ; ---- Save current context if there are existing docs ----
    CMP     BYTE [doc_count], 0
    JE      .do_no_save
    CALL    doc_save_active
.do_no_save:

    ; ---- Zero segment fields for clean error recovery ----
    ; After save-out, BSS still has the old doc's segment addresses.
    ; Zeroing ensures free_all in error path only frees NEW allocations.
    MOV     WORD [orig_count], 0
    MOV     WORD [pt_seg], 0
    MOV     WORD [add_seg], 0

    ; ---- Open file ----
    CALL    open_file
    JC      .do_err_cleanup

    ; ---- Load file chunks ----
    CALL    load_file
    JC      .do_err_close_file

    ; ---- Close file handle (data is in memory now) ----
    MOV     BX, [file_hnd]
    MOV     AH, 3Eh
    INT     21h

    ; ---- Initialize piece table ----
    CALL    pt_init
    JC      .do_err_free

    ; ---- Allocate add buffer ----
    MOV     BX, ADD_BUF_PARA
    MOV     AH, 48h
    INT     21h
    JC      .do_err_free
    MOV     [add_seg], AX
    MOV     WORD [add_used], 0

    ; ---- Build metadata ----
    CALL    rebuild_meta
    CALL    clamp_cursor
    MOV     BYTE [meta_dirty], 0

    ; ---- Reset cursor / scroll state ----
    MOV     WORD [top_line], 0
    MOV     WORD [cur_line], 0
    MOV     WORD [cur_col], 0
    MOV     BYTE [dirty], 0
    MOV     WORD [last_ins_pi], 0FFFFh

    ; ---- Success: update management state ----
    MOV     AL, [doc_count]
    MOV     [doc_active], AL
    INC     BYTE [doc_count]
    MOV     BYTE [fw_state + FW_DIRTY], 1

    POP     ES
    POP     DI
    POP     SI
    POP     DX
    POP     CX
    POP     BX
    POP     AX
    CLC
    RET

.do_err_close_file:
    ; load_file failed — close the file handle, then free
    MOV     BX, [file_hnd]
    MOV     AH, 3Eh
    INT     21h
    ; fall through to .do_err_free

.do_err_free:
    ; Free any partially allocated document data.
    ; orig_count/orig_seg reflect loaded chunks (if any).
    ; pt_seg/add_seg are non-zero only if their allocations succeeded.
    ; free_all handles all combinations correctly.
    CALL    free_all
    ; fall through to .do_err_cleanup

.do_err_cleanup:
    ; Free the save-buffer segment we allocated for the new slot
    MOV     BL, [doc_count]
    XOR     BH, BH
    SHL     BX, 1
    PUSH    ES
    MOV     ES, [BX + doc_segs]
    MOV     AH, 49h
    INT     21h
    POP     ES

    ; Restore previous document if there was one
    CMP     BYTE [doc_count], 0
    JE      .do_fail
    MOV     AL, [doc_active]
    CALL    doc_load_into_bss

.do_fail:
    POP     ES
    POP     DI
    POP     SI
    POP     DX
    POP     CX
    POP     BX
    POP     AX
    STC
    RET
doc_open ENDP

; ============================================================
; doc_switch_next — Switch to the next document (wrapping).
; No-op if doc_count <= 1.
; ============================================================

doc_switch_next PROC
    CMP     BYTE [doc_count], 2
    JB      .dsn_done

    PUSH    AX
    MOV     AL, [doc_active]
    INC     AL
    CMP     AL, [doc_count]
    JB      .dsn_no_wrap
    XOR     AL, AL             ; wrap to 0
.dsn_no_wrap:
    CALL    doc_swap
    POP     AX

.dsn_done:
    RET
doc_switch_next ENDP
