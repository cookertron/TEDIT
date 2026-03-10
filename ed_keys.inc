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

    ; Ctrl+N (ASCII 0Eh = 14 decimal) — new document
    CMP     AL, 0Eh
    JZ      .do_new_doc

    ; Ctrl+W (ASCII 17h = 23 decimal) — close document
    CMP     AL, 17h
    JZ      .do_close_doc

    ; Enter
    CMP     AL, KEY_ENTER
    JZ      .do_enter

    ; Backspace
    CMP     AL, KEY_BACKSPACE
    JZ      .do_bksp

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

    ; Ctrl+Tab (scan 94h) — next document
    CMP     AL, 94h
    JZ      .do_next_doc

    ; Ctrl+PgDn (scan 76h) — next document (alternative)
    CMP     AL, 76h
    JZ      .do_next_doc

    ; Ctrl+PgUp (scan 84h) — previous document
    CMP     AL, 84h
    JZ      .do_prev_doc

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

; ============================================================
; Ctrl+N — new document
; ============================================================
.do_new_doc:
    CALL    menu_new_handler       ; shows error dialog on failure
    CLC
    RET

; ============================================================
; Ctrl+Tab — switch to next document
; ============================================================
.do_next_doc:
    CALL    doc_switch_next
    CLC
    RET

; ============================================================
; Ctrl+W — close document
; ============================================================
.do_close_doc:
    CALL    doc_close              ; handles its own save dialogs
    CLC
    RET

; ============================================================
; Ctrl+PgUp — switch to previous document
; ============================================================
.do_prev_doc:
    CALL    doc_switch_prev
    CLC
    RET

; ---- Navigation keys ----

.key_up:
    CMP     WORD [cur_line], 0
    JE      .handled
    DEC     WORD [cur_line]
    JMP     .vert_moved

.key_down:
    MOV     AX, [cur_line]
    INC     AX
    CMP     AX, [total_lines]
    JAE     .handled
    MOV     [cur_line], AX
    JMP     .vert_moved

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
