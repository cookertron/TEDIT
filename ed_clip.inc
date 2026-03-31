; ============================================================
; ed_clip.inc — Clipboard Operations (Copy, Cut, Paste)
;
; Depends on: ed_const.inc, ed_core.inc, ed_sel.inc
; BSS: clip_seg, clip_used, clip_lines, clip_last_col,
;      ec_s_line, ec_s_col, ec_e_line, ec_e_col,
;      ec_save_line, ec_save_col, ec_byte_count
; ============================================================

; ============================================================
; ed_copy — Copy selected text to clipboard buffer
;
; If no selection active or collapsed: no-op, returns CF=1.
; Selection remains active after copy (no document modification).
;
; Uses BSS scratch variables (ec_*) instead of stack-relative
; addressing. All values that survive across CALLs are in
; named memory locations.
;
; Output: CF=0 copied, CF=1 nothing to copy
; Preserves: BX, BP, DI (per codebase convention)
; Clobbers:  AX, CX, DX, SI
; ============================================================

ed_copy PROC
    ; ================================================================
    ; STAGE 0: Check preconditions
    ;   Registers: don't care (haven't saved anything yet)
    ;   DS = CS, ES = caller's
    ; ================================================================

    CMP     BYTE [sel_active], 0
    JE      .ec_nothing

    PUSH    BX
    PUSH    BP
    PUSH    DI
    PUSH    ES

    ; ================================================================
    ; STAGE 1: Normalize selection, save to BSS scratch
    ;   DS = CS
    ;   sel_normalize output: BX=s_line, CX=s_col, DX=e_line, SI=e_col
    ;   sel_normalize preserves: AX
    ;   sel_normalize clobbers: BX, CX, DX, SI
    ; ================================================================

    CALL    sel_normalize

    ; Check collapsed selection (start == end)
    CMP     BX, DX
    JNE     .ec_has_range
    CMP     CX, SI
    JNE     .ec_has_range
    JMP     .ec_fail                ; collapsed = nothing to copy

.ec_has_range:
    ; Save normalized endpoints to named BSS variables
    MOV     [ec_s_line], BX
    MOV     [ec_s_col], CX
    MOV     [ec_e_line], DX
    MOV     [ec_e_col], SI

    ; Save current cursor position (restored in stage 6)
    MOV     AX, [cur_line]
    MOV     [ec_save_line], AX
    MOV     AX, [cur_col]
    MOV     [ec_save_col], AX

    ; ================================================================
    ; STAGE 2: Convert selection start to byte offset
    ;   DS = CS
    ;   line_col_to_offset input:  BX = line, CX = col
    ;   line_col_to_offset output: DX:AX = 32-bit byte offset
    ;   line_col_to_offset also sets: rnd_pi, rnd_off (piece position)
    ;   line_col_to_offset preserves: BX, BP, DI
    ;   line_col_to_offset clobbers:  AX, CX, DX, SI, rnd_pi, rnd_off
    ;
    ;   BX = ec_s_line from stage 1 (still in register)
    ;   CX = ec_s_col from stage 1 (still in register)
    ; ================================================================

    CALL    line_col_to_offset

    ; Save start piece position for copy loop (stage 5)
    ; Repurpose ec_s_line/ec_s_col (no longer needed after this call)
    MOV     SI, [rnd_pi]
    MOV     [ec_s_line], SI
    MOV     SI, [rnd_off]
    MOV     [ec_s_col], SI

    ; Save 32-bit start offset on stack
    PUSH    DX                      ; start_off_hi
    PUSH    AX                      ; start_off_lo

    ; ================================================================
    ; STAGE 3: Convert selection end to byte offset, compute byte count
    ;   DS = CS
    ;   Reload end coords from BSS (stage 2 clobbered CX, DX, SI)
    ; ================================================================

    MOV     BX, [ec_e_line]
    MOV     CX, [ec_e_col]
    CALL    line_col_to_offset      ; DX:AX = end byte offset (32-bit)

    ; 32-bit subtract: byte_count = end_offset - start_offset
    POP     CX                      ; CX = start_off_lo
    POP     SI                      ; SI = start_off_hi
    SUB     AX, CX
    SBB     DX, SI
    TEST    DX, DX
    JNZ     .ec_offset_overflow     ; byte count > 64K
    TEST    AX, AX
    JZ      .ec_restore_fail        ; zero byte count
    CMP     AX, CLIP_BUF_BYTES
    JA      .ec_too_large           ; exceeds clipboard capacity

    MOV     [ec_byte_count], AX

    ; Restore start piece position (set by stage 2's line_col_to_offset)
    MOV     AX, [ec_s_line]
    MOV     [rnd_pi], AX
    MOV     AX, [ec_s_col]
    MOV     [rnd_off], AX

    ; ================================================================
    ; STAGE 5: Copy loop — extract bytes from piece table to clipboard
    ;
    ; OUTER LOOP INVARIANT (at top of .ec_piece_loop):
    ;   DS = CS
    ;   ES = undefined (clobbered by resolve_piece)
    ;   BP = remaining bytes to copy (decremented each iteration)
    ;   DI = write offset in clipboard (0 on first iteration)
    ;   [rnd_pi]  = next piece to read from
    ;   [rnd_off] = offset within that piece (0 after first chunk)
    ;
    ; INNER LOOP INVARIANT (during .ec_byte_loop):
    ;   DS = text segment (orig or add buffer)
    ;   ES = clip_seg
    ;   SI = read position (set by resolve_piece + rnd_off adjustment)
    ;   DI = write position in clipboard
    ;   CX = bytes remaining in this chunk
    ;   BP = bytes remaining after this chunk (already decremented)
    ;
    ; resolve_piece contract:
    ;   Input:  BX = piece index, DS = CS
    ;   Output: AX = segment, SI = chunk base, DX = piece length
    ;   Preserves: BX, DI, BP   ← critical for our DI and BP
    ;   Clobbers: AX, CX, DX, SI, ES
    ; ================================================================

    MOV     BP, [ec_byte_count]     ; BP = bytes remaining
    XOR     DI, DI                  ; DI = clipboard write position

    MOV     WORD [clip_lines], 0
    MOV     WORD [clip_last_col], 0

.ec_piece_loop:
    ; ---- DS = CS here. Check if done. ----
    TEST    BP, BP
    JZ      .ec_copy_done

    MOV     BX, [rnd_pi]
    CMP     BX, [pt_count]
    JAE     .ec_copy_done           ; past all pieces (shouldn't happen)

    ; ---- Resolve piece: get segment, base, length ----
    ; Clobbers ES (sets to pt_seg). Preserves BX, DI, BP.
    CALL    resolve_piece
    JC      .ec_copy_done

    ; AX = text segment, SI = chunk base offset, DX = piece length
    ; Compute read start and available bytes in this chunk
    ADD     SI, [rnd_off]           ; SI = absolute read start position
    SUB     DX, [rnd_off]           ; DX = bytes available from rnd_off

    ; CX = min(available, remaining) = bytes to copy this iteration
    MOV     CX, DX
    CMP     CX, BP
    JBE     .ec_min_ok
    MOV     CX, BP
.ec_min_ok:

    ; Deduct from remaining count BEFORE we change DS.
    ; BP is a register — no memory access needed, no CS: prefix.
    SUB     BP, CX

    ; ---- Enter text-segment mode ----
    ; After this point until POP DS:
    ;   DS = text segment (for LODSB: reads [DS:SI])
    ;   ES = clip_seg (for STOSB: writes [ES:DI])
    ;   All BSS access MUST use CS: prefix
    PUSH    DS                      ; save CS on stack
    MOV     DS, AX                  ; DS = text segment

    MOV     AX, CS:[clip_seg]
    MOV     ES, AX                  ; ES = clipboard segment

    ; SI = read pos (set above by resolve_piece + rnd_off)
    ; DI = write pos (maintained across iterations by resolve_piece preserving DI)
    ; CX = byte count for this chunk

.ec_byte_loop:
    JCXZ    .ec_chunk_done

    LODSB                           ; AL = [DS:SI], SI += 1
    STOSB                           ; [ES:DI] = AL, DI += 1

    CMP     AL, 0Ah
    JE      .ec_got_lf
    CMP     AL, 0Dh
    JE      .ec_got_cr
    ; Visible character — increment column counter
    INC     WORD CS:[clip_last_col]
    DEC     CX
    JMP     .ec_byte_loop

.ec_got_lf:
    INC     WORD CS:[clip_lines]
    MOV     WORD CS:[clip_last_col], 0
    DEC     CX
    JMP     .ec_byte_loop

.ec_got_cr:
    ; CR: copied to clipboard, no metric change
    DEC     CX
    JMP     .ec_byte_loop

.ec_chunk_done:
    ; ---- Exit text-segment mode ----
    POP     DS                      ; DS = CS again

    ; Advance to next piece
    INC     WORD [rnd_pi]
    MOV     WORD [rnd_off], 0
    JMP     .ec_piece_loop

    ; ================================================================
    ; STAGE 6: Finalize — store result, restore cursor
    ;   DS = CS
    ;   DI = total bytes written to clipboard
    ; ================================================================

.ec_copy_done:
    MOV     [clip_used], DI

    ; Restore original cursor position (copy is non-destructive)
    MOV     AX, [ec_save_line]
    MOV     [cur_line], AX
    MOV     AX, [ec_save_col]
    MOV     [cur_col], AX

    POP     ES
    POP     DI
    POP     BP
    POP     BX
    CLC
    RET

    ; ================================================================
    ; ERROR PATHS
    ; ================================================================

.ec_too_large:
    ; ---- Show "Selection too large" dialog ----
    PUSH    SI                          ; prevent register leak to caller
    MOV     SI, m_str_clip_full
    CALL    show_error_msg
    POP     SI                          ; restore SI
    JMP     .ec_restore_fail

.ec_offset_overflow:
    MOV     SI, s_err_copy_big
    CALL    show_error_msg
    JMP     .ec_restore_fail

.ec_restore_fail:
    ; Something failed after we saved the cursor — restore it
    MOV     AX, [ec_save_line]
    MOV     [cur_line], AX
    MOV     AX, [ec_save_col]
    MOV     [cur_col], AX

.ec_fail:
    ; Common failure exit (also reached for collapsed selection)
    POP     ES
    POP     DI
    POP     BP
    POP     BX
    STC
    RET

.ec_nothing:
    ; No selection active — immediate return, nothing pushed
    STC
    RET
ed_copy ENDP

; ============================================================
; menu_copy_handler — Edit > Copy menu entry handler
; ============================================================

menu_copy_handler:
    JMP     ed_copy

; ============================================================
; ed_cut — Cut selected text (Copy + Delete)
;
; If no selection active: no-op (ed_copy returns CF=1).
; If selection exists: copies to clipboard, then deletes selection.
;
; Undo: sel_delete pushes a UR_DEL_RANGE record. One Ctrl+Z
; undoes the entire cut. Clipboard content is NOT affected by undo.
;
; Output: CF=0 cut performed, CF=1 nothing to cut
; Preserves: BX, BP, DI (same contract as ed_copy)
; Clobbers:  AX, CX, DX, SI
; ============================================================

ed_cut PROC
    ; ---- Copy selection to clipboard ----
    CALL    ed_copy
    JC      .cut_nothing            ; no selection or collapsed → no-op

    ; ---- Delete the selection ----
    ; ed_copy left sel_active=1, cursor/anchor unchanged.
    ; sel_delete reads these BSS vars directly (no register inputs).
    ; sel_delete clears sel_active, positions cursor at selection start,
    ; sets dirty/meta_dirty/FW_DIRTY, pushes UR_DEL_RANGE undo record.
    CALL    sel_delete
    ; CF from sel_delete: 0=deleted, 1=nothing (shouldn't happen since
    ; ed_copy just confirmed an active non-collapsed selection)

    CLC
    RET

.cut_nothing:
    STC
    RET
ed_cut ENDP

; ============================================================
; menu_cut_handler — Edit > Cut menu entry handler
; ============================================================

menu_cut_handler:
    JMP     ed_cut

; ============================================================
; ed_paste — Paste clipboard contents at cursor position
;
; If clipboard empty: no-op.
; If selection active: deletes selection first, then pastes.
; If add buffer full: calls ed_buffer_full, retries.
;
; The pasted content creates a single piece in the piece table
; pointing to the add buffer. Cursor advances to end of paste.
;
; Undo: pushes UR_INS_RANGE. One Ctrl+Z undoes the paste.
; If paste replaced a selection, two Ctrl+Z fully undoes
; (consistent with type-over-selection behavior).
;
; Output: CF=0 pasted, CF=1 nothing to paste (empty clipboard)
; Clobbers: AX, BX, CX, DX, SI, DI, BP, ES
; ============================================================

ed_paste PROC
    ; ================================================================
    ; STAGE 0: Check clipboard not empty
    ; ================================================================

    CMP     WORD [clip_used], 0
    JE      .ep_nothing

    PUSH    BX
    PUSH    BP
    PUSH    DI
    PUSH    ES

    ; ---- Snapshot add_used for safe rollback on any failure path ----
    MOV     AX, [add_used]
    MOV     [ep_save_add], AX

    ; ================================================================
    ; STAGE 1: Delete selection if active
    ;   sel_delete pushes UR_DEL_RANGE, clears sel_active,
    ;   positions cursor at selection start.
    ; ================================================================

    CMP     BYTE [sel_active], 0
    JE      .ep_no_sel
    CALL    sel_delete
.ep_no_sel:

    ; ================================================================
    ; STAGE 2: Check add buffer capacity
    ;   Need clip_used bytes of free space in add buffer.
    ;   If insufficient, call ed_buffer_full (save+reload).
    ;   After compact, reload add_seg/add_used (both may change).
    ; ================================================================

    MOV     AX, [add_used]
    ADD     AX, [clip_used]
    JNC     .ep_have_space          ; no carry = fits in 64K buffer

    ; Overflow: need compact (save+reload to free space)
    CALL    ed_buffer_full
    JC      .ep_fail                ; user cancelled or compact failed

    ; Re-verify capacity after compaction
    MOV     AX, [add_used]
    ADD     AX, [clip_used]
    JC      .ep_fail                ; still overflows 64K

.ep_have_space:

    ; ================================================================
    ; STAGE 3: Append clipboard to add buffer (bulk copy)
    ;   DS = CS (source setup uses clip_seg)
    ;   ES = add_seg (destination)
    ;   SI = 0 (start of clipboard)
    ;   DI = add_used (append position)
    ;   CX = clip_used (byte count)
    ; ================================================================

    ; ep_save_add already set at top of function (snapshot for rollback)
    ; Update it now to the current add_used (may differ after compact)
    MOV     AX, [add_used]
    MOV     [ep_save_add], AX

    ; Set up bulk copy: clip_seg:0 -> add_seg:add_used
    PUSH    DS
    MOV     AX, [clip_seg]
    MOV     DS, AX                  ; DS = clip_seg (source)
    XOR     SI, SI                  ; SI = 0 (start of clipboard)

    MOV     AX, CS:[add_seg]
    MOV     ES, AX                  ; ES = add_seg (destination)
    MOV     DI, CS:[add_used]       ; DI = current end of add buffer

    MOV     CX, CS:[clip_used]      ; CX = bytes to copy
    CLD
    REP     MOVSB                   ; bulk copy
    POP     DS                      ; DS = CS again

    ; Update add_used
    MOV     AX, [clip_used]
    ADD     [add_used], AX

    ; ================================================================
    ; STAGE 4: Find insertion point in piece table
    ;   cursor_to_offset uses cur_line/cur_col to find rnd_pi/rnd_off
    ; ================================================================

    CALL    cursor_to_offset        ; sets rnd_pi, rnd_off

    MOV     BX, [rnd_pi]
    MOV     SI, [rnd_off]           ; SI = offset within piece

    ; Guard: if cursor past all pieces, treat as insert-before
    CMP     BX, [pt_count]
    JAE     .ep_insert_before

    ; ---- Read piece length ----
    PUSH    DS
    MOV     AX, [pt_seg]
    MOV     DS, AX

    ; Compute descriptor offset = BX * 10
    CALL    mul_bx_10

    MOV     DX, [DI + PT_F_LENGTH]  ; DX = piece length (low word)
    POP     DS

    ; ---- Determine insertion case ----

    ; If SI == 0: insert before
    TEST    SI, SI
    JZ      .ep_insert_before

    ; If SI >= DX: insert after
    CMP     SI, DX
    JAE     .ep_insert_after

    ; Otherwise: split piece and insert between halves
    JMP     .ep_split_insert

    ; ================================================================
    ; CASE A: Insert new piece before rnd_pi
    ; ================================================================

.ep_insert_before:
    MOV     BYTE [meta_dirty], 1    ; checkpoint correction needed
    CALL    pt_insert               ; insert empty slot at BX
    JC      .ep_fail

    ; Write new piece descriptor at slot BX
    PUSH    ES
    MOV     AX, [pt_seg]
    MOV     ES, AX
    ; Compute DI = BX * 10
    CALL    mul_bx_10

    MOV     BYTE ES:[DI + PT_F_SOURCE], 1
    MOV     BYTE ES:[DI + PT_F_SOURCE + 1], 0
    MOV     AX, [ep_save_add]
    MOV     ES:[DI + PT_F_START], AX
    MOV     WORD ES:[DI + PT_F_START + 2], 0
    MOV     AX, [clip_used]
    MOV     ES:[DI + PT_F_LENGTH], AX
    MOV     WORD ES:[DI + PT_F_LENGTH + 2], 0
    POP     ES

    JMP     .ep_inserted

    ; ================================================================
    ; CASE B: Insert new piece after rnd_pi
    ; ================================================================

.ep_insert_after:
    INC     BX                      ; insert at BX+1
    CALL    pt_insert
    JC      .ep_fail

    PUSH    ES
    MOV     AX, [pt_seg]
    MOV     ES, AX
    CALL    mul_bx_10

    MOV     BYTE ES:[DI + PT_F_SOURCE], 1
    MOV     BYTE ES:[DI + PT_F_SOURCE + 1], 0
    MOV     AX, [ep_save_add]
    MOV     ES:[DI + PT_F_START], AX
    MOV     WORD ES:[DI + PT_F_START + 2], 0
    MOV     AX, [clip_used]
    MOV     ES:[DI + PT_F_LENGTH], AX
    MOV     WORD ES:[DI + PT_F_LENGTH + 2], 0
    POP     ES

    JMP     .ep_inserted

    ; ================================================================
    ; CASE C: Split piece at rnd_off, insert between halves
    ;
    ; Before: [piece BX: source, start, length=DX]
    ; After:  [left: source, start, length=SI]
    ;         [new paste piece: source=1, start=ep_save_add, length=clip_used]
    ;         [right: source, start+SI, length=DX-SI]
    ;
    ; SI = rnd_off (split point within piece)
    ; DX = original piece length
    ; ================================================================

.ep_split_insert:
    ; ES = pt_seg for reading original piece
    MOV     AX, [pt_seg]
    MOV     ES, AX

    ; Compute DI = BX * 10
    CALL    mul_bx_10

    ; Save fields for right piece (push order: source, start_lo, start_hi, right_len)
    MOV     AX, DX
    SUB     AX, SI
    PUSH    AX                      ; [stk+6] right_length = DX - SI
    MOV     AX, ES:[DI + PT_F_START + 2]
    PUSH    AX                      ; [stk+4] start_hi
    MOV     AX, ES:[DI + PT_F_START]
    PUSH    AX                      ; [stk+2] start_lo
    MOV     AL, ES:[DI + PT_F_SOURCE]
    XOR     AH, AH
    PUSH    AX                      ; [stk+0] source

    ; Shrink current piece to left half: length = SI
    MOV     ES:[DI + PT_F_LENGTH], SI
    MOV     WORD ES:[DI + PT_F_LENGTH + 2], 0

    ; Insert right piece at BX+1
    INC     BX
    CALL    pt_insert
    JC      .ep_split_fail

    ; Write right piece descriptor at BX
    MOV     AX, [pt_seg]
    MOV     ES, AX
    CALL    mul_bx_10

    POP     AX                      ; source
    MOV     ES:[DI + PT_F_SOURCE], AL
    MOV     BYTE ES:[DI + PT_F_SOURCE + 1], 0

    POP     AX                      ; start_lo
    ADD     AX, SI                  ; right.start = orig_start + SI
    MOV     ES:[DI + PT_F_START], AX
    POP     AX                      ; start_hi
    JNC     .ep_no_carry
    INC     AX
.ep_no_carry:
    MOV     ES:[DI + PT_F_START + 2], AX

    POP     AX                      ; right_length
    MOV     ES:[DI + PT_F_LENGTH], AX
    MOV     WORD ES:[DI + PT_F_LENGTH + 2], 0

    ; Insert paste piece at BX (pushes right to BX+1)
    CALL    pt_insert
    JC      .ep_fail

    ; Write paste piece descriptor at BX
    MOV     AX, [pt_seg]
    MOV     ES, AX
    CALL    mul_bx_10

    MOV     BYTE ES:[DI + PT_F_SOURCE], 1
    MOV     BYTE ES:[DI + PT_F_SOURCE + 1], 0
    MOV     AX, [ep_save_add]
    MOV     ES:[DI + PT_F_START], AX
    MOV     WORD ES:[DI + PT_F_START + 2], 0
    MOV     AX, [clip_used]
    MOV     ES:[DI + PT_F_LENGTH], AX
    MOV     WORD ES:[DI + PT_F_LENGTH + 2], 0

    JMP     .ep_inserted

.ep_split_fail:
    ADD     SP, 8                   ; discard 4 saved words
    JMP     .ep_fail

    ; ================================================================
    ; STAGE 5: Post-insertion — cursor, undo, dirty flags
    ; ================================================================

.ep_inserted:
    ; ---- Save insertion point for undo BEFORE moving cursor ----
    MOV     AX, [cur_line]
    MOV     [ec_save_line], AX      ; reuse ec_save_line as paste undo scratch
    MOV     AX, [cur_col]
    MOV     [ec_save_col], AX

    ; ---- Compute post-paste cursor position ----
    ; Uses pre-computed clip_lines and clip_last_col from ed_copy
    CMP     WORD [clip_lines], 0
    JNE     .ep_multi_line

    ; Single-line paste: just advance column
    MOV     AX, [clip_last_col]
    ADD     [cur_col], AX
    JMP     .ep_cursor_done

.ep_multi_line:
    ; Multi-line paste: advance line count, set col to last_col
    MOV     AX, [clip_lines]
    ADD     [cur_line], AX
    MOV     AX, [clip_last_col]
    MOV     [cur_col], AX

.ep_cursor_done:
    ; ---- Push UR_INS_RANGE undo record ----
    MOV     AL, UR_INS_RANGE
    XOR     AH, AH
    MOV     BX, [ec_save_line]
    MOV     CX, [ec_save_col]
    MOV     DX, [clip_used]         ; byte count in UR_CUR_LINE
    XOR     SI, SI                  ; 0 in UR_CUR_COL
    CALL    undo_push

    ; ---- Set dirty flags ----
    MOV     BYTE [dirty], 1
    MOV     BYTE [meta_dirty], 1
    MOV     WORD [last_ins_pi], 0FFFFh
    MOV     BYTE [fw_state + FW_DIRTY], 1

    ; ---- Scroll to new cursor position ----
    CALL    scroll_to_cursor

    POP     ES
    POP     DI
    POP     BP
    POP     BX
    CLC
    RET

.ep_fail:
    ; ---- Roll back add buffer to pre-paste state ----
    MOV     AX, [ep_save_add]
    MOV     [add_used], AX

    ; ---- Show error dialog ----
    MOV     SI, s_err_paste_pt
    CALL    show_error_msg

    POP     ES
    POP     DI
    POP     BP
    POP     BX
    STC
    RET

.ep_nothing:
    STC
    RET
ed_paste ENDP

; ============================================================
; menu_paste_handler — Edit > Paste menu entry handler
; ============================================================

menu_paste_handler:
    JMP     ed_paste
