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

    ; ---- Scrollbar visibility ----
    MOV     WORD [ed_text_cols], ED_COLS
    MOV     BYTE [ed_sb_active], 0
    MOV     AX, [total_lines]
    CMP     AX, ED_TEXT_ROWS
    JBE     .sb_setup_done
    MOV     WORD [ed_text_cols], ED_COLS - 1
    MOV     BYTE [ed_sb_active], 1
    ; ---- Pre-compute scrollbar thumb (16-bit safe) ----
    ; track_len = ED_TEXT_ROWS - 2 = 21
    ; thumb_size = max(1, min(track_len, (ED_TEXT_ROWS * track_len) / total))
    PUSH    DX
    MOV     AX, ED_TEXT_ROWS * (ED_TEXT_ROWS - 2)   ; visible * track_len
    XOR     DX, DX
    DIV     WORD [total_lines]          ; AX = thumb_size (16-bit div)
    OR      AX, AX
    JNZ     .sb_ts_ok
    MOV     AX, 1
.sb_ts_ok:
    CMP     AX, ED_TEXT_ROWS - 2
    JBE     .sb_ts_clamp
    MOV     AX, ED_TEXT_ROWS - 2
.sb_ts_clamp:
    MOV     [sb_scratch + 1], AL        ; [1] = thumb_size
    ; thumb_pos = (top_line * (track_len - thumb_size)) / max_scroll
    ; max_scroll = total_lines - ED_TEXT_ROWS
    MOV     CX, [total_lines]
    SUB     CX, ED_TEXT_ROWS            ; CX = max_scroll
    JCXZ    .sb_pos_zero
    MOV     AH, ED_TEXT_ROWS - 2
    SUB     AH, AL                      ; AH = range = track_len - thumb_size
    MOV     AL, AH
    XOR     AH, AH                      ; AX = range
    PUSH    BX
    MOV     BX, AX                      ; BX = range
    MOV     AX, [top_line]
    MUL     BX                          ; DX:AX = top_line * range
    DIV     CX                          ; AX = thumb_pos
    POP     BX
    CMP     AL, ED_TEXT_ROWS - 2
    JB      .sb_tp_ok
    MOV     AL, ED_TEXT_ROWS - 3        ; clamp to last possible pos
.sb_tp_ok:
    MOV     [sb_scratch], AL            ; [0] = thumb_pos
    POP     DX
    JMP     .sb_setup_done
.sb_pos_zero:
    MOV     BYTE [sb_scratch], 0
    POP     DX
.sb_setup_done:

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
    ; ---- Set up ES:DI for shadow_seg writing ----
    MOV     AX, [shadow_seg]
    MOV     ES, AX
    MOV     BL, [ed_abs_row]
    XOR     BH, BH
    SHL     BX, 1
    MOV     DI, [BX + row_offsets]

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
    CMP     CX, CS:[left_col]
    JA      .has_overshoot          ; rnd_visual_col > left_col → tab overshoot
    XOR     CX, CX                  ; no overshoot: zero column counter
    JMP     .no_overshoot
.has_overshoot:
    SUB     CX, CS:[left_col]
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
    CMP     CX, CS:[ed_text_cols]
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
    CMP     CX, CS:[ed_text_cols]
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
    PUSH    DI
    CALL    mul_bx_10
    MOV     SI, DI
    POP     DI

    ; Read descriptor from pt_seg via ES (temporarily)
    PUSH    ES
    MOV     AX, [pt_seg]
    MOV     ES, AX

    MOV     DX, ES:[SI + PT_F_LENGTH]
    MOV     AX, ES:[SI + PT_F_START]

    CMP     BYTE ES:[SI + PT_F_SOURCE], 0
    JNE     .adv_add_src

    ; ---- Source 0: Original buffer (file-backed via cache) ----
    ; Read start_hi while ES still points at pt_seg
    MOV     BX, ES:[SI + PT_F_START + 2]   ; BX = start_hi (temp)
    POP     ES                              ; restore original ES
    ; Save screen column (CX) — cache_lookup clobbers CX
    PUSH    CX
    MOV     CX, BX                          ; CX = start_hi for cache_lookup
    ; AX = start_lo (already loaded at line 400)
    CALL    cache_lookup                     ; AX = segment, SI = chunk offset
    POP     CX                              ; restore screen column
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
    MOV     AX, [ed_text_cols]
    SUB     AX, CX
    MOV     CX, AX
    JCXZ    .row_next
    MOV     AX, ED_ATTR_NORM * 256 + 20h
    REP     STOSW
    JMP     .row_next

.blank_row:
    MOV     CX, [ed_text_cols]
    MOV     AX, ED_ATTR_NORM * 256 + 20h
    REP     STOSW

.row_next:
    ; ---- Write scrollbar column if active ----
    CMP     BYTE [ed_sb_active], 0
    JE      .rn_no_sb
    PUSH    AX
    MOV     AX, [rnd_row]
    ; Row 0 = up arrow
    TEST    AX, AX
    JNZ     .rn_not_up
    MOV     AX, CLR_SB_ARROW * 256 + SB_ARROW_UP
    JMP     .rn_sb_write
.rn_not_up:
    ; Last text row = down arrow
    CMP     AX, ED_TEXT_ROWS - 1
    JNZ     .rn_track
    MOV     AX, CLR_SB_ARROW * 256 + SB_ARROW_DN
    JMP     .rn_sb_write
.rn_track:
    ; Track region: track_row = rnd_row - 1
    DEC     AX                          ; AX = track_row (0-based)
    ; Check if within thumb: thumb_pos <= track_row < thumb_pos + thumb_size
    CMP     AL, [sb_scratch]            ; vs thumb_pos
    JB      .rn_track_bg
    MOV     AH, [sb_scratch]
    ADD     AH, [sb_scratch + 1]        ; AH = thumb_pos + thumb_size
    CMP     AL, AH
    JAE     .rn_track_bg
    MOV     AX, CLR_SB_THUMB * 256 + SB_THUMB
    JMP     .rn_sb_write
.rn_track_bg:
    MOV     AX, CLR_SB_TRACK * 256 + SB_TRACK
.rn_sb_write:
    STOSW
    POP     AX
    JMP     .rn_advance
.rn_no_sb:
.rn_advance:
    INC     WORD [rnd_row]
    INC     BP
    JMP     .row_loop

; ---- STATUS BAR ----
.do_status:
    PUSH    CS
    POP     DS
    ; DI already points to start of status bar row in shadow_seg
    ; ES = shadow_seg
    PUSH    DI                  ; save status bar start DI
    CALL    ed_draw_status_line
    POP     AX                  ; AX = status bar start DI
    ; Pad remainder of row
    ADD     AX, ED_COLS * 2     ; AX = end of status bar row
    MOV     BX, AX              ; BX = row end offset (save for clock)
    SUB     AX, DI              ; AX = remaining bytes
    JBE     .pad_done
    SHR     AX, 1               ; AX = remaining words
    MOV     CX, AX
    MOV     AX, ED_ATTR_STAT * 256 + 20h
    REP     STOSW
.pad_done:
    MOV     DI, BX              ; DI = row end (for clock positioning)

    ; ---- Clock display (right-aligned) ----
    CMP     BYTE [clock_visible], 0
    JE      .status_done
    CALL    ed_draw_clock

.status_done:
.draw_cursor_now:
    ; ---- Software cursor ----
    CALL    ed_draw_cursor
    ; ---- Side panel overlay ----
    CALL    ed_draw_panel

    POP     SI
    POP     BP
    POP     ES
    POP     DS
    POP     DI
    POP     BX
    RET

; ============================================================
; ed_draw_status_line — write status bar text to ES:DI
; Input: ES:DI = position in shadow_seg, DS = CS
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

    ; ---- Line ending indicator ----
    MOV     SI, s_stat_crlf
    CMP     BYTE [line_ending], LE_LF
    JNE     .show_le
    MOV     SI, s_stat_lf
.show_le:
    CALL    ed_wstr

    ; ---- Document counter [N/M] (only if multiple docs open) ----
    CMP     BYTE [doc_count], 1
    JBE     .skip_doc_counter
    MOV     AL, ' '
    MOV     AH, ED_ATTR_STAT
    STOSW
    MOV     AL, '['
    STOSW
    MOV     AL, [active_doc]
    INC     AL                      ; 1-20
    CALL    .ed_write_num
    MOV     AL, '/'
    STOSW
    MOV     AL, [doc_count]         ; 1-20
    CALL    .ed_write_num
    MOV     AL, ']'
    STOSW
.skip_doc_counter:

    RET

; Write number AL (1-20) as 1 or 2 ASCII digits to ES:DI
; Input:  AL = number, AH = attribute (preserved)
; Output: DI advanced
.ed_write_num:
    CMP     AL, 20
    JE      .ewn_20
    CMP     AL, 10
    JB      .ewn_single
    ; 10-19: write '1' then ones digit
    PUSH    AX
    MOV     AL, '1'
    STOSW
    POP     AX
    SUB     AL, 10
.ewn_single:
    ADD     AL, '0'
    STOSW
    RET
.ewn_20:
    MOV     AL, '2'
    STOSW
    MOV     AL, '0'
    STOSW
    RET

ed_draw_status_line ENDP

; ============================================================
; ed_wstr — write null-terminated string to ES:DI with ED_ATTR_STAT
; Input: DS:SI = string, ES:DI = shadow_seg position
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
; Input: AX = value, ES:DI = shadow_seg position
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
; ed_draw_clock — write HH:MM at right end of status bar
;
; Overwrites columns 74-79 with " HH:MM" using ED_ATTR_STAT.
; DI = end of status bar row (row_start + 160).
; Uses INT 21h AH=2Ch to read current time.
; ============================================================

ed_draw_clock PROC
    PUSH    AX
    PUSH    BX
    PUSH    CX
    PUSH    DX

    ; ---- Get current time ----
    MOV     AH, 2Ch
    INT     21h                     ; CH=hour, CL=minute
    MOV     BL, CH                  ; BL=hour
    MOV     BH, CL                  ; BH=minute

    ; ---- Position at column 74 (DI points past row end) ----
    SUB     DI, 12                  ; DI = col 74 in shadow_seg

    ; ---- Write leading space ----
    MOV     AL, ' '
    MOV     ES:[DI], AL
    MOV     BYTE ES:[DI+1], ED_ATTR_STAT
    ADD     DI, 2

    ; ---- Write hour as 2 digits ----
    MOV     AL, BL                  ; AL=hour (0-23)
    CALL    .clock_put2

    ; ---- Write ':' ----
    MOV     AL, ':'
    MOV     ES:[DI], AL
    MOV     BYTE ES:[DI+1], ED_ATTR_STAT
    ADD     DI, 2

    ; ---- Write minute as 2 digits ----
    MOV     AL, BH                  ; AL=minute (0-59)
    CALL    .clock_put2

    POP     DX
    POP     CX
    POP     BX
    POP     AX
    RET

    ; Helper: write AL (0-99) as 2 ASCII digits to ES:DI, advance DI by 4
.clock_put2:
    XOR     AH, AH
    MOV     DL, 10
    DIV     DL                      ; AL=tens, AH=ones
    ADD     AL, '0'
    MOV     ES:[DI], AL
    MOV     BYTE ES:[DI+1], ED_ATTR_STAT
    ADD     DI, 2
    MOV     AL, AH
    ADD     AL, '0'
    MOV     ES:[DI], AL
    MOV     BYTE ES:[DI+1], ED_ATTR_STAT
    ADD     DI, 2
    RET

ed_draw_clock ENDP

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

    MOV     AX, [cur_col]
    CALL    byte_to_visual_col     ; AX = visual column of cursor
    SUB     AX, [left_col]
    CMP     AX, [ed_text_cols]
    JAE     .no_cursor
    SHL     AX, 1
    ADD     DI, AX
    JMP     .swap_attr

.swap_attr:
    ; Fixed cursor attribute: bright white bg, blue fg (requires blink disable)
    MOV     BYTE ES:[DI + 1], 0F1h

.no_cursor:
    POP     DI
    POP     DX
    POP     CX
    POP     BX
    POP     AX
    RET
ed_draw_cursor ENDP

; ============================================================
; ed_draw_panel — Draw document switcher side panel overlay
;
; When panel_visible=1, draws a two-tier overlay on the left
; side of the editor area. Tier 1 (ALT+n) shows slots 0-9,
; tier 2 (ALT+SHFT+n) shows slots 10-19 if any are occupied.
; Format per row: │»N FILENAME.EXT*│  (18 chars, cols 0-17)
;
; Input:  ES = shadow_seg (from tui_ctrl_draw_editor context)
; Output: None
; Clobbers: AX, BX, CX, DX, SI, DI
; ============================================================
ed_draw_panel PROC
    CMP     BYTE CS:[panel_visible], 0
    JZ      .ep_done_early

    MOV     DH, 1                   ; DH = screen row (start at row 1)

    ; ---- Tier 1 header: "ALT+n:" ----
    PUSH    SI
    MOV     SI, .hdr_tier1
    CALL    .ep_draw_hdr
    POP     SI

    ; ---- Tier 1 occupied slots (0-9) ----
    PUSH    DS
    MOV     DS, CS:[doc_table_seg]  ; DS = doc_table_seg for slot accesses
    XOR     SI, SI                  ; SI = offset 0 within doc_table_seg
    XOR     DL, DL                  ; DL = slot counter
.ep_t1_loop:
    CMP     DL, 10
    JAE     .ep_t1_done
    TEST    BYTE [SI + DOC_FLAGS], DOCF_OCCUPIED
    JZ      .ep_t1_next
    CALL    .ep_draw_slot
.ep_t1_next:
    ADD     SI, DOC_SLOT_SIZE
    INC     DL
    JMP     .ep_t1_loop
.ep_t1_done:
    ; DL = 10, SI = doc_table + 10*DOC_SLOT_SIZE

    ; ---- Check if tier 2 has any occupied slots ----
    PUSH    SI
    PUSH    DX
    MOV     CL, DL                  ; CL = 10
.ep_t2_check:
    CMP     CL, MAX_DOCS
    JAE     .ep_no_t2
    TEST    BYTE [SI + DOC_FLAGS], DOCF_OCCUPIED
    JNZ     .ep_has_t2
    ADD     SI, DOC_SLOT_SIZE
    INC     CL
    JMP     .ep_t2_check
.ep_no_t2:
    POP     DX
    POP     SI
    JMP     .ep_fill
.ep_has_t2:
    POP     DX
    POP     SI
    ; SI and DL restored to slot 10

    ; ---- Tier 2 header: "ALT+SHFT+n:" ----
    PUSH    SI
    MOV     SI, .hdr_tier2
    CALL    .ep_draw_hdr
    POP     SI

    ; ---- Tier 2 occupied slots (10-19) ----
.ep_t2_loop:
    CMP     DL, MAX_DOCS
    JAE     .ep_fill
    TEST    BYTE [SI + DOC_FLAGS], DOCF_OCCUPIED
    JZ      .ep_t2_next
    CALL    .ep_draw_slot
.ep_t2_next:
    ADD     SI, DOC_SLOT_SIZE
    INC     DL
    JMP     .ep_t2_loop

    ; ---- Fill remaining rows to status bar with empty panel ----
.ep_fill:
    CMP     DH, ED_TEXT_ROWS + 1    ; rows 1 through 23
    JAE     .ep_shadow

    ; Compute shadow buffer position for row DH, col 0
    MOV     BL, DH
    XOR     BH, BH
    SHL     BX, 1
    MOV     DI, CS:[BX + row_offsets]
    ; DI = offset within shadow_seg

    MOV     AH, PANEL_ATTR_NORM

    ; Col 0: left border
    MOV     AL, 0B3h
    MOV     ES:[DI], AL
    MOV     ES:[DI+1], AH
    ADD     DI, 2

    ; Cols 1-16: spaces
    MOV     AL, ' '
    MOV     CX, 16
.ep_fill_sp:
    MOV     ES:[DI], AL
    MOV     ES:[DI+1], AH
    ADD     DI, 2
    DEC     CX
    JNZ     .ep_fill_sp

    ; Col 17: right border
    MOV     AL, 0B3h
    MOV     ES:[DI], AL
    MOV     ES:[DI+1], AH

    INC     DH
    JMP     .ep_fill

    ; ---- Right shadow column ----
    ; Darken col PANEL_WIDTH, rows 1..ED_TEXT_ROWS
.ep_shadow:
    ; Restore DS before calling tui_darken_cell (needs DS=CS for row_offsets/shadow_seg)
    POP     DS                      ; restore DS (pushed before doc_table_seg walk)
    MOV     DH, 1
    MOV     DL, PANEL_WIDTH
.ep_shadow_loop:
    CMP     DH, ED_TEXT_ROWS + 1
    JAE     .ep_done
    CALL    tui_darken_cell
    INC     DH
    JMP     .ep_shadow_loop

.ep_done:
.ep_done_early:
    RET

; ---- Local helper: draw header row ----
; Input:  SI = 16-char header string, DH = screen row
; Output: DH incremented
; Clobbers: AX, BX, CX, DI
.ep_draw_hdr:
    MOV     BL, DH
    XOR     BH, BH
    SHL     BX, 1
    MOV     DI, CS:[BX + row_offsets]
    ; DI = offset within shadow_seg
    ; Left border (always NORM white)
    MOV     AL, 0B3h
    MOV     BYTE ES:[DI], AL
    MOV     BYTE ES:[DI+1], PANEL_ATTR_NORM
    ADD     DI, 2
    ; 16 chars of header text (HDR color)
    MOV     AH, PANEL_ATTR_HDR
    MOV     CX, 16
.edh_copy:
    MOV     AL, CS:[SI]
    MOV     ES:[DI], AL
    MOV     ES:[DI+1], AH
    ADD     DI, 2
    INC     SI
    DEC     CX
    JNZ     .edh_copy
    ; Right border (always NORM white)
    MOV     AL, 0B3h
    MOV     ES:[DI], AL
    MOV     BYTE ES:[DI+1], PANEL_ATTR_NORM
    INC     DH
    RET

; ---- Local helper: draw one document slot row ----
; Input:  DL = slot index (0-19), DH = screen row,
;         SI = slot pointer in doc_table
; Output: DH incremented
; Clobbers: AX, BX, CX, DI
.ep_draw_slot:
    ; Compute shadow buffer position: row DH, col 0
    MOV     BL, DH
    XOR     BH, BH
    SHL     BX, 1
    MOV     DI, CS:[BX + row_offsets]
    ; DI = offset within shadow_seg

    ; Determine attribute
    MOV     AH, PANEL_ATTR_NORM
    CMP     DL, CS:[panel_sel]
    JNE     .eds_not_sel
    MOV     AH, PANEL_ATTR_HI
.eds_not_sel:

    ; Col 0: left border │ (always NORM white)
    MOV     AL, 0B3h
    MOV     ES:[DI], AL
    MOV     BYTE ES:[DI+1], PANEL_ATTR_NORM
    ADD     DI, 2

    ; Col 1: arrow indicator (» if highlighted, space if not)
    MOV     AL, ' '
    CMP     DL, CS:[panel_sel]
    JNE     .eds_no_arrow
    MOV     AL, 0AFh                ; » (CP437 175)
.eds_no_arrow:
    MOV     ES:[DI], AL
    MOV     ES:[DI+1], AH
    ADD     DI, 2

    ; Col 2: digit label (1-9, 0 for slot 9/19)
    MOV     AL, DL
    CMP     AL, 10
    JB      .eds_digit
    SUB     AL, 10                  ; reduce tier 2 to 0-9
.eds_digit:
    INC     AL                      ; 1-10
    CMP     AL, 10
    JNE     .eds_not_zero
    XOR     AL, AL                  ; 10 → 0
.eds_not_zero:
    ADD     AL, '0'
    MOV     ES:[DI], AL
    MOV     ES:[DI+1], AH
    ADD     DI, 2

    ; Col 3: space
    MOV     AL, ' '
    MOV     ES:[DI], AL
    MOV     ES:[DI+1], AH
    ADD     DI, 2

    ; Cols 4-15: basename (12 chars, space-padded)
    ; Find basename: scan DOC_FILENAME for last '\'
    PUSH    SI
    PUSH    DI
    MOV     BX, SI
    ADD     BX, DOC_FILENAME        ; BX = start of filename
    MOV     DI, BX                  ; DI = best basename start
.eds_scan_bs:
    MOV     AL, [BX]                ; DS = doc_table_seg
    TEST    AL, AL
    JZ      .eds_found_base
    CMP     AL, '\'
    JNE     .eds_scan_next
    MOV     DI, BX
    INC     DI
.eds_scan_next:
    INC     BX
    JMP     .eds_scan_bs
.eds_found_base:
    MOV     BX, DI                  ; BX = basename pointer
    POP     DI
    POP     SI

    ; Write up to 12 chars of basename, pad with spaces
    MOV     CX, 12
.eds_write_name:
    MOV     AL, [BX]                ; DS = doc_table_seg
    TEST    AL, AL
    JZ      .eds_pad_name
    MOV     ES:[DI], AL
    MOV     ES:[DI+1], AH
    ADD     DI, 2
    INC     BX
    DEC     CX
    JNZ     .eds_write_name
    JMP     .eds_after_name
.eds_pad_name:
    MOV     AL, ' '
.eds_pad_loop:
    JCXZ    .eds_after_name
    MOV     ES:[DI], AL
    MOV     ES:[DI+1], AH
    ADD     DI, 2
    DEC     CX
    JMP     .eds_pad_loop
.eds_after_name:

    ; Col 16: dirty indicator
    MOV     AL, ' '
    CMP     DL, CS:[active_doc]
    JNE     .eds_check_cached
    CMP     BYTE CS:[dirty], 0
    JE      .eds_write_dirty
    MOV     AL, '*'
    JMP     .eds_write_dirty
.eds_check_cached:
    TEST    BYTE [SI + DOC_FLAGS], DOCF_DIRTY
    JZ      .eds_write_dirty
    MOV     AL, '*'
.eds_write_dirty:
    MOV     ES:[DI], AL
    MOV     ES:[DI+1], AH
    ADD     DI, 2

    ; Col 17: right border │ (always NORM white)
    MOV     AL, 0B3h
    MOV     ES:[DI], AL
    MOV     BYTE ES:[DI+1], PANEL_ATTR_NORM

    INC     DH                      ; next screen row
    RET

; ---- Inline data: 16-char header strings (panel interior) ----
.hdr_tier1: DB ' ALT+n:         '
.hdr_tier2: DB ' ALT+SHFT+n:    '

ed_draw_panel ENDP

