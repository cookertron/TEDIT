; ============================================================
; ed_draw.inc — Editor Rendering
;
; tui_ctrl_draw_editor: renders 23 rows of text content from
; the piece table, 1 row of status bar, and a software cursor.
; ============================================================

; ============================================================
; tui_ctrl_draw_editor
;
; Input:  BX = pointer to editor control struct
;         DH = absolute screen row, DL = absolute screen col
;         CH = 1 if focused, 0 if not
; Must preserve: BX, DI
; ============================================================

tui_ctrl_draw_editor:
    PUSH    BX
    PUSH    DI
    PUSH    DS
    PUSH    ES
    PUSH    BP
    PUSH    SI

    ; Save absolute row for cursor drawing
    MOV     [ed_abs_row], DH

    ; ---- Set up ES:DI for shadow_buf writing ----
    MOV     AX, CS
    MOV     ES, AX
    MOV     BL, DH
    XOR     BH, BH
    SHL     BX, 1
    MOV     DI, [BX + row_offsets]
    ADD     DI, shadow_buf

    ; ---- Seek to top_line ----
    MOV     BP, [top_line]
    MOV     AX, [top_line]
    CALL    seek_to_line
    MOV     WORD [rnd_row], 0

    ; ---- Row loop ----
.row_loop:
    MOV     AX, [rnd_row]
    CMP     AX, ED_TEXT_ROWS
    JAE     .do_status
    MOV     AX, BP
    CMP     AX, [total_lines]
    JAE     .blank_row
    MOV     BX, [rnd_pi]
    CMP     BX, [pt_count]
    JAE     .blank_row
    CALL    resolve_piece
    JC      .blank_row
    MOV     [rnd_base], SI
    MOV     [rnd_seg], AX
    ADD     DX, SI
    MOV     [rnd_len], DX
    MOV     DS, AX
    MOV     SI, CS:[rnd_base]
    ADD     SI, CS:[rnd_off]
    XOR     CX, CX
    MOV     AH, ED_ATTR_NORM

.char_loop:
    CMP     CX, ED_COLS
    JAE     .row_done
    CMP     SI, CS:[rnd_len]
    JAE     .adv_piece
    MOV     AL, [SI]
    INC     SI
    CMP     AL, 0Dh
    JE      .char_loop
    CMP     AL, 0Ah
    JE      .row_done_lf
    MOV     ES:[DI], AX
    ADD     DI, 2
    INC     CX
    JMP     .char_loop

.adv_piece:
    MOV     AX, CS:[rnd_pi]
    INC     AX
    MOV     CS:[rnd_pi], AX
    CMP     AX, CS:[pt_count]
    JAE     .row_done

    ; ---- Inlined resolve_piece ----
    PUSH    CS
    POP     DS
    MOV     BX, AX

    ; SI = BX * 10
    MOV     SI, BX
    SHL     SI, 1
    MOV     AX, SI
    SHL     SI, 1
    SHL     SI, 1
    ADD     SI, AX

    ; Read descriptor from pt_seg via ES (temporarily)
    PUSH    ES
    MOV     AX, [pt_seg]
    MOV     ES, AX

    MOV     DX, ES:[SI + PT_F_LENGTH]
    MOV     AX, ES:[SI + PT_F_START]

    CMP     BYTE ES:[SI + PT_F_SOURCE], 0
    JNE     .adv_add_src

    ; ---- Source 0: Original buffer ----
    MOV     BX, ES:[SI + PT_F_START + 2]
    POP     ES

    MOV     SI, AX
    AND     SI, 7FFFh
    SHL     BX, 1
    TEST    AX, 8000h
    JZ      .adv_no_hi
    INC     BX
.adv_no_hi:
    SHL     BX, 1
    MOV     AX, [BX + orig_seg]
    JMP     .adv_resolved

.adv_add_src:
    ; ---- Source 1: Add buffer ----
    POP     ES
    MOV     SI, AX
    MOV     AX, [add_seg]

.adv_resolved:
    MOV     [rnd_base], SI
    MOV     [rnd_seg], AX
    ADD     DX, SI
    MOV     [rnd_len], DX
    MOV     WORD [rnd_off], 0
    MOV     DS, AX
    MOV     SI, CS:[rnd_base]
    MOV     AH, ED_ATTR_NORM
    JMP     .char_loop

.row_done_lf:
    PUSH    AX
    MOV     AX, SI
    SUB     AX, CS:[rnd_base]
    MOV     CS:[rnd_off], AX
    POP     AX
    JMP     .row_pad

.row_done:
    ; Wrap mode: continue on next screen row without advancing logical line
    CMP     BYTE CS:[wrap_mode], 0
    JNE     .row_done_wrap
.skip_to_lf:
    CMP     SI, CS:[rnd_len]
    JAE     .skip_adv_piece
    MOV     AL, [SI]
    INC     SI
    CMP     AL, 0Ah
    JNE     .skip_to_lf
    ; Found LF — store position past it
    PUSH    AX
    MOV     AX, SI
    SUB     AX, CS:[rnd_base]
    MOV     CS:[rnd_off], AX
    POP     AX
    JMP     .row_pad

.skip_adv_piece:
    ; Advance to next piece while skipping to LF
    MOV     AX, CS:[rnd_pi]
    INC     AX
    MOV     CS:[rnd_pi], AX
    CMP     AX, CS:[pt_count]
    JAE     .row_done_store
    MOV     BX, AX
    PUSH    CS
    POP     DS
    PUSH    CX
    CALL    resolve_piece
    POP     CX
    JC      .row_done_store_com
    MOV     CS:[rnd_base], SI
    MOV     CS:[rnd_seg], AX
    ADD     DX, SI
    MOV     CS:[rnd_len], DX
    MOV     WORD CS:[rnd_off], 0
    MOV     DS, AX
    MOV     SI, CS:[rnd_base]
    JMP     .skip_to_lf

.row_done_store_com:
    PUSH    CS
    POP     DS
    JMP     .row_pad_com

.row_done_store:
    PUSH    AX
    MOV     AX, SI
    SUB     AX, CS:[rnd_base]
    MOV     CS:[rnd_off], AX
    POP     AX

.row_pad:
    PUSH    CS
    POP     DS
.row_pad_com:
    MOV     AX, ED_COLS
    SUB     AX, CX
    MOV     CX, AX
    JCXZ    .row_next
    MOV     AX, ED_ATTR_NORM * 256 + 20h
    REP     STOSW
    JMP     .row_next

.row_done_wrap:
    ; ---- Wrap mode: line continues on next screen row ----
    ; Store current piece position
    PUSH    AX
    MOV     AX, SI
    SUB     AX, CS:[rnd_base]
    MOV     CS:[rnd_off], AX
    POP     AX
    ; Pad remainder of screen row (CX == ED_COLS so usually nothing to pad)
    PUSH    CS
    POP     DS
    MOV     AX, ED_COLS
    SUB     AX, CX
    MOV     CX, AX
    JCXZ    .row_next_wrap
    MOV     AX, ED_ATTR_NORM * 256 + 20h
    REP     STOSW
.row_next_wrap:
    ; Advance screen row but NOT logical line (BP stays same)
    INC     WORD [rnd_row]
    JMP     .row_loop

.blank_row:
    MOV     CX, ED_COLS
    MOV     AX, ED_ATTR_NORM * 256 + 20h
    REP     STOSW

.row_next:
    INC     WORD [rnd_row]
    INC     BP
    JMP     .row_loop

; ---- STATUS BAR ----
.do_status:
    PUSH    CS
    POP     DS
    ; DI already points to start of status bar row in shadow_buf
    ; ES = CS (shadow_buf segment)
    PUSH    DI                  ; save status bar start DI
    CALL    ed_draw_status_line
    POP     AX                  ; AX = status bar start DI
    ; Pad remainder of row
    ADD     AX, ED_COLS * 2     ; AX = end of status bar row
    SUB     AX, DI              ; AX = remaining bytes
    JBE     .status_done
    SHR     AX, 1               ; AX = remaining words
    MOV     CX, AX
    MOV     AX, ED_ATTR_STAT * 256 + 20h
    REP     STOSW

.status_done:
    ; ---- Software cursor ----
    CALL    ed_draw_cursor

    POP     SI
    POP     BP
    POP     ES
    POP     DS
    POP     DI
    POP     BX
    RET

; ============================================================
; ed_draw_status_line — write status bar text to ES:DI
; Input: ES:DI = position in shadow_buf, DS = CS
; Output: DI advanced past written text
; ============================================================

ed_draw_status_line PROC
    ; ---- Filename or "Untitled" ----
    MOV     AL, ' '
    MOV     ES:[DI], AL
    MOV     BYTE ES:[DI+1], ED_ATTR_STAT
    ADD     DI, 2

    CMP     BYTE [filename], 0
    JE      .stat_untitled

    ; Find the basename: scan backward from end for '\'
    ; If no '\' found, use the whole filename string.
    MOV     SI, filename
    MOV     BX, SI                  ; BX = start of filename (default basename)
.stat_scan_fn:
    CMP     BYTE [SI], 0
    JZ      .stat_fn_found
    CMP     BYTE [SI], '\'
    JNE     .stat_scan_next
    ; Found backslash — basename starts after it
    MOV     BX, SI
    INC     BX
.stat_scan_next:
    INC     SI
    JMP     .stat_scan_fn

.stat_fn_found:
    MOV     SI, BX                  ; SI = basename start
    ; Write up to 20 characters of basename
    MOV     CL, 20                  ; max chars to display
.stat_fn_loop:
    CMP     BYTE [SI], 0
    JZ      .stat_fn_done
    OR      CL, CL
    JZ      .stat_fn_done
    MOV     AL, [SI]
    MOV     ES:[DI], AL
    MOV     BYTE ES:[DI+1], ED_ATTR_STAT
    ADD     DI, 2
    INC     SI
    DEC     CL
    JMP     .stat_fn_loop

.stat_fn_done:
    JMP     .stat_after_name

.stat_untitled:
    MOV     SI, s_stat_untitled
    CALL    ed_wstr

.stat_after_name:
    ; ---- Line N of M (existing) ----
    MOV     SI, s_stat_ln
    CALL    ed_wstr

    MOV     AX, [cur_line]
    INC     AX
    CALL    ed_wdec

    MOV     SI, s_stat_of
    CALL    ed_wstr

    MOV     AX, [total_lines]
    CALL    ed_wdec

    ; Column display
    MOV     SI, s_stat_col
    CALL    ed_wstr
    MOV     AX, [cur_col]
    INC     AX                      ; 1-based display
    CALL    ed_wdec

    ; Wrap indicator
    CMP     BYTE [wrap_mode], 0
    JE      .no_wrap
    MOV     SI, s_stat_wrap
    CALL    ed_wstr
.no_wrap:

    CMP     BYTE [dirty], 0
    JE      .no_mod
    MOV     SI, s_stat_mod
    CALL    ed_wstr
.no_mod:

    RET
ed_draw_status_line ENDP

; ============================================================
; ed_wstr — write null-terminated string to ES:DI with ED_ATTR_STAT
; Input: DS:SI = string, ES:DI = shadow_buf position
; Output: DI advanced, SI advanced past null
; ============================================================

ed_wstr PROC
.lp:
    MOV     AL, [SI]
    CMP     AL, 0
    JE      .done
    MOV     ES:[DI], AL
    MOV     BYTE ES:[DI+1], ED_ATTR_STAT
    ADD     DI, 2
    INC     SI
    JMP     .lp
.done:
    RET
ed_wstr ENDP

; ============================================================
; ed_wdec — write AX as decimal digits to ES:DI with ED_ATTR_STAT
; Input: AX = value, ES:DI = shadow_buf position
; Output: DI advanced past written digits
; ============================================================

ed_wdec PROC
    PUSH    AX
    PUSH    BX
    PUSH    CX
    PUSH    DX
    MOV     BX, 10
    XOR     CX, CX
    TEST    AX, AX
    JNZ     .split
    MOV     BYTE ES:[DI], '0'
    MOV     BYTE ES:[DI+1], ED_ATTR_STAT
    ADD     DI, 2
    JMP     .done
.split:
    TEST    AX, AX
    JZ      .emit
    XOR     DX, DX
    DIV     BX
    PUSH    DX
    INC     CX
    JMP     .split
.emit:
    POP     DX
    MOV     AL, DL
    ADD     AL, '0'
    MOV     ES:[DI], AL
    MOV     BYTE ES:[DI+1], ED_ATTR_STAT
    ADD     DI, 2
    LOOP    .emit
.done:
    POP     DX
    POP     CX
    POP     BX
    POP     AX
    RET
ed_wdec ENDP

; ============================================================
; ed_draw_cursor — software block cursor (swap attribute nibbles)
; ============================================================

ed_draw_cursor PROC
    PUSH    AX
    PUSH    BX
    PUSH    CX
    PUSH    DI

    ; Compute screen row of cursor
    MOV     AX, [cur_line]
    SUB     AX, [top_line]
    ; Check bounds: must be 0 <= AX < ED_TEXT_ROWS
    CMP     AX, ED_TEXT_ROWS
    JAE     .no_cursor

    ; AX = row within text area; add abs_row_start to get screen row
    ADD     AL, [ed_abs_row]
    XOR     AH, AH

    ; Compute shadow_buf offset for this row
    MOV     BX, AX
    SHL     BX, 1
    MOV     DI, [BX + row_offsets]
    ADD     DI, shadow_buf

    ; Add column offset: cur_col * 2
    MOV     AX, [cur_col]
    CMP     AX, ED_COLS
    JAE     .no_cursor
    SHL     AX, 1
    ADD     DI, AX

    ; Swap nibbles of attribute byte at DI+1
    MOV     AL, [DI + 1]           ; read current attribute (DS=CS)
    MOV     CL, 4
    ROL     AL, CL                 ; swap high and low nibbles
    MOV     [DI + 1], AL           ; write back

.no_cursor:
    POP     DI
    POP     CX
    POP     BX
    POP     AX
    RET
ed_draw_cursor ENDP
