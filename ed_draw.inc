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

ed_hscroll_skip PROC
    JCXZ    .done

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
    ; Visible character — count it
    INC     SI
    DEC     CX
    JZ      .found
    JMP     .inner

.skip_cr:
    INC     SI
    JMP     .inner

.at_eol:
    ; Line is shorter than left_col. Leave rnd_off pointing AT
    ; the LF so char_loop reads it and pads the row with spaces.
    POP     DS
    MOV     AX, SI
    SUB     AX, DI                  ; piece-relative offset of LF
    MOV     [rnd_off], AX
    RET

.found:
    ; Skipped enough. SI points to next char to be drawn.
    POP     DS
    CMP     SI, DX
    JB      .found_in_piece
    ; Exactly at piece end — advance to next piece
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

    ; ---- Init wrap-mode scroll adjustment ----
    MOV     WORD [cur_vis_row], 0FFFFh
    MOV     BYTE [wrap_retry], ED_TEXT_ROWS

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
    MOV     WORD [rnd_doc_col], 0

    ; ---- Row loop ----
.row_loop:
    MOV     AX, [rnd_row]
    CMP     AX, ED_TEXT_ROWS
    JAE     .do_status

    ; Store row-to-line mapping (for wrap-mode mouse support)
    MOV     BX, AX
    SHL     BX, 1
    MOV     [BX + _row_to_line], BP

    MOV     AX, BP
    CMP     AX, [total_lines]
    JAE     .blank_row

    ; ---- Capture cur_vis_row (first visual row of cur_line) ----
    CMP     WORD [cur_vis_row], 0FFFFh
    JNE     .vis_captured               ; already captured
    CMP     AX, [cur_line]              ; AX = BP = current logical line
    JNE     .vis_captured
    MOV     AX, [rnd_row]
    MOV     [cur_vis_row], AX
.vis_captured:

    ; ---- Horizontal scroll: skip left_col chars (non-wrap only) ----
    CMP     BYTE [wrap_mode], 0
    JNE     .no_hscroll
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
    ; ---- Per-character selection attribute ----
    MOV     AH, ED_ATTR_NORM
    CMP     BYTE CS:[sel_rendering], 0
    JE      .cl_write
    CMP     BYTE CS:[wrap_mode], 0
    JNE     .cl_wrap_col
    MOV     DX, CS:[left_col]
    ADD     DX, CX
    JMP     .cl_check
.cl_wrap_col:
    MOV     DX, CS:[rnd_doc_col]
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
    CMP     BYTE CS:[wrap_mode], 0
    JE      .char_loop
    INC     WORD CS:[rnd_doc_col]
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
    MOV     WORD [rnd_doc_col], 0
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
    ; ---- Wrap-mode scroll adjustment ----
    CMP     BYTE [wrap_mode], 0
    JE      .draw_cursor_now

    ; Check if cursor's line was rendered at all
    CMP     WORD [cur_vis_row], 0FFFFh
    JE      .wrap_not_visible

    ; Compute cursor's visual row: cur_vis_row + cur_col / ED_COLS
    MOV     AX, [cur_col]
    XOR     DX, DX
    MOV     CX, ED_COLS
    DIV     CX                          ; AX = extra_rows
    ADD     AX, [cur_vis_row]           ; AX = cursor visual row
    CMP     AX, ED_TEXT_ROWS
    JB      .draw_cursor_now            ; on screen — draw it

    ; Cursor off bottom: increment top_line and re-render
    JMP     .wrap_adjust_down

.wrap_not_visible:
    ; cur_line wasn't rendered — set top_line = cur_line to bring it on screen
    MOV     AX, [cur_line]
    MOV     [top_line], AX
    JMP     .wrap_re_render

.wrap_adjust_down:
    INC     WORD [top_line]
    ; Clamp: top_line must not exceed cur_line
    MOV     AX, [cur_line]
    CMP     [top_line], AX
    JBE     .wrap_re_render
    MOV     [top_line], AX              ; safety clamp

.wrap_re_render:
    MOV     WORD [cur_vis_row], 0FFFFh  ; reset sentinel
    DEC     BYTE [wrap_retry]
    JZ      .draw_cursor_now            ; give up — max retries reached
    JMP     .render_restart

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

    CMP     BYTE [wrap_mode], 0
    JNE     .stat_vis_line

    ; Non-wrap: logical line numbers
    MOV     AX, [cur_line]
    INC     AX
    CALL    ed_wdec
    MOV     SI, s_stat_of
    CALL    ed_wstr
    MOV     AX, [total_lines]
    CALL    ed_wdec
    JMP     .stat_col

.stat_vis_line:
    ; Wrap: visual line numbers
    ; N = cur_vis_line + cur_col / ED_COLS + 1
    MOV     AX, [cur_col]
    XOR     DX, DX
    MOV     CX, ED_COLS
    DIV     CX                      ; AX = cur_col / ED_COLS
    ADD     AX, [cur_vis_line]
    INC     AX                      ; 1-based
    CALL    ed_wdec
    MOV     SI, s_stat_of
    CALL    ed_wstr
    MOV     AX, [total_vis_lines]
    CALL    ed_wdec

.stat_col:
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

    CMP     BYTE [sel_active], 0
    JE      .no_sel
    MOV     SI, s_stat_sel
    CALL    ed_wstr
.no_sel:

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

    CMP     BYTE [wrap_mode], 0
    JNE     .wrap_cursor

    ; ============================================================
    ; Non-wrap cursor (unchanged from Phase A)
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
    SUB     AX, [left_col]
    CMP     AX, ED_COLS
    JAE     .no_cursor
    SHL     AX, 1
    ADD     DI, AX
    JMP     .swap_attr

    ; ============================================================
    ; Wrap-mode cursor
    ; ============================================================
.wrap_cursor:
    ; Check if cur_vis_row was captured during rendering
    CMP     WORD [cur_vis_row], 0FFFFh
    JE      .no_cursor

    ; Compute: extra_rows = cur_col / ED_COLS, screen_col = cur_col % ED_COLS
    MOV     AX, [cur_col]
    XOR     DX, DX
    MOV     CX, ED_COLS
    DIV     CX                  ; AX = extra_rows, DX = screen_col

    ; Visual row = cur_vis_row + extra_rows
    ADD     AX, [cur_vis_row]
    CMP     AX, ED_TEXT_ROWS
    JAE     .no_cursor

    ; Compute shadow_buf row offset
    ADD     AL, [ed_abs_row]
    XOR     AH, AH
    MOV     BX, AX
    SHL     BX, 1
    MOV     DI, [BX + row_offsets]
    ADD     DI, shadow_buf

    ; Add column offset (DX = screen_col from DIV remainder)
    MOV     AX, DX
    SHL     AX, 1
    ADD     DI, AX

.swap_attr:
    ; Swap fg/bg nibbles of attribute byte (block cursor)
    MOV     AL, [DI + 1]
    MOV     CL, 4
    ROL     AL, CL
    MOV     [DI + 1], AL

.no_cursor:
    POP     DI
    POP     DX
    POP     CX
    POP     BX
    POP     AX
    RET
ed_draw_cursor ENDP
