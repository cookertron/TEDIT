; ============================================================
; ed_sel.inc — Selection Helper Procedures
;
; Depends on: ed_core.inc (cur_line, cur_col BSS variables),
;             TUI framework (fw_state, FW_DIRTY)
; Must come before: ed_draw.inc, ed_keys.inc, ed_mouse.inc
; ============================================================

; ============================================================
; sel_clear — Clear any active selection
;
; If sel_active is 0: immediate RET, no side effects.
; If sel_active is 1: zeros it and requests redraw.
; Preserves ALL registers.
; ============================================================

sel_clear PROC
    CMP     BYTE [sel_active], 0
    JE      .done
    MOV     BYTE [sel_active], 0
    MOV     BYTE [fw_state + FW_DIRTY], 1
.done:
    RET
sel_clear ENDP

; ============================================================
; sel_start_or_extend — Begin or extend a selection
;
; If sel_active == 0: snapshots (cur_line, cur_col) into anchor,
;                     sets active.
; If sel_active == 1: no-op (anchor stays where it was).
; Preserves all registers except flags.
; ============================================================

sel_start_or_extend PROC
    CMP     BYTE [sel_active], 0
    JNE     .done
    PUSH    AX
    MOV     AX, [cur_line]
    MOV     [sel_anchor_line], AX
    MOV     AX, [cur_col]
    MOV     [sel_anchor_col], AX
    MOV     BYTE [sel_active], 1
    POP     AX
.done:
    RET
sel_start_or_extend ENDP

; ============================================================
; sel_normalize — Return selection range in document order
;
; Outputs: BX=start_line, CX=start_col, DX=end_line, SI=end_col
; Start is always <= end in document order.
; Caller MUST verify sel_active == 1 before calling.
; Clobbers: BX, CX, DX, SI (all outputs). AX preserved.
; ============================================================

sel_normalize PROC
    MOV     BX, [sel_anchor_line]
    MOV     CX, [sel_anchor_col]
    MOV     DX, [cur_line]
    MOV     SI, [cur_col]
    CMP     BX, DX
    JB      .ordered
    JA      .swap
    CMP     CX, SI
    JBE     .ordered
.swap:
    XCHG    BX, DX
    XCHG    CX, SI
.ordered:
    RET
sel_normalize ENDP

; ============================================================
; sel_delete — Delete the selected range (fast path)
;
; Attempts range deletion via range_delete_pieces (O(P)).
; Falls back to sel_delete_legacy (O(N²)) on failure.
;
; If no selection active: returns CF=1 (caller proceeds normally).
; If collapsed selection:  clears selection, returns CF=1.
; If selection deleted:    returns CF=0, cursor at sel start.
; Preserves: AX, BX, CX, DX, SI, BP, DI.
; ============================================================

sel_delete PROC
    CMP     BYTE [sel_active], 0
    JE      .sd_nothing

    PUSH    AX
    PUSH    BX
    PUSH    CX
    PUSH    DX
    PUSH    SI
    PUSH    BP

    ; ---- Get normalized range ----
    CALL    sel_normalize           ; BX=s_line, CX=s_col, DX=e_line, SI=e_col

    ; ---- Check for collapsed selection ----
    CMP     BX, DX
    JNE     .sd_has_range
    CMP     CX, SI
    JNE     .sd_has_range
    CALL    sel_clear
    POP     BP
    POP     SI
    POP     DX
    POP     CX
    POP     BX
    POP     AX
    STC
    RET

.sd_has_range:
    ; ---- Save selection endpoints and pre-cursor ----
    MOV     [sel_s_line], BX
    MOV     [sel_s_col], CX
    MOV     [sel_e_line], DX
    MOV     [sel_e_col], SI

    MOV     AX, [cur_line]
    MOV     [undo_pre_line], AX
    MOV     AX, [cur_col]
    MOV     [undo_pre_col], AX

    ; ---- Convert start to byte offset ----
    MOV     BX, [sel_s_line]
    MOV     CX, [sel_s_col]
    CALL    line_col_to_offset      ; DX:AX = start offset
    TEST    DX, DX
    JNZ     .sd_fallback            ; >64K: use legacy path
    MOV     [rd_start_off], AX      ; save start offset (16-bit)

    ; ---- Convert end to byte offset ----
    MOV     BX, [sel_e_line]
    MOV     CX, [sel_e_col]
    CALL    line_col_to_offset      ; DX:AX = end offset
    TEST    DX, DX
    JNZ     .sd_fallback            ; >64K: use legacy path
    ; AX = end offset

    ; ---- Validate: start < end ----
    MOV     DX, AX                  ; DX = end_off
    MOV     AX, [rd_start_off]      ; AX = start_off
    CMP     AX, DX
    JAE     .sd_fallback

    ; ---- Execute range delete ----
    CALL    range_delete_pieces     ; AX=start_off, DX=end_off
    JC      .sd_fallback

    ; ---- Push single undo record ----
    MOV     BX, [sel_s_line]
    MOV     CX, [sel_s_col]
    MOV     DX, [undo_pre_line]
    MOV     SI, [undo_pre_col]
    CALL    undo_push_range

    ; ---- Position cursor at selection start ----
    MOV     AX, [sel_s_line]
    MOV     [cur_line], AX
    MOV     AX, [sel_s_col]
    MOV     [cur_col], AX

    ; ---- Clean up ----
    CALL    sel_clear
    CALL    scroll_to_cursor
    MOV     BYTE [fw_state + FW_DIRTY], 1
    MOV     WORD [last_ins_pi], 0FFFFh
    MOV     BYTE [undo_group_active], 0
    MOV     BYTE [meta_dirty], 1
    MOV     BYTE [dirty], 1

    POP     BP
    POP     SI
    POP     DX
    POP     CX
    POP     BX
    POP     AX
    CLC
    RET

.sd_fallback:
    ; ---- Restore pre-cursor before legacy attempt ----
    MOV     AX, [undo_pre_line]
    MOV     [cur_line], AX
    MOV     AX, [undo_pre_col]
    MOV     [cur_col], AX

    POP     BP
    POP     SI
    POP     DX
    POP     CX
    POP     BX
    POP     AX

    ; ---- Fall through to legacy char-by-char deletion ----
    JMP     sel_delete_legacy

.sd_nothing:
    STC
    RET
sel_delete ENDP

; ============================================================
; sel_delete_legacy — Delete the selected range (char-by-char fallback)
;
; Walks backward from selection end to start, calling delete_at
; for each character. Each deletion is recorded as a grouped
; undo record (first record plain, subsequent with URF_GROUP_CONT).
;
; If no selection active: returns CF=1 (caller proceeds normally).
; If collapsed selection:  clears selection, returns CF=1.
; If selection deleted:    returns CF=0, cursor at sel start.
; Preserves: AX, BX, CX, DX, SI, BP, DI.
; ============================================================

sel_delete_legacy PROC
    CMP     BYTE [sel_active], 0
    JE      .sdl_nothing

    PUSH    AX
    PUSH    BX
    PUSH    CX
    PUSH    DX
    PUSH    SI
    PUSH    BP

    ; ---- Get normalized range ----
    CALL    sel_normalize           ; BX=s_line, CX=s_col, DX=e_line, SI=e_col

    ; ---- Check for collapsed selection (start == end) ----
    CMP     BX, DX
    JNE     .sdl_has_range
    CMP     CX, SI
    JNE     .sdl_has_range
    ; Collapsed — treat as no selection
    CALL    sel_clear
    POP     BP
    POP     SI
    POP     DX
    POP     CX
    POP     BX
    POP     AX
    STC
    RET

.sdl_has_range:
    ; ---- Save stop condition ----
    MOV     [sd_s_line], BX
    MOV     [sd_s_col], CX

    ; ---- M3: Rough capacity check ----
    MOV     AX, [pt_count]
    ADD     AX, 100             ; conservative headroom
    CMP     AX, MAX_PT_PIECES
    JBE     .sdl_capacity_ok
    ; Near capacity — too risky for char-by-char deletion
    PUSH    SI
    PUSH    DI
    MOV     SI, s_err_pt_full
    CALL    show_error_msg
    POP     DI
    POP     SI
    CALL    sel_clear
    POP     BP
    POP     SI
    POP     DX
    POP     CX
    POP     BX
    POP     AX
    STC
    RET
.sdl_capacity_ok:

    ; ---- Position cursor at selection end ----
    MOV     [cur_line], DX
    MOV     [cur_col], SI

    ; ---- First-record flag ----
    MOV     BYTE [sd_first], 1

.sdl_loop:
    ; ---- Check stop condition ----
    MOV     AX, [cur_line]
    CMP     AX, [sd_s_line]
    JB      .sdl_done
    JA      .sdl_continue
    MOV     AX, [cur_col]
    CMP     AX, [sd_s_col]
    JBE     .sdl_done

.sdl_continue:
    ; ---- Save pre-position (before backward move) ----
    MOV     AX, [cur_line]
    MOV     [undo_pre_line], AX
    MOV     AX, [cur_col]
    MOV     [undo_pre_col], AX

    ; ---- Move cursor backward by one position ----
    CMP     WORD [cur_col], 0
    JE      .sdl_prev_line
    DEC     WORD [cur_col]
    JMP     .sdl_do_del

.sdl_prev_line:
    DEC     WORD [cur_line]
    CALL    line_length
    MOV     [cur_col], AX

.sdl_do_del:
    MOV     BYTE [undo_del_crlf], 0FFh
    CALL    delete_at

    CMP     BYTE [undo_del_crlf], 0FFh
    JE      .sdl_done

    ; ---- Build and push undo record ----
    PUSH    SI

    CMP     BYTE [undo_del_crlf], 0
    JNE     .sdl_rec_crlf
    MOV     AL, UR_DEL_CHAR
    MOV     AH, [undo_del_char]
    JMP     .sdl_rec_group
.sdl_rec_crlf:
    MOV     AL, UR_DEL_CRLF
    XOR     AH, AH

.sdl_rec_group:
    CMP     BYTE [sd_first], 1
    JE      .sdl_no_group
    OR      AL, URF_GROUP_CONT
.sdl_no_group:
    MOV     BYTE [sd_first], 0

    MOV     BX, [cur_line]
    MOV     CX, [cur_col]
    MOV     DX, [undo_pre_line]
    MOV     SI, [undo_pre_col]
    CALL    undo_push

    POP     SI
    JMP     .sdl_loop

.sdl_done:
    POP     BP
    POP     SI
    POP     DX
    POP     CX
    POP     BX

    CALL    sel_clear
    CALL    scroll_to_cursor
    MOV     BYTE [fw_state + FW_DIRTY], 1
    MOV     WORD [last_ins_pi], 0FFFFh
    MOV     BYTE [undo_group_active], 0
    MOV     BYTE [meta_dirty], 1

    POP     AX              ; restore AX last (after scroll_to_cursor clobbers it)
    CLC
    RET

.sdl_nothing:
    STC
    RET
sel_delete_legacy ENDP
