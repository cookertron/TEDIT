; ============================================================
; ed_mouse.inc — Editor Mouse Handlers
; ============================================================

; ============================================================
; tui_ed_mouse_press — Handle mouse click in editor area
;
; Entry: BX = editor control pointer (ed_ctrl)
;        SI = window struct pointer
;        mouse_state + MS_COL / MS_ROW = click cell
; ============================================================

tui_ed_mouse_press:
    PUSH    AX
    PUSH    BX
    PUSH    CX
    PUSH    DX
    PUSH    SI

    ; ---- Compute editor control absolute origin ----
    MOV     AL, [SI + WIN_X]
    TEST    BYTE [SI + WIN_FLAGS], WINF_BORDER
    JZ      .press_no_bdr_x
    INC     AL
.press_no_bdr_x:
    ADD     AL, [BX + CTRL_X]
    MOV     DL, AL                  ; DL = abs_x

    MOV     AL, [SI + WIN_Y]
    TEST    BYTE [SI + WIN_FLAGS], WINF_BORDER
    JZ      .press_no_bdr_y
    INC     AL
.press_no_bdr_y:
    ADD     AL, [BX + CTRL_Y]
    MOV     DH, AL                  ; DH = abs_y

    ; ---- Compute editor-relative row/col ----
    MOV     AL, [mouse_state + MS_ROW]
    SUB     AL, DH                  ; AL = ed_row
    MOV     AH, [mouse_state + MS_COL]
    SUB     AH, DL                  ; AH = ed_col

    ; ---- Bounds check: click on status bar? ----
    CMP     AL, ED_TEXT_ROWS
    JAE     .press_done             ; status bar row or below -> ignore

    ; ---- Compute target line ----
    XOR     DH, DH
    MOV     DL, AL                  ; DX = ed_row (zero-extended)
    MOV     CX, [top_line]
    ADD     CX, DX                  ; CX = target_line = top_line + ed_row

    ; Clamp to total_lines - 1
    CMP     CX, [total_lines]
    JB      .press_line_ok
    MOV     CX, [total_lines]
    DEC     CX
.press_line_ok:
    MOV     [cur_line], CX

    ; ---- Set cur_col, then clamp to line length ----
    MOV     AL, AH                  ; AL = ed_col
    XOR     AH, AH
    MOV     [cur_col], AX

    CALL    line_length             ; AX = visible chars on cur_line
    CMP     [cur_col], AX
    JBE     .press_col_ok
    MOV     [cur_col], AX
.press_col_ok:

    ; ---- Reset insert tracking ----
    MOV     WORD [last_ins_pi], 0FFFFh

    ; ---- Start control drag (for continuous cursor tracking) ----
    MOV     [mouse_state + MS_PRESSED], BX
    OR      BYTE [mouse_state + MS_FLAGS], MSF_CTRL_DRAG

    ; ---- Request redraw ----
    MOV     BYTE [fw_state + FW_DIRTY], 1

.press_done:
    POP     SI
    POP     DX
    POP     CX
    POP     BX
    POP     AX
    RET

; ============================================================
; tui_ed_mouse_drag — Handle mouse drag in editor area
;
; Entry: BX = control from MS_PRESSED (editor control pointer)
; ============================================================

tui_ed_mouse_drag:
    PUSH    AX
    PUSH    BX
    PUSH    CX
    PUSH    DX
    PUSH    SI

    ; Get top window struct
    MOV     AL, [fw_state + FW_TOPWIN]
    CMP     AL, 0FFh
    JZ      .drag_done
    XOR     AH, AH
    MOV     BL, WIN_SIZE
    MUL     BL
    MOV     SI, AX
    ADD     SI, win_table

    ; BX = editor control (from MS_PRESSED)
    MOV     BX, [mouse_state + MS_PRESSED]
    OR      BX, BX
    JZ      .drag_done

    ; ---- Compute absolute origin ----
    MOV     AL, [SI + WIN_Y]
    TEST    BYTE [SI + WIN_FLAGS], WINF_BORDER
    JZ      .drag_no_bdr_y
    INC     AL
.drag_no_bdr_y:
    ADD     AL, [BX + CTRL_Y]
    MOV     DH, AL                  ; DH = abs_y

    MOV     AL, [SI + WIN_X]
    TEST    BYTE [SI + WIN_FLAGS], WINF_BORDER
    JZ      .drag_no_bdr_x
    INC     AL
.drag_no_bdr_x:
    ADD     AL, [BX + CTRL_X]
    MOV     DL, AL                  ; DL = abs_x

    ; ed_row, ed_col
    MOV     AL, [mouse_state + MS_ROW]
    SUB     AL, DH
    ; Clamp ed_row: if negative (above editor), treat as 0
    CMP     AL, 80h
    JB      .drag_row_not_neg
    XOR     AL, AL
.drag_row_not_neg:
    CMP     AL, ED_TEXT_ROWS
    JB      .drag_row_in_range
    MOV     AL, ED_TEXT_ROWS
    DEC     AL                      ; max row = 22
.drag_row_in_range:
    MOV     AH, [mouse_state + MS_COL]
    SUB     AH, DL
    CMP     AH, 80h
    JB      .drag_col_not_neg
    XOR     AH, AH
.drag_col_not_neg:

    ; target_line = top_line + ed_row
    XOR     DH, DH
    MOV     DL, AL
    MOV     CX, [top_line]
    ADD     CX, DX
    CMP     CX, [total_lines]
    JB      .drag_line_ok
    MOV     CX, [total_lines]
    DEC     CX
.drag_line_ok:
    MOV     [cur_line], CX

    MOV     AL, AH
    XOR     AH, AH
    MOV     [cur_col], AX
    CALL    line_length
    CMP     [cur_col], AX
    JBE     .drag_col_ok
    MOV     [cur_col], AX
.drag_col_ok:
    MOV     WORD [last_ins_pi], 0FFFFh
    CALL    scroll_to_cursor
    MOV     BYTE [fw_state + FW_DIRTY], 1

.drag_done:
    POP     SI
    POP     DX
    POP     CX
    POP     BX
    POP     AX
    RET
