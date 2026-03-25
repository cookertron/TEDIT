; ============================================================
; ed_find.inc — Find / Search Engine
;
; Depends on: ed_const.inc, ed_core.inc
; BSS: find_buf, find_len, find_match_*, find_cur_line/col,
;      find_needle_pos, find_start_line/col
; ============================================================

; ============================================================
; FIND_FORWARD — Search forward from (line, col)
;
; Input:  BX = start line (0-based), CX = start column (0-based)
;         [find_buf] = null-terminated search needle
;         [find_len] = needle length (WORD, 1-64)
;         DS = CS
; Output: CF=0 found (find_match_* set), CF=1 not found
; Preserves: BP, DI
; Clobbers:  AX, BX, CX, DX, SI, ES
; ============================================================

find_forward PROC
    PUSH    BP
    PUSH    DI

    ; ---- Guards ----
    CMP     WORD [pt_count], 0
    JE      .ff_notfound
    CMP     WORD [find_len], 0
    JE      .ff_notfound

    ; ---- Convert (line, col) to byte offset ----
    ; line_col_to_offset: BX=line, CX=col -> DX:AX = byte offset
    ; NOTE: rnd_pi/rnd_off are set to LINE START, not column position.
    ;       We must walk pieces to find the correct rnd_pi/rnd_off.
    ; Preserves: BX, BP, DI.  Clobbers: AX, CX, DX, SI, ES.
    PUSH    BX
    PUSH    CX
    CALL    line_col_to_offset
    POP     CX                      ; CX = start_col
    POP     BX                      ; BX = start_line

    ; ---- Init search state ----
    MOV     [find_cur_line], BX
    MOV     [find_cur_col], CX
    MOV     WORD [find_needle_pos], 0

    ; ---- Resolve byte offset DX:AX to correct rnd_pi/rnd_off ----
    MOV     WORD [rnd_pi], 0
.ff_find_piece:
    MOV     BX, [rnd_pi]
    CMP     BX, [pt_count]
    JAE     .ff_notfound            ; offset past document end

    PUSH    DX                      ; save high byte offset
    PUSH    AX                      ; save low byte offset
    CALL    resolve_piece           ; DX = piece length
    MOV     CX, DX                  ; CX = piece length
    POP     AX                      ; restore low byte offset
    POP     DX                      ; restore high byte offset
    JC      .ff_notfound            ; CF preserved through MOV/POP

    ; If DX > 0 (high word), offset definitely >= piece length
    TEST    DX, DX
    JNZ     .ff_skip_piece
    CMP     AX, CX                  ; remaining offset < piece length?
    JB      .ff_piece_found

.ff_skip_piece:
    SUB     AX, CX
    SBB     DX, 0
    INC     WORD [rnd_pi]
    JMP     .ff_find_piece

.ff_piece_found:
    MOV     [rnd_off], AX           ; rnd_off = offset within this piece

    ; ---- Piece loop ----
.ff_piece_loop:
    MOV     BX, [rnd_pi]
    CMP     BX, [pt_count]
    JAE     .ff_notfound

    ; resolve_piece: BX=piece -> AX=seg, SI=base, DX=len
    ; Preserves: BX, DI, BP.  Clobbers: AX, CX, DX, SI, ES.
    CALL    resolve_piece
    JC      .ff_notfound

    ; Compute read window within this piece
    ADD     SI, [rnd_off]
    SUB     DX, [rnd_off]

    ; Enter text-segment mode: DS=text, all BSS via CS:
    PUSH    DS
    MOV     DS, AX

    ; ---- Byte loop ----
.ff_byte_loop:
    TEST    DX, DX
    JZ      .ff_next_piece

    LODSB                           ; AL = [DS:SI++]
    DEC     DX

    ; ---- Compare against needle (before position tracking) ----
.ff_compare:
    MOV     BX, CS:[find_needle_pos]

    ; Case-insensitive: uppercase text byte if flag set
    TEST    BYTE CS:[find_flags], 01h
    JZ      .ff_cmp_exact
    CMP     AL, 'a'
    JB      .ff_cmp_exact
    CMP     AL, 'z'
    JA      .ff_cmp_exact
    SUB     AL, 20h
.ff_cmp_exact:

    CMP     AL, CS:[BX + find_buf]
    JNE     .ff_mismatch

    ; ---- Byte matched ----
    TEST    BX, BX
    JNZ     .ff_match_cont

    ; First matched byte: capture start position (pre-update)
    MOV     CX, CS:[find_cur_line]
    MOV     CS:[find_start_line], CX
    MOV     CX, CS:[find_cur_col]
    MOV     CS:[find_start_col], CX

.ff_match_cont:
    INC     BX
    MOV     CS:[find_needle_pos], BX
    CMP     BX, CS:[find_len]
    JAE     .ff_found

    ; ---- Track position (after comparison) ----
.ff_track:
    CMP     AL, 0Ah
    JE      .ff_track_lf
    CMP     AL, 0Dh
    JE      .ff_byte_loop           ; CR: no position change

    INC     WORD CS:[find_cur_col]
    JMP     .ff_byte_loop

.ff_track_lf:
    INC     WORD CS:[find_cur_line]
    MOV     WORD CS:[find_cur_col], 0
    JMP     .ff_byte_loop

    ; ---- Mismatch ----
.ff_mismatch:
    TEST    BX, BX
    JZ      .ff_track               ; no partial: track position, continue

    ; Partial match failed: reset needle_pos, re-compare same byte
    MOV     WORD CS:[find_needle_pos], 0
    JMP     .ff_compare

    ; ---- Full match found ----
.ff_found:
    ; Track position of last matched byte for exclusive-end
    CMP     AL, 0Ah
    JE      .ff_found_lf
    CMP     AL, 0Dh
    JE      .ff_found_emit          ; CR: no position update
    INC     WORD CS:[find_cur_col]
    JMP     .ff_found_emit
.ff_found_lf:
    INC     WORD CS:[find_cur_line]
    MOV     WORD CS:[find_cur_col], 0

.ff_found_emit:
    ; Copy results to match output variables
    MOV     AX, CS:[find_start_line]
    MOV     CS:[find_match_line], AX
    MOV     AX, CS:[find_start_col]
    MOV     CS:[find_match_col], AX
    MOV     AX, CS:[find_cur_line]
    MOV     CS:[find_match_end_line], AX
    MOV     AX, CS:[find_cur_col]
    MOV     CS:[find_match_end_col], AX

    ; ---- Selection-only bounds check ----
    TEST    BYTE CS:[find_flags], FIND_FLAG_SEL
    JZ      .ff_found_ok

    ; Match end must be <= selection end
    MOV     AX, CS:[find_match_end_line]
    CMP     AX, CS:[find_sel_e_line]
    JA      .ff_found_past_sel
    JB      .ff_found_ok
    MOV     AX, CS:[find_match_end_col]
    CMP     AX, CS:[find_sel_e_col]
    JA      .ff_found_past_sel

.ff_found_ok:
    POP     DS                      ; restore CS
    POP     DI
    POP     BP
    CLC
    RET

.ff_found_past_sel:
    ; Match extends past selection boundary — not found
    POP     DS
    JMP     .ff_notfound

    ; ---- Piece boundary ----
.ff_next_piece:
    POP     DS                      ; restore CS
    INC     WORD [rnd_pi]
    MOV     WORD [rnd_off], 0
    JMP     .ff_piece_loop

    ; ---- Not found ----
.ff_notfound:
    POP     DI
    POP     BP
    STC
    RET

find_forward ENDP

; ============================================================================
; _find_show_dialog — Custom Find dialog with checkboxes
;
; Layout (interior 6 rows, WIN_H=8):
;   Row 0: "Find:" label
;   Row 1: [textbox]
;   Row 2: [X] Match case
;   Row 3: [ ] In selection
;   Row 4: (separator)
;   Row 5: [ OK ]  [ Cancel ]
;
; Input:  AH = 1 if multi-line selection available, 0 if not
; Output: AL = DLG_OK or DLG_CANCEL
;         find_flags updated from checkbox states
;         find_buf contains entered text (if DLG_OK)
; Clobbers: AX, BX, CX, DX, SI, DI (all saved by PushAll)
; ============================================================================
_find_show_dialog PROC
    PUSH    AX                      ; save AH (multi-line sel flag)
    PushAll

    ; ---- Interior width = 28 (enough for checkboxes + buttons + textbox) ----
    MOV     BL, 28                  ; interior width
    MOV     BH, 30                  ; total width (interior + 2 border)

    ; Center on screen
    MOV     AL, SCR_W
    SUB     AL, BH
    SHR     AL, 1
    MOV     DL, AL                  ; DL = dialog X
    MOV     DH, (SCR_H - 8) / 2    ; DH = dialog Y (height=8)

    ; ---- Label: "Find:" at row 0 ----
    MOV     DI, dlg_label
    MOV     BYTE [DI + CTRL_TYPE], CTYPE_LABEL
    MOV     BYTE [DI + CTRL_FLAGS], CTRLF_VISIBLE
    MOV     BYTE [DI + CTRL_X], 1
    MOV     BYTE [DI + CTRL_Y], 0
    MOV     BYTE [DI + CTRL_W], 5
    MOV     BYTE [DI + CTRL_H], 1
    MOV     WORD [DI + CTRL_NEXT], dlg_textbox
    MOV     WORD [DI + CTRL_TEXT], s_find_prompt
    MOV     BYTE [DI + CTRL_ATTR], CLR_LABEL
    MOV     BYTE [DI + CTRL_FATTR], CLR_LABEL
    MOV     WORD [DI + CTRL_HANDLER], 0

    ; ---- TextBox at row 1, width = interior - 2 = 26 ----
    MOV     DI, dlg_textbox
    MOV     BYTE [DI + CTRL_TYPE], CTYPE_TEXTBOX
    MOV     BYTE [DI + CTRL_FLAGS], CTRLF_VISIBLE + CTRLF_ENABLED + CTRLF_FOCUSABLE
    MOV     BYTE [DI + CTRL_X], 1
    MOV     BYTE [DI + CTRL_Y], 1
    MOV     BYTE [DI + CTRL_W], 26
    MOV     BYTE [DI + CTRL_H], 1
    MOV     WORD [DI + CTRL_NEXT], dlg_chk1
    MOV     WORD [DI + CTRL_TEXT], find_buf
    MOV     BYTE [DI + CTRL_ATTR], CLR_TEXTBOX
    MOV     BYTE [DI + CTRL_FATTR], CLR_TB_FOCUS
    MOV     WORD [DI + CTRL_HANDLER], 0
    MOV     BYTE [DI + CTRL_TB_MAXLEN], FIND_MAX_LEN
    MOV     BYTE [DI + CTRL_TB_CURPOS], 0
    MOV     BYTE [DI + CTRL_TB_SCROLL], 0
    ; Compute initial text length
    PUSH    SI
    PUSH    CX
    MOV     SI, find_buf
    XOR     CX, CX
.fsd_len:
    CMP     BYTE [SI], 0
    JZ      .fsd_len_done
    INC     SI
    INC     CL
    JMP     .fsd_len
.fsd_len_done:
    MOV     [DI + CTRL_TB_LEN], CL
    MOV     [DI + CTRL_TB_CURPOS], CL
    POP     CX
    POP     SI

    ; ---- Checkbox 1: "Match case" at row 2 ----
    MOV     DI, dlg_chk1
    MOV     BYTE [DI + CTRL_TYPE], CTYPE_CHECKBOX
    MOV     BYTE [DI + CTRL_FLAGS], CTRLF_VISIBLE + CTRLF_ENABLED + CTRLF_FOCUSABLE
    MOV     BYTE [DI + CTRL_X], 1
    MOV     BYTE [DI + CTRL_Y], 2
    MOV     BYTE [DI + CTRL_W], 14          ; "[X] Match case"
    MOV     BYTE [DI + CTRL_H], 1
    MOV     WORD [DI + CTRL_NEXT], dlg_chk2
    MOV     WORD [DI + CTRL_TEXT], s_find_matchcase
    MOV     BYTE [DI + CTRL_ATTR], CLR_CHECKBOX
    MOV     BYTE [DI + CTRL_FATTR], CLR_CB_FOCUS
    MOV     WORD [DI + CTRL_HANDLER], 0
    ; State: checked = case-sensitive (bit 0 = 0), unchecked = CI (bit 0 = 1)
    MOV     AL, [find_flags]
    AND     AL, FIND_FLAG_CI
    XOR     AL, 1                           ; invert: 0→1, 1→0
    MOV     [DI + CTRL_CB_STATE], AL

    ; ---- Checkbox 2: "In selection" at row 3 ----
    MOV     DI, dlg_chk2
    MOV     BYTE [DI + CTRL_TYPE], CTYPE_CHECKBOX
    MOV     BYTE [DI + CTRL_X], 1
    MOV     BYTE [DI + CTRL_Y], 3
    MOV     BYTE [DI + CTRL_W], 16          ; "[X] In selection"
    MOV     BYTE [DI + CTRL_H], 1
    MOV     WORD [DI + CTRL_NEXT], dlg_btn1
    MOV     WORD [DI + CTRL_TEXT], s_find_insel
    MOV     BYTE [DI + CTRL_ATTR], CLR_CHECKBOX
    MOV     BYTE [DI + CTRL_FATTR], CLR_CB_FOCUS
    MOV     WORD [DI + CTRL_HANDLER], 0
    ; Enable only if multi-line selection available (check saved AH flag)
    POP     AX                              ; restore full AX (PushAll is still on stack)
    PUSH    AX                              ; re-save for PopAll
    ; AH from original push before PushAll is at [SP + 16] — too deep.
    ; Instead, read from the separate PUSH AX at the start.
    ; Actually, the stack is: {PushAll regs}, {original AX}
    ; Let me use a different approach: save flag to BSS before PushAll.
    ; FIXME: restructure — for now, just read find_flags bit 1 for pre-set

    ; Check the flag passed on the original stack
    ; Stack layout: [SP] = PushAll (12 bytes: AX,BX,CX,DX,SI,DI)
    ;               [SP+12] = original AX from PUSH AX
    MOV     BP, SP
    MOV     AX, [BP + 12]                  ; original AX
    TEST    AH, AH
    JZ      .fsd_no_sel

    ; Multi-line selection available: enable checkbox, pre-checked
    MOV     BYTE [DI + CTRL_FLAGS], CTRLF_VISIBLE + CTRLF_ENABLED + CTRLF_FOCUSABLE
    MOV     BYTE [DI + CTRL_CB_STATE], 1
    JMP     .fsd_build_btns

.fsd_no_sel:
    ; No multi-line selection: enabled but unchecked
    MOV     BYTE [DI + CTRL_FLAGS], CTRLF_VISIBLE + CTRLF_ENABLED + CTRLF_FOCUSABLE
    MOV     BYTE [DI + CTRL_CB_STATE], 0

.fsd_build_btns:
    ; ---- OK button at row 5, centered ----
    MOV     CH, 4                           ; ok_x = (28 - 20) / 2 = 4
    MOV     DI, dlg_btn1
    MOV     BYTE [DI + CTRL_TYPE], CTYPE_BUTTON
    MOV     BYTE [DI + CTRL_FLAGS], CTRLF_VISIBLE + CTRLF_ENABLED + CTRLF_FOCUSABLE
    MOV     [DI + CTRL_X], CH
    MOV     BYTE [DI + CTRL_Y], 5
    MOV     BYTE [DI + CTRL_W], DLG_BTN_OK_W
    MOV     BYTE [DI + CTRL_H], 1
    MOV     WORD [DI + CTRL_NEXT], dlg_btn2
    MOV     WORD [DI + CTRL_TEXT], dlg_str_ok
    MOV     BYTE [DI + CTRL_ATTR], CLR_BTN
    MOV     BYTE [DI + CTRL_FATTR], CLR_BTN_FOCUS
    MOV     WORD [DI + CTRL_HANDLER], dlg_handler_ok

    ; ---- Cancel button ----
    MOV     AL, CH
    ADD     AL, DLG_BTN_OK_W + DLG_BTN_GAP
    MOV     DI, dlg_btn2
    MOV     BYTE [DI + CTRL_TYPE], CTYPE_BUTTON
    MOV     BYTE [DI + CTRL_FLAGS], CTRLF_VISIBLE + CTRLF_ENABLED + CTRLF_FOCUSABLE
    MOV     [DI + CTRL_X], AL
    MOV     BYTE [DI + CTRL_Y], 5
    MOV     BYTE [DI + CTRL_W], DLG_BTN_CAN_W
    MOV     BYTE [DI + CTRL_H], 1
    MOV     WORD [DI + CTRL_NEXT], 0
    MOV     WORD [DI + CTRL_TEXT], dlg_str_cancel
    MOV     BYTE [DI + CTRL_ATTR], CLR_BTN
    MOV     BYTE [DI + CTRL_FATTR], CLR_BTN_FOCUS
    MOV     WORD [DI + CTRL_HANDLER], dlg_handler_cancel

    ; ---- Window template ----
    MOV     DI, dlg_tmpl
    MOV     BYTE [DI + WIN_FLAGS], WINF_VISIBLE + WINF_BORDER + WINF_TITLE + WINF_SHADOW
    MOV     [DI + WIN_X], DL
    MOV     [DI + WIN_Y], DH
    MOV     [DI + WIN_W], BH
    MOV     BYTE [DI + WIN_H], 8
    MOV     BYTE [DI + WIN_ATTR], CLR_WIN_BG
    MOV     BYTE [DI + WIN_BATTR], CLR_WIN_BDR
    MOV     BYTE [DI + WIN_TATTR], CLR_WIN_TTL
    MOV     WORD [DI + WIN_TITLE], s_find_title
    MOV     WORD [DI + WIN_FIRST], dlg_label
    MOV     WORD [DI + WIN_FOCUS], dlg_textbox
    MOV     WORD [DI + WIN_HANDLER], 0
    MOV     BYTE [DI + WIN_ZORDER], 0
    MOV     BYTE [DI + WIN_ID], 0
    MOV     BYTE [DI + WIN_SCROLLX], 0
    MOV     BYTE [DI + WIN_SCROLLY], 0

    CALL    tui_dlg_run_modal

    ; ---- Read checkbox states back into find_flags ----
    MOV     AL, [fw_state + FW_DLG_RESULT]
    CMP     AL, DLG_OK
    JNE     .fsd_return

    ; Match case: CB_STATE=1 means case-sensitive (clear CI flag)
    AND     BYTE [find_flags], ~(FIND_FLAG_CI | FIND_FLAG_SEL)
    CMP     BYTE [dlg_chk1 + CTRL_CB_STATE], 0
    JNE     .fsd_no_ci
    OR      BYTE [find_flags], FIND_FLAG_CI         ; unchecked = case-insensitive
.fsd_no_ci:
    ; In selection: CB_STATE=1 means selection-only (set SEL flag)
    CMP     BYTE [dlg_chk2 + CTRL_CB_STATE], 0
    JE      .fsd_no_sel_flag
    OR      BYTE [find_flags], FIND_FLAG_SEL
.fsd_no_sel_flag:

.fsd_return:
    PopAll
    POP     AX                      ; discard saved AX from start
    MOV     AL, [fw_state + FW_DLG_RESULT]
    RET
_find_show_dialog ENDP

; ============================================================================
; menu_find_handler — Show Find dialog, search forward with wrap, select result
; Input:   None (reads cur_line, cur_col, sel_active, find_buf)
; Output:  None (side effects: selection set, viewport scrolled, FW_DIRTY)
; Preserves: SI, DI (per menu handler convention)
; Clobbers: AX, BX, CX, DX
; ============================================================================
menu_find_handler PROC
    PUSH    SI
    PUSH    DI
    PUSH    BX
    PUSH    CX

    ; ---- Step 1: Detect selection and pre-fill ----
    XOR     AH, AH                              ; AH = 0: no multi-line selection
    CMP     BYTE [sel_active], 0
    JE      .mfh_show_dlg

    ; Selection active — check if multi-line
    CALL    sel_normalize                        ; BX=s_line, CX=s_col, DX=e_line, SI=e_col
    CMP     BX, DX
    JE      .mfh_single_line_sel

    ; Multi-line selection — save bounds, flag for checkbox
    MOV     [find_sel_s_line], BX
    MOV     [find_sel_s_col], CX
    MOV     [find_sel_e_line], DX
    MOV     [find_sel_e_col], SI
    MOV     AH, 1                               ; AH = 1: multi-line sel available
    JMP     .mfh_show_dlg

.mfh_single_line_sel:
    CALL    _find_prefill_from_sel

.mfh_show_dlg:
    ; ---- Step 2: Show custom Find dialog with checkboxes ----
    CALL    _find_show_dialog                   ; AH=multi-line flag; returns AL=result
    CMP     AL, DLG_OK
    JNE     .mfh_done

    ; ---- Step 3: Compute find_len, guard empty ----
    XOR     CX, CX
    MOV     BX, find_buf
.mfh_len_loop:
    CMP     BYTE [BX], 0
    JE      .mfh_len_done
    INC     BX
    INC     CX
    CMP     CX, FIND_MAX_LEN
    JB      .mfh_len_loop
.mfh_len_done:
    MOV     [find_len], CX
    TEST    CX, CX
    JZ      .mfh_done

    ; ---- Step 3a: Uppercase needle if case-insensitive ----
    CALL    _find_upcase_needle

    ; ---- Step 4: Break undo group ----
    MOV     BYTE [undo_group_active], 0

    ; ---- Step 5: Forward search ----
    TEST    BYTE [find_flags], FIND_FLAG_SEL
    JZ      .mfh_search_normal
    ; Guard: if sel bounds are empty (no selection was saved), clear flag
    MOV     AX, [find_sel_s_line]
    CMP     AX, [find_sel_e_line]
    JNE     .mfh_search_sel
    MOV     AX, [find_sel_s_col]
    CMP     AX, [find_sel_e_col]
    JNE     .mfh_search_sel
    AND     BYTE [find_flags], ~FIND_FLAG_SEL   ; no valid bounds, revert to normal
.mfh_search_normal:

    ; ---- Normal search: from cursor, wrap to BOF ----
    MOV     BX, [cur_line]
    MOV     CX, [cur_col]
    CALL    find_forward
    JNC     .mfh_found

    ; Wrap to BOF
    XOR     BX, BX
    XOR     CX, CX
    CALL    find_forward
    JNC     .mfh_found
    JMP     .mfh_not_found

    ; ---- Selection-only search: from sel_start, wrap within selection ----
.mfh_search_sel:
    MOV     BX, [find_sel_s_line]
    MOV     CX, [find_sel_s_col]
    CALL    find_forward
    JNC     .mfh_found
    ; No wrapping in selection-only mode
    JMP     .mfh_not_found

.mfh_not_found:
    MOV     SI, s_find_notfound
    CALL    _find_select_title                  ; DI = title
    CALL    tui_dlg_msgbox
    JMP     .mfh_done

.mfh_found:
    ; ---- Step 6: Set selection to match ----
    MOV     AX, [find_match_line]
    MOV     [sel_anchor_line], AX
    MOV     AX, [find_match_col]
    MOV     [sel_anchor_col], AX
    MOV     AX, [find_match_end_line]
    MOV     [cur_line], AX
    MOV     AX, [find_match_end_col]
    MOV     [cur_col], AX
    MOV     BYTE [sel_active], 1

    ; ---- Step 7: Scroll into view + redraw ----
    CALL    scroll_to_cursor
    MOV     BYTE [fw_state + FW_DIRTY], 1

.mfh_done:
    POP     CX
    POP     BX
    POP     DI
    POP     SI
    RET
menu_find_handler ENDP

; ============================================================================
; menu_findnxt_handler — Repeat forward search from cursor position (F3)
; Input:   None (reads cur_line, cur_col, find_buf, find_len)
; Output:  None (side effects: selection set, viewport scrolled, FW_DIRTY)
; Preserves: SI, DI (per menu handler convention)
; Clobbers: AX, BX, CX, DX
; ============================================================================
menu_findnxt_handler PROC
    PUSH    SI
    PUSH    DI
    PUSH    BX
    PUSH    CX

    ; ---- Step 1: Guard — no search term ----
    CMP     WORD [find_len], 0
    JE      .fnxt_no_term

    ; ---- Step 1a: Uppercase needle if case-insensitive ----
    CALL    _find_upcase_needle

    ; ---- Step 2: Break undo group ----
    MOV     BYTE [undo_group_active], 0

    ; ---- Step 3: Clear selection ----
    CALL    sel_clear

    ; ---- Step 4: Forward search from cursor ----
    MOV     BX, [cur_line]
    MOV     CX, [cur_col]
    CALL    find_forward
    JNC     .fnxt_found

    ; ---- Step 4a: Wrap ----
    TEST    BYTE [find_flags], FIND_FLAG_SEL
    JNZ     .fnxt_wrap_sel

    ; Normal: wrap to BOF
    XOR     BX, BX
    XOR     CX, CX
    CALL    find_forward
    JNC     .fnxt_found
    JMP     .fnxt_not_found

    ; Selection-only: wrap to selection start
.fnxt_wrap_sel:
    MOV     BX, [find_sel_s_line]
    MOV     CX, [find_sel_s_col]
    CALL    find_forward
    JNC     .fnxt_found

.fnxt_not_found:
    MOV     SI, s_find_notfound
    CALL    _find_select_title
    CALL    tui_dlg_msgbox
    JMP     .fnxt_done

.fnxt_found:
    ; ---- Step 5: Set selection to match ----
    MOV     AX, [find_match_line]
    MOV     [sel_anchor_line], AX
    MOV     AX, [find_match_col]
    MOV     [sel_anchor_col], AX
    MOV     AX, [find_match_end_line]
    MOV     [cur_line], AX
    MOV     AX, [find_match_end_col]
    MOV     [cur_col], AX
    MOV     BYTE [sel_active], 1

    ; ---- Step 6: Scroll into view + redraw ----
    CALL    scroll_to_cursor
    MOV     BYTE [fw_state + FW_DIRTY], 1
    JMP     .fnxt_done

.fnxt_no_term:
    MOV     SI, s_find_noterm
    CALL    _find_select_title
    CALL    tui_dlg_msgbox

.fnxt_done:
    POP     CX
    POP     BX
    POP     DI
    POP     SI
    RET
menu_findnxt_handler ENDP

; ============================================================================
; find_backward — Find last match whose start is strictly before (BX, CX)
; Input:  BX = target line, CX = target col (search before this position)
;         [find_buf] = null-terminated needle
;         [find_len] = needle length
; Output: CF=0 found (find_match_* updated), CF=1 not found
; Preserves: BP, DI (same contract as find_forward)
; Clobbers: AX, BX, CX, DX, SI, ES
; ============================================================================
find_backward PROC
    PUSH    BP
    PUSH    DI
    MOV     DI, BX                  ; DI = target_line (preserved by find_forward)
    MOV     BP, CX                  ; BP = target_col  (preserved by find_forward)

    ; ---- Init prev sentinel ----
    MOV     WORD [find_prev_line], 0FFFFh

    ; ---- Step 1: Search from BOF ----
    XOR     BX, BX
    XOR     CX, CX
    CALL    find_forward
    JC      .fb_notfound            ; no matches at all

    ; ---- Step 2: Scan forward, tracking "previous" match ----
.fb_loop:
    ; Compare find_match_line/col against target (DI/BP)
    MOV     AX, [find_match_line]
    CMP     AX, DI
    JA      .fb_check               ; match line > target line -> at/past
    JB      .fb_save                ; match line < target line -> before
    MOV     AX, [find_match_col]
    CMP     AX, BP
    JAE     .fb_check               ; same line, col >= target -> at/past

.fb_save:
    ; This match is before target — save as candidate
    MOV     AX, [find_match_line]
    MOV     [find_prev_line], AX
    MOV     AX, [find_match_col]
    MOV     [find_prev_col], AX
    MOV     AX, [find_match_end_line]
    MOV     [find_prev_eline], AX
    MOV     AX, [find_match_end_col]
    MOV     [find_prev_ecol], AX

    ; Continue to next match
    MOV     BX, [find_match_end_line]
    MOV     CX, [find_match_end_col]
    CALL    find_forward
    JC      .fb_use_prev            ; EOF -> last saved is the answer
    JMP     .fb_loop

    ; ---- Step 3: Match at/past target ----
.fb_check:
    CMP     WORD [find_prev_line], 0FFFFh
    JNE     .fb_use_prev            ; found a match before target
    ; No match before target -> wrap to last match in file
    JMP     .fb_wrap

    ; ---- Step 4: Return saved previous match ----
.fb_use_prev:
    ; Copy find_prev_* -> find_match_*
    MOV     AX, [find_prev_line]
    MOV     [find_match_line], AX
    MOV     AX, [find_prev_col]
    MOV     [find_match_col], AX
    MOV     AX, [find_prev_eline]
    MOV     [find_match_end_line], AX
    MOV     AX, [find_prev_ecol]
    MOV     [find_match_end_col], AX

    POP     DI
    POP     BP
    CLC
    RET

    ; ---- Step 5: Wrap — find last match in entire file ----
.fb_wrap:
    ; Current match is at/past target and no earlier match exists.
    ; Save current match as first candidate, then scan to EOF.
    MOV     AX, [find_match_line]
    MOV     [find_prev_line], AX
    MOV     AX, [find_match_col]
    MOV     [find_prev_col], AX
    MOV     AX, [find_match_end_line]
    MOV     [find_prev_eline], AX
    MOV     AX, [find_match_end_col]
    MOV     [find_prev_ecol], AX

.fb_wrap_loop:
    MOV     BX, [find_match_end_line]
    MOV     CX, [find_match_end_col]
    CALL    find_forward
    JC      .fb_use_prev            ; no more -> find_prev is last match

    ; Save newer match, keep scanning
    MOV     AX, [find_match_line]
    MOV     [find_prev_line], AX
    MOV     AX, [find_match_col]
    MOV     [find_prev_col], AX
    MOV     AX, [find_match_end_line]
    MOV     [find_prev_eline], AX
    MOV     AX, [find_match_end_col]
    MOV     [find_prev_ecol], AX
    JMP     .fb_wrap_loop

    ; ---- Not found (no matches at all) ----
.fb_notfound:
    POP     DI
    POP     BP
    STC
    RET
find_backward ENDP

; ============================================================================
; menu_findprv_handler — Repeat backward search (Shift+F3 / Edit > Find Prev)
; Input:   None (reads cur_line, cur_col, sel_active, sel_anchor_*, find_buf, find_len)
; Output:  None (side effects: selection set, viewport scrolled, FW_DIRTY)
; Preserves: SI, DI (per menu handler convention)
; Clobbers: AX, BX, CX, DX
; ============================================================================
menu_findprv_handler PROC
    PUSH    SI
    PUSH    DI
    PUSH    BX
    PUSH    CX

    ; ---- Step 1: Guard — no search term ----
    CMP     WORD [find_len], 0
    JE      .fprv_no_term

    ; ---- Step 1a: Uppercase needle if case-insensitive ----
    CALL    _find_upcase_needle

    ; ---- Step 2: Break undo group ----
    MOV     BYTE [undo_group_active], 0

    ; ---- Step 3: Determine target (BEFORE clearing selection) ----
    CMP     BYTE [sel_active], 0
    JE      .fprv_from_cursor
    ; Selection active -> target = anchor (match start from previous find)
    MOV     BX, [sel_anchor_line]
    MOV     CX, [sel_anchor_col]
    JMP     .fprv_search
.fprv_from_cursor:
    ; No selection -> target = cursor position
    MOV     BX, [cur_line]
    MOV     CX, [cur_col]

.fprv_search:
    ; ---- Step 4: Clear selection ----
    ; BX/CX already loaded, sel_clear preserves all registers
    CALL    sel_clear

    ; ---- Step 5: Backward search ----
    CALL    find_backward           ; BX=target_line, CX=target_col
    JNC     .fprv_found

    ; ---- Step 5a: Not found ----
    MOV     SI, s_find_notfound
    CALL    _find_select_title
    CALL    tui_dlg_msgbox
    JMP     .fprv_done

.fprv_found:
    ; ---- Step 6: Set selection to match ----
    MOV     AX, [find_match_line]
    MOV     [sel_anchor_line], AX
    MOV     AX, [find_match_col]
    MOV     [sel_anchor_col], AX
    MOV     AX, [find_match_end_line]
    MOV     [cur_line], AX
    MOV     AX, [find_match_end_col]
    MOV     [cur_col], AX
    MOV     BYTE [sel_active], 1

    ; ---- Step 7: Scroll into view + redraw ----
    CALL    scroll_to_cursor
    MOV     BYTE [fw_state + FW_DIRTY], 1
    JMP     .fprv_done

.fprv_no_term:
    MOV     SI, s_find_noterm
    CALL    _find_select_title
    CALL    tui_dlg_msgbox

.fprv_done:
    POP     CX
    POP     BX
    POP     DI
    POP     SI
    RET
menu_findprv_handler ENDP

; ============================================================================
; _find_upcase_needle — Uppercase find_buf in-place if case-insensitive
; Input:   [find_flags] bit 0, [find_len], [find_buf]
; Output:  find_buf uppercased (if CI mode)
; Clobbers: AX, BX, CX
; ============================================================================
_find_upcase_needle PROC
    TEST    BYTE [find_flags], 01h
    JZ      .done
    MOV     BX, find_buf
    MOV     CX, [find_len]
    JCXZ    .done
.loop:
    MOV     AL, [BX]
    CMP     AL, 'a'
    JB      .next
    CMP     AL, 'z'
    JA      .next
    SUB     AL, 20h
    MOV     [BX], AL
.next:
    INC     BX
    LOOP    .loop
.done:
    RET
_find_upcase_needle ENDP

; ============================================================================
; menu_case_toggle — Toggle case-insensitive search flag
; Input:   None
; Output:  [find_flags] bit 0 toggled
; ============================================================================
menu_case_toggle PROC
    XOR     BYTE [find_flags], 01h
    RET
menu_case_toggle ENDP

; ============================================================================
; _find_select_title — Choose Find dialog title based on find_flags
; Output: DI = title string pointer
; Clobbers: none (only DI)
; ============================================================================
_find_select_title PROC
    TEST    BYTE [find_flags], FIND_FLAG_SEL
    JNZ     .sel
    TEST    BYTE [find_flags], FIND_FLAG_CI
    JNZ     .ci
    MOV     DI, s_find_title                ; "Find"
    RET
.ci:
    MOV     DI, s_find_title_ci             ; "Find [Aa]"
    RET
.sel:
    TEST    BYTE [find_flags], FIND_FLAG_CI
    JNZ     .sel_ci
    MOV     DI, s_find_title_sel            ; "Find in Selection"
    RET
.sel_ci:
    MOV     DI, s_find_title_sel_ci         ; "Find in Selection [Aa]"
    RET
_find_select_title ENDP

; ============================================================================
; _find_count_matches — Count total matches in document (or selection)
;
; Input:  [find_buf], [find_len], [find_flags] all set
; Output: AX = match count (0-65535)
; Preserves: BP, DI
; Clobbers: AX, BX, CX, DX, SI, ES
; ============================================================================
_find_count_matches PROC
    PUSH    BP
    PUSH    DI
    XOR     DI, DI                  ; DI = count

    ; Start position: sel_start if selection-only, else BOF
    TEST    BYTE [find_flags], FIND_FLAG_SEL
    JNZ     .fcm_from_sel
    XOR     BX, BX
    XOR     CX, CX
    JMP     .fcm_loop
.fcm_from_sel:
    MOV     BX, [find_sel_s_line]
    MOV     CX, [find_sel_s_col]

.fcm_loop:
    CALL    find_forward
    JC      .fcm_done
    INC     DI
    MOV     BX, [find_match_end_line]
    MOV     CX, [find_match_end_col]
    JMP     .fcm_loop

.fcm_done:
    MOV     AX, DI
    POP     DI
    POP     BP
    RET
_find_count_matches ENDP

; ============================================================================
; _find_prefill_from_sel — Copy single-line selection into find_buf
; Input:   [sel_active], [sel_anchor_line/col], [cur_line/col]
; Output:  find_buf populated + null-terminated (if eligible), else unchanged
; Clobbers: AX, BX, CX, DX, SI
; ============================================================================
_find_prefill_from_sel PROC
    CMP     BYTE [sel_active], 0
    JE      .pfr_done
    PUSH    ES

    ; ---- Normalize: AX=start_line, BX=start_col, DX=end_line, CX=end_col ----
    MOV     AX, [sel_anchor_line]
    MOV     DX, [cur_line]
    MOV     BX, [sel_anchor_col]
    MOV     CX, [cur_col]
    CMP     AX, DX
    JB      .pfr_ordered
    JA      .pfr_swap
    CMP     BX, CX
    JBE     .pfr_ordered
.pfr_swap:
    XCHG    AX, DX
    XCHG    BX, CX
.pfr_ordered:
    ; Single-line?
    CMP     AX, DX
    JNE     .pfr_pop_es

    ; Length = end_col - start_col
    SUB     CX, BX
    JZ      .pfr_pop_es
    CMP     CX, FIND_MAX_LEN
    JA      .pfr_pop_es

    ; ---- Get byte offset for (start_line, start_col) ----
    PUSH    CX                          ; save length
    MOV     CX, BX                      ; CX = start_col
    MOV     BX, AX                      ; BX = start_line
    CALL    line_col_to_offset          ; DX:AX = byte offset

    ; ---- Resolve byte offset to piece/offset ----
    MOV     WORD [rnd_pi], 0
.pfr_find_piece:
    MOV     BX, [rnd_pi]
    CMP     BX, [pt_count]
    JAE     .pfr_pop_done
    PUSH    DX
    PUSH    AX
    CALL    resolve_piece               ; AX=seg, SI=base, DX=len
    MOV     CX, DX
    POP     AX
    POP     DX
    JC      .pfr_pop_done
    TEST    DX, DX
    JNZ     .pfr_skip
    CMP     AX, CX
    JB      .pfr_piece_found
.pfr_skip:
    SUB     AX, CX
    SBB     DX, 0
    INC     WORD [rnd_pi]
    JMP     .pfr_find_piece

.pfr_piece_found:
    MOV     [rnd_off], AX
    POP     CX                          ; CX = bytes to copy
    MOV     DI, find_buf

    ; ---- Piece-walking copy loop ----
.pfr_piece_loop:
    TEST    CX, CX
    JZ      .pfr_null_term

    MOV     BX, [rnd_pi]
    CMP     BX, [pt_count]
    JAE     .pfr_null_term

    PUSH    CX
    CALL    resolve_piece               ; AX=seg, SI=base, DX=len
    POP     CX
    JC      .pfr_null_term

    ADD     SI, [rnd_off]
    SUB     DX, [rnd_off]

    ; Copy min(DX, CX) bytes
    PUSH    DS
    MOV     DS, AX
    CMP     DX, CX
    JBE     .pfr_use_dx
    MOV     DX, CX
.pfr_use_dx:
    SUB     CX, DX
.pfr_byte:
    LODSB
    MOV     CS:[DI], AL
    INC     DI
    DEC     DX
    JNZ     .pfr_byte

    POP     DS
    INC     WORD [rnd_pi]
    MOV     WORD [rnd_off], 0
    JMP     .pfr_piece_loop

.pfr_null_term:
    MOV     BYTE [DI], 0
.pfr_pop_es:
    POP     ES
.pfr_done:
    RET

.pfr_pop_done:
    POP     CX
    JMP     .pfr_pop_es
_find_prefill_from_sel ENDP

; ============================================================================
; _repl_save_replaced — Stream piece table to file with replacements applied
;
; Walks the piece table byte-by-byte, scanning for the search needle.
; Non-matching bytes are written to the output file; matches are replaced
; with the replacement text. Single O(n) pass — no rebuild_meta, no undo,
; no piece table manipulation.
;
; Input:  [filename] = output file path
;         [find_buf], [find_len] = search needle (pre-uppercased if CI)
;         [replace_buf], [replace_len] = replacement text
;         [find_flags] bit 0 = case-insensitive
; Output: CF=0 success, CF=1 failure.  [replace_count] updated.
; Clobbers: AX, BX, CX, DX, SI, DI, BP, ES
; ============================================================================

_repl_save_replaced PROC
    ; ---- Create/truncate output file ----
    MOV     DX, filename
    XOR     CX, CX
    MOV     AH, 3Ch
    INT     21h
    JC      .rsr_fail
    MOV     [file_hnd], AX

    MOV     WORD [find_needle_pos], 0

    ; ---- Set up write buffer: ES = add_seg, DI = 0 ----
    MOV     ES, [add_seg]
    XOR     DI, DI                      ; write position in buffer

    ; ---- Walk pieces ----
    XOR     BP, BP                      ; BP = piece index
.rsr_piece_loop:
    CMP     BP, [pt_count]
    JAE     .rsr_done_pieces

    PUSH    DI                          ; save write pos (resolve_piece clobbers DI)
    MOV     BX, BP
    CALL    resolve_piece               ; AX=seg, SI=base, DX=piece_len
    POP     DI
    JC      .rsr_done_pieces

    PUSH    DS
    MOV     DS, AX
    ADD     DX, SI                      ; DX = piece end (absolute)

    ; ---- Byte loop within this piece ----
.rsr_byte:
    CMP     SI, DX
    JAE     .rsr_next_piece
    LODSB                               ; AL = byte, SI++

    ; ---- Compare against needle ----
    MOV     BL, AL                      ; BL = original byte (safe across compare)
    TEST    BYTE CS:[find_flags], FIND_FLAG_CI
    JZ      .rsr_cmp
    CMP     AL, 'a'
    JB      .rsr_cmp
    CMP     AL, 'z'
    JA      .rsr_cmp
    SUB     AL, 20h                     ; uppercase for comparison
.rsr_cmp:
    PUSH    BX
    MOV     BX, CS:[find_needle_pos]
    CMP     AL, CS:[BX + find_buf]
    POP     BX
    MOV     AL, BL                      ; restore original byte
    JNE     .rsr_mismatch

    ; ---- Byte matched: save original in rd_save_buf (partial buffer) ----
    PUSH    BX
    MOV     BX, CS:[find_needle_pos]
    MOV     CS:[BX + rd_save_buf], AL
    INC     BX
    MOV     CS:[find_needle_pos], BX
    CMP     BX, CS:[find_len]
    POP     BX
    JB      .rsr_byte                   ; partial match, continue

    ; ---- Full match: write replacement text ----
    PUSH    SI
    PUSH    DX
    PUSH    DS
    PUSH    CS
    POP     DS
    MOV     CX, [replace_len]
    JCXZ    .rsr_match_done
    MOV     SI, replace_buf
.rsr_write_repl:
    LODSB
    CALL    .rsr_write_byte
    LOOP    .rsr_write_repl
.rsr_match_done:
    INC     WORD [replace_count]
    MOV     WORD [find_needle_pos], 0
    POP     DS
    POP     DX
    POP     SI
    JMP     .rsr_byte

    ; ---- Mismatch ----
.rsr_mismatch:
    PUSH    BX
    MOV     BX, CS:[find_needle_pos]
    TEST    BX, BX
    POP     BX
    JZ      .rsr_write_plain

    ; Flush partial match bytes from rd_save_buf
    PUSH    AX                          ; save current byte for re-processing
    PUSH    SI
    PUSH    DX
    PUSH    DS
    PUSH    CS
    POP     DS
    MOV     CX, [find_needle_pos]
    MOV     SI, rd_save_buf
.rsr_flush:
    LODSB
    CALL    .rsr_write_byte
    LOOP    .rsr_flush
    MOV     WORD [find_needle_pos], 0
    POP     DS
    POP     DX
    POP     SI
    POP     AX                          ; AL = current (mismatching) byte

    ; Re-compare current byte against needle[0]
    MOV     BL, AL
    TEST    BYTE CS:[find_flags], FIND_FLAG_CI
    JZ      .rsr_re_cmp
    CMP     AL, 'a'
    JB      .rsr_re_cmp
    CMP     AL, 'z'
    JA      .rsr_re_cmp
    SUB     AL, 20h
.rsr_re_cmp:
    CMP     AL, CS:[find_buf]          ; needle[0]
    MOV     AL, BL                      ; restore original
    JNE     .rsr_write_plain

    ; Matches needle[0] → start new partial
    MOV     CS:[rd_save_buf], AL
    MOV     WORD CS:[find_needle_pos], 1
    JMP     .rsr_byte

.rsr_write_plain:
    CALL    .rsr_write_byte
    JMP     .rsr_byte

    ; ---- Next piece ----
.rsr_next_piece:
    POP     DS
    INC     BP
    JMP     .rsr_piece_loop

    ; ---- Done: flush remaining partial match + write buffer ----
.rsr_done_pieces:
    ; Flush any pending partial match (no match at EOF)
    MOV     CX, [find_needle_pos]
    JCXZ    .rsr_flush_buf
    MOV     SI, rd_save_buf
.rsr_final_partial:
    LODSB
    CALL    .rsr_write_byte
    LOOP    .rsr_final_partial

.rsr_flush_buf:
    ; Flush remaining bytes in write buffer
    TEST    DI, DI
    JZ      .rsr_close
    PUSH    DS
    MOV     DS, [add_seg]
    XOR     DX, DX
    MOV     CX, DI
    MOV     BX, CS:[file_hnd]
    MOV     AH, 40h
    INT     21h
    POP     DS

.rsr_close:
    MOV     BX, [file_hnd]
    MOV     AH, 3Eh
    INT     21h
    MOV     BYTE [dirty], 0
    CLC
    RET

.rsr_fail:
    STC
    RET

    ; ---- Internal: write AL to ES:DI buffer, flush when full ----
.rsr_write_byte:
    STOSB                               ; ES:[DI++] = AL
    CMP     DI, 16384
    JB      .rsr_wb_ret
    ; Flush 16KB buffer to file
    PUSH    AX
    PUSH    BX
    PUSH    CX
    PUSH    DX
    PUSH    DS
    MOV     DS, CS:[add_seg]
    XOR     DX, DX
    MOV     CX, DI
    MOV     BX, CS:[file_hnd]
    MOV     AH, 40h
    INT     21h
    POP     DS
    XOR     DI, DI                      ; reset write position
    POP     DX
    POP     CX
    POP     BX
    POP     AX
.rsr_wb_ret:
    RET

_repl_save_replaced ENDP

; ============================================================================
; _repl_format_count — Format "N replacements made." into repl_msg_buf
; Input:   [replace_count] = number
; Output:  repl_msg_buf filled with ASCIIZ string
; Clobbers: AX, BX, CX, DX, SI, DI
; ============================================================================
_repl_format_count PROC
    MOV     DI, repl_msg_buf
    MOV     AX, [replace_count]

    ; Simple 16-bit itoa: divide by 10, push remainders, pop in order
    XOR     CX, CX                      ; digit count
    MOV     BX, 10
.rfc_div:
    XOR     DX, DX
    DIV     BX                          ; AX = quotient, DX = remainder
    PUSH    DX                          ; save digit
    INC     CX
    TEST    AX, AX
    JNZ     .rfc_div

    ; Pop digits in reverse (most significant first)
.rfc_pop:
    POP     AX
    ADD     AL, '0'
    MOV     [DI], AL
    INC     DI
    LOOP    .rfc_pop

    ; Append " replacements made.\0"
    MOV     SI, s_repl_done_sfx
.rfc_copy:
    LODSB
    MOV     [DI], AL
    INC     DI
    TEST    AL, AL
    JNZ     .rfc_copy

    RET
_repl_format_count ENDP

; ============================================================================
; _repl_insert — Insert replace_buf at cursor position (bulk insert helper)
;
; Follows the ed_paste pattern exactly but sources from BSS (replace_buf)
; instead of clip_seg. DS=CS throughout (no segment switching for source).
;
; Input:   [replace_buf] = text to insert, [replace_len] = byte count (>0)
;          [cur_line], [cur_col] = insertion point
; Output:  CF=0 success (cursor advanced, undo pushed, dirty set)
;          CF=1 failure (add buffer full / piece table full)
; Clobbers: AX, BX, CX, DX, SI, DI, BP, ES
; ============================================================================
_repl_insert PROC
    ; ---- Stage 1: Check add buffer capacity ----
    MOV     AX, [add_used]
    ADD     AX, [replace_len]
    JNC     .ri_have_space

    ; Overflow — try compact
    CALL    ed_buffer_full
    JC      .ri_fail                    ; user declined or compact failed
    ; After compact, re-check
    MOV     AX, [add_used]
    ADD     AX, [replace_len]
    JC      .ri_fail                    ; still doesn't fit

.ri_have_space:
    ; ---- Stage 2: Snapshot add_used ----
    MOV     AX, [add_used]
    MOV     [ep_save_add], AX           ; reuse existing BSS variable

    ; ---- Stage 3: Copy replace_buf → add buffer ----
    ; Save pre-insert cursor position for undo
    PUSH    WORD [cur_line]
    PUSH    WORD [cur_col]

    MOV     ES, [add_seg]
    MOV     DI, [add_used]              ; ES:DI = destination in add buffer
    MOV     SI, replace_buf             ; DS:SI = source (BSS, DS=CS)
    MOV     CX, [replace_len]
    CLD
    REP     MOVSB

    ; Update add_used
    MOV     AX, [replace_len]
    ADD     [add_used], AX

    ; ---- Stage 4: Find insertion point ----
    CALL    cursor_to_offset            ; sets rnd_pi, rnd_off

    MOV     BX, [rnd_pi]
    MOV     SI, [rnd_off]              ; SI = offset within piece

    ; Guard: if cursor past all pieces, treat as insert-before
    CMP     BX, [pt_count]
    JAE     .ri_insert_before

    ; ---- Read piece length from pt_seg ----
    PUSH    DS
    MOV     AX, [pt_seg]
    MOV     DS, AX
    ; Compute DI = BX * 10 (PT_DESC_SIZE)
    MOV     DI, BX
    SHL     DI, 1
    MOV     AX, DI
    SHL     DI, 1
    SHL     DI, 1
    ADD     DI, AX                      ; DI = BX * 10
    MOV     DX, [DI + PT_F_LENGTH]      ; DX = piece length (low word)
    POP     DS

    ; ---- Determine insertion case ----
    TEST    SI, SI
    JZ      .ri_insert_before           ; cursor at piece start
    CMP     SI, DX
    JAE     .ri_insert_after            ; cursor at or past piece end
    JMP     .ri_split                   ; cursor mid-piece

    ; ================================================================
    ; CASE A: Insert new piece before rnd_pi
    ; ================================================================
.ri_insert_before:
    MOV     BYTE [meta_dirty], 1
    CALL    pt_insert                   ; insert empty slot at BX
    JC      .ri_pt_fail
    JMP     .ri_fill_piece

    ; ================================================================
    ; CASE B: Insert new piece after rnd_pi
    ; ================================================================
.ri_insert_after:
    INC     BX                          ; insert at BX+1
    CALL    pt_insert
    JC      .ri_pt_fail
    JMP     .ri_fill_piece

    ; ================================================================
    ; CASE C: Split piece at rnd_off, insert between halves
    ; ================================================================
.ri_split:
    ; split_piece_at: BX=piece_index, SI=split_offset
    ; Clobbers: AX, BX, CX, DX, SI, ES. Preserves: BP, DI.
    PUSH    ES
    CALL    split_piece_at
    POP     ES

    INC     BX                          ; insert after left half
    CALL    pt_insert
    JC      .ri_pt_fail
    MOV     BYTE [meta_dirty], 1

    ; ================================================================
    ; Fill new piece descriptor at slot BX
    ; ================================================================
.ri_fill_piece:
    PUSH    ES
    MOV     AX, [pt_seg]
    MOV     ES, AX
    ; Compute DI = BX * 10
    MOV     DI, BX
    SHL     DI, 1
    MOV     AX, DI
    SHL     DI, 1
    SHL     DI, 1
    ADD     DI, AX

    MOV     BYTE ES:[DI + PT_F_SOURCE], 1       ; source = add buffer
    MOV     BYTE ES:[DI + PT_F_SOURCE + 1], 0
    MOV     AX, [ep_save_add]
    MOV     ES:[DI + PT_F_START], AX            ; start offset (low word)
    MOV     WORD ES:[DI + PT_F_START + 2], 0    ; start offset (high word)
    MOV     AX, [replace_len]
    MOV     ES:[DI + PT_F_LENGTH], AX           ; byte count (low word)
    MOV     WORD ES:[DI + PT_F_LENGTH + 2], 0   ; byte count (high word)
    POP     ES

    ; ---- Stage 5: Advance cursor ----
    ; Scan replace_buf for LF bytes to compute line/col delta
    MOV     SI, replace_buf
    MOV     CX, [replace_len]
    XOR     DX, DX                      ; DX = LF count
    XOR     BX, BX                      ; BX = col after last LF

.ri_scan:
    TEST    CX, CX
    JZ      .ri_scan_done
    LODSB
    DEC     CX
    CMP     AL, 0Ah                     ; LF?
    JE      .ri_scan_lf
    CMP     AL, 0Dh                     ; CR? skip, don't count as column
    JE      .ri_scan
    INC     BX                          ; visible char: col++
    JMP     .ri_scan

.ri_scan_lf:
    INC     DX                          ; LF count++
    XOR     BX, BX                      ; reset col counter
    JMP     .ri_scan

.ri_scan_done:
    ; DX = number of LFs, BX = column width of last line
    TEST    DX, DX
    JZ      .ri_same_line

    ; Multi-line replacement
    ADD     [cur_line], DX
    MOV     [cur_col], BX
    JMP     .ri_push_undo

.ri_same_line:
    ; Single-line replacement: just advance column
    ADD     [cur_col], BX

.ri_push_undo:
    ; ---- Stage 6: Push UR_INS_RANGE undo record ----
    ; Stack has: [SP] = pre-insert cur_col, [SP+2] = pre-insert cur_line
    POP     CX                          ; CX = pre-insert cur_col
    POP     BX                          ; BX = pre-insert cur_line

    MOV     AL, UR_INS_RANGE
    XOR     AH, AH
    ; undo_push: AL=type, AH=char, BX=doc_line, CX=doc_col, DX=cur_line, SI=cur_col
    ; For UR_INS_RANGE: DX (UR_CUR_LINE) repurposed for byte count
    MOV     DX, [replace_len]           ; byte count → UR_CUR_LINE
    XOR     SI, SI                      ; 0 → UR_CUR_COL
    CALL    undo_push

    ; ---- Stage 7: Set dirty flags ----
    MOV     BYTE [dirty], 1
    MOV     BYTE [meta_dirty], 1
    MOV     WORD [last_ins_pi], 0FFFFh

    CLC
    RET

.ri_pt_fail:
    ; Piece table full — rollback add_used
    MOV     AX, [ep_save_add]
    MOV     [add_used], AX
    ; Clean up saved cursor from stack
    POP     CX
    POP     BX
.ri_fail:
    STC
    RET
_repl_insert ENDP

; ============================================================================
; _repl_show_dialog — Custom Replace dialog with 2 fields + 3 checkboxes
;
; Layout (interior 9 rows, WIN_H=11):
;   Row 0: "Find:" label
;   Row 1: [textbox 1]
;   Row 2: "Replace with:" label
;   Row 3: [textbox 2]
;   Row 4: [X] Match case
;   Row 5: [ ] In selection
;   Row 6: [ ] Replace all
;   Row 7: (separator)
;   Row 8: [ OK ]  [ Cancel ]
;
; Input:  AH = 1 if multi-line selection available, 0 if not
; Output: AL = DLG_OK or DLG_CANCEL
;         find_flags updated from checkboxes (Match case, In selection)
;         find_buf, replace_buf contain entered text (if DLG_OK)
;         dlg_chk3 + CTRL_CB_STATE = 1 if "Replace all" checked
; Clobbers: AX, BX, CX, DX, SI, DI (all saved by PushAll)
; ============================================================================
_repl_show_dialog PROC
    PUSH    AX                      ; save AH (multi-line sel flag)
    PushAll

    MOV     BL, 28                  ; interior width
    MOV     BH, 30                  ; total width

    ; Center on screen (height=11)
    MOV     AL, SCR_W
    SUB     AL, BH
    SHR     AL, 1
    MOV     DL, AL                  ; DL = dialog X
    MOV     DH, (SCR_H - 11) / 2   ; DH = dialog Y

    ; ---- Label 1: "Find:" at row 0 ----
    MOV     DI, dlg_label
    MOV     BYTE [DI + CTRL_TYPE], CTYPE_LABEL
    MOV     BYTE [DI + CTRL_FLAGS], CTRLF_VISIBLE
    MOV     BYTE [DI + CTRL_X], 1
    MOV     BYTE [DI + CTRL_Y], 0
    MOV     BYTE [DI + CTRL_W], 5
    MOV     BYTE [DI + CTRL_H], 1
    MOV     WORD [DI + CTRL_NEXT], dlg_textbox
    MOV     WORD [DI + CTRL_TEXT], s_find_prompt
    MOV     BYTE [DI + CTRL_ATTR], CLR_LABEL
    MOV     BYTE [DI + CTRL_FATTR], CLR_LABEL
    MOV     WORD [DI + CTRL_HANDLER], 0

    ; ---- TextBox 1 at row 1 ----
    MOV     DI, dlg_textbox
    MOV     BYTE [DI + CTRL_TYPE], CTYPE_TEXTBOX
    MOV     BYTE [DI + CTRL_FLAGS], CTRLF_VISIBLE + CTRLF_ENABLED + CTRLF_FOCUSABLE
    MOV     BYTE [DI + CTRL_X], 1
    MOV     BYTE [DI + CTRL_Y], 1
    MOV     BYTE [DI + CTRL_W], 26
    MOV     BYTE [DI + CTRL_H], 1
    MOV     WORD [DI + CTRL_NEXT], dlg_label2
    MOV     WORD [DI + CTRL_TEXT], find_buf
    MOV     BYTE [DI + CTRL_ATTR], CLR_TEXTBOX
    MOV     BYTE [DI + CTRL_FATTR], CLR_TB_FOCUS
    MOV     WORD [DI + CTRL_HANDLER], 0
    MOV     BYTE [DI + CTRL_TB_MAXLEN], FIND_MAX_LEN
    MOV     BYTE [DI + CTRL_TB_CURPOS], 0
    MOV     BYTE [DI + CTRL_TB_SCROLL], 0
    PUSH    SI
    PUSH    CX
    MOV     SI, find_buf
    XOR     CX, CX
.rsd_len1:
    CMP     BYTE [SI], 0
    JZ      .rsd_len1_done
    INC     SI
    INC     CL
    JMP     .rsd_len1
.rsd_len1_done:
    MOV     [DI + CTRL_TB_LEN], CL
    MOV     [DI + CTRL_TB_CURPOS], CL
    POP     CX
    POP     SI

    ; ---- Label 2: "Replace with:" at row 2 ----
    MOV     DI, dlg_label2
    MOV     BYTE [DI + CTRL_TYPE], CTYPE_LABEL
    MOV     BYTE [DI + CTRL_FLAGS], CTRLF_VISIBLE
    MOV     BYTE [DI + CTRL_X], 1
    MOV     BYTE [DI + CTRL_Y], 2
    MOV     BYTE [DI + CTRL_W], 13
    MOV     BYTE [DI + CTRL_H], 1
    MOV     WORD [DI + CTRL_NEXT], dlg_textbox2
    MOV     WORD [DI + CTRL_TEXT], s_repl_prompt
    MOV     BYTE [DI + CTRL_ATTR], CLR_LABEL
    MOV     BYTE [DI + CTRL_FATTR], CLR_LABEL
    MOV     WORD [DI + CTRL_HANDLER], 0

    ; ---- TextBox 2 at row 3 ----
    MOV     DI, dlg_textbox2
    MOV     BYTE [DI + CTRL_TYPE], CTYPE_TEXTBOX
    MOV     BYTE [DI + CTRL_FLAGS], CTRLF_VISIBLE + CTRLF_ENABLED + CTRLF_FOCUSABLE
    MOV     BYTE [DI + CTRL_X], 1
    MOV     BYTE [DI + CTRL_Y], 3
    MOV     BYTE [DI + CTRL_W], 26
    MOV     BYTE [DI + CTRL_H], 1
    MOV     WORD [DI + CTRL_NEXT], dlg_chk1
    MOV     WORD [DI + CTRL_TEXT], replace_buf
    MOV     BYTE [DI + CTRL_ATTR], CLR_TEXTBOX
    MOV     BYTE [DI + CTRL_FATTR], CLR_TB_FOCUS
    MOV     WORD [DI + CTRL_HANDLER], 0
    MOV     BYTE [DI + CTRL_TB_MAXLEN], FIND_MAX_LEN
    MOV     BYTE [DI + CTRL_TB_CURPOS], 0
    MOV     BYTE [DI + CTRL_TB_SCROLL], 0
    PUSH    SI
    PUSH    CX
    MOV     SI, replace_buf
    XOR     CX, CX
.rsd_len2:
    CMP     BYTE [SI], 0
    JZ      .rsd_len2_done
    INC     SI
    INC     CL
    JMP     .rsd_len2
.rsd_len2_done:
    MOV     [DI + CTRL_TB_LEN], CL
    MOV     [DI + CTRL_TB_CURPOS], CL
    POP     CX
    POP     SI

    ; ---- Checkbox 1: "Match case" at row 4 ----
    MOV     DI, dlg_chk1
    MOV     BYTE [DI + CTRL_TYPE], CTYPE_CHECKBOX
    MOV     BYTE [DI + CTRL_FLAGS], CTRLF_VISIBLE + CTRLF_ENABLED + CTRLF_FOCUSABLE
    MOV     BYTE [DI + CTRL_X], 1
    MOV     BYTE [DI + CTRL_Y], 4
    MOV     BYTE [DI + CTRL_W], 14
    MOV     BYTE [DI + CTRL_H], 1
    MOV     WORD [DI + CTRL_NEXT], dlg_chk2
    MOV     WORD [DI + CTRL_TEXT], s_find_matchcase
    MOV     BYTE [DI + CTRL_ATTR], CLR_CHECKBOX
    MOV     BYTE [DI + CTRL_FATTR], CLR_CB_FOCUS
    MOV     WORD [DI + CTRL_HANDLER], 0
    MOV     AL, [find_flags]
    AND     AL, FIND_FLAG_CI
    XOR     AL, 1                           ; invert: CI=1 → unchecked, CI=0 → checked
    MOV     [DI + CTRL_CB_STATE], AL

    ; ---- Checkbox 2: "In selection" at row 5 ----
    MOV     DI, dlg_chk2
    MOV     BYTE [DI + CTRL_TYPE], CTYPE_CHECKBOX
    MOV     BYTE [DI + CTRL_FLAGS], CTRLF_VISIBLE + CTRLF_ENABLED + CTRLF_FOCUSABLE
    MOV     BYTE [DI + CTRL_X], 1
    MOV     BYTE [DI + CTRL_Y], 5
    MOV     BYTE [DI + CTRL_W], 16
    MOV     BYTE [DI + CTRL_H], 1
    MOV     WORD [DI + CTRL_NEXT], dlg_chk3
    MOV     WORD [DI + CTRL_TEXT], s_find_insel
    MOV     BYTE [DI + CTRL_ATTR], CLR_CHECKBOX
    MOV     BYTE [DI + CTRL_FATTR], CLR_CB_FOCUS
    MOV     WORD [DI + CTRL_HANDLER], 0

    ; Check multi-line sel flag from stack
    MOV     BP, SP
    MOV     AX, [BP + 12]                  ; original AX from PUSH AX
    TEST    AH, AH
    JZ      .rsd_no_sel
    MOV     BYTE [DI + CTRL_CB_STATE], 1   ; pre-checked
    JMP     .rsd_chk3
.rsd_no_sel:
    MOV     BYTE [DI + CTRL_CB_STATE], 0

.rsd_chk3:
    ; ---- Checkbox 3: "Replace all" at row 6 ----
    MOV     DI, dlg_chk3
    MOV     BYTE [DI + CTRL_TYPE], CTYPE_CHECKBOX
    MOV     BYTE [DI + CTRL_FLAGS], CTRLF_VISIBLE + CTRLF_ENABLED + CTRLF_FOCUSABLE
    MOV     BYTE [DI + CTRL_X], 1
    MOV     BYTE [DI + CTRL_Y], 6
    MOV     BYTE [DI + CTRL_W], 15
    MOV     BYTE [DI + CTRL_H], 1
    MOV     WORD [DI + CTRL_NEXT], dlg_btn1
    MOV     WORD [DI + CTRL_TEXT], s_repl_all
    MOV     BYTE [DI + CTRL_ATTR], CLR_CHECKBOX
    MOV     BYTE [DI + CTRL_FATTR], CLR_CB_FOCUS
    MOV     WORD [DI + CTRL_HANDLER], 0
    MOV     BYTE [DI + CTRL_CB_STATE], 0   ; unchecked by default

    ; ---- OK button at row 8, centered ----
    MOV     CH, 4                           ; ok_x = (28 - 20) / 2 = 4
    MOV     DI, dlg_btn1
    MOV     BYTE [DI + CTRL_TYPE], CTYPE_BUTTON
    MOV     BYTE [DI + CTRL_FLAGS], CTRLF_VISIBLE + CTRLF_ENABLED + CTRLF_FOCUSABLE
    MOV     [DI + CTRL_X], CH
    MOV     BYTE [DI + CTRL_Y], 8
    MOV     BYTE [DI + CTRL_W], DLG_BTN_OK_W
    MOV     BYTE [DI + CTRL_H], 1
    MOV     WORD [DI + CTRL_NEXT], dlg_btn2
    MOV     WORD [DI + CTRL_TEXT], dlg_str_ok
    MOV     BYTE [DI + CTRL_ATTR], CLR_BTN
    MOV     BYTE [DI + CTRL_FATTR], CLR_BTN_FOCUS
    MOV     WORD [DI + CTRL_HANDLER], dlg_handler_ok

    ; ---- Cancel button ----
    MOV     AL, CH
    ADD     AL, DLG_BTN_OK_W + DLG_BTN_GAP
    MOV     DI, dlg_btn2
    MOV     BYTE [DI + CTRL_TYPE], CTYPE_BUTTON
    MOV     BYTE [DI + CTRL_FLAGS], CTRLF_VISIBLE + CTRLF_ENABLED + CTRLF_FOCUSABLE
    MOV     [DI + CTRL_X], AL
    MOV     BYTE [DI + CTRL_Y], 8
    MOV     BYTE [DI + CTRL_W], DLG_BTN_CAN_W
    MOV     BYTE [DI + CTRL_H], 1
    MOV     WORD [DI + CTRL_NEXT], 0
    MOV     WORD [DI + CTRL_TEXT], dlg_str_cancel
    MOV     BYTE [DI + CTRL_ATTR], CLR_BTN
    MOV     BYTE [DI + CTRL_FATTR], CLR_BTN_FOCUS
    MOV     WORD [DI + CTRL_HANDLER], dlg_handler_cancel

    ; ---- Window template ----
    MOV     DI, dlg_tmpl
    MOV     BYTE [DI + WIN_FLAGS], WINF_VISIBLE + WINF_BORDER + WINF_TITLE + WINF_SHADOW
    MOV     [DI + WIN_X], DL
    MOV     [DI + WIN_Y], DH
    MOV     [DI + WIN_W], BH
    MOV     BYTE [DI + WIN_H], 11
    MOV     BYTE [DI + WIN_ATTR], CLR_WIN_BG
    MOV     BYTE [DI + WIN_BATTR], CLR_WIN_BDR
    MOV     BYTE [DI + WIN_TATTR], CLR_WIN_TTL
    MOV     WORD [DI + WIN_TITLE], s_repl_title
    MOV     WORD [DI + WIN_FIRST], dlg_label
    MOV     WORD [DI + WIN_FOCUS], dlg_textbox
    MOV     WORD [DI + WIN_HANDLER], 0
    MOV     BYTE [DI + WIN_ZORDER], 0
    MOV     BYTE [DI + WIN_ID], 0
    MOV     BYTE [DI + WIN_SCROLLX], 0
    MOV     BYTE [DI + WIN_SCROLLY], 0

    CALL    tui_dlg_run_modal

    ; ---- Read checkbox states on OK ----
    MOV     AL, [fw_state + FW_DLG_RESULT]
    CMP     AL, DLG_OK
    JNE     .rsd_return

    AND     BYTE [find_flags], ~(FIND_FLAG_CI | FIND_FLAG_SEL)
    CMP     BYTE [dlg_chk1 + CTRL_CB_STATE], 0
    JNE     .rsd_no_ci
    OR      BYTE [find_flags], FIND_FLAG_CI
.rsd_no_ci:
    CMP     BYTE [dlg_chk2 + CTRL_CB_STATE], 0
    JE      .rsd_no_sel_flag
    OR      BYTE [find_flags], FIND_FLAG_SEL
.rsd_no_sel_flag:

.rsd_return:
    PopAll
    POP     AX                      ; discard saved AX
    MOV     AL, [fw_state + FW_DLG_RESULT]
    RET
_repl_show_dialog ENDP

; ============================================================================
; menu_replace_handler — Show Replace dialog, perform interactive replace loop
;
; Input:   None (reads cur_line, cur_col, sel_active, find_buf, replace_buf)
; Output:  None (text modified, selection set, viewport scrolled, FW_DIRTY)
; Preserves: SI, DI (per menu handler convention)
; Clobbers: AX, BX, CX, DX
; ============================================================================
menu_replace_handler PROC
    PUSH    SI
    PUSH    DI
    PUSH    BX
    PUSH    CX

    ; ==== STAGE 0: Detect selection and pre-fill ====
    XOR     AH, AH                          ; AH = 0: no multi-line selection
    CMP     BYTE [sel_active], 0
    JE      .mr_show_dlg

    CALL    sel_normalize                    ; BX=s_line, CX=s_col, DX=e_line, SI=e_col
    CMP     BX, DX
    JE      .mr_single_line_sel

    ; Multi-line selection — save bounds
    MOV     [find_sel_s_line], BX
    MOV     [find_sel_s_col], CX
    MOV     [find_sel_e_line], DX
    MOV     [find_sel_e_col], SI
    MOV     AH, 1
    JMP     .mr_show_dlg

.mr_single_line_sel:
    CALL    _find_prefill_from_sel

.mr_show_dlg:
    ; ==== STAGE 1: Show custom Replace dialog ====
    CALL    _repl_show_dialog               ; AH=multi-line flag; returns AL=result
    CMP     AL, DLG_OK
    JNE     .mr_done

    ; ==== STAGE 2: Compute lengths, guard empty find ====
    ; -- find_len --
    XOR     CX, CX
    MOV     BX, find_buf
.mr_flen_loop:
    CMP     BYTE [BX], 0
    JE      .mr_flen_done
    INC     BX
    INC     CX
    CMP     CX, FIND_MAX_LEN
    JB      .mr_flen_loop
.mr_flen_done:
    MOV     [find_len], CX
    TEST    CX, CX
    JZ      .mr_done                    ; empty find term: silently return

    ; -- Uppercase needle if case-insensitive --
    CALL    _find_upcase_needle

    ; -- replace_len --
    XOR     CX, CX
    MOV     BX, replace_buf
.mr_rlen_loop:
    CMP     BYTE [BX], 0
    JE      .mr_rlen_done
    INC     BX
    INC     CX
    CMP     CX, FIND_MAX_LEN
    JB      .mr_rlen_loop
.mr_rlen_done:
    MOV     [replace_len], CX

    ; -- Selection-only guard (same as Find) --
    TEST    BYTE [find_flags], FIND_FLAG_SEL
    JZ      .mr_sel_ok
    MOV     AX, [find_sel_s_line]
    CMP     AX, [find_sel_e_line]
    JNE     .mr_sel_ok
    MOV     AX, [find_sel_s_col]
    CMP     AX, [find_sel_e_col]
    JNE     .mr_sel_ok
    AND     BYTE [find_flags], ~FIND_FLAG_SEL
.mr_sel_ok:

    ; -- Init counter --
    MOV     WORD [replace_count], 0

    ; ==== STAGE 3: Find first match ====
    TEST    BYTE [find_flags], FIND_FLAG_SEL
    JNZ     .mr_search_sel

    ; Normal: from cursor, wrap to BOF
    MOV     BX, [cur_line]
    MOV     CX, [cur_col]
    CALL    find_forward
    JNC     .mr_first_found
    XOR     BX, BX
    XOR     CX, CX
    CALL    find_forward
    JNC     .mr_first_found
    JMP     .mr_not_found

    ; Selection-only: from sel_start
.mr_search_sel:
    MOV     BX, [find_sel_s_line]
    MOV     CX, [find_sel_s_col]
    CALL    find_forward
    JNC     .mr_first_found

.mr_not_found:
    MOV     SI, s_find_notfound
    CALL    _find_select_title
    CALL    tui_dlg_msgbox
    JMP     .mr_done

.mr_first_found:
    ; ==== STAGE 3a: Check "Replace all" checkbox ====
    CMP     BYTE [dlg_chk3 + CTRL_CB_STATE], 0
    JNE     .mr_replace_all_from_first

    ; ==== STAGE 4: Interactive replace loop ====
.mr_show_confirm:
    ; Set selection to highlight match
    MOV     AX, [find_match_line]
    MOV     [sel_anchor_line], AX
    MOV     AX, [find_match_col]
    MOV     [sel_anchor_col], AX
    MOV     AX, [find_match_end_line]
    MOV     [cur_line], AX
    MOV     AX, [find_match_end_col]
    MOV     [cur_col], AX
    MOV     BYTE [sel_active], 1
    CALL    scroll_to_cursor
    MOV     BYTE [fw_state + FW_DIRTY], 1

    ; Show Yes/No/All/Cancel dialog
    ; (The modal dialog event loop repaints the background, so the
    ;  selection highlight is visible underneath the dialog overlay)
    MOV     SI, s_repl_confirm          ; "Replace this match?"
    MOV     DI, s_repl_title            ; "Replace"
    CALL    tui_dlg_confirm4
    CMP     AL, DLG_YES
    JE      .mr_do_replace
    CMP     AL, DLG_REPL_ALL
    JE      .mr_replace_all
    CMP     AL, DLG_NO
    JE      .mr_find_next
    ; DLG_CANCEL or Escape — exit loop
    JMP     .mr_replace_done

.mr_do_replace:
    ; ---- Delete matched text ----
    CALL    sel_delete                  ; UR_DEL_RANGE pushed, sel_active=0
                                        ; cursor at selection start (match start)
    JC      .mr_replace_done            ; sel_delete failed (shouldn't happen)

    ; ---- Insert replacement text ----
    CMP     WORD [replace_len], 0
    JE      .mr_skip_insert             ; empty replacement = delete only

    CALL    _repl_insert                ; bulk-insert replace_buf
    JC      .mr_replace_done            ; insert failed (buffer full)

.mr_skip_insert:
    INC     WORD [replace_count]
    JMP     .mr_find_next

    ; ==== Replace All from checkbox — count matches, choose path ====
.mr_replace_all_from_first:
    ; Count total matches to decide if permanent path is needed
    CALL    _find_count_matches         ; AX = match count
    MOV     [replace_count], AX         ; temporarily store total for display

    ; If count > threshold → permanent (no undo) path
    CMP     AX, REPL_PERM_THRESHOLD
    JA      .mr_perm_ask

    ; Small count — use normal undoable Replace All
    ; Re-find first match from BOF (or sel start) so we replace the whole document
    MOV     WORD [replace_count], 0     ; reset counter for actual replacements
    TEST    BYTE [find_flags], FIND_FLAG_SEL
    JNZ     .mr_ra_from_sel
    XOR     BX, BX
    XOR     CX, CX
    JMP     .mr_ra_refind
.mr_ra_from_sel:
    MOV     BX, [find_sel_s_line]
    MOV     CX, [find_sel_s_col]
.mr_ra_refind:
    CALL    find_forward
    JC      .mr_replace_done            ; shouldn't happen — we just counted > 0
    JMP     .mr_ra_set_sel

    ; ---- Permanent replace: warn user ----
.mr_perm_ask:
    PUSH    SI
    PUSH    DI
    MOV     SI, s_repl_perm             ; "Cannot undo. Save and replace all?"
    MOV     DI, s_repl_perm_ttl         ; "Warning"
    CALL    tui_dlg_confirm
    POP     DI
    POP     SI
    CMP     AL, DLG_YES
    JNE     .mr_done                    ; user declined

    ; ---- Ensure file has a name (streaming writes directly to file) ----
    CMP     BYTE [filename], 0
    JNE     .mr_perm_have_name
    CALL    menu_save_handler
    CMP     BYTE [dirty], 0
    JNE     .mr_done
.mr_perm_have_name:

    ; ---- Stream piece table to file with replacements applied (O(n)) ----
    MOV     WORD [replace_count], 0
    CALL    _repl_save_replaced
    JC      .mr_perm_fail

    ; ---- Reload replaced file from disk ----
    CALL    ed_compact
    CALL    sel_clear
    MOV     BYTE [fw_state + FW_DIRTY], 1

    CMP     WORD [replace_count], 0
    JE      .mr_done
    CALL    _repl_format_count
    MOV     SI, repl_msg_buf
    MOV     DI, s_repl_title
    CALL    tui_dlg_msgbox
    JMP     .mr_done

.mr_perm_fail:
    PUSH    SI
    PUSH    DI
    MOV     SI, s_err_save_fail
    MOV     DI, m_str_error
    CALL    tui_dlg_msgbox
    POP     DI
    POP     SI
    JMP     .mr_done

    ; ---- Normal undoable Replace All: set up selection for first match ----
.mr_ra_set_sel:
    MOV     AX, [find_match_line]
    MOV     [sel_anchor_line], AX
    MOV     AX, [find_match_col]
    MOV     [sel_anchor_col], AX
    MOV     AX, [find_match_end_line]
    MOV     [cur_line], AX
    MOV     AX, [find_match_end_col]
    MOV     [cur_col], AX
    MOV     BYTE [sel_active], 1
    ; Fall through to normal batch replace

    ; ==== Replace All — batch replace without confirmation ====
.mr_replace_all:
    ; Replace current match first (same as .mr_do_replace)
    CALL    sel_delete                  ; DEL_RANGE = group head (no flag)
    JC      .mr_replace_done
    CMP     WORD [replace_len], 0
    JE      .mr_ra_skip_ins1
    CALL    _repl_insert
    JC      .mr_replace_done

    ; Post-patch: group INS_RANGE with preceding DEL_RANGE
    PUSH    ES
    MOV     ES, [undo_seg]
    MOV     AX, [undo_pos]
    DEC     AX
    MOV     BX, AX
    SHL     BX, 1
    MOV     CX, BX
    SHL     BX, 1
    SHL     BX, 1
    ADD     BX, CX                      ; BX = AX * 10
    OR      BYTE ES:[BX + UR_TYPE], URF_GROUP_CONT
    POP     ES
.mr_ra_skip_ins1:
    INC     WORD [replace_count]

    ; ---- Batch loop: find and replace remaining matches ----
.mr_batch_loop:
    MOV     BX, [cur_line]
    MOV     CX, [cur_col]
    CALL    find_forward
    JC      .mr_replace_done            ; no more matches

    ; Set selection to match (for sel_delete)
    MOV     AX, [find_match_line]
    MOV     [sel_anchor_line], AX
    MOV     AX, [find_match_col]
    MOV     [sel_anchor_col], AX
    MOV     AX, [find_match_end_line]
    MOV     [cur_line], AX
    MOV     AX, [find_match_end_col]
    MOV     [cur_col], AX
    MOV     BYTE [sel_active], 1

    ; ---- Delete match ----
    CALL    sel_delete
    JC      .mr_replace_done

    ; ---- Post-patch undo: group-continue the DEL_RANGE record ----
    CMP     WORD [replace_count], 0
    JE      .mr_batch_skip_g1
    PUSH    ES
    MOV     ES, [undo_seg]
    MOV     AX, [undo_pos]
    DEC     AX
    MOV     BX, AX
    SHL     BX, 1
    MOV     CX, BX
    SHL     BX, 1
    SHL     BX, 1
    ADD     BX, CX                      ; BX = AX * 10
    OR      BYTE ES:[BX + UR_TYPE], URF_GROUP_CONT
    POP     ES
.mr_batch_skip_g1:

    ; ---- Insert replacement ----
    CMP     WORD [replace_len], 0
    JE      .mr_batch_skip_ins

    CALL    _repl_insert
    JC      .mr_replace_done

    ; ---- Post-patch undo: group-continue the INS_RANGE record ----
    CMP     WORD [replace_count], 0
    JE      .mr_batch_skip_g2
    PUSH    ES
    MOV     ES, [undo_seg]
    MOV     AX, [undo_pos]
    DEC     AX
    MOV     BX, AX
    SHL     BX, 1
    MOV     CX, BX
    SHL     BX, 1
    SHL     BX, 1
    ADD     BX, CX                      ; BX = AX * 10
    OR      BYTE ES:[BX + UR_TYPE], URF_GROUP_CONT
    POP     ES
.mr_batch_skip_g2:

.mr_batch_skip_ins:
    INC     WORD [replace_count]
    JMP     .mr_batch_loop

.mr_find_next:
    ; Search for next match from current cursor position
    ; (cursor is at match end after skip, or at replacement end after replace)
    MOV     BX, [cur_line]
    MOV     CX, [cur_col]
    CALL    find_forward
    JNC     .mr_show_confirm            ; found another match

    ; No more matches — show summary
.mr_replace_done:
    CALL    sel_clear                   ; clean up any lingering selection

    CMP     WORD [replace_count], 0
    JE      .mr_done                    ; no replacements, no message needed

    ; Format "N replacements made." into repl_msg_buf
    CALL    _repl_format_count
    MOV     SI, repl_msg_buf
    MOV     DI, s_repl_title
    CALL    tui_dlg_msgbox

.mr_done:
    POP     CX
    POP     BX
    POP     DI
    POP     SI
    RET
menu_replace_handler ENDP
