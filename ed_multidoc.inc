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

    ; ---- Copy filename from source buffer ----
    ; [doc_fname_src] points to the filename string.
    ; For startup: points to [filename] (already set by parse_args).
    ; For menu open: points to [open_file_buf] (set by file dialog).
    ; Must happen AFTER doc_save_active so old doc keeps its filename.
    PUSH    SI
    PUSH    DI
    MOV     SI, [doc_fname_src]
    MOV     DI, filename
.do_copy_fn:
    MOV     AL, [SI]
    MOV     [DI], AL
    INC     SI
    INC     DI
    TEST    AL, AL
    JNZ     .do_copy_fn
    POP     DI
    POP     SI

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

; ============================================================
; doc_switch_prev — Switch to the previous document (wrapping).
; No-op if doc_count <= 1.
; ============================================================

doc_switch_prev PROC
    CMP     BYTE [doc_count], 2
    JB      .dsp_done

    PUSH    AX
    MOV     AL, [doc_active]
    OR      AL, AL
    JNZ     .dsp_no_wrap
    MOV     AL, [doc_count]        ; wrap: 0 → count-1
.dsp_no_wrap:
    DEC     AL
    CALL    doc_swap
    POP     AX

.dsp_done:
    RET
doc_switch_prev ENDP

; ============================================================
; doc_close — Close the active document.
; Prompts to save if dirty. On cancel, aborts close.
; If last document is closed, auto-creates a new empty one.
; ============================================================

doc_close PROC
    PUSH    AX
    PUSH    BX
    PUSH    CX
    PUSH    SI
    PUSH    DI
    PUSH    ES

    ; ---- If dirty, prompt to save ----
    CMP     BYTE [dirty], 0
    JE      .dc_do_close

    PUSH    SI
    PUSH    DI
    MOV     SI, m_str_quit_ask     ; "Save changes before closing?"
    MOV     DI, m_str_quit_ttl     ; "Quit" title (reuse)
    CALL    tui_dlg_confirm
    POP     DI
    POP     SI

    CMP     AL, DLG_YES
    JE      .dc_save
    CMP     AL, DLG_NO
    JE      .dc_do_close
    ; Cancel (Escape) — abort close
    JMP     .dc_done

.dc_save:
    ; Save the document (handle Save As if no filename)
    CMP     BYTE [filename], 0
    JE      .dc_save_as
    CALL    save_file
    JMP     .dc_do_close

.dc_save_as:
    PUSH    SI
    PUSH    DI
    PUSH    BX
    PUSH    CX
    MOV     SI, m_str_save_pr      ; "Filename:"
    MOV     DI, m_str_save_ttl     ; "Save As"
    MOV     BX, filename
    MOV     CL, 80
    CALL    tui_dlg_input
    POP     CX
    POP     BX
    POP     DI
    POP     SI
    CMP     AL, DLG_OK
    JNE     .dc_done               ; cancelled Save As → abort close
    CMP     BYTE [filename], 0
    JE      .dc_done               ; empty filename → abort close
    CALL    save_file

.dc_do_close:
    ; ---- Free active document's DOS allocations ----
    CALL    free_all

    ; ---- Free active document's save-buffer segment ----
    MOV     BL, [doc_active]
    XOR     BH, BH
    SHL     BX, 1
    PUSH    ES
    MOV     ES, [BX + doc_segs]
    MOV     AH, 49h
    INT     21h
    POP     ES

    ; ---- Compact doc_segs array ----
    ; Shift entries [doc_active+1 .. doc_count-1] down by one
    MOV     BL, [doc_active]
    XOR     BH, BH
.dc_compact:
    MOV     AL, BL
    INC     AL
    CMP     AL, [doc_count]
    JAE     .dc_compact_done
    ; doc_segs[BL] = doc_segs[BL+1]
    PUSH    BX
    SHL     BX, 1                  ; BX = BL * 2 (word index)
    MOV     AX, [BX + doc_segs + 2] ; next slot's segment
    MOV     [BX + doc_segs], AX    ; shift down
    POP     BX
    INC     BL
    JMP     .dc_compact
.dc_compact_done:

    ; ---- Decrement doc_count ----
    DEC     BYTE [doc_count]

    ; ---- If no documents left, auto-create empty ----
    CMP     BYTE [doc_count], 0
    JNE     .dc_load_another

    ; Create a fresh empty document
    CALL    doc_new
    ; doc_new sets doc_active=0, doc_count=1, FW_DIRTY=1
    JMP     .dc_done

.dc_load_another:
    ; ---- Adjust doc_active ----
    ; If we closed the last slot, move back by one
    MOV     AL, [doc_active]
    CMP     AL, [doc_count]
    JB      .dc_active_ok
    DEC     AL
    MOV     [doc_active], AL
.dc_active_ok:

    ; ---- Load the new active document from its save segment ----
    MOV     AL, [doc_active]
    CALL    doc_load_into_bss
    MOV     BYTE [fw_state + FW_DIRTY], 1

.dc_done:
    POP     ES
    POP     DI
    POP     SI
    POP     CX
    POP     BX
    POP     AX
    RET
doc_close ENDP

; ============================================================
; doc_quit_all — Close all documents and exit.
; Prompts to save each dirty document. Cancel aborts quit.
;
; Algorithm: save active to its segment so all docs are in
; save segments. Process from doc_count-1 down to 0: load
; each into BSS, handle dirty, free. No array compaction
; needed (always removing the last element).
; ============================================================

doc_quit_all PROC
    PUSH    AX
    PUSH    BX
    PUSH    SI
    PUSH    DI
    PUSH    ES

    ; ---- Save active context so all docs are in save segments ----
    CALL    doc_save_active

.dqa_loop:
    CMP     BYTE [doc_count], 0
    JE      .dqa_exit

    ; ---- Load last document into BSS ----
    MOV     AL, [doc_count]
    DEC     AL
    CALL    doc_load_into_bss
    MOV     [doc_active], AL

    ; ---- If dirty, prompt to save ----
    CMP     BYTE [dirty], 0
    JE      .dqa_not_dirty

    PUSH    SI
    PUSH    DI
    MOV     SI, m_str_quit_ask     ; "Save changes before closing?"
    MOV     DI, m_str_quit_ttl     ; "Quit"
    CALL    tui_dlg_confirm
    POP     DI
    POP     SI

    CMP     AL, DLG_YES
    JE      .dqa_save
    CMP     AL, DLG_NO
    JE      .dqa_not_dirty
    ; Cancel — abort quit. Current doc is in BSS, state is consistent.
    JMP     .dqa_cancel

.dqa_save:
    ; Save (handle Save As if no filename)
    CMP     BYTE [filename], 0
    JE      .dqa_save_as
    CALL    save_file
    JMP     .dqa_not_dirty

.dqa_save_as:
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
    JNE     .dqa_cancel            ; cancelled Save As → abort quit
    CMP     BYTE [filename], 0
    JE      .dqa_cancel            ; empty filename → abort quit
    CALL    save_file

.dqa_not_dirty:
    ; ---- Free this document's DOS allocations ----
    CALL    free_all

    ; ---- Free its save-buffer segment ----
    MOV     BL, [doc_active]
    XOR     BH, BH
    SHL     BX, 1
    PUSH    ES
    MOV     ES, [BX + doc_segs]
    MOV     AH, 49h
    INT     21h
    POP     ES

    ; ---- Decrement and loop ----
    DEC     BYTE [doc_count]
    JMP     .dqa_loop

.dqa_exit:
    ; All documents closed — exit application
    MOV     BYTE [fw_state + FW_RUNNING], 0

    POP     ES
    POP     DI
    POP     SI
    POP     BX
    POP     AX
    RET

.dqa_cancel:
    ; Quit aborted. Current document is in BSS, doc_active and
    ; doc_count are consistent (doc_count reflects remaining docs
    ; that haven't been freed yet).
    MOV     BYTE [fw_state + FW_DIRTY], 1

    POP     ES
    POP     DI
    POP     SI
    POP     BX
    POP     AX
    RET
doc_quit_all ENDP
