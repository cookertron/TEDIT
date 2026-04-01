; ============================================================
; ed_multidoc.inc — Multi-Document Table Management
;
; Provides the document table data structure and slot
; management primitives for the Hot Seat multi-doc architecture.
; ============================================================

; ============================================================
; doc_table_init — Zero all 8 document table slots
;
; Called once at startup, before any document is loaded.
; Sets all flags to 0 (unoccupied), all fields zeroed.
; Sets active_doc = 0, doc_count = 0.
;
; Input:  None
; Output: None
; Clobbers: AX, CX, DI, ES
; ============================================================
doc_table_init PROC
    ; Zero the entire doc_table (MAX_DOCS * DOC_SLOT_SIZE bytes)
    MOV     ES, [doc_table_seg]
    XOR     DI, DI
    MOV     CX, MAX_DOCS * DOC_SLOT_SIZE
    XOR     AL, AL
    REP     STOSB

    ; Init global state
    MOV     BYTE [active_doc], 0
    MOV     BYTE [doc_count], 0

    ; Write constant DOC_SLOT_ID for each slot (0 to MAX_DOCS-1)
    MOV     ES, [doc_table_seg]
    XOR     DI, DI
    XOR     AL, AL
.init_ids:
    MOV     ES:[DI + DOC_SLOT_ID], AL
    ADD     DI, DOC_SLOT_SIZE
    INC     AL
    CMP     AL, MAX_DOCS
    JB      .init_ids

    RET
doc_table_init ENDP

; ============================================================
; doc_get_slot — Return pointer to document table slot
;
; Input:  AL = slot index (0-7)
; Output: ES:DI = pointer to DOC_SLOT struct in doc_table_seg
; Clobbers: AX, ES
; ============================================================
doc_get_slot PROC
    PUSH    DX
    MOV     ES, [doc_table_seg]
    XOR     AH, AH
    MOV     DI, DOC_SLOT_SIZE
    MUL     DI              ; AX = slot_index * 152 (DX clobbered)
    MOV     DI, AX          ; DI = offset within doc_table_seg
    POP     DX
    RET
doc_get_slot ENDP

; ============================================================
; doc_find_free_slot — Find first free slot in document table
;
; Input:  None
; Output: CF=0 found: AL = slot index (0 to MAX_DOCS-1)
;         CF=1 not found: all slots occupied
; Clobbers: AX, DI, ES
; ============================================================
doc_find_free_slot PROC
    MOV     ES, [doc_table_seg]
    XOR     DI, DI
    XOR     AL, AL
.scan:
    TEST    BYTE ES:[DI + DOC_FLAGS], DOCF_OCCUPIED
    JZ      .found
    ADD     DI, DOC_SLOT_SIZE
    INC     AL
    CMP     AL, MAX_DOCS
    JB      .scan
    ; All occupied
    STC
    RET
.found:
    CLC
    RET
doc_find_free_slot ENDP

; ============================================================
; doc_slot_init — Initialize a document table slot
;
; Sets the slot as occupied, copies a filename into it,
; clears cached fields, and generates the swap path.
;
; Input:  AL = slot index (0 to MAX_DOCS-1)
;         SI = pointer to filename (null-terminated, or SI=0 for untitled)
; Output: ES:DI = pointer to initialized slot
; Clobbers: AX, BX, CX, DI, ES
; ============================================================
doc_slot_init PROC
    PUSH    SI

    ; Save slot index for swap path generation
    PUSH    AX

    ; Get slot pointer
    CALL    doc_get_slot        ; ES:DI = slot pointer

    ; Set flags
    POP     AX                  ; restore slot index in AL
    PUSH    AX                  ; save again for swap path

    TEST    SI, SI
    JZ      .untitled
    MOV     BYTE ES:[DI + DOC_FLAGS], DOCF_OCCUPIED
    JMP     .copy_name

.untitled:
    MOV     BYTE ES:[DI + DOC_FLAGS], DOCF_OCCUPIED | DOCF_UNTITLED
    ; Generate "unsavedNN.txt" where NN = slot index + 1 (01-20)
    PUSH    DI
    PUSH    AX
    ADD     DI, DOC_FILENAME
    MOV     BYTE ES:[DI+0], 'u'
    MOV     BYTE ES:[DI+1], 'n'
    MOV     BYTE ES:[DI+2], 's'
    MOV     BYTE ES:[DI+3], 'a'
    MOV     BYTE ES:[DI+4], 'v'
    MOV     BYTE ES:[DI+5], 'e'
    MOV     BYTE ES:[DI+6], 'd'
    INC     AL                      ; slot+1 → 1-20
    XOR     AH, AH
    MOV     CL, 10
    DIV     CL                      ; AL = tens, AH = ones
    ADD     AL, '0'
    MOV     ES:[DI+7], AL
    MOV     AL, AH
    ADD     AL, '0'
    MOV     ES:[DI+8], AL
    MOV     BYTE ES:[DI+9], '.'
    MOV     BYTE ES:[DI+10], 't'
    MOV     BYTE ES:[DI+11], 'x'
    MOV     BYTE ES:[DI+12], 't'
    MOV     BYTE ES:[DI+13], 0
    POP     AX
    POP     DI
    JMP     .gen_swap

.copy_name:
    ; Copy filename from SI to ES:[DI + DOC_FILENAME], max 127 + null
    PUSH    DI
    ADD     DI, DOC_FILENAME
    MOV     CX, 127
.copy_loop:
    MOV     AL, [SI]
    MOV     ES:[DI], AL
    TEST    AL, AL
    JZ      .copy_done
    INC     SI
    INC     DI
    DEC     CX
    JNZ     .copy_loop
    MOV     BYTE ES:[DI], 0       ; ensure null termination
.copy_done:
    POP     DI

.gen_swap:
    ; Generate swap path "TEDT00NN.SWP" where NN = slot index (00-19)
    POP     AX                  ; AL = slot index
    MOV     BX, DI
    ADD     BX, DOC_SWAP_PATH
    MOV     BYTE ES:[BX+0], 'T'
    MOV     BYTE ES:[BX+1], 'E'
    MOV     BYTE ES:[BX+2], 'D'
    MOV     BYTE ES:[BX+3], 'T'
    MOV     BYTE ES:[BX+4], '0'
    MOV     BYTE ES:[BX+5], '0'
    ; Two-digit slot index (00-19)
    XOR     AH, AH
    MOV     CL, 10
    DIV     CL                  ; AL = tens, AH = ones
    ADD     AL, '0'
    MOV     ES:[BX+6], AL
    MOV     AL, AH
    ADD     AL, '0'
    MOV     ES:[BX+7], AL
    MOV     BYTE ES:[BX+8], '.'
    MOV     BYTE ES:[BX+9], 'S'
    MOV     BYTE ES:[BX+10], 'W'
    MOV     BYTE ES:[BX+11], 'P'
    MOV     BYTE ES:[BX+12], 0

    ; Zero cached fields
    MOV     WORD ES:[DI + DOC_CACHED_LINES], 0
    MOV     WORD ES:[DI + DOC_CACHED_LINE], 0
    MOV     WORD ES:[DI + DOC_CACHED_COL], 0
    MOV     WORD ES:[DI + DOC_RESERVED], 0

    POP     SI
    RET
doc_slot_init ENDP

; ============================================================
; doc_slot_clear — Release a document table slot
;
; Clears the occupied flag. Does NOT free memory or delete
; swap files — caller must do that before calling this.
;
; Input:  AL = slot index (0-7)
; Output: None
; Clobbers: AX, DI, ES
; ============================================================
doc_slot_clear PROC
    CALL    doc_get_slot        ; ES:DI = slot pointer
    MOV     BYTE ES:[DI + DOC_FLAGS], 0
    DEC     BYTE [doc_count]
    RET
doc_slot_clear ENDP

; ============================================================
; doc_swap_out — Serialize active document to swap file
;
; Writes the swap file (header + piece table + add buffer +
; undo records + rd_save_buf), caches display info in the
; document table slot, then frees all document memory segments.
;
; Segments NOT freed: undo_seg (persistent, reused by next doc)
;                     clip_seg (shared clipboard)
;
; On failure: partial swap file is deleted, segments are NOT
;             freed, document remains active.
;
; Input:  None (operates on active_doc slot)
; Output: CF=0 success, CF=1 failure
; Clobbers: AX, BX, CX, DX, SI, DI, ES
; ============================================================
doc_swap_out PROC
    PUSH    BP
    MOV     BP, SP

    LOG "swap_out_start", BYTE [active_doc]

    ; Get active slot pointer
    MOV     AL, [active_doc]
    CALL    doc_get_slot            ; ES:DI = slot pointer
    PUSH    DI                      ; save slot offset on stack

    ; ---- Cache display info in slot before anything can fail ----
    MOV     AX, [total_lines]
    MOV     ES:[DI + DOC_CACHED_LINES], AX
    MOV     AX, [cur_line]
    MOV     ES:[DI + DOC_CACHED_LINE], AX
    MOV     AX, [cur_col]
    MOV     ES:[DI + DOC_CACHED_COL], AX
    ; Cache dirty flag
    TEST    BYTE [dirty], 0FFh
    JZ      .cache_not_dirty
    OR      BYTE ES:[DI + DOC_FLAGS], DOCF_DIRTY
    JMP     .cache_done
.cache_not_dirty:
    AND     BYTE ES:[DI + DOC_FLAGS], ~DOCF_DIRTY
.cache_done:

    ; ---- Zero the header buffer first ----
    PUSH    DI
    PUSH    ES
    MOV     AX, DS
    MOV     ES, AX              ; ES=DS for STOSB into BSS
    MOV     DI, swap_hdr_buf
    MOV     CX, SWAP_HEADER_SIZE
    XOR     AL, AL
    REP     STOSB
    POP     ES
    POP     DI

    ; ---- Fill header fields ----
    MOV     WORD [swap_hdr_buf + SWH_MAGIC], SWAP_MAGIC
    MOV     WORD [swap_hdr_buf + SWH_VERSION], SWAP_VERSION

    MOV     AX, [pt_count]
    MOV     [swap_hdr_buf + SWH_PT_COUNT], AX

    MOV     AX, [add_used]
    MOV     [swap_hdr_buf + SWH_ADD_USED], AX

    MOV     AX, [undo_pos]
    MOV     [swap_hdr_buf + SWH_UNDO_POS], AX

    MOV     AX, [undo_count]
    MOV     [swap_hdr_buf + SWH_UNDO_COUNT], AX

    MOV     AX, [undo_save_pos]
    MOV     [swap_hdr_buf + SWH_UNDO_SPOS], AX

    MOV     AX, [rd_save_used]
    MOV     [swap_hdr_buf + SWH_RD_SAVE_USED], AX

    MOV     AX, [cur_line]
    MOV     [swap_hdr_buf + SWH_CUR_LINE], AX

    MOV     AX, [cur_col]
    MOV     [swap_hdr_buf + SWH_CUR_COL], AX

    MOV     AX, [top_line]
    MOV     [swap_hdr_buf + SWH_TOP_LINE], AX

    MOV     AX, [left_col]
    MOV     [swap_hdr_buf + SWH_LEFT_COL], AX

    MOV     AL, [dirty]
    MOV     [swap_hdr_buf + SWH_DIRTY], AL

    MOV     BYTE [swap_hdr_buf + SWH_META_DIRTY], 1

    MOV     AL, [overwrite_mode]
    MOV     [swap_hdr_buf + SWH_OVERWRITE], AL

    MOV     AL, [find_flags]
    MOV     [swap_hdr_buf + SWH_FIND_FLAGS], AL

    MOV     AX, [tab_width]
    MOV     [swap_hdr_buf + SWH_TAB_WIDTH], AX

    MOV     AX, [tab_mask]
    MOV     [swap_hdr_buf + SWH_TAB_MASK], AX

    MOV     AX, [total_lines]
    MOV     [swap_hdr_buf + SWH_TOTAL_LINES], AX

    MOV     AX, [orig_count]
    MOV     [swap_hdr_buf + SWH_ORIG_COUNT], AX

    ; ---- Copy doc_cwd into header ----
    PUSH    DI
    PUSH    ES
    MOV     AX, DS
    MOV     ES, AX
    MOV     SI, doc_cwd
    MOV     DI, swap_hdr_buf + SWH_DOC_CWD
    MOV     CX, SWH_DOC_CWD_SIZE
    REP     MOVSB
    POP     ES
    POP     DI

    LOG "swap_hdr_magic", WORD [swap_hdr_buf + SWH_MAGIC]
    LOG "swap_hdr_pt_count", WORD [swap_hdr_buf + SWH_PT_COUNT]
    LOG "swap_hdr_add_used", WORD [swap_hdr_buf + SWH_ADD_USED]

    ; ---- CHDIR to startup dir for swap file I/O ----
    PUSH    SI
    MOV     SI, startup_cwd
    CALL    swap_chdir
    POP     SI

    ; ---- Create/truncate swap file ----
    POP     DI                      ; restore slot offset
    PUSH    DI                      ; keep on stack
    MOV     DX, DI
    ADD     DX, DOC_SWAP_PATH       ; DX = offset of swap filename in doc_table_seg
    PUSH    DS
    PUSH    ES
    POP     DS                      ; DS = doc_table_seg (for DS:DX)
    XOR     CX, CX                  ; normal attributes
    MOV     AH, 3Ch
    INT     21h
    POP     DS                      ; restore DS
    JC      .fail                   ; can't create file
    MOV     [file_hnd], AX

    LOG "swap_file_created", WORD [file_hnd]

    ; ---- Write 64-byte header ----
    MOV     BX, [file_hnd]
    MOV     DX, swap_hdr_buf
    MOV     CX, SWAP_HEADER_SIZE
    MOV     AH, 40h
    INT     21h
    JC      .fail_close
    CMP     AX, CX
    JNE     .fail_close

    ; ---- Write piece table (pt_count * PT_DESC_SIZE from pt_seg) ----
    MOV     AX, [pt_count]
    MOV     CX, PT_DESC_SIZE
    MUL     CX                      ; AX = byte count (DX:AX, but <=40960)
    MOV     CX, AX
    JCXZ    .skip_pt

    PUSH    DS
    MOV     DS, CS:[pt_seg]
    XOR     DX, DX
    MOV     BX, CS:[file_hnd]
    MOV     AH, 40h
    INT     21h
    POP     DS
    JC      .fail_close
    CMP     AX, CX
    JNE     .fail_close
.skip_pt:

    ; ---- Write add buffer (add_used bytes from add_seg) ----
    MOV     CX, [add_used]
    JCXZ    .skip_add

    PUSH    DS
    MOV     DS, CS:[add_seg]
    XOR     DX, DX
    MOV     BX, CS:[file_hnd]
    MOV     AH, 40h
    INT     21h
    POP     DS
    JC      .fail_close
    CMP     AX, CX
    JNE     .fail_close
.skip_add:

    ; ---- Write undo records (undo_count * UR_SIZE from undo_seg) ----
    MOV     AX, [undo_count]
    MOV     CX, UR_SIZE
    MUL     CX                      ; AX = byte count
    MOV     CX, AX
    JCXZ    .skip_undo

    PUSH    DS
    MOV     DS, CS:[undo_seg]
    XOR     DX, DX
    MOV     BX, CS:[file_hnd]
    MOV     AH, 40h
    INT     21h
    POP     DS
    JC      .fail_close
    CMP     AX, CX
    JNE     .fail_close
.skip_undo:

    ; ---- Write rd_save_buf (rd_save_used * PT_DESC_SIZE from BSS) ----
    MOV     AX, [rd_save_used]
    MOV     CX, PT_DESC_SIZE
    MUL     CX
    MOV     CX, AX
    JCXZ    .skip_rd

    MOV     DX, rd_save_buf
    MOV     BX, [file_hnd]
    MOV     AH, 40h
    INT     21h
    JC      .fail_close
    CMP     AX, CX
    JNE     .fail_close
.skip_rd:

    ; ---- Close swap file — success ----
    MOV     BX, [file_hnd]
    MOV     AH, 3Eh
    INT     21h

    ; ---- Free document memory segments ----
    CALL    .free_doc_segs

    LOG "swap_out_success", BYTE [active_doc]
    LOG "swap_segs_freed", WORD [orig_count]

    ; ---- Mark slot as swapped ----
    POP     DI                      ; restore slot offset
    MOV     ES, [doc_table_seg]     ; re-load ES (clobbered by .free_doc_segs)
    OR      BYTE ES:[DI + DOC_FLAGS], DOCF_SWAPPED

    POP     BP
    CLC
    RET

.fail_close:
    ; Close the partial swap file
    MOV     BX, [file_hnd]
    MOV     AH, 3Eh
    INT     21h

    ; Delete the partial swap file
    POP     DI                      ; restore slot offset
    PUSH    DI
    MOV     DX, DI
    ADD     DX, DOC_SWAP_PATH
    PUSH    DS
    MOV     DS, CS:[doc_table_seg]  ; DS:DX = swap path in doc_table_seg
    MOV     AH, 41h                 ; delete file
    INT     21h                     ; ignore error
    POP     DS

.fail:
    ; Do NOT free segments — document remains active
    POP     DI                      ; balance the stack
    POP     BP
    STC
    RET

; ---- .free_doc_segs — Free orig_seg[], pt_seg, add_seg ----
; undo_seg and clip_seg are NOT freed (persistent/shared).
.free_doc_segs:
    ; Free all orig_seg[] chunks
    XOR     CX, CX
.free_orig_loop:
    CMP     CX, [orig_count]
    JAE     .free_pt
    MOV     BX, CX
    SHL     BX, 1                   ; BX = CX * 2 (word index)
    MOV     AX, [BX + orig_seg]
    TEST    AX, AX
    JZ      .free_orig_next
    MOV     ES, AX
    MOV     AH, 49h
    INT     21h
    MOV     WORD [BX + orig_seg], 0
.free_orig_next:
    INC     CX
    JMP     .free_orig_loop

.free_pt:
    ; Free pt_seg
    MOV     AX, [pt_seg]
    TEST    AX, AX
    JZ      .free_add
    MOV     ES, AX
    MOV     AH, 49h
    INT     21h
    MOV     WORD [pt_seg], 0

.free_add:
    ; Free add_seg
    MOV     AX, [add_seg]
    TEST    AX, AX
    JZ      .free_done
    MOV     ES, AX
    MOV     AH, 49h
    INT     21h
    MOV     WORD [add_seg], 0

.free_done:
    ; Zero counts to prevent stale references
    MOV     WORD [orig_count], 0
    MOV     WORD [pt_count], 0
    MOV     WORD [add_used], 0
    RET

doc_swap_out ENDP

; ============================================================
; doc_swap_in — Restore a swapped-out document from disk
;
; Reopens the original file, reloads chunk data, then reads
; the swap file to restore the edited piece table, add buffer,
; undo history, and all cursor/editor state.
;
; For untitled documents: skips file reload, uses ed_new_doc
; to allocate empty structures, then populates from swap.
;
; On success: swap file is deleted, slot SWAPPED flag cleared,
;             active_doc updated, screen marked dirty.
;
; On failure: partial state may be allocated — caller must
;             handle cleanup. Returns CF=1.
;
; Input:  AL = slot index to swap in
; Output: CF=0 success, CF=1 failure
; Clobbers: AX, BX, CX, DX, SI, DI, BP, ES
; ============================================================
doc_swap_in PROC
    PUSH    BP
    MOV     BP, SP
    PUSH    AX                      ; save slot index [BP-2]

    XOR     AH, AH
    LOG "swap_in_start", AX

    CALL    doc_get_slot            ; ES:DI = slot pointer
    PUSH    DI                      ; save slot offset [BP-4]

    ; ---- CHDIR to startup dir for swap file access ----
    PUSH    SI
    MOV     SI, startup_cwd
    CALL    swap_chdir
    POP     SI

    ; ---- Open swap file (in startup dir) ----
    MOV     DX, DI
    ADD     DX, DOC_SWAP_PATH       ; DX = offset in doc_table_seg
    PUSH    DS
    PUSH    ES
    POP     DS                      ; DS = doc_table_seg (for DS:DX)
    MOV     AL, 0                   ; read-only
    MOV     AH, 3Dh
    INT     21h
    POP     DS                      ; restore DS
    JC      .fail_noswap
    MOV     [swap_hnd], AX

    ; ---- Read 128-byte header into swap_hdr_buf ----
    MOV     BX, [swap_hnd]
    MOV     DX, swap_hdr_buf
    MOV     CX, SWAP_HEADER_SIZE
    MOV     AH, 3Fh
    INT     21h
    JC      .fail_close_swap
    CMP     AX, SWAP_HEADER_SIZE
    JNE     .fail_close_swap        ; truncated header

    ; ---- Validate magic and version ----
    CMP     WORD [swap_hdr_buf + SWH_MAGIC], SWAP_MAGIC
    JNE     .fail_close_swap        ; bad magic
    CMP     WORD [swap_hdr_buf + SWH_VERSION], SWAP_VERSION
    JNE     .fail_close_swap        ; version mismatch

    LOG "swap_in_header_ok", WORD [swap_hdr_buf + SWH_PT_COUNT]

    ; ---- Extract document CWD from header ----
    CMP     BYTE [swap_hdr_buf + SWH_DOC_CWD], 0
    JE      .skip_cwd               ; empty = no CWD stored
    PUSH    DI
    PUSH    ES
    MOV     AX, DS
    MOV     ES, AX
    MOV     SI, swap_hdr_buf + SWH_DOC_CWD
    MOV     DI, doc_cwd
    MOV     CX, SWH_DOC_CWD_SIZE
    REP     MOVSB
    POP     ES
    POP     DI
.skip_cwd:

    ; ---- CHDIR to document's directory ----
    PUSH    SI
    MOV     SI, doc_cwd
    CALL    swap_chdir
    POP     SI

    ; ---- Check if untitled ----
    POP     DI                      ; restore slot offset
    PUSH    DI                      ; keep on stack
    MOV     ES, [doc_table_seg]     ; re-load ES (may have been clobbered)
    TEST    BYTE ES:[DI + DOC_FLAGS], DOCF_UNTITLED
    JNZ     .untitled

    ; ---- Named document: copy filename from slot to BSS ----
    ; Source = doc_table_seg:[SI+DOC_FILENAME], Dest = DS:[filename]
    PUSH    DI
    MOV     SI, DI
    ADD     SI, DOC_FILENAME        ; SI = offset of filename in doc_table_seg
    PUSH    DS
    PUSH    ES
    MOV     AX, DS
    MOV     ES, AX                  ; ES = DS (STOSB writes to BSS)
    MOV     DS, CS:[doc_table_seg]  ; DS = doc_table_seg (LODSB reads from slot)
    MOV     DI, filename            ; DI = dest (BSS filename)
    MOV     CX, 128
.copy_fn:
    LODSB                           ; read from DS:SI (doc_table_seg)
    STOSB                           ; write to ES:DI (BSS)
    TEST    AL, AL
    JZ      .copy_fn_done
    DEC     CX
    JNZ     .copy_fn
    MOV     BYTE ES:[DI-1], 0      ; ensure null termination
.copy_fn_done:
    POP     ES
    POP     DS
    POP     DI

    ; ---- Open and reload original file ----
    CALL    open_file
    JC      .fail_nofile

    CALL    load_file
    JC      .fail_nomem_closefile

    ; Close the file handle (data is in orig_seg[] now)
    MOV     BX, [file_hnd]
    MOV     AH, 3Eh
    INT     21h

    ; ---- Allocate piece table + add buffer ----
    CALL    pt_init                 ; allocs pt_seg, creates initial pieces
    JC      .fail_nomem

    MOV     BX, ADD_BUF_PARA
    MOV     AH, 48h
    INT     21h
    JC      .fail_nomem
    MOV     [add_seg], AX
    MOV     WORD [add_used], 0

    JMP     .load_swap

.untitled:
    ; ---- Untitled: set empty filename, call ed_new_doc ----
    MOV     BYTE [filename], 0
    CALL    ed_new_doc              ; allocs pt_seg + add_seg, seeds metadata
    JC      .fail_nomem

.load_swap:
    ; ---- Read piece table (SWH_PT_COUNT * PT_DESC_SIZE) into pt_seg ----
    MOV     AX, [swap_hdr_buf + SWH_PT_COUNT]
    MOV     CX, PT_DESC_SIZE
    MUL     CX                      ; AX = byte count
    MOV     CX, AX
    JCXZ    .skip_pt_read

    PUSH    DS
    MOV     DS, CS:[pt_seg]
    XOR     DX, DX                  ; read to offset 0 of pt_seg
    MOV     BX, CS:[swap_hnd]
    MOV     AH, 3Fh
    INT     21h
    POP     DS
    JC      .fail_close_swap
    CMP     AX, CX
    JNE     .fail_close_swap
.skip_pt_read:

    ; ---- Set pt_count from header (overwrites pt_init's value) ----
    MOV     AX, [swap_hdr_buf + SWH_PT_COUNT]
    MOV     [pt_count], AX

    ; ---- Read add buffer (SWH_ADD_USED bytes) into add_seg ----
    MOV     CX, [swap_hdr_buf + SWH_ADD_USED]
    JCXZ    .skip_add_read

    PUSH    DS
    MOV     DS, CS:[add_seg]
    XOR     DX, DX
    MOV     BX, CS:[swap_hnd]
    MOV     AH, 3Fh
    INT     21h
    POP     DS
    JC      .fail_close_swap
    CMP     AX, CX
    JNE     .fail_close_swap
.skip_add_read:

    ; ---- Set add_used from header ----
    MOV     AX, [swap_hdr_buf + SWH_ADD_USED]
    MOV     [add_used], AX

    ; ---- Read undo records (SWH_UNDO_COUNT * UR_SIZE) into undo_seg ----
    MOV     AX, [swap_hdr_buf + SWH_UNDO_COUNT]
    MOV     CX, UR_SIZE
    MUL     CX
    MOV     CX, AX
    JCXZ    .skip_undo_read

    PUSH    DS
    MOV     DS, CS:[undo_seg]
    XOR     DX, DX
    MOV     BX, CS:[swap_hnd]
    MOV     AH, 3Fh
    INT     21h
    POP     DS
    JC      .fail_close_swap
    CMP     AX, CX
    JNE     .fail_close_swap
.skip_undo_read:

    ; ---- Read rd_save_buf (SWH_RD_SAVE_USED * PT_DESC_SIZE) ----
    MOV     AX, [swap_hdr_buf + SWH_RD_SAVE_USED]
    MOV     CX, PT_DESC_SIZE
    MUL     CX
    MOV     CX, AX
    JCXZ    .skip_rd_read

    MOV     DX, rd_save_buf
    MOV     BX, [swap_hnd]
    MOV     AH, 3Fh
    INT     21h
    JC      .fail_close_swap
    CMP     AX, CX
    JNE     .fail_close_swap
.skip_rd_read:

    ; ---- Close swap file ----
    MOV     BX, [swap_hnd]
    MOV     AH, 3Eh
    INT     21h

    ; ---- Delete swap file (CHDIR to startup, delete, CHDIR back) ----
    PUSH    SI
    MOV     SI, startup_cwd
    CALL    swap_chdir
    POP     SI
    POP     DI                      ; restore slot offset
    PUSH    DI
    MOV     DX, DI
    ADD     DX, DOC_SWAP_PATH
    PUSH    DS
    MOV     DS, CS:[doc_table_seg]  ; DS:DX = swap path in doc_table_seg
    MOV     AH, 41h
    INT     21h                     ; ignore delete error
    POP     DS
    PUSH    SI
    MOV     SI, doc_cwd
    CALL    swap_chdir
    POP     SI

    ; ---- Restore cursor and editor state from header ----
    MOV     AX, [swap_hdr_buf + SWH_CUR_LINE]
    MOV     [cur_line], AX

    MOV     AX, [swap_hdr_buf + SWH_CUR_COL]
    MOV     [cur_col], AX

    MOV     AX, [swap_hdr_buf + SWH_TOP_LINE]
    MOV     [top_line], AX

    MOV     AX, [swap_hdr_buf + SWH_LEFT_COL]
    MOV     [left_col], AX

    MOV     AL, [swap_hdr_buf + SWH_DIRTY]
    MOV     [dirty], AL

    MOV     AL, [swap_hdr_buf + SWH_OVERWRITE]
    MOV     [overwrite_mode], AL

    MOV     AL, [swap_hdr_buf + SWH_FIND_FLAGS]
    MOV     [find_flags], AL

    MOV     AX, [swap_hdr_buf + SWH_TAB_WIDTH]
    MOV     [tab_width], AX

    MOV     AX, [swap_hdr_buf + SWH_TAB_MASK]
    MOV     [tab_mask], AX

    ; ---- Restore undo state ----
    MOV     AX, [swap_hdr_buf + SWH_UNDO_POS]
    MOV     [undo_pos], AX

    MOV     AX, [swap_hdr_buf + SWH_UNDO_COUNT]
    MOV     [undo_count], AX

    MOV     AX, [swap_hdr_buf + SWH_UNDO_SPOS]
    MOV     [undo_save_pos], AX

    MOV     AX, [swap_hdr_buf + SWH_RD_SAVE_USED]
    MOV     [rd_save_used], AX

    ; ---- Reset transient state that shouldn't persist ----
    MOV     WORD [last_ins_pi], 0FFFFh
    MOV     BYTE [undo_group_active], 0
    MOV     BYTE [undo_recording], 1
    MOV     BYTE [sel_active], 0
    MOV     BYTE [meta_dirty], 1

    ; ---- Rebuild checkpoints from the restored piece table ----
    CALL    rebuild_meta
    CALL    clamp_cursor
    MOV     BYTE [meta_dirty], 0

    ; ---- Update scrollbar state ----
    MOV     WORD [ed_text_cols], ED_COLS
    MOV     BYTE [ed_sb_active], 0
    MOV     AX, [total_lines]
    CMP     AX, ED_TEXT_ROWS
    JBE     .no_sb
    MOV     WORD [ed_text_cols], ED_COLS - 1
    MOV     BYTE [ed_sb_active], 1
.no_sb:

    CALL    scroll_to_cursor

    ; ---- Update document table slot ----
    POP     DI                      ; restore slot offset
    MOV     ES, [doc_table_seg]     ; re-load ES (clobbered by rebuild_meta etc.)
    AND     BYTE ES:[DI + DOC_FLAGS], ~DOCF_SWAPPED

    ; ---- Set active document ----
    POP     AX                      ; restore slot index
    MOV     [active_doc], AL
    MOV     [panel_sel], AL

    ; ---- Mark screen dirty ----
    MOV     BYTE [fw_state + FW_DIRTY], 1

    LOG "swap_in_success", BYTE [active_doc]

    POP     BP
    CLC
    RET

.fail_nofile:
    LOG "swap_in_fail_nofile"
    JMP     .fail_common

.fail_nomem_closefile:
    MOV     BX, [file_hnd]
    MOV     AH, 3Eh
    INT     21h
    JMP     .fail_nomem

.fail_noswap:
    LOG "swap_in_fail_noswap"
    JMP     .fail_common

.fail_close_swap:
    MOV     BX, [swap_hnd]
    MOV     AH, 3Eh
    INT     21h
    LOG "swap_in_fail_corrupt"
    JMP     .fail_common

.fail_nomem:
    LOG "swap_in_fail_nomem"

.fail_common:
    POP     DI                      ; balance slot pointer
    POP     AX                      ; balance slot index
    POP     BP
    STC
    RET

doc_swap_in ENDP

; ============================================================
; doc_switch — Switch from active document to a different slot
;
; Swaps out the current document, swaps in the target.
; If target == active_doc or target is unoccupied, no-op.
;
; On swap-out failure: document stays active, error dialog shown.
; On swap-in failure (catastrophic): attempts to recover by
;   swapping the original document back in. If recovery fails,
;   falls back to an empty untitled document.
;
; Input:  AL = target slot index (0-7)
; Output: CF=0 success, CF=1 failure (switch aborted)
; Clobbers: AX, BX, CX, DX, SI, DI, BP, ES
; ============================================================
doc_switch PROC
    ; ---- Guard: same slot? ----
    CMP     AL, [active_doc]
    JE      .noop

    ; ---- Guard: target slot occupied? ----
    PUSH    AX                      ; save target slot index
    CALL    doc_get_slot            ; ES:DI = target slot pointer
    TEST    BYTE ES:[DI + DOC_FLAGS], DOCF_OCCUPIED
    JZ      .not_occupied

    ; ---- Guard: target slot actually swapped? ----
    TEST    BYTE ES:[DI + DOC_FLAGS], DOCF_SWAPPED
    JZ      .not_occupied

    ; ---- Clear selection in active document ----
    CALL    sel_clear

    ; ---- Break undo group (prevent cross-doc merging) ----
    MOV     WORD [last_ins_pi], 0FFFFh
    MOV     BYTE [undo_group_active], 0

    ; ---- Swap out active document ----
    CALL    doc_swap_out
    JC      .swap_out_fail

    ; ---- Swap in target document ----
    POP     AX                      ; restore target slot index
    PUSH    AX                      ; keep for recovery
    CALL    doc_swap_in
    JC      .swap_in_fail

    ; ---- Success ----
    POP     AX                      ; balance stack
    LOG "doc_switch_success", BYTE [active_doc]
    CLC
    RET

.noop:
    CLC
    RET

.not_occupied:
    POP     AX                      ; balance target slot index
    STC
    RET

.swap_out_fail:
    ; Swap-out failed — document is still active, segments intact.
    POP     AX                      ; balance target slot index
    PUSH    SI
    PUSH    DI
    MOV     SI, s_err_swap_out
    CALL    show_error_msg
    POP     DI
    POP     SI
    STC
    RET

.swap_in_fail:
    ; CATASTROPHIC: swap-out succeeded (segments freed), swap-in failed.
    ; Recovery: try to swap the original document back in.
    POP     AX                      ; discard target slot index
    MOV     AL, [active_doc]        ; try to recover the original doc
    CALL    doc_swap_in
    JC      .recovery_failed
    ; Recovery succeeded — original doc is back
    PUSH    SI
    PUSH    DI
    MOV     SI, s_err_swap_in
    CALL    show_error_msg
    POP     DI
    POP     SI
    STC
    RET

.recovery_failed:
    ; Both swap-in attempts failed. Last resort: create empty document.
    MOV     BYTE [filename], 0
    CALL    ed_new_doc
    CALL    rebuild_meta
    MOV     BYTE [meta_dirty], 0
    MOV     BYTE [fw_state + FW_DIRTY], 1
    STC
    RET

doc_switch ENDP

; ============================================================
; doc_next_occupied — Find next occupied slot (wrapping)
;
; Scans forward from active_doc+1, wrapping at MAX_DOCS.
; Skips the active_doc slot. Returns CF=1 if only one doc open.
;
; Input:  None
; Output: CF=0 found: AL = next occupied slot index
;         CF=1 not found: only one document open
; Clobbers: AX, CX, DI, ES
; ============================================================
doc_next_occupied PROC
    MOV     AL, [active_doc]
    MOV     CX, MAX_DOCS - 1        ; check at most 7 other slots
.scan:
    INC     AL
    CMP     AL, MAX_DOCS
    JB      .no_wrap
    XOR     AL, AL                   ; wrap to slot 0
.no_wrap:
    CMP     AL, [active_doc]
    JE      .none                    ; wrapped all the way around
    PUSH    AX
    CALL    doc_get_slot             ; ES:DI = slot pointer
    POP     AX
    TEST    BYTE ES:[DI + DOC_FLAGS], DOCF_OCCUPIED
    JNZ     .found
    DEC     CX
    JNZ     .scan
.none:
    STC
    RET
.found:
    CLC
    RET
doc_next_occupied ENDP

; ============================================================
; doc_prev_occupied — Find previous occupied slot (wrapping)
;
; Scans backward from active_doc-1, wrapping at 0→7.
;
; Input:  None
; Output: CF=0 found: AL = previous occupied slot index
;         CF=1 not found: only one document open
; Clobbers: AX, CX, DI, ES
; ============================================================
doc_prev_occupied PROC
    MOV     AL, [active_doc]
    MOV     CX, MAX_DOCS - 1
.scan:
    DEC     AL
    CMP     AL, 0FFh                ; underflow check (was 0, now FFh)
    JNE     .no_wrap
    MOV     AL, MAX_DOCS - 1         ; wrap to last slot
.no_wrap:
    CMP     AL, [active_doc]
    JE      .none
    PUSH    AX
    CALL    doc_get_slot             ; ES:DI = slot pointer
    POP     AX
    TEST    BYTE ES:[DI + DOC_FLAGS], DOCF_OCCUPIED
    JNZ     .found
    DEC     CX
    JNZ     .scan
.none:
    STC
    RET
.found:
    CLC
    RET
doc_prev_occupied ENDP

; ============================================================
; swap_chdir — CHDIR to path stored in "D:\path" format
;
; Input:  SI = pointer to path string "D:\path\0"
; Output: None
; Clobbers: AX, DX
; ============================================================
swap_chdir PROC
    MOV     DL, [SI]
    SUB     DL, 'A'                 ; drive number (0=A)
    MOV     AH, 0Eh
    INT     21h                     ; set drive
    MOV     DX, SI
    ADD     DX, 2                   ; skip "D:", point to "\path"
    MOV     AH, 3Bh
    INT     21h                     ; CHDIR
    RET
swap_chdir ENDP

; ============================================================
; swap_get_cwd — Capture current drive + CWD into buffer
;
; Format: "D:\path\0" (drive letter + colon + backslash + path + null)
;
; Input:  DI = destination buffer (68+ bytes)
; Output: buffer filled
; Clobbers: AX, DL, SI
; ============================================================
swap_get_cwd PROC
    MOV     AH, 19h
    INT     21h                     ; AL = drive (0=A)
    ADD     AL, 'A'
    MOV     [DI], AL
    MOV     BYTE [DI+1], ':'
    MOV     BYTE [DI+2], '\'
    MOV     SI, DI
    ADD     SI, 3                   ; DS:SI = past "D:\"
    MOV     DL, 0                   ; 0 = current drive
    MOV     AH, 47h
    INT     21h                     ; DS:SI filled with CWD (no leading \)
    RET
swap_get_cwd ENDP

; ============================================================
; doc_cleanup_orphans — Delete any orphaned swap files
;
; Scans current directory for TEDT????.SWP and deletes them.
; Called once at startup before any documents are loaded.
;
; Input:  None
; Output: None
; Clobbers: AX, CX, DX
; ============================================================
doc_cleanup_orphans PROC
    ; Set DTA to swap_hdr_buf (64 bytes, DTA needs 43)
    MOV     DX, swap_hdr_buf
    MOV     AH, 1Ah
    INT     21h

    ; FindFirst: TEDT????.SWP
    MOV     DX, .swp_pattern
    XOR     CX, CX                  ; normal files only
    MOV     AH, 4Eh
    INT     21h
    JC      .done                   ; no matches

.del_loop:
    ; DTA+1Eh = found filename (13 bytes, 8.3 null-terminated)
    MOV     DX, swap_hdr_buf
    ADD     DX, 1Eh                 ; DTA offset of filename
    MOV     AH, 41h
    INT     21h                     ; ignore delete errors

    ; FindNext
    MOV     AH, 4Fh
    INT     21h
    JNC     .del_loop               ; more matches

.done:
    RET

.swp_pattern: DB 'TEDT????.SWP', 0
doc_cleanup_orphans ENDP

; ============================================================
; doc_find_by_name — Find an occupied slot with matching filename
;
; Compares the given filename against all occupied slots'
; DOC_FILENAME fields (case-insensitive, per DOS convention).
;
; Input:  SI = pointer to null-terminated filename to search
; Output: CF=0 found: AL = slot index
;         CF=1 not found
; Clobbers: AX, BX, CX, DI, ES
; ============================================================
doc_find_by_name PROC
    PUSH    SI
    PUSH    DX
    MOV     ES, [doc_table_seg]
    XOR     DI, DI
    XOR     DL, DL                      ; DL = slot counter

.search:
    CMP     DL, MAX_DOCS
    JAE     .not_found
    TEST    BYTE ES:[DI + DOC_FLAGS], DOCF_OCCUPIED
    JZ      .search_next
    TEST    BYTE ES:[DI + DOC_FLAGS], DOCF_UNTITLED
    JNZ     .search_next                ; untitled docs can't match

    ; Compare SI against ES:[DI + DOC_FILENAME]
    PUSH    SI
    PUSH    DI
    ADD     DI, DOC_FILENAME
.cmp_loop:
    MOV     AL, [SI]
    MOV     AH, ES:[DI]
    ; Case-insensitive: uppercase both
    CMP     AL, 'a'
    JB      .cmp_no_up1
    CMP     AL, 'z'
    JA      .cmp_no_up1
    SUB     AL, 20h
.cmp_no_up1:
    CMP     AH, 'a'
    JB      .cmp_no_up2
    CMP     AH, 'z'
    JA      .cmp_no_up2
    SUB     AH, 20h
.cmp_no_up2:
    CMP     AL, AH
    JNE     .cmp_mismatch
    TEST    AL, AL                      ; both null = match
    JZ      .cmp_match
    INC     SI
    INC     DI
    JMP     .cmp_loop

.cmp_mismatch:
    POP     DI
    POP     SI
    JMP     .search_next

.cmp_match:
    POP     DI
    POP     SI
    MOV     AL, DL                      ; AL = matching slot index
    POP     DX
    POP     SI
    CLC
    RET

.search_next:
    ADD     DI, DOC_SLOT_SIZE
    INC     DL
    JMP     .search

.not_found:
    POP     DX
    POP     SI
    STC
    RET
doc_find_by_name ENDP

; Compile-time verification: procedure addresses are defined
PRINT "doc_swap_out = ", doc_swap_out
PRINT "doc_swap_in = ", doc_swap_in
PRINT "doc_switch = ", doc_switch
PRINT "doc_next_occupied = ", doc_next_occupied
PRINT "doc_prev_occupied = ", doc_prev_occupied
PRINT "doc_cleanup_orphans = ", doc_cleanup_orphans
PRINT "doc_find_by_name = ", doc_find_by_name
