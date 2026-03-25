; ============================================================
; ed_draw.inc — Editor Rendering
;
; tui_ctrl_draw_editor: renders 23 rows of text content from
; the piece table, 1 row of status bar, and a software cursor.
; ============================================================

; ============================================================
; ed_hscroll_skip — advance rnd_pi/rnd_off past CX visible chars
;
; Input:  CX = characters to skip
;         rnd_pi/rnd_off = start of current line
; Output: rnd_pi/rnd_off = position CX visible chars into line,
;         or at LF/EOF if line is shorter than CX
; Clobbers: AX, BX, CX, DX, SI, DI
; DS must be CS on entry (preserved on return).
; ============================================================

; ============================================================
; ed_hscroll_skip — skip CX visual columns (tab-aware)
;
; Input:  CX = visual columns to skip (left_col)
;         rnd_pi/rnd_off = start of current line
; Output: rnd_pi/rnd_off = position after skip,
;         rnd_visual_col = visual columns consumed (may overshoot CX via tab),
;         rnd_byte_col = bytes consumed
; Clobbers: AX, BX, CX, DX, SI, DI
; DS must be CS on entry (preserved on return).
; ============================================================

ed_hscroll_skip PROC
    TEST    CX, CX
    JZ      .done

.walk_piece:
    MOV     BX, [rnd_pi]
    CMP     BX, [pt_count]
    JAE     .done
    PUSH    CX
    CALL    resolve_piece           ; AX=seg, SI=base, DX=piece_len
    POP     CX
    JC      .done

    PUSH    DS
    MOV     DS, AX
    MOV     DI, SI                  ; DI = piece base
    ADD     DX, SI                  ; DX = piece end (absolute)
    ADD     SI, CS:[rnd_off]        ; SI = current read position

.inner:
    CMP     SI, DX
    JAE     .next_piece
    MOV     AL, [SI]
    CMP     AL, 0Dh
    JE      .skip_cr
    CMP     AL, 0Ah
    JE      .at_eol
    CMP     AL, 09h
    JE      .handle_tab
    ; Normal visible char — 1 visual column
    INC     SI
    INC     WORD CS:[rnd_visual_col]
    INC     WORD CS:[rnd_byte_col]
    DEC     CX
    JZ      .found
    JMP     .inner

.handle_tab:
    ; tab_w = tab_width - (visual_col & tab_mask)
    INC     SI                              ; consume tab byte
    PUSH    BX
    MOV     BX, CS:[rnd_visual_col]
    AND     BX, CS:[tab_mask]
    MOV     AX, CS:[tab_width]
    SUB     AX, BX                          ; AX = tab_w
    ADD     CS:[rnd_visual_col], AX
    INC     WORD CS:[rnd_byte_col]
    POP     BX
    SUB     CX, AX                          ; CX -= tab_w
    JBE     .found                          ; CX <= 0: consumed enough (may overshoot)
    JMP     .inner

.skip_cr:
    INC     SI
    JMP     .inner

.at_eol:
    ; Line is shorter than left_col
    POP     DS
    MOV     AX, SI
    SUB     AX, DI
    MOV     [rnd_off], AX
    RET

.found:
    POP     DS
    CMP     SI, DX
    JB      .found_in_piece
    INC     WORD [rnd_pi]
    MOV     WORD [rnd_off], 0
    RET

.found_in_piece:
    MOV     AX, SI
    SUB     AX, DI
    MOV     [rnd_off], AX
    RET

.next_piece:
    POP     DS
    INC     WORD [rnd_pi]
    MOV     WORD [rnd_off], 0
    JMP     .walk_piece

.done:
    RET
ed_hscroll_skip ENDP

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

    ; ---- Selection pre-render setup ----
    MOV     BYTE [sel_rendering], 0
    CMP     BYTE [sel_active], 0
    JE      .no_sel_setup
    CALL    sel_normalize           ; BX=s_line, CX=s_col, DX=e_line, SI=e_col
    MOV     [sel_s_line], BX
    MOV     [sel_s_col], CX
    MOV     [sel_e_line], DX
    MOV     [sel_e_col], SI
    MOV     BYTE [sel_rendering], 1
.no_sel_setup:

.render_restart:
    ; ---- Set up ES:DI for shadow_buf writing ----
    MOV     AX, CS
    MOV     ES, AX
    MOV     BL, [ed_abs_row]
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

    ; ---- Horizontal scroll: skip left_col visual columns ----
    MOV     WORD [rnd_byte_col], 0
    MOV     WORD [rnd_visual_col], 0
    MOV     CX, [left_col]
    JCXZ    .no_hscroll
    PUSH    DI                      ; save shadow_buf write pointer
    CALL    ed_hscroll_skip
    POP     DI                      ; restore shadow_buf write pointer
.no_hscroll:

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

    ; ---- Handle tab overshoot from hscroll_skip ----
    ; If rnd_visual_col > left_col, a tab was partially consumed.
    ; Fill the overshoot columns with spaces.
    MOV     CX, CS:[rnd_visual_col]
    SUB     CX, CS:[left_col]
    JCXZ    .no_overshoot           ; no overshoot (or no hscroll)
    ; Determine attribute for the partially-visible tab
    MOV     AH, ED_ATTR_NORM
    CMP     BYTE CS:[sel_rendering], 0
    JE      .os_attr_done
    PUSH    DX
    MOV     DX, CS:[rnd_byte_col]
    DEC     DX                      ; byte col of the tab
    CMP     BP, CS:[sel_s_line]
    JB      .os_not_sel
    JA      .os_above
    CMP     DX, CS:[sel_s_col]
    JB      .os_not_sel
.os_above:
    CMP     BP, CS:[sel_e_line]
    JA      .os_not_sel
    JB      .os_sel
    CMP     DX, CS:[sel_e_col]
    JAE     .os_not_sel
.os_sel:
    MOV     AH, ED_ATTR_SEL
.os_not_sel:
    POP     DX
.os_attr_done:
    MOV     AL, ' '
    PUSH    CX                      ; save overshoot count
    REP     STOSW                   ; fill overshoot with spaces
    POP     CX                      ; CX = starting screen column
.no_overshoot:
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
    CMP     AL, 09h
    JE      .cl_tab
    ; ---- Normal character: per-character selection attribute ----
    MOV     AH, ED_ATTR_NORM
    CMP     BYTE CS:[sel_rendering], 0
    JE      .cl_write
    MOV     DX, CS:[rnd_byte_col]
.cl_check:
    CMP     BP, CS:[sel_s_line]
    JB      .cl_write
    JA      .cl_above_start
    CMP     DX, CS:[sel_s_col]
    JB      .cl_write
.cl_above_start:
    CMP     BP, CS:[sel_e_line]
    JA      .cl_write
    JB      .cl_sel
    CMP     DX, CS:[sel_e_col]
    JAE     .cl_write
.cl_sel:
    MOV     AH, ED_ATTR_SEL
.cl_write:
    MOV     ES:[DI], AX
    ADD     DI, 2
    INC     CX
    INC     WORD CS:[rnd_byte_col]
    INC     WORD CS:[rnd_visual_col]
    JMP     .char_loop

    ; ---- Tab expansion ----
.cl_tab:
    ; Determine attribute for this tab byte
    MOV     AH, ED_ATTR_NORM
    CMP     BYTE CS:[sel_rendering], 0
    JE      .cl_tab_attr_ok
    PUSH    DX
    MOV     DX, CS:[rnd_byte_col]
    CMP     BP, CS:[sel_s_line]
    JB      .cl_tab_not_sel
    JA      .cl_tab_above
    CMP     DX, CS:[sel_s_col]
    JB      .cl_tab_not_sel
.cl_tab_above:
    CMP     BP, CS:[sel_e_line]
    JA      .cl_tab_not_sel
    JB      .cl_tab_is_sel
    CMP     DX, CS:[sel_e_col]
    JAE     .cl_tab_not_sel
.cl_tab_is_sel:
    MOV     AH, ED_ATTR_SEL
.cl_tab_not_sel:
    POP     DX
.cl_tab_attr_ok:
    ; Compute tab spaces: tab_width - (visual_col & tab_mask)
    PUSH    BX
    PUSH    DX
    MOV     BX, CS:[rnd_visual_col]
    AND     BX, CS:[tab_mask]
    MOV     DX, CS:[tab_width]
    SUB     DX, BX                  ; DX = spaces to render
.cl_tab_loop:
    CMP     CX, ED_COLS
    JAE     .cl_tab_done
    MOV     AL, ' '
    MOV     ES:[DI], AX
    ADD     DI, 2
    INC     CX
    INC     WORD CS:[rnd_visual_col]
    DEC     DX
    JNZ     .cl_tab_loop
.cl_tab_done:
    POP     DX
    POP     BX
    INC     WORD CS:[rnd_byte_col]
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
.draw_cursor_now:
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
    ; ---- Line N of M ----
    MOV     SI, s_stat_ln
    CALL    ed_wstr

    ; Logical line numbers
    MOV     AX, [cur_line]
    INC     AX
    CALL    ed_wdec
    MOV     SI, s_stat_of
    CALL    ed_wstr
    MOV     AX, [total_lines]
    CALL    ed_wdec
    JMP     .stat_col

.stat_col:
    ; Column display
    MOV     SI, s_stat_col
    CALL    ed_wstr
    MOV     AX, [cur_col]
    INC     AX                      ; 1-based display
    CALL    ed_wdec

    ; Wrap indicator
.no_wrap:

    CMP     BYTE [dirty], 0
    JE      .no_mod
    MOV     SI, s_stat_mod
    CALL    ed_wstr
.no_mod:

    CMP     BYTE [sel_active], 0
    JE      .no_sel
    MOV     SI, s_stat_sel
    CALL    ed_wstr
.no_sel:

    CMP     BYTE [overwrite_mode], 0
    JE      .no_ovr
    MOV     SI, s_stat_ovr
    CALL    ed_wstr
.no_ovr:

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
    PUSH    DX
    PUSH    DI

    ; ============================================================
    ; Cursor position (visual column aware for tabs)
    ; ============================================================
    MOV     AX, [cur_line]
    SUB     AX, [top_line]
    CMP     AX, ED_TEXT_ROWS
    JAE     .no_cursor

    ADD     AL, [ed_abs_row]
    XOR     AH, AH

    MOV     BX, AX
    SHL     BX, 1
    MOV     DI, [BX + row_offsets]
    ADD     DI, shadow_buf

    MOV     AX, [cur_col]
    CALL    byte_to_visual_col     ; AX = visual column of cursor
    SUB     AX, [left_col]
    CMP     AX, ED_COLS
    JAE     .no_cursor
    SHL     AX, 1
    ADD     DI, AX
    JMP     .swap_attr

.swap_attr:
    ; Fixed cursor attribute: bright white bg, blue fg (requires blink disable)
    MOV     BYTE [DI + 1], 0F1h

.no_cursor:
    POP     DI
    POP     DX
    POP     CX
    POP     BX
    POP     AX
    RET
ed_draw_cursor ENDP
