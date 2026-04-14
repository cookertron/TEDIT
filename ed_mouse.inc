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

    ; ---- Side panel click intercept ----
    CMP     BYTE [panel_visible], 0
    JZ      .press_no_panel

    ; Check if click is inside panel area (cols 0-16)
    CMP     AH, PANEL_WIDTH
    JAE     .panel_click_outside

    ; Map editor row to occupied slot index (accounting for header rows)
    ; AL = editor-relative row: row 0 = tier 1 header "ALT+n:"
    TEST    AL, AL
    JZ      .panel_click_outside    ; clicked tier 1 header

    PUSH    SI
    MOV     ES, [doc_table_seg]
    XOR     SI, SI
    XOR     CL, CL                 ; CL = slot counter (0-19)
    MOV     CH, 1                  ; CH = visible row (1 = first slot after hdr)

    ; --- Tier 1 slots (0-9) ---
.pc_t1:
    CMP     CL, 10
    JAE     .pc_t1_end
    TEST    BYTE ES:[SI + DOC_FLAGS], DOCF_OCCUPIED
    JZ      .pc_t1_next
    CMP     CH, AL
    JE      .pc_found
    INC     CH
.pc_t1_next:
    ADD     SI, DOC_SLOT_SIZE
    INC     CL
    JMP     .pc_t1
.pc_t1_end:
    ; CL = 10, SI at slot 10 — check for tier 2 docs
    PUSH    CX
    PUSH    SI
.pc_t2_scan:
    CMP     CL, MAX_DOCS
    JAE     .pc_no_t2_pop
    TEST    BYTE ES:[SI + DOC_FLAGS], DOCF_OCCUPIED
    JNZ     .pc_has_t2_pop
    ADD     SI, DOC_SLOT_SIZE
    INC     CL
    JMP     .pc_t2_scan
.pc_no_t2_pop:
    POP     SI
    POP     CX
    JMP     .pc_outside_pop         ; no tier 2, click in empty area
.pc_has_t2_pop:
    POP     SI
    POP     CX
    ; CH = tier 2 header row
    CMP     CH, AL
    JE      .pc_outside_pop         ; clicked tier 2 header
    INC     CH                      ; skip tier 2 header

    ; --- Tier 2 slots (10-19) ---
.pc_t2:
    CMP     CL, MAX_DOCS
    JAE     .pc_outside_pop
    TEST    BYTE ES:[SI + DOC_FLAGS], DOCF_OCCUPIED
    JZ      .pc_t2_next
    CMP     CH, AL
    JE      .pc_found
    INC     CH
.pc_t2_next:
    ADD     SI, DOC_SLOT_SIZE
    INC     CL
    JMP     .pc_t2

.pc_found:
    POP     SI
    MOV     [panel_sel], CL
    MOV     BYTE [panel_visible], 0
    MOV     AL, CL
    CMP     AL, [active_doc]
    JE      .panel_click_done
    CALL    doc_switch
.panel_click_done:
    MOV     BYTE [fw_state + FW_DIRTY], 1
    JMP     .press_done

.pc_outside_pop:
    POP     SI
.panel_click_outside:
    MOV     BYTE [panel_visible], 0
    MOV     BYTE [fw_state + FW_DIRTY], 1
    JMP     .press_done

.press_no_panel:
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

    ; ---- Classify click: single / double / triple ----
    ; Read BIOS tick counter (0040:006C)
    PUSH    ES
    MOV     DX, 0040h
    MOV     ES, DX
    MOV     DX, ES:[006Ch]         ; DX = current BIOS tick
    POP     ES

    ; Check if same position as last click
    MOV     CX, [cur_line]
    CMP     CX, [ms_ed_click_line]
    JNE     .press_single_new
    MOV     CX, [cur_col]
    CMP     CX, [ms_ed_click_col]
    JNE     .press_single_new

    ; Same position — check timing
    MOV     CX, DX
    SUB     CX, [ms_ed_click_tick]
    CMP     CX, DBLCLICK_TICKS
    JA      .press_single_new

    ; Within window + same position: increment count
    CMP     BYTE [ms_ed_click_count], 2
    JAE     .press_triclick
    ; count was 1, now 2 → double click
    MOV     BYTE [ms_ed_click_count], 2
    MOV     [ms_ed_click_tick], DX
    JMP     .press_dblclick

.press_triclick:
    ; count was 2, now 3 → triple click, reset cycle
    MOV     BYTE [ms_ed_click_count], 0
    MOV     [ms_ed_click_tick], DX
    JMP     .press_triclick_action

.press_single_new:
    ; New position or timed out — record state, count = 1
    MOV     [ms_ed_click_tick], DX
    MOV     CX, [cur_line]
    MOV     [ms_ed_click_line], CX
    MOV     CX, [cur_col]
    MOV     [ms_ed_click_col], CX
    MOV     BYTE [ms_ed_click_count], 1

    ; ---- Single click: clear selection, set anchor ----
.press_single:
    CALL    sel_clear
    MOV     AX, [cur_line]
    MOV     [sel_anchor_line], AX
    MOV     AX, [cur_col]
    MOV     [sel_anchor_col], AX
    ; NOTE: sel_active stays 0 — only drag activates selection
    JMP     .press_click_done

    ; ---- Double click: select word ----
.press_dblclick:
    CALL    ed_select_word
    JMP     .press_click_done

    ; ---- Triple click: select line ----
.press_triclick_action:
    CALL    ed_select_line
    JMP     .press_click_done

    ; ---- Common tail: start drag tracking, request redraw ----
.press_click_done:
    MOV     [mouse_state + MS_PRESSED], BX
    OR      BYTE [mouse_state + MS_FLAGS], MSF_CTRL_DRAG
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

    ; ---- Skip if mouse hasn't moved since last poll ----
    MOV     AL, [mouse_state + MS_COL]
    CMP     AL, [mouse_state + MS_PCOL]
    JNE     .drag_moved
    MOV     AL, [mouse_state + MS_ROW]
    CMP     AL, [mouse_state + MS_PROW]
    JE      .drag_done
.drag_moved:

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

; ============================================================
; is_word_char — Classify character as word or non-word
;
; Entry: AL = character byte
; Exit:  CF = 0 if word char (A-Z, a-z, 0-9, _)
;        CF = 1 if non-word char
; Preserves: all registers except flags
; ============================================================

is_word_char PROC
    CMP     AL, '_'
    JE      .yes
    CMP     AL, '0'
    JB      .no
    CMP     AL, '9'
    JBE     .yes
    CMP     AL, 'A'
    JB      .no
    CMP     AL, 'Z'
    JBE     .yes
    CMP     AL, 'a'
    JB      .no
    CMP     AL, 'z'
    JBE     .yes
.no:
    STC
    RET
.yes:
    CLC
    RET
is_word_char ENDP

; ============================================================
; ed_read_line_buf — Read current line content into word_scan_buf
;
; Reads up to 255 visible characters from cur_line into
; word_scan_buf[]. Stops at line ending (CR/LF) or 255 chars.
; Sets word_scan_len to the number of bytes read.
;
; Entry: cur_line set to target line
; Exit:  word_scan_buf filled, word_scan_len set
; Clobbers: AX, BX, CX, DX, SI, DI
; ============================================================

ed_read_line_buf PROC
    MOV     AX, [cur_line]
    CALL    seek_to_line            ; sets rnd_pi, rnd_off
    MOV     DI, word_scan_buf       ; DI = write pointer into buf
    XOR     CX, CX                  ; CX = bytes read so far

.rlb_piece:
    MOV     BX, [rnd_pi]
    CMP     BX, [pt_count]
    JAE     .rlb_done
    PUSH    CX
    CALL    resolve_piece           ; AX=seg, SI=base, DX=piece_len
    POP     CX
    JC      .rlb_done

    PUSH    DS
    MOV     DS, AX
    ADD     DX, SI                  ; DX = piece end (absolute)
    ADD     SI, CS:[rnd_off]        ; SI = start position in piece

.rlb_byte:
    CMP     SI, DX
    JAE     .rlb_next_piece         ; piece exhausted
    CMP     CX, 255
    JAE     .rlb_done_ds            ; buffer full

    MOV     AL, [SI]
    CMP     AL, 0Dh
    JE      .rlb_done_ds            ; CR = line end
    CMP     AL, 0Ah
    JE      .rlb_done_ds            ; LF = line end

    MOV     CS:[DI], AL             ; write to buf (in CS/default segment)
    INC     DI
    INC     SI
    INC     CX
    JMP     .rlb_byte

.rlb_next_piece:
    POP     DS
    INC     WORD [rnd_pi]
    MOV     WORD [rnd_off], 0
    JMP     .rlb_piece

.rlb_done_ds:
    POP     DS
.rlb_done:
    MOV     [word_scan_len], CX
    RET
ed_read_line_buf ENDP

; ============================================================
; ed_select_word — Select word at cursor position (double-click)
;
; Reads the current line into word_scan_buf, finds word
; boundaries around cur_col, and sets up a selection spanning
; the word. If cursor is on a non-word character, selects just
; that single character.
;
; Entry: cur_line, cur_col set to clicked position
; Exit:  sel_active = 1, sel_anchor_line/col and cur_col
;        updated to span the word
; Preserves: BX (editor control pointer)
; ============================================================

ed_select_word PROC
    PUSH    BX

    ; ---- Step 1: Read line content into scratch buffer ----
    CALL    ed_read_line_buf        ; word_scan_buf filled, word_scan_len set

    ; ---- Step 2: Validate cur_col is within line ----
    MOV     AX, [cur_col]
    CMP     AX, [word_scan_len]
    JAE     .sw_at_eol

    ; ---- Step 3: Read char under cursor, classify ----
    MOV     SI, word_scan_buf
    ADD     SI, AX                  ; SI = &word_scan_buf[cur_col]
    MOV     AL, [SI]
    CALL    is_word_char
    JC      .sw_non_word

    ; ---- Step 4a: Word char — scan left for word start ----
    MOV     CX, [cur_col]          ; CX = col index (for left scan)
    MOV     SI, word_scan_buf
    ADD     SI, CX                  ; SI = &buf[cur_col]

.sw_scan_left:
    TEST    CX, CX
    JZ      .sw_left_done           ; at column 0, stop
    DEC     SI
    MOV     AL, [SI]
    CALL    is_word_char
    JC      .sw_left_done           ; non-word char at [CX-1], word starts at CX
    DEC     CX
    JMP     .sw_scan_left

.sw_left_done:
    ; CX = word start column
    MOV     [sel_anchor_col], CX
    MOV     AX, [cur_line]
    MOV     [sel_anchor_line], AX

    ; ---- Step 4b: Scan right for word end ----
    MOV     DX, [cur_col]          ; DX = col index (for right scan)
    MOV     SI, word_scan_buf
    ADD     SI, DX                  ; SI = &buf[cur_col]

.sw_scan_right:
    CMP     DX, [word_scan_len]
    JAE     .sw_right_done          ; at line end, stop
    MOV     AL, [SI]
    CALL    is_word_char
    JC      .sw_right_done          ; non-word char, stop
    INC     SI
    INC     DX
    JMP     .sw_scan_right

.sw_right_done:
    ; DX = word end column (exclusive = first char past word)
    MOV     [cur_col], DX
    MOV     BYTE [sel_active], 1
    MOV     BYTE [fw_state + FW_DIRTY], 1
    POP     BX
    RET

    ; ---- Non-word character: select single char ----
.sw_non_word:
    MOV     AX, [cur_line]
    MOV     [sel_anchor_line], AX
    MOV     AX, [cur_col]
    MOV     [sel_anchor_col], AX
    INC     AX
    MOV     [cur_col], AX
    MOV     BYTE [sel_active], 1
    MOV     BYTE [fw_state + FW_DIRTY], 1
    POP     BX
    RET

    ; ---- Click at or past end of line: no selection ----
.sw_at_eol:
    POP     BX
    RET

ed_select_word ENDP

; ============================================================
; ed_select_line — Select entire line at cursor (triple-click)
;
; Sets selection anchor to (cur_line, 0) and cursor to
; (cur_line, line_length). Activates selection.
;
; Entry: cur_line set to clicked line
; Exit:  sel_active = 1, anchor at line start, cursor at line end
; Preserves: BX (editor control pointer)
; ============================================================

ed_select_line PROC
    MOV     AX, [cur_line]
    MOV     [sel_anchor_line], AX
    MOV     WORD [sel_anchor_col], 0
    CALL    line_length             ; AX = visible chars on cur_line
    MOV     [cur_col], AX
    MOV     BYTE [sel_active], 1
    MOV     BYTE [fw_state + FW_DIRTY], 1
    RET
ed_select_line ENDP
