; ============================================================
; ed_edit.inc — Editing Operations
;
; Procedures ported from TEXTEDIT.ASM. Pure piece-table
; operations — never touch shadow_buf or VRAM. Redraw is
; triggered via FW_DIRTY.
; ============================================================

; ============================================================
; INSERT_CHAR — insert a single character at the cursor position
; Input: AL = character to insert
; ============================================================

insert_char PROC
    PUSH    AX
    PUSH    BX
    PUSH    CX
    PUSH    DX
    PUSH    SI
    PUSH    DI
    PUSH    ES

    MOV     CL, AL            ; save char in CL

    ; ---- Append char to add buffer ----
    MOV     AX, [add_seg]
    MOV     ES, AX
    MOV     DI, [add_used]
    CMP     DI, ADD_BUF_BYTES - 1    ; overflow check
    JB      .ic_have_space

    ; ---- Buffer full: save and reload to free space ----
    CALL    ed_buffer_full
    JC      .ic_done           ; user cancelled or compact failed
    ; Compact succeeded — reload add_seg/add_used (both changed)
    MOV     AX, [add_seg]
    MOV     ES, AX
    MOV     DI, [add_used]

.ic_have_space:
    MOV     ES:[DI], CL
    INC     WORD [add_used]

    ; ---- FAST PATH: try to extend tracked piece WITHOUT cursor resolve ----
    CMP     WORD [last_ins_pi], 0FFFFh
    JE      .ic_need_resolve       ; no tracked piece — need full resolve

    ; Read tracked piece descriptor
    MOV     BX, [last_ins_pi]
    PUSH    DS
    MOV     AX, [pt_seg]
    MOV     DS, AX

    ; Compute descriptor offset = BX * 10
    MOV     DI, BX
    SHL     DI, 1
    MOV     AX, DI
    SHL     DI, 1
    SHL     DI, 1
    ADD     DI, AX                 ; DI = BX * 10

    ; Check source == 1 (add buffer)
    CMP     BYTE [DI + PT_F_SOURCE], 1
    JNE     .ic_fast_bail

    ; Check contiguity: piece.start + piece.length + 1 == add_used
    MOV     AX, [DI + PT_F_START]
    ADD     AX, [DI + PT_F_LENGTH]
    INC     AX
    CMP     AX, CS:[add_used]
    JNE     .ic_fast_bail          ; not contiguous

    ; ---- All conditions met: extend in place ----
    INC     WORD [DI + PT_F_LENGTH]
    POP     DS                     ; restore COM segment

    MOV     BYTE [dirty], 1
.ic_fast_no_vis:
    INC     WORD [cur_col]
    CALL    scroll_to_cursor
    MOV     BYTE [fw_state + FW_DIRTY], 1
    JMP     .ic_done

.ic_fast_bail:
    POP     DS                     ; restore COM segment

.ic_need_resolve:
    ; ---- SLOW PATH: full cursor resolution ----
    CALL    cursor_to_offset   ; sets rnd_pi, rnd_off
    MOV     BX, [rnd_pi]
    MOV     SI, [rnd_off]      ; SI = piece-relative offset

    ; Guard: if cursor is past all pieces, treat as insert-before
    CMP     BX, [pt_count]
    JAE     .ic_insert_before

    ; ---- Check if we're at piece boundary (offset 0 or end) ----
    ; Read piece length from descriptor
    PUSH    DS
    MOV     AX, [pt_seg]
    MOV     DS, AX

    ; Compute descriptor offset = BX * 10
    MOV     DI, BX
    SHL     DI, 1
    MOV     AX, DI
    SHL     DI, 1
    SHL     DI, 1
    ADD     DI, AX            ; DI = BX * 10

    MOV     DX, [DI + PT_F_LENGTH]  ; DX = piece length (low word)
    POP     DS                 ; restore COM segment

    ; ---- Insert at offset SI within piece BX (length DX) ----

    ; If SI == 0: insert new piece before BX
    TEST    SI, SI
    JZ      .ic_insert_before

    ; If SI == DX: insert new piece after BX
    CMP     SI, DX
    JAE     .ic_insert_after

    ; Otherwise: split piece BX at offset SI
    ; ---- S1: PRE-CHECK split+insert needs 2 free slots ----
    MOV     AX, [pt_count]
    ADD     AX, 2
    CMP     AX, MAX_PT_PIECES
    JBE     .ic_split_ok
    PUSH    SI
    PUSH    DI
    MOV     SI, s_err_pt_full
    MOV     DI, m_str_error
    CALL    tui_dlg_msgbox
    POP     DI
    POP     SI
    JMP     .ic_done
.ic_split_ok:
    ; We need: [left(len=SI)] [new_char(len=1)] [right(len=DX-SI)]
    ; Read original piece info with ES = pt_seg
    MOV     AX, [pt_seg]
    MOV     ES, AX

    ; Save fields needed for the right piece
    MOV     AX, DX
    SUB     AX, SI
    PUSH    AX                          ; [stk+6] right_length
    MOV     AX, ES:[DI + PT_F_START + 2]
    PUSH    AX                          ; [stk+4] start_hi
    MOV     AX, ES:[DI + PT_F_START]
    PUSH    AX                          ; [stk+2] start_lo
    MOV     AL, ES:[DI + PT_F_SOURCE]
    XOR     AH, AH
    PUSH    AX                          ; [stk+0] source

    ; Shrink piece BX to length = SI (left part)
    MOV     ES:[DI + PT_F_LENGTH], SI
    MOV     WORD ES:[DI + PT_F_LENGTH + 2], 0

    ; Insert right piece at BX+1
    INC     BX
    CALL    pt_insert
    JC      .ic_split_fail

    ; Write right piece descriptor at slot BX
    MOV     AX, [pt_seg]
    MOV     ES, AX
    MOV     DI, BX
    SHL     DI, 1
    MOV     AX, DI
    SHL     DI, 1
    SHL     DI, 1
    ADD     DI, AX                      ; DI = BX * 10

    POP     AX                          ; source
    MOV     ES:[DI + PT_F_SOURCE], AL
    MOV     BYTE ES:[DI + PT_F_SOURCE + 1], 0

    POP     AX                          ; start_lo
    ADD     AX, SI                      ; right.start = orig_start + SI
    MOV     ES:[DI + PT_F_START], AX
    POP     AX                          ; start_hi
    JNC     .no_carry
    INC     AX
.no_carry:
    MOV     ES:[DI + PT_F_START + 2], AX

    POP     AX                          ; right_length
    MOV     ES:[DI + PT_F_LENGTH], AX
    MOV     WORD ES:[DI + PT_F_LENGTH + 2], 0

    ; Insert new char piece at BX (pushes right piece to BX+1)
    CALL    pt_insert
    JC      .ic_done

    ; Write new char piece at slot BX
    MOV     AX, [pt_seg]
    MOV     ES, AX
    MOV     DI, BX
    SHL     DI, 1
    MOV     AX, DI
    SHL     DI, 1
    SHL     DI, 1
    ADD     DI, AX
    MOV     BYTE ES:[DI + PT_F_SOURCE], 1
    MOV     BYTE ES:[DI + PT_F_SOURCE + 1], 0
    MOV     AX, [add_used]
    DEC     AX
    MOV     ES:[DI + PT_F_START], AX
    MOV     WORD ES:[DI + PT_F_START + 2], 0
    MOV     WORD ES:[DI + PT_F_LENGTH], 1
    MOV     WORD ES:[DI + PT_F_LENGTH + 2], 0

    MOV     [last_ins_pi], BX
    MOV     BYTE [dirty], 1
    CMP     CL, 0Dh
    JE      .ic_done
    CMP     CL, 0Ah
    JE      .ic_done
    INC     WORD [cur_col]
    CALL    scroll_to_cursor
    MOV     BYTE [meta_dirty], 1
    MOV     BYTE [fw_state + FW_DIRTY], 1
    JMP     .ic_done

.ic_split_fail:
    ADD     SP, 8                       ; discard 4 saved words
    JMP     .ic_done

.ic_insert_before:
    ; ---- S2: PRE-CHECK insert needs 1 free slot ----
    CMP     WORD [pt_count], MAX_PT_PIECES
    JB      .ic_ib_ok
    PUSH    SI
    PUSH    DI
    MOV     SI, s_err_pt_full
    MOV     DI, m_str_error
    CALL    tui_dlg_msgbox
    POP     DI
    POP     SI
    JMP     .ic_done
.ic_ib_ok:
    ; Insert new piece at index BX (before current piece)
    MOV     BYTE [meta_dirty], 1
    CALL    pt_insert
    JC      .ic_done

    ; Write new piece: source=1, start=add_used-1, length=1
    PUSH    ES
    MOV     AX, [pt_seg]
    MOV     ES, AX
    ; Recompute DI for piece BX
    MOV     DI, BX
    SHL     DI, 1
    MOV     AX, DI
    SHL     DI, 1
    SHL     DI, 1
    ADD     DI, AX
    MOV     BYTE ES:[DI + PT_F_SOURCE], 1
    MOV     BYTE ES:[DI + PT_F_SOURCE + 1], 0
    MOV     AX, [add_used]
    DEC     AX
    MOV     ES:[DI + PT_F_START], AX
    MOV     WORD ES:[DI + PT_F_START + 2], 0
    MOV     WORD ES:[DI + PT_F_LENGTH], 1
    MOV     WORD ES:[DI + PT_F_LENGTH + 2], 0
    POP     ES

    MOV     [last_ins_pi], BX
    MOV     BYTE [dirty], 1
    ; Advance cursor column (if not CR/LF)
    CMP     CL, 0Dh
    JE      .ic_done
    CMP     CL, 0Ah
    JE      .ic_done
    INC     WORD [cur_col]
    CALL    scroll_to_cursor
    MOV     BYTE [fw_state + FW_DIRTY], 1
    JMP     .ic_done

.ic_insert_after:
    ; ---- S3: PRE-CHECK insert needs 1 free slot ----
    CMP     WORD [pt_count], MAX_PT_PIECES
    JB      .ic_ia_ok
    PUSH    SI
    PUSH    DI
    MOV     SI, s_err_pt_full
    MOV     DI, m_str_error
    CALL    tui_dlg_msgbox
    POP     DI
    POP     SI
    JMP     .ic_done
.ic_ia_ok:
    ; Insert new piece at index BX+1 (after current piece)
    INC     BX
    CALL    pt_insert
    JC      .ic_done

    PUSH    ES
    MOV     AX, [pt_seg]
    MOV     ES, AX
    MOV     DI, BX
    SHL     DI, 1
    MOV     AX, DI
    SHL     DI, 1
    SHL     DI, 1
    ADD     DI, AX
    MOV     BYTE ES:[DI + PT_F_SOURCE], 1
    MOV     BYTE ES:[DI + PT_F_SOURCE + 1], 0
    MOV     AX, [add_used]
    DEC     AX
    MOV     ES:[DI + PT_F_START], AX
    MOV     WORD ES:[DI + PT_F_START + 2], 0
    MOV     WORD ES:[DI + PT_F_LENGTH], 1
    MOV     WORD ES:[DI + PT_F_LENGTH + 2], 0
    POP     ES

    MOV     [last_ins_pi], BX
    MOV     BYTE [dirty], 1
    CMP     CL, 0Dh
    JE      .ic_done
    CMP     CL, 0Ah
    JE      .ic_done
.ic_after_no_vis:
    INC     WORD [cur_col]
    CALL    scroll_to_cursor
    MOV     BYTE [fw_state + FW_DIRTY], 1

.ic_done:
    POP     ES
    POP     DI
    POP     SI
    POP     DX
    POP     CX
    POP     BX
    POP     AX
    RET
insert_char ENDP

; ============================================================
; INSERT_CRLF — insert CR+LF (2-byte piece) at cursor position
; ============================================================

insert_crlf PROC
    PUSH    AX
    PUSH    BX
    PUSH    CX
    PUSH    DX
    PUSH    SI
    PUSH    DI
    PUSH    ES

    ; Append CR+LF to add buffer
    MOV     AX, [add_seg]
    MOV     ES, AX
    MOV     DI, [add_used]
    CMP     DI, ADD_BUF_BYTES - 2
    JB      .icr_have_space

    ; ---- Buffer full: save and reload to free space ----
    CALL    ed_buffer_full
    JC      .icr_done          ; user cancelled or compact failed
    ; Compact succeeded — reload add_seg/add_used
    MOV     AX, [add_seg]
    MOV     ES, AX
    MOV     DI, [add_used]

.icr_have_space:
    MOV     BYTE ES:[DI], 0Dh
    MOV     BYTE ES:[DI+1], 0Ah
    ADD     WORD [add_used], 2

    ; Find cursor position
    CALL    cursor_to_offset
    MOV     BX, [rnd_pi]
    MOV     SI, [rnd_off]

    ; Guard: if cursor is past all pieces, treat as insert-before
    CMP     BX, [pt_count]
    JAE     .icr_insert_before

    ; Read piece length
    PUSH    DS
    MOV     AX, [pt_seg]
    MOV     DS, AX
    MOV     DI, BX
    SHL     DI, 1
    MOV     AX, DI
    SHL     DI, 1
    SHL     DI, 1
    ADD     DI, AX
    MOV     DX, [DI + PT_F_LENGTH]
    POP     DS

    ; Same insert logic as insert_char: before, after, or split
    TEST    SI, SI
    JZ      .icr_insert_before
    CMP     SI, DX
    JAE     .icr_insert_after

    ; ---- S4: PRE-CHECK split+insert needs 2 free slots ----
    MOV     AX, [pt_count]
    ADD     AX, 2
    CMP     AX, MAX_PT_PIECES
    JBE     .icr_split_ok
    PUSH    SI
    PUSH    DI
    MOV     SI, s_err_pt_full
    MOV     DI, m_str_error
    CALL    tui_dlg_msgbox
    POP     DI
    POP     SI
    JMP     .icr_done
.icr_split_ok:
    ; ---- Split piece: [left] [CRLF] [right] ----
    MOV     AX, [pt_seg]
    MOV     ES, AX

    MOV     AX, DX
    SUB     AX, SI
    PUSH    AX                          ; right_length
    MOV     AX, ES:[DI + PT_F_START + 2]
    PUSH    AX                          ; start_hi
    MOV     AX, ES:[DI + PT_F_START]
    PUSH    AX                          ; start_lo
    MOV     AL, ES:[DI + PT_F_SOURCE]
    XOR     AH, AH
    PUSH    AX                          ; source

    MOV     ES:[DI + PT_F_LENGTH], SI
    MOV     WORD ES:[DI + PT_F_LENGTH + 2], 0

    INC     BX
    CALL    pt_insert
    JC      .icr_split_fail

    ; Write right piece at BX
    MOV     AX, [pt_seg]
    MOV     ES, AX
    MOV     DI, BX
    SHL     DI, 1
    MOV     AX, DI
    SHL     DI, 1
    SHL     DI, 1
    ADD     DI, AX
    POP     AX                          ; source
    MOV     ES:[DI + PT_F_SOURCE], AL
    MOV     BYTE ES:[DI + PT_F_SOURCE + 1], 0
    POP     AX                          ; start_lo
    ADD     AX, SI
    MOV     ES:[DI + PT_F_START], AX
    POP     AX                          ; start_hi
    JNC     .icr_nc
    INC     AX
.icr_nc:
    MOV     ES:[DI + PT_F_START + 2], AX
    POP     AX                          ; right_length
    MOV     ES:[DI + PT_F_LENGTH], AX
    MOV     WORD ES:[DI + PT_F_LENGTH + 2], 0

    ; Insert CRLF piece at BX (pushes right to BX+1)
    CALL    pt_insert
    JC      .icr_done
    JMP     .icr_write_piece

.icr_split_fail:
    ADD     SP, 8
    JMP     .icr_done

.icr_insert_before:
    ; ---- S5: PRE-CHECK insert needs 1 free slot ----
    CMP     WORD [pt_count], MAX_PT_PIECES
    JB      .icr_ib_ok
    PUSH    SI
    PUSH    DI
    MOV     SI, s_err_pt_full
    MOV     DI, m_str_error
    CALL    tui_dlg_msgbox
    POP     DI
    POP     SI
    JMP     .icr_done
.icr_ib_ok:
    CALL    pt_insert
    JC      .icr_done
    JMP     .icr_write_piece

.icr_insert_after:
    ; ---- S6: PRE-CHECK insert needs 1 free slot ----
    CMP     WORD [pt_count], MAX_PT_PIECES
    JB      .icr_ia_ok
    PUSH    SI
    PUSH    DI
    MOV     SI, s_err_pt_full
    MOV     DI, m_str_error
    CALL    tui_dlg_msgbox
    POP     DI
    POP     SI
    JMP     .icr_done
.icr_ia_ok:
    INC     BX
    CALL    pt_insert
    JC      .icr_done
    JMP     .icr_write_piece

.icr_write_piece:
    ; Write CRLF piece at index BX: source=1, start=add_used-2, length=2
    PUSH    ES
    MOV     AX, [pt_seg]
    MOV     ES, AX
    MOV     DI, BX
    SHL     DI, 1
    MOV     AX, DI
    SHL     DI, 1
    SHL     DI, 1
    ADD     DI, AX
    MOV     BYTE ES:[DI + PT_F_SOURCE], 1
    MOV     BYTE ES:[DI + PT_F_SOURCE + 1], 0
    MOV     AX, [add_used]
    SUB     AX, 2
    MOV     ES:[DI + PT_F_START], AX
    MOV     WORD ES:[DI + PT_F_START + 2], 0
    MOV     WORD ES:[DI + PT_F_LENGTH], 2
    MOV     WORD ES:[DI + PT_F_LENGTH + 2], 0
    POP     ES

    MOV     BYTE [dirty], 1
    MOV     WORD [last_ins_pi], 0FFFFh

.icr_done:
    POP     ES
    POP     DI
    POP     SI
    POP     DX
    POP     CX
    POP     BX
    POP     AX
    RET
insert_crlf ENDP

; ============================================================
; DELETE_BYTE — delete one byte at {rnd_pi, rnd_off}
; Output: CL = deleted byte, CF=0 success, CF=1 error
; ============================================================

delete_byte PROC
    MOV     BX, [rnd_pi]
    CMP     BX, [pt_count]
    JAE     .db_fail

    ; Read the byte at rnd_off
    PUSH    CX
    CALL    resolve_piece      ; AX=seg, SI=base, DX=piece_len
    POP     CX
    JC      .db_fail
    MOV     ES, AX
    MOV     DI, SI
    ADD     DI, [rnd_off]
    MOV     CL, ES:[DI]       ; CL = byte being deleted

    ; Get piece length from descriptor
    MOV     BX, [rnd_pi]
    MOV     SI, [rnd_off]     ; SI = offset within piece
    PUSH    DS
    MOV     AX, [pt_seg]
    MOV     DS, AX
    MOV     DI, BX
    SHL     DI, 1
    MOV     AX, DI
    SHL     DI, 1
    SHL     DI, 1
    ADD     DI, AX            ; DI = BX * 10
    MOV     DX, [DI + PT_F_LENGTH]
    POP     DS

    ; Case 1: piece length == 1 → remove entire piece
    CMP     DX, 1
    JE      .db_remove

    ; Case 2: offset == 0 → shrink from start
    TEST    SI, SI
    JZ      .db_shrink_start

    ; Case 3: offset == length-1 → shrink from end
    MOV     AX, DX
    DEC     AX
    CMP     SI, AX
    JAE     .db_shrink_end

    ; Case 4: middle → split piece, skip 1 byte
    JMP     .db_split

.db_remove:
    MOV     BYTE [meta_dirty], 1
    CALL    pt_remove
    CLC
    RET

.db_shrink_start:
    PUSH    DS
    MOV     AX, [pt_seg]
    MOV     DS, AX
    MOV     DI, BX
    SHL     DI, 1
    MOV     AX, DI
    SHL     DI, 1
    SHL     DI, 1
    ADD     DI, AX
    MOV     AX, [DI + PT_F_START]
    ADD     AX, 1
    MOV     [DI + PT_F_START], AX
    JNC     .ss_nc
    INC     WORD [DI + PT_F_START + 2]
.ss_nc:
    DEC     WORD [DI + PT_F_LENGTH]
    POP     DS
    CLC
    RET

.db_shrink_end:
    PUSH    DS
    MOV     AX, [pt_seg]
    MOV     DS, AX
    MOV     DI, BX
    SHL     DI, 1
    MOV     AX, DI
    SHL     DI, 1
    SHL     DI, 1
    ADD     DI, AX
    DEC     WORD [DI + PT_F_LENGTH]
    POP     DS
    CLC
    RET

.db_split:
    ; ---- S7: PRE-CHECK split needs 1 free slot ----
    CMP     WORD [pt_count], MAX_PT_PIECES
    JB      .db_split_cap_ok
    STC
    RET                             ; CF=1: caller handles gracefully
.db_split_cap_ok:
    ; Left: length = SI.  Right: start += SI+1, length = DX - SI - 1
    PUSH    DS
    MOV     AX, [pt_seg]
    MOV     ES, AX
    MOV     DI, BX
    SHL     DI, 1
    MOV     AX, DI
    SHL     DI, 1
    SHL     DI, 1
    ADD     DI, AX

    ; Save right piece fields
    MOV     AX, DX
    SUB     AX, SI
    DEC     AX
    PUSH    AX                          ; right_length
    MOV     AX, ES:[DI + PT_F_START + 2]
    PUSH    AX                          ; start_hi
    MOV     AX, ES:[DI + PT_F_START]
    PUSH    AX                          ; start_lo
    MOV     AL, ES:[DI + PT_F_SOURCE]
    XOR     AH, AH
    PUSH    AX                          ; source

    ; Shrink left to length = SI
    MOV     ES:[DI + PT_F_LENGTH], SI
    MOV     WORD ES:[DI + PT_F_LENGTH + 2], 0

    ; Insert right piece at BX+1
    INC     BX
    POP     DS
    PUSH    DS                          ; save source, restore later

    PUSH    CS
    POP     DS
    CALL    pt_insert
    JC      .db_split_fail
    MOV     BYTE CS:[meta_dirty], 1

    ; Write right piece descriptor
    MOV     AX, [pt_seg]
    MOV     ES, AX
    MOV     DI, BX
    SHL     DI, 1
    MOV     AX, DI
    SHL     DI, 1
    SHL     DI, 1
    ADD     DI, AX

    POP     AX                          ; source
    MOV     ES:[DI + PT_F_SOURCE], AL
    MOV     BYTE ES:[DI + PT_F_SOURCE + 1], 0
    POP     AX                          ; start_lo
    ADD     AX, SI
    INC     AX
    MOV     ES:[DI + PT_F_START], AX
    POP     AX                          ; start_hi
    JNC     .ds_nc
    INC     AX
.ds_nc:
    MOV     ES:[DI + PT_F_START + 2], AX
    POP     AX                          ; right_length
    MOV     ES:[DI + PT_F_LENGTH], AX
    MOV     WORD ES:[DI + PT_F_LENGTH + 2], 0

    POP     DS                          ; restore original DS

    ; Remove zero-length left piece if needed
    DEC     BX
    TEST    SI, SI
    JNZ     .db_split_ck_right
    CALL    pt_remove
    CLC
    RET

.db_split_ck_right:
    INC     BX
    MOV     AX, DX
    SUB     AX, SI
    DEC     AX
    TEST    AX, AX
    JNZ     .db_split_ok
    CALL    pt_remove
.db_split_ok:
    CLC
    RET

.db_split_fail:
    ADD     SP, 8                       ; discard 4 saved words
    POP     DS
.db_fail:
    STC
    RET
delete_byte ENDP

; ============================================================
; DELETE_AT — delete 1 byte (+ CRLF companion) at cursor pos
; Uses cursor_to_offset, then delete_byte, then CRLF handling.
; ============================================================

delete_at PROC
    PUSH    AX
    PUSH    BX
    PUSH    CX
    PUSH    DX
    PUSH    SI
    PUSH    DI
    PUSH    ES

    CALL    cursor_to_offset   ; sets rnd_pi, rnd_off
    CALL    delete_byte        ; CL = deleted byte
    JC      .da_done

    MOV     BYTE [dirty], 1
    MOV     [undo_del_char], CL
    MOV     BYTE [undo_del_crlf], 0

    ; ---- CRLF handling ----
    CMP     CL, 0Dh
    JE      .da_deleted_cr
    CMP     CL, 0Ah
    JE      .da_deleted_lf
    JMP     .da_done

.da_deleted_cr:
    MOV     BYTE [meta_dirty], 1
    ; Deleted CR — check if byte now at cursor position is LF
    CALL    cursor_to_offset
    MOV     BX, [rnd_pi]
    CMP     BX, [pt_count]
    JAE     .da_done
    PUSH    CX
    CALL    resolve_piece      ; AX=seg, SI=base, DX=piece_len
    POP     CX
    JC      .da_done
    MOV     ES, AX
    MOV     DI, SI
    ADD     DI, [rnd_off]
    ; Check bounds
    ADD     SI, DX             ; SI = piece end
    CMP     DI, SI
    JAE     .da_done
    CMP     BYTE ES:[DI], 0Ah
    JNE     .da_done
    ; Delete the LF (rnd_pi/rnd_off already correct)
    CALL    delete_byte
    MOV     BYTE [meta_dirty], 1
    MOV     BYTE [undo_del_crlf], 1
    JMP     .da_done

.da_deleted_lf:
    MOV     BYTE [meta_dirty], 1
    ; Deleted LF — check if preceding byte is CR
    CALL    cursor_to_offset
    MOV     BX, [rnd_pi]
    MOV     SI, [rnd_off]
    TEST    SI, SI
    JNZ     .da_lf_same_piece
    ; Offset 0 — look at last byte of previous piece
    TEST    BX, BX
    JZ      .da_done
    DEC     BX
    PUSH    CX
    CALL    resolve_piece      ; AX=seg, SI=base, DX=piece_len
    POP     CX
    JC      .da_done
    MOV     ES, AX
    MOV     DI, SI
    ADD     DI, DX
    DEC     DI
    CMP     BYTE ES:[DI], 0Dh
    JNE     .da_done
    ; Set rnd_pi/rnd_off to point to this CR
    MOV     [rnd_pi], BX
    DEC     DX
    MOV     [rnd_off], DX
    CALL    delete_byte
    MOV     BYTE [meta_dirty], 1
    MOV     BYTE [undo_del_crlf], 1
    JMP     .da_done

.da_lf_same_piece:
    ; Offset > 0 — check byte at rnd_off-1 in same piece
    PUSH    CX
    CALL    resolve_piece
    POP     CX
    JC      .da_done
    MOV     ES, AX
    MOV     DI, SI
    ADD     DI, [rnd_off]
    DEC     DI
    CMP     BYTE ES:[DI], 0Dh
    JNE     .da_done
    DEC     WORD [rnd_off]
    CALL    delete_byte
    MOV     BYTE [meta_dirty], 1
    MOV     BYTE [undo_del_crlf], 1
    JMP     .da_done

.da_done:
    MOV     WORD [last_ins_pi], 0FFFFh
    POP     ES
    POP     DI
    POP     SI
    POP     DX
    POP     CX
    POP     BX
    POP     AX
    RET
delete_at ENDP

; ============================================================
; SAVE_FILE — write piece table contents to filename
; Clears dirty flag on success.
; ============================================================

save_file PROC
    PUSH    AX
    PUSH    BX
    PUSH    CX
    PUSH    DX
    PUSH    SI
    PUSH    DI
    PUSH    BP
    PUSH    DS
    PUSH    ES

    ; Create/truncate output file
    MOV     DX, filename
    XOR     CX, CX            ; normal attributes
    MOV     AH, 3Ch
    INT     21h
    JNC     .sf_created
    ; ---- Creation failed — show dialog ----
    PUSH    SI
    PUSH    DI
    MOV     SI, s_err_save_fail
    MOV     DI, m_str_error
    CALL    tui_dlg_msgbox
    POP     DI
    POP     SI
    JMP     .sf_done           ; dirty flag remains set
.sf_created:

    MOV     BP, AX             ; BP = file handle

    ; Walk pieces, coalescing contiguous same-segment runs.
    XOR     DI, DI             ; DI = piece index

    ; Resolve first piece to seed the pending write
    CMP     DI, [pt_count]
    JAE     .sf_close_ok
    MOV     BX, DI
    CALL    resolve_piece      ; AX=seg, SI=base, DX=piece_len
    JC      .sf_close_ok

    ; Seed pending write: segment AX, offset SI, length DX
    PUSH    AX                 ; [stk] = pending segment
    MOV     [rnd_base], SI     ; pending start offset
    MOV     [rnd_len], DX      ; pending length
    INC     DI

.sf_coalesce:
    CMP     DI, [pt_count]
    JAE     .sf_flush           ; no more pieces — flush pending

    MOV     BX, DI
    CALL    resolve_piece      ; AX=seg, SI=base, DX=piece_len
    JC      .sf_flush

    ; Check if this piece is contiguous with pending write
    POP     BX                 ; BX = pending segment
    PUSH    BX
    CMP     AX, BX
    JNE     .sf_break          ; different segment

    MOV     BX, [rnd_base]
    ADD     BX, [rnd_len]      ; BX = pending end offset
    CMP     SI, BX
    JNE     .sf_break          ; not contiguous

    ; Contiguous — extend pending write
    ADD     [rnd_len], DX
    INC     DI
    JMP     .sf_coalesce

.sf_break:
    ; Flush pending write, then seed new one from current piece
    POP     BX                 ; BX = pending segment (consumed)

    PUSH    AX                 ; save current piece segment
    PUSH    SI                 ; save current piece base
    PUSH    DX                 ; save current piece length

    MOV     DS, BX
    MOV     DX, CS:[rnd_base]
    MOV     CX, CS:[rnd_len]
    MOV     BX, BP
    MOV     AH, 40h
    INT     21h

    PUSH    CS
    POP     DS
    JC      .sf_write_failed_break
    CMP     AX, CX
    JE      .sf_write_ok_break
.sf_write_failed_break:
    PUSH    SI
    PUSH    DI
    MOV     SI, s_err_save_write
    MOV     DI, m_str_error
    CALL    tui_dlg_msgbox
    POP     DI
    POP     SI
    JMP     .sf_break_err

.sf_write_ok_break:
    POP     DX                 ; current piece length
    POP     SI                 ; current piece base
    POP     AX                 ; current piece segment

    PUSH    AX                 ; push new pending segment
    MOV     [rnd_base], SI
    MOV     [rnd_len], DX
    INC     DI
    JMP     .sf_coalesce

.sf_flush:
    ; Write final pending block
    POP     BX                 ; BX = pending segment
    MOV     DS, BX
    MOV     DX, CS:[rnd_base]
    MOV     CX, CS:[rnd_len]
    MOV     BX, BP
    MOV     AH, 40h
    INT     21h

    PUSH    CS
    POP     DS
    JC      .sf_write_failed_flush
    CMP     AX, CX
    JE      .sf_close_ok
.sf_write_failed_flush:
    PUSH    SI
    PUSH    DI
    MOV     SI, s_err_save_write
    MOV     DI, m_str_error
    CALL    tui_dlg_msgbox
    POP     DI
    POP     SI
    JMP     .sf_err

.sf_close_ok:
    PUSH    CS
    POP     DS
    MOV     BX, BP
    MOV     AH, 3Eh
    INT     21h
    MOV     BYTE [dirty], 0
    JMP     .sf_done

.sf_break_err:
    ADD     SP, 6              ; discard 3 saved words
    ; fall through to sf_err

.sf_err:
    PUSH    CS
    POP     DS
    MOV     BX, BP
    MOV     AH, 3Eh
    INT     21h
    ; dirty stays set

.sf_done:
    POP     ES
    POP     DS
    POP     BP
    POP     DI
    POP     SI
    POP     DX
    POP     CX
    POP     BX
    POP     AX
    RET
save_file ENDP
