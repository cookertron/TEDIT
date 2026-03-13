; ============================================================
; ed_undo.inc — Undo/Redo Buffer Primitives
;
; Circular buffer of fixed-size undo records in a separately
; allocated segment. Supports push, peek-undo, peek-redo,
; clear, and automatic discard-oldest-half on overflow.
;
; BSS variables (defined in TEDIT.ASM):
;   undo_seg, undo_pos, undo_count, undo_max,
;   undo_recording, undo_save_pos
; ============================================================

; ============================================================
; undo_init — Reset undo buffer to empty, set capacity.
; Called once after allocation, and on file load/new doc.
; Clobbers: none
; ============================================================

undo_init PROC
    MOV     WORD [undo_pos], 0
    MOV     WORD [undo_count], 0
    MOV     WORD [undo_max], UNDO_MAX_RECORDS
    MOV     BYTE [undo_recording], 1
    MOV     WORD [undo_save_pos], 0
    MOV     BYTE [undo_group_active], 0
    RET
undo_init ENDP

; ============================================================
; undo_push — Write one undo record.
;
; Input:  AL = UR_TYPE (type + flags)
;         AH = UR_CHAR
;         BX = UR_DOC_LINE
;         CX = UR_DOC_COL
;         DX = UR_CUR_LINE
;         SI = UR_CUR_COL
; Output: CF=0 written, CF=1 suppressed (undo_recording==0)
; Clobbers: AX, BX, CX, DX, SI, DI
; ============================================================

undo_push PROC
    CMP     BYTE [undo_recording], 0
    JE      .skip

    ; ---- Save all input values ----
    PUSH    ES
    PUSH    AX                      ; type + char
    PUSH    BX                      ; doc_line
    PUSH    CX                      ; doc_col
    PUSH    DX                      ; cur_line
    PUSH    SI                      ; cur_col

    ; ---- Check buffer full ----
    MOV     AX, [undo_pos]
    CMP     AX, [undo_max]
    JB      .have_space
    CALL    undo_discard_half

.have_space:
    ; ---- Compute byte offset: DI = undo_pos * UR_SIZE ----
    MOV     AX, [undo_pos]
    MOV     BX, UR_SIZE
    MUL     BX                      ; AX = offset (fits 16 bits)
    MOV     DI, AX

    ; ---- Point ES to undo segment ----
    MOV     AX, [undo_seg]
    MOV     ES, AX

    ; ---- Write fields from stack (reverse push order) ----
    POP     AX                      ; cur_col
    MOV     ES:[DI + UR_CUR_COL], AX
    POP     AX                      ; cur_line
    MOV     ES:[DI + UR_CUR_LINE], AX
    POP     AX                      ; doc_col
    MOV     ES:[DI + UR_DOC_COL], AX
    POP     AX                      ; doc_line
    MOV     ES:[DI + UR_DOC_LINE], AX
    POP     AX                      ; type + char
    MOV     BYTE ES:[DI + UR_TYPE], AL
    MOV     BYTE ES:[DI + UR_CHAR], AH

    POP     ES

    ; ---- Advance position, truncate redo tail ----
    INC     WORD [undo_pos]
    MOV     AX, [undo_pos]

    ; ---- Check if redo tail was truncated ----
    CMP     AX, [undo_count]
    JA      .no_trunc                   ; was append, no redo existed

    ; Redo tail truncated. Invalidate save_pos if it was at or past
    ; the position we just wrote (= AX - 1, the old undo_pos).
    CMP     WORD [undo_save_pos], 0FFFFh
    JE      .no_trunc                   ; already invalid
    MOV     BX, AX
    DEC     BX                          ; BX = position just written
    CMP     [undo_save_pos], BX
    JB      .no_trunc                   ; save_pos in safe history
    MOV     WORD [undo_save_pos], 0FFFFh

.no_trunc:
    MOV     [undo_count], AX

    CLC
    RET

.skip:
    STC
    RET
undo_push ENDP

; ============================================================
; undo_discard_half — Discard oldest half of records.
; Shifts the newest half down to position 0.
; Adjusts undo_pos, undo_count, undo_save_pos.
; Clobbers: none (all regs saved/restored)
; ============================================================

undo_discard_half PROC
    PUSH    ES
    PUSH    DS
    PUSH    AX
    PUSH    BX
    PUSH    CX
    PUSH    SI
    PUSH    DI

    ; ---- Compute half ----
    MOV     AX, [undo_max]
    SHR     AX, 1                   ; AX = half
    MOV     BX, AX                  ; BX = half (save)

    ; ---- Compute source byte offset = half * UR_SIZE ----
    MOV     CX, UR_SIZE
    MUL     CX                      ; AX = half * 10
    MOV     SI, AX                  ; SI = source offset

    ; ---- Compute word count = (undo_pos - half) * UR_SIZE / 2 ----
    MOV     AX, [undo_pos]
    SUB     AX, BX                  ; AX = records to keep
    MUL     CX                      ; AX = byte count (CX still = UR_SIZE)
    SHR     AX, 1                   ; AX = word count
    MOV     CX, AX

    ; ---- Block copy within undo segment ----
    MOV     AX, [undo_seg]
    MOV     DS, AX
    MOV     ES, AX
    XOR     DI, DI                  ; dest = 0
    CLD
    REP     MOVSW

    ; ---- Restore DS to access BSS ----
    PUSH    CS
    POP     DS

    ; ---- Adjust positions ----
    SUB     [undo_pos], BX
    SUB     [undo_count], BX

    ; ---- Adjust save position (5a: sentinel guard) ----
    CMP     WORD [undo_save_pos], 0FFFFh
    JE      .done                       ; sentinel — leave it alone
    CMP     [undo_save_pos], BX
    JAE     .save_ok
    MOV     WORD [undo_save_pos], 0FFFFh
    JMP     .done

.save_ok:
    SUB     [undo_save_pos], BX

.done:
    POP     DI
    POP     SI
    POP     CX
    POP     BX
    POP     AX
    POP     DS
    POP     ES
    RET
undo_discard_half ENDP

; ============================================================
; undo_peek_undo — Get pointer to the most recent undo record.
;
; Output: CF=0: ES:BX = record at (undo_pos - 1)
;         CF=1: nothing to undo (undo_pos == 0)
; Clobbers: AX, BX, ES
; ============================================================

undo_peek_undo PROC
    CMP     WORD [undo_pos], 0
    JE      .nothing

    MOV     AX, [undo_pos]
    DEC     AX
    PUSH    DX
    MOV     BX, UR_SIZE
    MUL     BX                      ; AX = offset
    POP     DX
    MOV     BX, AX
    MOV     AX, [undo_seg]
    MOV     ES, AX
    CLC
    RET

.nothing:
    STC
    RET
undo_peek_undo ENDP

; ============================================================
; undo_peek_redo — Get pointer to the next redo record.
;
; Output: CF=0: ES:BX = record at undo_pos
;         CF=1: nothing to redo (undo_pos >= undo_count)
; Clobbers: AX, BX, ES
; ============================================================

undo_peek_redo PROC
    MOV     AX, [undo_pos]
    CMP     AX, [undo_count]
    JAE     .nothing

    PUSH    DX
    MOV     BX, UR_SIZE
    MUL     BX
    POP     DX
    MOV     BX, AX
    MOV     AX, [undo_seg]
    MOV     ES, AX
    CLC
    RET

.nothing:
    STC
    RET
undo_peek_redo ENDP

; ============================================================
; undo_clear — Reset undo buffer to empty.
; Clobbers: none
; ============================================================

undo_clear PROC
    MOV     WORD [undo_pos], 0
    MOV     WORD [undo_count], 0
    MOV     WORD [undo_save_pos], 0         ; position 0 = clean state
    MOV     BYTE [undo_group_active], 0
    RET
undo_clear ENDP

; ============================================================
; undo_execute — Undo the most recent operation (or group).
;
; Decrements undo_pos and executes the inverse of the record.
; If the record has URF_GROUP_CONT, continues undoing previous
; records until a record without the flag is reached.
;
; Input:  none
; Output: none (document and cursor state modified)
; Clobbers: AX, BX, CX, DX, SI, DI
; ============================================================

undo_execute PROC
    ; ---- Check if anything to undo ----
    CMP     WORD [undo_pos], 0
    JE      .nothing

    ; ---- Disable recording (prevent inverse ops from recording) ----
    MOV     BYTE [undo_recording], 0

.u_loop:
    ; ---- Decrement undo_pos ----
    DEC     WORD [undo_pos]

    ; ---- Read record: ES:BX = record at undo_pos ----
    MOV     AX, [undo_pos]
    PUSH    DX
    MOV     BX, UR_SIZE
    MUL     BX                      ; AX = undo_pos * 10
    POP     DX
    MOV     BX, AX                  ; BX = byte offset
    MOV     AX, [undo_seg]
    MOV     ES, AX                  ; ES:BX = record

    ; ---- Save cursor-restore values before inverse op clobbers ES:BX ----
    MOV     AX, ES:[BX + UR_CUR_LINE]
    MOV     [undo_pre_line], AX
    MOV     AX, ES:[BX + UR_CUR_COL]
    MOV     [undo_pre_col], AX

    ; ---- Save raw type for group check ----
    MOV     AL, ES:[BX + UR_TYPE]
    MOV     [undo_last_type], AL

    ; ---- Save doc position and char on stack for inverse op ----
    MOV     AX, ES:[BX + UR_DOC_LINE]
    PUSH    AX                          ; [stk+4]
    MOV     AX, ES:[BX + UR_DOC_COL]
    PUSH    AX                          ; [stk+2]
    MOV     AL, ES:[BX + UR_CHAR]
    XOR     AH, AH
    PUSH    AX                          ; [stk+0] char in AL

    ; ---- Dispatch by type ----
    MOV     AL, [undo_last_type]
    AND     AL, UR_TYPE_MASK

    CMP     AL, UR_INS_CHAR
    JE      .u_ins_char
    CMP     AL, UR_INS_CRLF
    JE      .u_ins_crlf
    CMP     AL, UR_DEL_CHAR
    JE      .u_del_char
    CMP     AL, UR_DEL_CRLF
    JE      .u_del_crlf
    ; Unknown type — clean stack, skip
    ADD     SP, 6
    JMP     .u_finish

    ; ---- UR_INS_CHAR: undo insert -> delete ----
.u_ins_char:
    ADD     SP, 2                       ; discard char (unused)
    POP     AX                          ; doc_col
    MOV     [cur_col], AX
    POP     AX                          ; doc_line
    MOV     [cur_line], AX
    CALL    delete_at
    JMP     .u_restore

    ; ---- UR_INS_CRLF: undo CRLF insert -> delete ----
.u_ins_crlf:
    ADD     SP, 2                       ; discard char
    POP     AX                          ; doc_col
    MOV     [cur_col], AX
    POP     AX                          ; doc_line
    MOV     [cur_line], AX
    CALL    delete_at
    JMP     .u_restore

    ; ---- UR_DEL_CHAR: undo delete -> re-insert ----
.u_del_char:
    POP     AX                          ; AL = char to re-insert
    POP     CX                          ; doc_col
    MOV     [cur_col], CX
    POP     CX                          ; doc_line
    MOV     [cur_line], CX
    CALL    insert_char                 ; AL = character
    JMP     .u_restore

    ; ---- UR_DEL_CRLF: undo CRLF delete -> re-insert ----
.u_del_crlf:
    ADD     SP, 2                       ; discard char
    POP     AX                          ; doc_col
    MOV     [cur_col], AX
    POP     AX                          ; doc_line
    MOV     [cur_line], AX
    CALL    insert_crlf
    JMP     .u_restore

    ; ---- Restore cursor and check for group continuation ----
.u_restore:
    MOV     AX, [undo_pre_line]
    MOV     [cur_line], AX
    MOV     AX, [undo_pre_col]
    MOV     [cur_col], AX

    ; ---- Group check: continue if record had URF_GROUP_CONT ----
    TEST    BYTE [undo_last_type], URF_GROUP_CONT
    JZ      .u_finish
    CMP     WORD [undo_pos], 0
    JE      .u_finish
    JMP     .u_loop

.u_finish:
    ; ---- Reset state ----
    MOV     WORD [last_ins_pi], 0FFFFh
    MOV     BYTE [meta_dirty], 1
    CALL    scroll_to_cursor

    ; ---- Smart dirty: check if we've returned to saved state ----
    MOV     AX, [undo_pos]
    CMP     AX, [undo_save_pos]
    JNE     .u_still_dirty
    MOV     BYTE [dirty], 0
    JMP     .u_dirty_done
.u_still_dirty:
    MOV     BYTE [dirty], 1
.u_dirty_done:
    MOV     BYTE [fw_state + FW_DIRTY], 1

    ; ---- Re-enable recording ----
    MOV     BYTE [undo_recording], 1

.nothing:
    RET
undo_execute ENDP

; ============================================================
; redo_execute — Redo the next undone operation (or group).
;
; Reads the record at undo_pos and re-executes the forward
; operation. Increments undo_pos. If the NEXT record has
; URF_GROUP_CONT, continues redoing until it doesn't.
;
; Input:  none
; Output: none (document and cursor state modified)
; Clobbers: AX, BX, CX, DX, SI, DI
; ============================================================

redo_execute PROC
    ; ---- Check if anything to redo ----
    MOV     AX, [undo_pos]
    CMP     AX, [undo_count]
    JAE     .nothing

    ; ---- Disable recording ----
    MOV     BYTE [undo_recording], 0

.r_loop:
    ; ---- Read record: ES:BX = record at undo_pos ----
    MOV     AX, [undo_pos]
    PUSH    DX
    MOV     BX, UR_SIZE
    MUL     BX                      ; AX = undo_pos * 10
    POP     DX
    MOV     BX, AX                  ; BX = byte offset
    MOV     AX, [undo_seg]
    MOV     ES, AX                  ; ES:BX = record

    ; ---- Save doc position and char on stack ----
    MOV     AX, ES:[BX + UR_DOC_LINE]
    PUSH    AX                          ; [stk+4]
    MOV     AX, ES:[BX + UR_DOC_COL]
    PUSH    AX                          ; [stk+2]
    MOV     AL, ES:[BX + UR_CHAR]
    XOR     AH, AH
    PUSH    AX                          ; [stk+0] char in AL

    ; ---- Extract type for dispatch ----
    MOV     AL, ES:[BX + UR_TYPE]
    AND     AL, UR_TYPE_MASK

    ; ---- Dispatch by type ----
    CMP     AL, UR_INS_CHAR
    JE      .r_ins_char
    CMP     AL, UR_INS_CRLF
    JE      .r_ins_crlf
    CMP     AL, UR_DEL_CHAR
    JE      .r_del_char
    CMP     AL, UR_DEL_CRLF
    JE      .r_del_crlf
    ; Unknown type — clean stack, skip
    ADD     SP, 6
    JMP     .r_finish

    ; ---- UR_INS_CHAR: redo insert ----
.r_ins_char:
    POP     AX                          ; AL = char to insert
    POP     CX                          ; doc_col
    MOV     [cur_col], CX
    POP     CX                          ; doc_line
    MOV     [cur_line], CX
    CALL    insert_char                 ; AL = character
    JMP     .r_advance

    ; ---- UR_INS_CRLF: redo CRLF insert ----
.r_ins_crlf:
    ADD     SP, 2                       ; discard char
    POP     AX                          ; doc_col
    MOV     [cur_col], AX
    POP     AX                          ; doc_line
    MOV     [cur_line], AX
    CALL    insert_crlf
    INC     WORD [cur_line]
    MOV     WORD [cur_col], 0
    JMP     .r_advance

    ; ---- UR_DEL_CHAR: redo delete ----
.r_del_char:
    ADD     SP, 2                       ; discard char (unused for delete)
    POP     AX                          ; doc_col
    MOV     [cur_col], AX
    POP     AX                          ; doc_line
    MOV     [cur_line], AX
    CALL    delete_at
    JMP     .r_advance

    ; ---- UR_DEL_CRLF: redo CRLF delete ----
.r_del_crlf:
    ADD     SP, 2                       ; discard char
    POP     AX                          ; doc_col
    MOV     [cur_col], AX
    POP     AX                          ; doc_line
    MOV     [cur_line], AX
    CALL    delete_at
    JMP     .r_advance

    ; ---- Advance position and check for group continuation ----
.r_advance:
    INC     WORD [undo_pos]

    ; ---- Group check: does the NEXT record have URF_GROUP_CONT? ----
    MOV     AX, [undo_pos]
    CMP     AX, [undo_count]
    JAE     .r_finish                   ; no more records

    ; Peek at next record's type
    PUSH    DX
    MOV     BX, UR_SIZE
    MUL     BX
    POP     DX
    MOV     BX, AX
    MOV     AX, [undo_seg]
    MOV     ES, AX
    TEST    BYTE ES:[BX + UR_TYPE], URF_GROUP_CONT
    JNZ     .r_loop                     ; next is continuation → keep going

.r_finish:
    MOV     WORD [last_ins_pi], 0FFFFh
    MOV     BYTE [meta_dirty], 1
    CALL    scroll_to_cursor

    ; ---- Smart dirty: check if we've returned to saved state ----
    MOV     AX, [undo_pos]
    CMP     AX, [undo_save_pos]
    JNE     .r_still_dirty
    MOV     BYTE [dirty], 0
    JMP     .r_dirty_done
.r_still_dirty:
    MOV     BYTE [dirty], 1
.r_dirty_done:
    MOV     BYTE [fw_state + FW_DIRTY], 1

    ; ---- Re-enable recording ----
    MOV     BYTE [undo_recording], 1

.nothing:
    RET
redo_execute ENDP
