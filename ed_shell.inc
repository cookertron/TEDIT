; ============================================================
; ed_shell.inc — Shell swap: dump/restore memory for Shell to DOS
;
; When /d flag is active, Shell to DOS writes all allocated
; segments to TEDTSHEL.SWP, frees them, then restores after
; EXEC returns.  On startup, detects orphan swap files from
; crashes and offers restore or delete.
;
; Procedures:
;   shell_swap_out    — serialize state to disk, free segments
;   shell_swap_in     — reallocate segments, restore from disk
;   shell_check_orphan — startup orphan swap file detection
; ============================================================

; ============================================================
; write_seg_data — Write CX bytes from segment AX:offset DX
;
; Input:  AX = source segment, DX = offset, CX = byte count
;         [swap_hnd] = open file handle
; Output: CF=0 success, CF=1 write error or partial write
; Clobbers: AX, BX
; ============================================================
write_seg_data PROC
    PUSH    DS
    MOV     DS, AX
    MOV     BX, CS:[swap_hnd]
    MOV     AH, 40h
    INT     21h
    POP     DS
    JC      .err
    CMP     AX, CX
    JNE     .err
    CLC
    RET
.err:
    STC
    RET
write_seg_data ENDP

; ============================================================
; read_seg_data — Read CX bytes into segment AX:offset DX
;
; Input:  AX = dest segment, DX = offset, CX = byte count
;         [swap_hnd] = open file handle
; Output: CF=0 success, CF=1 read error or short read
; Clobbers: AX, BX
; ============================================================
read_seg_data PROC
    PUSH    DS
    MOV     DS, AX
    MOV     BX, CS:[swap_hnd]
    MOV     AH, 3Fh
    INT     21h
    POP     DS
    JC      .err
    CMP     AX, CX
    JNE     .err
    CLC
    RET
.err:
    STC
    RET
read_seg_data ENDP

; ============================================================
; shell_swap_out — Serialize all state to TEDTSHEL.SWP
;
; Writes: global header, doc table, clipboard, active doc
;         (per-doc header + piece table + add buf + undo + rd_save)
; Then frees ALL allocated segments.
;
; Output: CF=0 success (segments freed), CF=1 failure (intact)
; Clobbers: AX, BX, CX, DX, SI, DI, ES
; ============================================================
shell_swap_out PROC
    ; ---- CHDIR to startup_cwd\_TEDIT ----
    CALL    swap_ensure_dir
    JC      .fail                   ; path too long

    ; ---- Create swap file ----
    MOV     DX, s_shell_swap_name
    XOR     CX, CX                  ; normal attributes
    MOV     AH, 3Ch
    INT     21h
    JC      .fail                   ; can't create file
    MOV     [swap_hnd], AX

    ; ==== WRITE GLOBAL HEADER (128 bytes in swap_hdr_buf) ====
    ; Zero the buffer
    MOV     AX, DS
    MOV     ES, AX
    MOV     DI, swap_hdr_buf
    MOV     CX, SHELL_HDR_SIZE
    XOR     AL, AL
    REP     STOSB

    ; Fill fields
    MOV     WORD [swap_hdr_buf + SHL_MAGIC], SHELL_SWAP_MAGIC
    MOV     WORD [swap_hdr_buf + SHL_VERSION], SHELL_SWAP_VERSION
    MOV     AL, [doc_count]
    MOV     [swap_hdr_buf + SHL_DOC_COUNT], AL
    MOV     AL, [active_doc]
    MOV     [swap_hdr_buf + SHL_ACTIVE_DOC], AL
    MOV     AX, [clip_used]
    MOV     [swap_hdr_buf + SHL_CLIP_USED], AX
    MOV     AX, [clip_lines]
    MOV     [swap_hdr_buf + SHL_CLIP_LINES], AX
    MOV     AX, [clip_last_col]
    MOV     [swap_hdr_buf + SHL_CLIP_LAST_COL], AX

    ; Write global header
    MOV     BX, [swap_hnd]
    MOV     DX, swap_hdr_buf
    MOV     CX, SHELL_HDR_SIZE
    MOV     AH, 40h
    INT     21h
    JC      .fail_close_delete
    CMP     AX, CX
    JNE     .fail_close_delete

    ; ==== WRITE DOC TABLE (doc_count * DOC_SLOT_SIZE) ====
    MOV     AL, [doc_count]
    XOR     AH, AH
    MOV     CX, DOC_SLOT_SIZE
    MUL     CX                      ; AX = byte count
    MOV     CX, AX
    JCXZ    .skip_doctbl

    MOV     AX, [doc_table_seg]
    XOR     DX, DX
    CALL    write_seg_data
    JC      .fail_close_delete
.skip_doctbl:

    ; ==== WRITE CLIPBOARD (clip_used bytes) ====
    MOV     CX, [clip_used]
    JCXZ    .skip_clip

    MOV     AX, [clip_seg]
    XOR     DX, DX
    CALL    write_seg_data
    JC      .fail_close_delete
.skip_clip:

    ; ==== WRITE PER-DOC HEADER (reuse swap_hdr_buf, 128 bytes) ====
    ; Zero buffer
    MOV     AX, DS
    MOV     ES, AX
    MOV     DI, swap_hdr_buf
    MOV     CX, SWAP_HEADER_SIZE
    XOR     AL, AL
    REP     STOSB

    ; Fill per-doc fields (same layout as doc_swap_out)
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
    MOV     WORD [swap_hdr_buf + SWH_ORIG_COUNT], 0
    MOV     AL, [line_ending]
    MOV     [swap_hdr_buf + SWH_LINE_ENDING], AL

    ; Copy doc_cwd into header
    MOV     SI, doc_cwd
    MOV     DI, swap_hdr_buf + SWH_DOC_CWD
    MOV     CX, SWH_DOC_CWD_SIZE
    REP     MOVSB

    ; Write per-doc header
    MOV     BX, [swap_hnd]
    MOV     DX, swap_hdr_buf
    MOV     CX, SWAP_HEADER_SIZE
    MOV     AH, 40h
    INT     21h
    JC      .fail_close_delete
    CMP     AX, CX
    JNE     .fail_close_delete

    ; ==== WRITE PIECE TABLE ====
    MOV     AX, [pt_count]
    MOV     CX, PT_DESC_SIZE
    MUL     CX
    MOV     CX, AX
    JCXZ    .skip_pt

    MOV     AX, [pt_seg]
    XOR     DX, DX
    CALL    write_seg_data
    JC      .fail_close_delete
.skip_pt:

    ; ==== WRITE ADD BUFFER ====
    MOV     CX, [add_used]
    JCXZ    .skip_add

    MOV     AX, [add_seg]
    XOR     DX, DX
    CALL    write_seg_data
    JC      .fail_close_delete
.skip_add:

    ; ==== WRITE UNDO RECORDS ====
    MOV     AX, [undo_count]
    MOV     CX, UR_SIZE
    MUL     CX
    MOV     CX, AX
    JCXZ    .skip_undo

    MOV     AX, [undo_seg]
    XOR     DX, DX
    CALL    write_seg_data
    JC      .fail_close_delete
.skip_undo:

    ; ==== WRITE RD_SAVE_BUF ====
    MOV     AX, [rd_save_used]
    MOV     CX, PT_DESC_SIZE
    MUL     CX
    MOV     CX, AX
    JCXZ    .skip_rd

    MOV     BX, [swap_hnd]
    MOV     DX, rd_save_buf
    MOV     AH, 40h
    INT     21h
    JC      .fail_close_delete
    CMP     AX, CX
    JNE     .fail_close_delete
.skip_rd:

    ; ==== CLOSE FILE — all writes succeeded ====
    MOV     BX, [swap_hnd]
    MOV     AH, 3Eh
    INT     21h

    ; ==== CHDIR back to startup directory (out of _TEDIT) ====
    MOV     SI, startup_cwd
    CALL    swap_chdir

    ; ==== FREE ALL SEGMENTS ====
    ; Close file-backed original handle
    MOV     BX, [orig_file_hnd]
    TEST    BX, BX
    JZ      .free_cache
    MOV     AH, 3Eh
    INT     21h
    MOV     WORD [orig_file_hnd], 0

.free_cache:
    ; Free cache segments
    XOR     SI, SI
.free_cache_loop:
    CMP     SI, CACHE_SLOTS * 2
    JAE     .free_pt_seg
    MOV     AX, [SI + cache_seg]
    TEST    AX, AX
    JZ      .free_cache_next
    MOV     ES, AX
    MOV     AH, 49h
    INT     21h
    MOV     WORD [SI + cache_seg], 0
.free_cache_next:
    ADD     SI, 2
    JMP     .free_cache_loop

.free_pt_seg:
    MOV     AX, [pt_seg]
    TEST    AX, AX
    JZ      .free_add_seg
    MOV     ES, AX
    MOV     AH, 49h
    INT     21h
    MOV     WORD [pt_seg], 0

.free_add_seg:
    MOV     AX, [add_seg]
    TEST    AX, AX
    JZ      .free_undo_seg
    MOV     ES, AX
    MOV     AH, 49h
    INT     21h
    MOV     WORD [add_seg], 0

.free_undo_seg:
    MOV     AX, [undo_seg]
    TEST    AX, AX
    JZ      .free_clip_seg
    MOV     ES, AX
    MOV     AH, 49h
    INT     21h
    MOV     WORD [undo_seg], 0

.free_clip_seg:
    MOV     AX, [clip_seg]
    TEST    AX, AX
    JZ      .free_shadow
    MOV     ES, AX
    MOV     AH, 49h
    INT     21h
    MOV     WORD [clip_seg], 0

.free_shadow:
    MOV     AX, [shadow_seg]
    TEST    AX, AX
    JZ      .free_doctbl
    MOV     ES, AX
    MOV     AH, 49h
    INT     21h
    MOV     WORD [shadow_seg], 0

.free_doctbl:
    MOV     AX, [doc_table_seg]
    TEST    AX, AX
    JZ      .free_done
    MOV     ES, AX
    MOV     AH, 49h
    INT     21h
    MOV     WORD [doc_table_seg], 0

.free_done:
    MOV     WORD [pt_count], 0
    MOV     WORD [add_used], 0

    ; Restore ES = DS
    MOV     AX, DS
    MOV     ES, AX

    CLC
    RET

.fail_close_delete:
    ; Close the partial swap file
    MOV     BX, [swap_hnd]
    MOV     AH, 3Eh
    INT     21h
    ; Delete the partial file
    MOV     DX, s_shell_swap_name
    MOV     AH, 41h
    INT     21h                     ; ignore error

.fail:
    ; Restore ES = DS (may have been clobbered)
    MOV     AX, DS
    MOV     ES, AX
    STC
    RET

shell_swap_out ENDP

; ============================================================
; shell_swap_in — Reallocate segments and restore from swap file
;
; Allocates all segments, opens TEDTSHEL.SWP, reads state,
; reloads original file from disk, restores piece table etc.
;
; Output: CF=0 success, CF=1 fatal failure
; Clobbers: AX, BX, CX, DX, SI, DI, ES
; ============================================================
shell_swap_in PROC
    ; ---- CHDIR to startup_cwd\_TEDIT ----
    CALL    swap_ensure_dir
    ; (ignore CF — dir must exist if swap was written; allocations proceed regardless)

    ; ---- Allocate shared segments ----
    ; doc_table_seg
    MOV     BX, DOC_TBL_SEG_PARA
    MOV     AH, 48h
    INT     21h
    JC      .fatal
    MOV     [doc_table_seg], AX

    ; Zero doc_table_seg so unoccupied slots have DOC_FLAGS=0
    ; (only doc_count slots are restored from swap file)
    PUSH    ES
    MOV     ES, AX
    XOR     DI, DI
    XOR     AX, AX
    MOV     CX, DOC_TBL_SEG_PARA * 8   ; paragraph count * 8 = word count
    REP     STOSW
    POP     ES

    ; shadow_seg
    MOV     BX, SHADOW_SEG_PARA
    MOV     AH, 48h
    INT     21h
    JC      .fatal
    MOV     [shadow_seg], AX

    ; undo_seg
    MOV     BX, UNDO_SEG_PARA
    MOV     AH, 48h
    INT     21h
    JC      .fatal
    MOV     [undo_seg], AX

    ; clip_seg
    MOV     BX, CLIP_SEG_PARA
    MOV     AH, 48h
    INT     21h
    JC      .fatal
    MOV     [clip_seg], AX

    ; cache_seg[0..1]
    CALL    cache_init
    JC      .fatal

    ; ---- Open swap file ----
    MOV     DX, s_shell_swap_name
    MOV     AL, 0                   ; read-only
    MOV     AH, 3Dh
    INT     21h
    JC      .swap_file_missing      ; deleted during shell — graceful fallback
    MOV     [swap_hnd], AX

    ; ==== READ GLOBAL HEADER ====
    MOV     BX, [swap_hnd]
    MOV     DX, swap_hdr_buf
    MOV     CX, SHELL_HDR_SIZE
    MOV     AH, 3Fh
    INT     21h
    JC      .fail_close
    CMP     AX, SHELL_HDR_SIZE
    JNE     .fail_close

    ; Validate magic and version
    CMP     WORD [swap_hdr_buf + SHL_MAGIC], SHELL_SWAP_MAGIC
    JNE     .fail_close
    CMP     WORD [swap_hdr_buf + SHL_VERSION], SHELL_SWAP_VERSION
    JNE     .fail_close

    ; Extract global fields
    MOV     AL, [swap_hdr_buf + SHL_DOC_COUNT]
    MOV     [doc_count], AL
    MOV     AL, [swap_hdr_buf + SHL_ACTIVE_DOC]
    MOV     [active_doc], AL
    MOV     [panel_sel], AL
    MOV     AX, [swap_hdr_buf + SHL_CLIP_USED]
    MOV     [clip_used], AX
    MOV     AX, [swap_hdr_buf + SHL_CLIP_LINES]
    MOV     [clip_lines], AX
    MOV     AX, [swap_hdr_buf + SHL_CLIP_LAST_COL]
    MOV     [clip_last_col], AX

    ; ==== READ DOC TABLE ====
    MOV     AL, [doc_count]
    XOR     AH, AH
    MOV     CX, DOC_SLOT_SIZE
    MUL     CX
    MOV     CX, AX
    JCXZ    .skip_doctbl_read

    MOV     AX, [doc_table_seg]
    XOR     DX, DX
    CALL    read_seg_data
    JC      .fail_close
.skip_doctbl_read:

    ; ==== READ CLIPBOARD ====
    MOV     CX, [clip_used]
    JCXZ    .skip_clip_read

    MOV     AX, [clip_seg]
    XOR     DX, DX
    CALL    read_seg_data
    JC      .fail_close
.skip_clip_read:

    ; ==== READ PER-DOC HEADER ====
    MOV     BX, [swap_hnd]
    MOV     DX, swap_hdr_buf
    MOV     CX, SWAP_HEADER_SIZE
    MOV     AH, 3Fh
    INT     21h
    JC      .fail_close
    CMP     AX, SWAP_HEADER_SIZE
    JNE     .fail_close

    ; Validate per-doc magic
    CMP     WORD [swap_hdr_buf + SWH_MAGIC], SWAP_MAGIC
    JNE     .fail_close

    ; ==== RESTORE DOC CWD ====
    CMP     BYTE [swap_hdr_buf + SWH_DOC_CWD], 0
    JE      .skip_cwd
    PUSH    ES
    MOV     AX, DS
    MOV     ES, AX
    MOV     SI, swap_hdr_buf + SWH_DOC_CWD
    MOV     DI, doc_cwd
    MOV     CX, SWH_DOC_CWD_SIZE
    REP     MOVSB
    POP     ES
.skip_cwd:

    ; CHDIR to document's directory
    MOV     SI, doc_cwd
    CALL    swap_chdir

    ; ==== RELOAD ORIGINAL FILE ====
    ; Get filename from doc table slot
    MOV     AL, [active_doc]
    XOR     AH, AH
    MOV     CX, DOC_SLOT_SIZE
    MUL     CX                      ; AX = slot offset
    MOV     DI, AX
    MOV     ES, [doc_table_seg]

    ; Check if untitled
    TEST    BYTE ES:[DI + DOC_FLAGS], DOCF_UNTITLED
    JNZ     .untitled

    ; Copy filename from slot to BSS
    PUSH    DI
    PUSH    ES
    MOV     SI, DI
    ADD     SI, DOC_FILENAME
    PUSH    DS
    MOV     AX, DS
    MOV     ES, AX                  ; ES = DS for STOSB
    MOV     DS, CS:[doc_table_seg]  ; DS = doc_table_seg for LODSB
    MOV     DI, filename
    MOV     CX, 128
.copy_fn:
    LODSB
    STOSB
    TEST    AL, AL
    JZ      .copy_fn_done
    DEC     CX
    JNZ     .copy_fn
    MOV     BYTE ES:[DI-1], 0      ; ensure null termination
.copy_fn_done:
    POP     DS
    POP     ES
    POP     DI

    ; Open and reload original file
    CALL    open_file
    JC      .fail_close             ; original file missing

    CALL    load_file
    JC      .fail_close             ; memory error

    ; Allocate piece table + add buffer
    CALL    pt_init
    JC      .fail_close

    MOV     BX, ADD_BUF_PARA
    MOV     AH, 48h
    INT     21h
    JC      .fail_close
    MOV     [add_seg], AX
    MOV     WORD [add_used], 0

    JMP     .load_swap_data

.untitled:
    MOV     BYTE [filename], 0
    CALL    ed_new_doc
    JC      .fail_close

.load_swap_data:
    ; ==== READ PIECE TABLE ====
    MOV     AX, [swap_hdr_buf + SWH_PT_COUNT]
    MOV     CX, PT_DESC_SIZE
    MUL     CX
    MOV     CX, AX
    JCXZ    .skip_pt_read

    MOV     AX, [pt_seg]
    XOR     DX, DX
    CALL    read_seg_data
    JC      .fail_close
.skip_pt_read:

    MOV     AX, [swap_hdr_buf + SWH_PT_COUNT]
    MOV     [pt_count], AX

    ; ==== READ ADD BUFFER ====
    MOV     CX, [swap_hdr_buf + SWH_ADD_USED]
    JCXZ    .skip_add_read

    MOV     AX, [add_seg]
    XOR     DX, DX
    CALL    read_seg_data
    JC      .fail_close
.skip_add_read:

    MOV     AX, [swap_hdr_buf + SWH_ADD_USED]
    MOV     [add_used], AX

    ; ==== READ UNDO RECORDS ====
    MOV     AX, [swap_hdr_buf + SWH_UNDO_COUNT]
    MOV     CX, UR_SIZE
    MUL     CX
    MOV     CX, AX
    JCXZ    .skip_undo_read

    MOV     AX, [undo_seg]
    XOR     DX, DX
    CALL    read_seg_data
    JC      .fail_close
.skip_undo_read:

    ; ==== READ RD_SAVE_BUF ====
    MOV     AX, [swap_hdr_buf + SWH_RD_SAVE_USED]
    MOV     CX, PT_DESC_SIZE
    MUL     CX
    MOV     CX, AX
    JCXZ    .skip_rd_read

    MOV     BX, [swap_hnd]
    MOV     DX, rd_save_buf
    MOV     AH, 3Fh
    INT     21h
    JC      .fail_close
    CMP     AX, CX
    JNE     .fail_close
.skip_rd_read:

    ; ==== CLOSE AND DELETE SWAP FILE ====
    MOV     BX, [swap_hnd]
    MOV     AH, 3Eh
    INT     21h

    ; CHDIR to _TEDIT for delete, then remove dir, then back to doc_cwd
    CALL    swap_ensure_dir         ; CHDIR to startup_cwd\_TEDIT
    MOV     DX, s_shell_swap_name
    MOV     AH, 41h
    INT     21h                     ; ignore error
    CALL    swap_remove_dir         ; try removing _TEDIT (if now empty)
    MOV     SI, doc_cwd
    CALL    swap_chdir

    ; ==== RESTORE BSS STATE FROM PER-DOC HEADER ====
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
    MOV     AL, [swap_hdr_buf + SWH_LINE_ENDING]
    MOV     [line_ending], AL
    CALL    update_le_label

    ; Restore undo state
    MOV     AX, [swap_hdr_buf + SWH_UNDO_POS]
    MOV     [undo_pos], AX
    MOV     AX, [swap_hdr_buf + SWH_UNDO_COUNT]
    MOV     [undo_count], AX
    MOV     AX, [swap_hdr_buf + SWH_UNDO_SPOS]
    MOV     [undo_save_pos], AX
    MOV     AX, [swap_hdr_buf + SWH_RD_SAVE_USED]
    MOV     [rd_save_used], AX

    ; Undo max (derived from segment size)
    MOV     WORD [undo_max], UNDO_MAX_RECORDS

    ; Reset transient state
    MOV     WORD [last_ins_pi], 0FFFFh
    MOV     BYTE [undo_group_active], 0
    MOV     BYTE [undo_recording], 1
    MOV     BYTE [sel_active], 0
    MOV     BYTE [meta_dirty], 1

    ; Invalidate cache (fresh segments, no data loaded)
    CALL    cache_invalidate

    ; Rebuild checkpoints from the restored piece table
    CALL    rebuild_meta
    CALL    clamp_cursor
    MOV     BYTE [meta_dirty], 0

    ; Update scrollbar state
    MOV     WORD [ed_text_cols], ED_COLS
    MOV     BYTE [ed_sb_active], 0
    MOV     AX, [total_lines]
    CMP     AX, ED_TEXT_ROWS
    JBE     .no_sb
    MOV     WORD [ed_text_cols], ED_COLS - 1
    MOV     BYTE [ed_sb_active], 1
.no_sb:

    CALL    scroll_to_cursor

    ; Restore ES = DS
    MOV     AX, DS
    MOV     ES, AX

    CLC
    RET

.swap_file_missing:
    ; ---- Swap file was deleted during shell session ----
    ; Shared segments (doc_table, shadow, undo, clip, cache) are allocated.
    ; Fall back to an empty untitled document so the editor is usable.

    ; Create empty document (allocates pt_seg + add_seg)
    MOV     BYTE [filename], 0
    CALL    ed_new_doc
    JC      .fatal                  ; truly out of memory

    ; Initialize doc table slot 0
    MOV     ES, [doc_table_seg]
    XOR     DI, DI
    MOV     CX, DOC_SLOT_SIZE
    XOR     AL, AL
    REP     STOSB                   ; zero entire slot
    XOR     DI, DI
    MOV     BYTE ES:[DI + DOC_FLAGS], DOCF_OCCUPIED | DOCF_UNTITLED
    MOV     BYTE ES:[DI + DOC_SLOT_ID], 0
    MOV     AX, DS
    MOV     ES, AX

    MOV     BYTE [active_doc], 0
    MOV     BYTE [doc_count], 1
    MOV     BYTE [panel_sel], 0

    ; Set doc_cwd = startup_cwd
    MOV     SI, startup_cwd
    MOV     DI, doc_cwd
    MOV     CX, 68
    REP     MOVSB

    ; Initialize undo + clipboard
    CALL    undo_init
    MOV     WORD [clip_used], 0
    MOV     WORD [clip_lines], 0
    MOV     WORD [clip_last_col], 0

    ; Reset editor state
    MOV     WORD [cur_line], 0
    MOV     WORD [cur_col], 0
    MOV     WORD [top_line], 0
    MOV     WORD [left_col], 0
    MOV     BYTE [dirty], 0
    MOV     WORD [last_ins_pi], 0FFFFh
    MOV     BYTE [sel_active], 0
    MOV     BYTE [line_ending], LE_CRLF
    CALL    update_le_label
    MOV     BYTE [undo_group_active], 0
    MOV     BYTE [undo_recording], 1
    MOV     WORD [ed_text_cols], ED_COLS
    MOV     BYTE [ed_sb_active], 0

    CALL    cache_invalidate
    CALL    rebuild_meta
    MOV     BYTE [meta_dirty], 0

    ; Signal caller to show warning after TUI is restored
    MOV     BYTE [shell_recovering], 1

    CLC
    RET

.fail_close:
    MOV     BX, [swap_hnd]
    MOV     AH, 3Eh
    INT     21h
    ; Do NOT delete — user may want to retry recovery
.fatal:
    MOV     AX, DS
    MOV     ES, AX
    STC
    RET

shell_swap_in ENDP

; ============================================================
; shell_check_orphan — Check for orphan swap file on startup
;
; Called before TUI init.  If TEDTSHEL.SWP exists in startup_cwd,
; prompts user: Restore[R] or Delete[D]?
;
; Output: CF=0 no orphan (or user chose delete)
;         CF=1 user chose restore (caller should call shell_swap_in)
; Clobbers: AX, BX, CX, DX, SI
; ============================================================
shell_check_orphan PROC
    ; CHDIR to startup_cwd\_TEDIT (if it exists)
    CALL    swap_ensure_dir
    JC      .no_orphan              ; no _TEDIT dir — no orphan

    ; Try to open the swap file
    MOV     DX, s_shell_swap_name
    MOV     AL, 0                   ; read-only
    MOV     AH, 3Dh
    INT     21h
    JC      .no_orphan              ; file doesn't exist

    ; File exists — close it immediately
    MOV     BX, AX
    MOV     AH, 3Eh
    INT     21h

.prompt:
    ; Print prompt via DOS (no TUI yet)
    MOV     AH, 09h
    MOV     DX, s_swap_prompt
    INT     21h

    ; Read one character with echo
    MOV     AH, 01h
    INT     21h

    ; Print CRLF
    PUSH    AX
    MOV     DL, 0Dh
    MOV     AH, 02h
    INT     21h
    MOV     DL, 0Ah
    MOV     AH, 02h
    INT     21h
    POP     AX

    ; Check response
    CMP     AL, 'R'
    JE      .restore
    CMP     AL, 'r'
    JE      .restore
    CMP     AL, 'D'
    JE      .delete
    CMP     AL, 'd'
    JE      .delete
    JMP     .prompt                 ; re-prompt on invalid input

.restore:
    STC
    RET

.delete:
    ; Delete the swap file
    MOV     DX, s_shell_swap_name
    MOV     AH, 41h
    INT     21h                     ; ignore error

    ; Also clean up any orphan per-doc swap files
    CALL    doc_cleanup_orphans

.no_orphan:
    ; ---- CHDIR back to startup directory (may be in _TEDIT) ----
    MOV     SI, startup_cwd
    CALL    swap_chdir
    CLC
    RET

shell_check_orphan ENDP
