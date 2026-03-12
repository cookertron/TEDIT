; ============================================================
; ed_keys.inc — Editor Keyboard Handling
; ============================================================

; ============================================================
; tui_ed_handle_key
;
; Input:  AH = key type (KTYPE_NORMAL or KTYPE_EXTENDED)
;         AL = ASCII char (normal) or scan code (extended)
;         BX = focused control pointer
;         SI = window struct pointer
; Output: CF=0 handled, CF=1 not handled
; ============================================================

tui_ed_handle_key PROC
    ; ---- Check key type ----
    CMP     AH, KTYPE_EXTENDED
    JZ      .extended

    ; ---- Normal key dispatch ----

    ; Ctrl+S (ASCII 13h = 19 decimal)
    CMP     AL, 13h
    JZ      .do_save

    ; Ctrl+Z / Ctrl+Shift+Z (ASCII 1Ah)
    CMP     AL, 1Ah
    JZ      .do_undo_or_redo

    ; Ctrl+X (ASCII 18h)
    CMP     AL, 18h
    JZ      .do_cut

    ; Ctrl+C (ASCII 03h)
    CMP     AL, 03h
    JZ      .do_copy

    ; Ctrl+V (ASCII 16h)
    CMP     AL, 16h
    JZ      .do_paste

    ; Ctrl+F (ASCII 06h)
    CMP     AL, 06h
    JZ      .do_find

    ; Ctrl+G (ASCII 07h)
    CMP     AL, 07h
    JZ      .do_goto

    ; Enter
    CMP     AL, KEY_ENTER
    JZ      .do_enter

    ; Backspace / Ctrl+H (both ASCII 08h)
    CMP     AL, KEY_BACKSPACE
    JZ      .do_bksp_or_replace

    ; Escape — not handled here (falls through to framework/menu)
    CMP     AL, KEY_ESCAPE
    JZ      .not_handled

    ; Printable ASCII (20h–7Eh)
    CMP     AL, 20h
    JB      .not_handled
    CMP     AL, 7Eh
    JA      .not_handled
    JMP     .do_char

.not_handled:
    STC
    RET

; ============================================================
; Insert printable character
; ============================================================
.do_char:
    CALL    insert_char
    ; insert_char handles: dirty, cur_col, scroll_to_cursor, FW_DIRTY
    CLC
    RET

; ============================================================
; Enter — insert CR+LF, move to start of next line
; ============================================================
.do_enter:
    MOV     WORD [last_ins_pi], 0FFFFh
    CALL    insert_crlf
    INC     WORD [cur_line]
    MOV     WORD [cur_col], 0
    CALL    scroll_to_cursor
    MOV     BYTE [meta_dirty], 1
    MOV     BYTE [fw_state + FW_DIRTY], 1
    CLC
    RET

; ============================================================
; Backspace — delete character before cursor
; ============================================================
.do_bksp:
    MOV     WORD [last_ins_pi], 0FFFFh
    ; If at col 0: merge with previous line
    MOV     AX, [cur_col]
    TEST    AX, AX
    JNZ     .bksp_col
    ; At column 0 — move to end of previous line
    CMP     WORD [cur_line], 0
    JE      .bksp_done              ; at start of document, nothing to do
    DEC     WORD [cur_line]
    CALL    line_length             ; AX = visible chars on (now current) line
    MOV     [cur_col], AX
    JMP     .bksp_do
.bksp_col:
    DEC     WORD [cur_col]
.bksp_do:
    CALL    delete_at
    CALL    scroll_to_cursor
    MOV     BYTE [fw_state + FW_DIRTY], 1
.bksp_done:
    CLC
    RET

; ============================================================
; Ctrl+S — save file
; ============================================================
.do_save:
    MOV     WORD [last_ins_pi], 0FFFFh
    CALL    menu_save_handler
    CLC
    RET

; ============================================================
; Undo / Redo disambiguation (Ctrl+Z vs Ctrl+Shift+Z)
; ============================================================
.do_undo_or_redo:
    TEST    BYTE [_key_modifiers], 03h  ; either Shift bit?
    JNZ     .do_redo
    ; fall through to .do_undo

.do_undo:
    CALL    menu_edit_stub
    MOV     BYTE [fw_state + FW_DIRTY], 1
    CLC
    RET

.do_redo:
    CALL    menu_edit_stub
    MOV     BYTE [fw_state + FW_DIRTY], 1
    CLC
    RET

; ============================================================
; Backspace / Replace disambiguation (08h: Ctrl+H vs Backspace)
; ============================================================
.do_bksp_or_replace:
    TEST    BYTE [_key_modifiers], 04h  ; Ctrl bit?
    JNZ     .do_replace
    JMP     .do_bksp                    ; existing Backspace handler

.do_replace:
    CALL    menu_edit_stub
    MOV     BYTE [fw_state + FW_DIRTY], 1
    CLC
    RET

; ============================================================
; Remaining Edit menu shortcut stubs
; ============================================================
.do_cut:
.do_copy:
.do_paste:
.do_find:
.do_findnxt:
.do_findprv:
.do_goto:
.do_timedate:
    CALL    menu_edit_stub
    MOV     BYTE [fw_state + FW_DIRTY], 1
    CLC
    RET

; ============================================================
; Extended key dispatch (navigation + Delete)
; ============================================================
.extended:
    CMP     AL, KEY_UP
    JZ      .key_up
    CMP     AL, KEY_DOWN
    JZ      .key_down
    CMP     AL, KEY_LEFT
    JZ      .key_left
    CMP     AL, KEY_RIGHT
    JZ      .key_right
    CMP     AL, KEY_PGDN
    JZ      .key_pgdn
    CMP     AL, KEY_PGUP
    JZ      .key_pgup
    CMP     AL, KEY_HOME
    JZ      .key_home
    CMP     AL, KEY_END
    JZ      .key_end
    CMP     AL, KEY_DELETE
    JZ      .do_del

    ; F3 → Find Next (scan 3Dh)
    CMP     AL, 3Dh
    JZ      .do_findnxt

    ; Shift+F3 → Find Prev (scan 54h)
    CMP     AL, 54h
    JZ      .do_findprv

    ; F5 → Time/Date (scan 3Fh)
    CMP     AL, 3Fh
    JZ      .do_timedate

    ; Unrecognized extended key
    STC
    RET

; ============================================================
; Delete key — delete character at cursor
; ============================================================
.do_del:
    MOV     WORD [last_ins_pi], 0FFFFh
    CALL    delete_at
    CALL    scroll_to_cursor
    MOV     BYTE [fw_state + FW_DIRTY], 1
    CLC
    RET

; ---- Navigation keys ----

.key_up:
    CMP     BYTE [wrap_mode], 0
    JNE     .key_up_wrap
    CMP     WORD [cur_line], 0
    JE      .handled
    DEC     WORD [cur_line]
    JMP     .vert_moved

.key_up_wrap:
    ; Wrap mode: move up one visual row
    MOV     AX, [cur_col]
    CMP     AX, ED_COLS
    JB      .up_prev_line
    ; Same logical line — just move up one visual row
    SUB     WORD [cur_col], ED_COLS
    JMP     .cursor_moved

.up_prev_line:
    ; First visual row — move to last visual row of previous line
    CMP     WORD [cur_line], 0
    JE      .handled
    PUSH    AX                      ; save screen_col (= cur_col < ED_COLS)
    DEC     WORD [cur_line]
    MOV     BYTE [meta_dirty], 1
    CALL    line_length             ; AX = length of previous line
    MOV     BX, AX                  ; BX = line_length
    XOR     DX, DX
    MOV     CX, ED_COLS
    DIV     CX                      ; AX = line_length / ED_COLS
    MUL     CX                      ; AX = last_vis_row_start
    POP     CX                      ; CX = screen_col
    ADD     AX, CX                  ; AX = target col
    CMP     AX, BX
    JBE     .up_set_col
    MOV     AX, BX                  ; clamp to line_length
.up_set_col:
    MOV     [cur_col], AX
    JMP     .cursor_moved

.key_down:
    CMP     BYTE [wrap_mode], 0
    JNE     .key_down_wrap
    MOV     AX, [cur_line]
    INC     AX
    CMP     AX, [total_lines]
    JAE     .handled
    MOV     [cur_line], AX
    JMP     .vert_moved

.key_down_wrap:
    ; Wrap mode: move down one visual row
    MOV     AX, [cur_col]
    XOR     DX, DX
    MOV     CX, ED_COLS
    DIV     CX                      ; AX = cur_row_in_line, DX = screen_col
    PUSH    DX                      ; save screen_col
    INC     AX
    MUL     CX                      ; AX = next_vis_row_start
    MOV     BX, AX                  ; BX = next_vis_row_start
    CALL    line_length             ; AX = current line length
    CMP     BX, AX
    JA      .down_next_line
    ; Same line: next visual row
    POP     CX                      ; CX = screen_col
    ADD     BX, CX                  ; BX = target col
    CMP     BX, AX                  ; AX still = line_length
    JBE     .down_set_col
    MOV     BX, AX                  ; clamp to line_length
.down_set_col:
    MOV     [cur_col], BX
    JMP     .cursor_moved

.down_next_line:
    POP     CX                      ; CX = screen_col
    MOV     AX, [cur_line]
    INC     AX
    CMP     AX, [total_lines]
    JAE     .handled
    MOV     [cur_line], AX
    MOV     [cur_col], CX
    MOV     BYTE [meta_dirty], 1
    CALL    line_length             ; AX = new line length
    CMP     [cur_col], AX
    JBE     .down_col_ok
    MOV     [cur_col], AX
.down_col_ok:
    JMP     .cursor_moved

.key_left:
    CMP     WORD [cur_col], 0
    JE      .handled
    DEC     WORD [cur_col]
    JMP     .cursor_moved

.key_right:
    CALL    peek_at_cursor
    JZ      .handled
    INC     WORD [cur_col]
    JMP     .cursor_moved

.key_pgdn:
    MOV     AX, [cur_line]
    ADD     AX, ED_TEXT_ROWS
    CMP     AX, [total_lines]
    JB      .pgdn_ok
    MOV     AX, [total_lines]
    DEC     AX
.pgdn_ok:
    MOV     [cur_line], AX
    JMP     .vert_moved

.key_pgup:
    MOV     AX, [cur_line]
    SUB     AX, ED_TEXT_ROWS
    JNC     .pgup_ok
    XOR     AX, AX
.pgup_ok:
    MOV     [cur_line], AX
    JMP     .vert_moved

.key_home:
    MOV     WORD [cur_col], 0
    JMP     .cursor_moved

.key_end:
    CALL    line_length
    MOV     [cur_col], AX
    JMP     .cursor_moved

.vert_moved:
    ; In wrap mode, flag meta dirty so cur_vis_line gets recomputed
    CMP     BYTE [wrap_mode], 0
    JE      .vm_no_meta
    MOV     BYTE [meta_dirty], 1
.vm_no_meta:
    CALL    line_length
    CMP     [cur_col], AX
    JBE     .cursor_moved
    MOV     [cur_col], AX

.cursor_moved:
    MOV     WORD [last_ins_pi], 0FFFFh
    CALL    scroll_to_cursor
    MOV     BYTE [fw_state + FW_DIRTY], 1

.handled:
    CLC
    RET
tui_ed_handle_key ENDP
