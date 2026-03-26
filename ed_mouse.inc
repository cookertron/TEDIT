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

    ; ---- Scrollbar click? ----
    CMP     BYTE [ed_sb_active], 0
    JE      .press_not_sb
    PUSH    CX
    XOR     CH, CH
    MOV     CL, AH                  ; CL = ed_col
    CMP     CX, [ed_text_cols]      ; click on scrollbar column?
    POP     CX
    JB      .press_not_sb
    JMP     .press_scrollbar
.press_not_sb:

    ; ---- Compute target line + column ----
    ; ---- target_line = top_line + ed_row ----
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
    ADD     AX, [left_col]          ; doc col = left_col + screen col
    MOV     [cur_col], AX

    CALL    line_length             ; AX = visible chars on cur_line
    CMP     [cur_col], AX
    JBE     .press_col_ok
    MOV     [cur_col], AX
.press_col_ok:

    ; ---- Reset insert tracking ----
    MOV     WORD [last_ins_pi], 0FFFFh

    ; ---- Clear any existing selection ----
    CALL    sel_clear

    ; ---- Save click position as potential anchor ----
    MOV     AX, [cur_line]
    MOV     [sel_anchor_line], AX
    MOV     AX, [cur_col]
    MOV     [sel_anchor_col], AX
    ; NOTE: sel_active stays 0 — only drag activates selection

    ; ---- Start control drag (for continuous cursor tracking) ----
    MOV     [mouse_state + MS_PRESSED], BX
    OR      BYTE [mouse_state + MS_FLAGS], MSF_CTRL_DRAG

    ; ---- Request redraw ----
    MOV     BYTE [fw_state + FW_DIRTY], 1

    JMP     .press_done

    ; ============================================================
    ; Scrollbar click handler
    ; AL = ed_row (0..ED_TEXT_ROWS-1), BX = ctrl ptr, SI = win ptr
    ; ============================================================
.press_scrollbar:
    ; Row 0 = up arrow → scroll up 1 line
    TEST    AL, AL
    JNZ     .sb_not_up
    CMP     WORD [top_line], 0
    JE      .sb_arrow_done
    DEC     WORD [top_line]
    DEC     WORD [cur_line]
.sb_arrow_done:
    ; Start arrow repeat
    MOV     [mouse_state + MS_SB_CTRL], BX
    MOV     BYTE [mouse_state + MS_SB_DIR], 0   ; up
    OR      BYTE [mouse_state + MS_FLAGS], MSF_SB_ARROW
    JMP     .sb_scroll_done

.sb_not_up:
    ; Last row = down arrow → scroll down 1 line
    CMP     AL, ED_TEXT_ROWS - 1
    JNE     .sb_track_click
    MOV     CX, [total_lines]
    SUB     CX, ED_TEXT_ROWS        ; CX = max top_line
    CMP     [top_line], CX
    JAE     .sb_dn_arrow_done
    INC     WORD [top_line]
    INC     WORD [cur_line]
.sb_dn_arrow_done:
    ; Start arrow repeat
    MOV     [mouse_state + MS_SB_CTRL], BX
    MOV     BYTE [mouse_state + MS_SB_DIR], 1   ; down
    OR      BYTE [mouse_state + MS_FLAGS], MSF_SB_ARROW
    JMP     .sb_scroll_done

.sb_track_click:
    ; Track region — compute thumb position to decide page up/down or drag
    ; track_row = ed_row - 1 (0-based within track)
    DEC     AL                      ; AL = track_row
    ; Compare with thumb region (pre-computed in sb_scratch by draw)
    CMP     AL, [sb_scratch]        ; vs thumb_pos
    JB      .sb_page_up
    MOV     AH, [sb_scratch]
    ADD     AH, [sb_scratch + 1]    ; AH = thumb_pos + thumb_size
    CMP     AL, AH
    JAE     .sb_page_down

    ; ---- On thumb → start drag ----
    SUB     AL, [sb_scratch]        ; AL = offset within thumb
    MOV     [mouse_state + MS_SB_CTRL], BX
    MOV     BYTE [mouse_state + MS_SB_DIR], 0       ; vertical
    MOV     [mouse_state + MS_SB_OFFSET], AL
    OR      BYTE [mouse_state + MS_FLAGS], MSF_SB_DRAG
    JMP     .sb_scroll_done

.sb_page_up:
    ; Track-hold: page up, repeat until thumb reaches mouse
    MOV     CX, [top_line]
    TEST    CX, CX
    JZ      .sb_scroll_done
    SUB     CX, ED_TEXT_ROWS
    JNC     .sb_pu_set
    XOR     CX, CX
.sb_pu_set:
    MOV     [top_line], CX
    ; Start track-up repeat (dir 4)
    MOV     [mouse_state + MS_SB_CTRL], BX
    MOV     BYTE [mouse_state + MS_SB_DIR], 4
    OR      BYTE [mouse_state + MS_FLAGS], MSF_SB_ARROW
    JMP     .sb_scroll_done

.sb_page_down:
    ; Track-hold: page down, repeat until thumb reaches mouse
    MOV     CX, [total_lines]
    SUB     CX, ED_TEXT_ROWS        ; CX = max top_line
    CMP     [top_line], CX
    JAE     .sb_scroll_done
    MOV     AX, [top_line]
    ADD     AX, ED_TEXT_ROWS
    CMP     AX, CX
    JBE     .sb_pd_set
    MOV     AX, CX
.sb_pd_set:
    MOV     [top_line], AX
    ; Start track-down repeat (dir 5)
    MOV     [mouse_state + MS_SB_CTRL], BX
    MOV     BYTE [mouse_state + MS_SB_DIR], 5
    OR      BYTE [mouse_state + MS_FLAGS], MSF_SB_ARROW
    JMP     .sb_scroll_done

.sb_scroll_done:
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

    ; ---- Compute raw ed_row ----
    MOV     AL, [mouse_state + MS_ROW]
    SUB     AL, DH

    ; ---- Check: above editor? (negative -> unsigned >= 80h) ----
    CMP     AL, 80h
    JB      .drag_not_above
    JMP     .drag_scroll_up

.drag_not_above:
    ; ---- Check: below editor? ----
    CMP     AL, ED_TEXT_ROWS
    JB      .drag_row_in_range
    JMP     .drag_scroll_down

.drag_row_in_range:
    ; ---- In range: compute ed_col normally ----
    MOV     AH, [mouse_state + MS_COL]
    SUB     AH, DL
    CMP     AH, 80h
    JB      .drag_col_not_neg
    XOR     AH, AH
.drag_col_not_neg:

    ; ---- Compute target line + column ----
    ; ---- target_line = top_line + ed_row ----
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

.drag_col_set:
    MOV     [cur_col], AX
    CALL    line_length
    CMP     [cur_col], AX
    JBE     .drag_col_ok
    MOV     [cur_col], AX
.drag_col_ok:
    MOV     WORD [last_ins_pi], 0FFFFh
    CALL    scroll_to_cursor

    ; ---- Activate selection if cursor differs from anchor ----
    MOV     AX, [cur_line]
    CMP     AX, [sel_anchor_line]
    JNE     .drag_has_sel
    MOV     AX, [cur_col]
    CMP     AX, [sel_anchor_col]
    JNE     .drag_has_sel
    ; Same position as anchor — no selection
    MOV     BYTE [sel_active], 0
    JMP     .drag_sel_done
.drag_has_sel:
    MOV     BYTE [sel_active], 1
.drag_sel_done:
    MOV     BYTE [fw_state + FW_DIRTY], 1
    JMP     .drag_done

; ---- Mouse above editor: scroll up ----
.drag_scroll_up:
    MOV     CX, [top_line]
    TEST    CX, CX
    JZ      .dsu_set
    DEC     CX
.dsu_set:
    MOV     [cur_line], CX
    ; Compute ed_col from mouse position
    MOV     AL, [mouse_state + MS_COL]
    SUB     AL, DL
    CMP     AL, 80h
    JB      .dsu_col_ok
    XOR     AL, AL
.dsu_col_ok:
    XOR     AH, AH                  ; AX = ed_col
    ADD     AX, [left_col]          ; add horizontal scroll
    JMP     .drag_col_set

; ---- Mouse below editor: scroll down ----
.drag_scroll_down:
    ; cur_line = top_line + ED_TEXT_ROWS
    MOV     CX, [top_line]
    ADD     CX, ED_TEXT_ROWS
    CMP     CX, [total_lines]
    JB      .dsd_set
    MOV     CX, [total_lines]
    DEC     CX
.dsd_set:
    MOV     [cur_line], CX
    ; Compute ed_col from mouse position
    MOV     AL, [mouse_state + MS_COL]
    SUB     AL, DL
    CMP     AL, 80h
    JB      .dsd_col_ok
    XOR     AL, AL
.dsd_col_ok:
    XOR     AH, AH                  ; AX = ed_col
    ADD     AX, [left_col]          ; add horizontal scroll
    JMP     .drag_col_set

.drag_done:
    POP     SI
    POP     DX
    POP     CX
    POP     BX
    POP     AX
    RET
