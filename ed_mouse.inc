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

    ; ---- Compute target line + column ----
    CMP     BYTE [wrap_mode], 0
    JNE     .press_wrap

    ; ---- Non-wrap: target_line = top_line + ed_row ----
    XOR     DH, DH
    MOV     DL, AL                  ; DX = ed_row (zero-extended)
    MOV     CX, [top_line]
    ADD     CX, DX                  ; CX = target_line = top_line + ed_row
    CMP     CX, [total_lines]
    JB      .press_nw_lok
    MOV     CX, [total_lines]
    DEC     CX
.press_nw_lok:
    MOV     [cur_line], CX
    MOV     AL, AH                  ; AL = ed_col (screen column)
    XOR     AH, AH
    ADD     AX, [left_col]          ; non-wrap: doc col = left_col + screen col
    JMP     .press_col_set

.press_wrap:
    ; ---- Wrap: target_line = _row_to_line[ed_row] ----
    PUSH    AX                      ; save AL=ed_row, AH=ed_col
    XOR     AH, AH                  ; AX = ed_row
    MOV     SI, AX                  ; SI = ed_row (for backward scan)
    SHL     AX, 1
    MOV     BX, AX
    MOV     CX, [BX + _row_to_line] ; CX = logical line from table
    CMP     CX, [total_lines]
    JB      .press_w_lok
    MOV     CX, [total_lines]
    DEC     CX
.press_w_lok:
    MOV     [cur_line], CX
    ; Scan backward to find wrap_offset (how many rows before
    ; ed_row belong to this same logical line)
    XOR     DX, DX                  ; DX = wrap_offset
.press_w_scan:
    TEST    SI, SI
    JZ      .press_w_sdone
    DEC     SI
    MOV     BX, SI
    SHL     BX, 1
    CMP     [BX + _row_to_line], CX
    JNE     .press_w_sdone
    INC     DX
    JMP     .press_w_scan
.press_w_sdone:
    ; Column = wrap_offset * ED_COLS + ed_col
    MOV     AX, ED_COLS
    MUL     DX                      ; AX = wrap_offset * 80
    POP     DX                      ; DL=ed_row, DH=ed_col
    MOV     DL, DH                  ; DL = ed_col
    XOR     DH, DH                  ; DX = ed_col (16-bit)
    ADD     AX, DX                  ; AX = doc column

.press_col_set:
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

    ; ---- Compute target line + column ----
    CMP     BYTE [wrap_mode], 0
    JNE     .drag_wrap

    ; ---- Non-wrap: target_line = top_line + ed_row ----
    XOR     DH, DH
    MOV     DL, AL
    MOV     CX, [top_line]
    ADD     CX, DX
    CMP     CX, [total_lines]
    JB      .drag_nw_lok
    MOV     CX, [total_lines]
    DEC     CX
.drag_nw_lok:
    MOV     [cur_line], CX
    MOV     AL, AH
    XOR     AH, AH
    ADD     AX, [left_col]
    JMP     .drag_col_set

.drag_wrap:
    ; ---- Wrap: target_line = _row_to_line[ed_row] ----
    PUSH    AX                      ; save AL=ed_row, AH=ed_col
    XOR     AH, AH                  ; AX = ed_row
    MOV     SI, AX                  ; SI = ed_row (for backward scan)
    SHL     AX, 1
    MOV     BX, AX
    MOV     CX, [BX + _row_to_line]
    CMP     CX, [total_lines]
    JB      .drag_w_lok
    MOV     CX, [total_lines]
    DEC     CX
.drag_w_lok:
    MOV     [cur_line], CX
    XOR     DX, DX                  ; DX = wrap_offset
.drag_w_scan:
    TEST    SI, SI
    JZ      .drag_w_sdone
    DEC     SI
    MOV     BX, SI
    SHL     BX, 1
    CMP     [BX + _row_to_line], CX
    JNE     .drag_w_sdone
    INC     DX
    JMP     .drag_w_scan
.drag_w_sdone:
    MOV     AX, ED_COLS
    MUL     DX                      ; AX = wrap_offset * 80
    POP     DX                      ; DL=ed_row, DH=ed_col
    MOV     DL, DH
    XOR     DH, DH
    ADD     AX, DX                  ; AX = doc column

.drag_col_set:
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
