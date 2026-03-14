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
    CALL    sel_delete              ; delete selection if any (preserves AX)
    ; ---- Save pre-cursor for undo ----
    PUSH    AX
    MOV     AX, [cur_line]
    MOV     [undo_pre_line], AX
    MOV     AX, [cur_col]
    MOV     [undo_pre_col], AX
    POP     AX

    CALL    insert_char

    ; ---- Check if insert succeeded (cur_col must have advanced) ----
    PUSH    AX
    MOV     AX, [cur_col]
    CMP     AX, [undo_pre_col]
    POP     AX
    JE      .do_char_skip               ; insert failed — skip recording

    ; ---- Determine group flag ----
    PUSH    AX                          ; save original char in AL
    MOV     AL, UR_INS_CHAR
    CMP     BYTE [undo_group_active], 0
    JE      .no_group
    PUSH    BX
    MOV     BX, [undo_pre_line]
    CMP     BX, [undo_group_line]
    JNE     .no_group_pop
    MOV     BX, [undo_group_col]
    INC     BX
    CMP     BX, [undo_pre_col]
    JNE     .no_group_pop
    OR      AL, URF_GROUP_CONT
    POP     BX
    JMP     .group_done
.no_group_pop:
    POP     BX
.no_group:
.group_done:
    MOV     CL, AL                      ; CL = type byte with flag
    POP     AX                          ; AL = original char

    ; ---- Record undo entry ----
    PUSH    AX
    PUSH    SI
    MOV     AH, AL                      ; AH = char
    MOV     AL, CL                      ; AL = type (with possible group flag)
    MOV     BX, [undo_pre_line]
    MOV     CX, [undo_pre_col]
    MOV     DX, [undo_pre_line]
    MOV     SI, [undo_pre_col]
    CALL    undo_push
    POP     SI
    POP     AX

    ; ---- Update group tracking ----
    MOV     BYTE [undo_group_active], 1
    PUSH    AX
    MOV     AX, [undo_pre_line]
    MOV     [undo_group_line], AX
    MOV     AX, [undo_pre_col]
    MOV     [undo_group_col], AX
    POP     AX

.do_char_skip:
    CLC
    RET

; ============================================================
; Enter — insert CR+LF, move to start of next line
; ============================================================
.do_enter:
    CALL    sel_delete              ; delete selection if any
    ; ---- Save pre-cursor for undo ----
    MOV     AX, [cur_line]
    MOV     [undo_pre_line], AX
    MOV     AX, [cur_col]
    MOV     [undo_pre_col], AX

    MOV     WORD [last_ins_pi], 0FFFFh
    MOV     BYTE [undo_group_active], 0

    ; ---- Save add_used to detect failure ----
    MOV     AX, [add_used]
    PUSH    AX

    CALL    insert_crlf

    ; ---- Check if insert succeeded ----
    POP     AX
    CMP     AX, [add_used]
    JE      .enter_skip                 ; unchanged -> insert failed

    INC     WORD [cur_line]
    MOV     WORD [cur_col], 0
    CALL    scroll_to_cursor
    MOV     BYTE [meta_dirty], 1
    MOV     BYTE [fw_state + FW_DIRTY], 1

    ; ---- Record undo entry ----
    PUSH    SI
    MOV     AL, UR_INS_CRLF
    MOV     AH, 0
    MOV     BX, [undo_pre_line]
    MOV     CX, [undo_pre_col]
    MOV     DX, [undo_pre_line]
    MOV     SI, [undo_pre_col]
    CALL    undo_push
    POP     SI

.enter_skip:
    CLC
    RET

; ============================================================
; Backspace — delete character before cursor
; ============================================================
.do_bksp:
    CALL    sel_delete
    JNC     .bksp_done              ; selection deleted — skip normal backspace
    MOV     WORD [last_ins_pi], 0FFFFh
    MOV     BYTE [undo_group_active], 0

    ; ---- Save pre-cursor for undo (before any adjustment) ----
    MOV     AX, [cur_line]
    MOV     [undo_pre_line], AX
    MOV     AX, [cur_col]
    MOV     [undo_pre_col], AX

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
    MOV     BYTE [undo_del_crlf], 0FFh     ; sentinel: no delete yet
    CALL    delete_at
    CALL    scroll_to_cursor
    MOV     BYTE [fw_state + FW_DIRTY], 1

    ; ---- Record undo entry (only if delete succeeded) ----
    CMP     BYTE [undo_del_crlf], 0FFh
    JE      .bksp_done                      ; delete_at failed — skip

    PUSH    SI
    CMP     BYTE [undo_del_crlf], 0
    JNE     .bksp_rec_crlf

    ; Single character deleted
    MOV     AL, UR_DEL_CHAR
    MOV     AH, [undo_del_char]
    MOV     BX, [cur_line]
    MOV     CX, [cur_col]
    MOV     DX, [undo_pre_line]
    MOV     SI, [undo_pre_col]
    CALL    undo_push
    JMP     .bksp_rec_done

.bksp_rec_crlf:
    ; CRLF pair deleted (line merge)
    MOV     AL, UR_DEL_CRLF
    MOV     AH, 0
    MOV     BX, [cur_line]
    MOV     CX, [cur_col]
    MOV     DX, [undo_pre_line]
    MOV     SI, [undo_pre_col]
    CALL    undo_push

.bksp_rec_done:
    POP     SI

.bksp_done:
    CLC
    RET

; ============================================================
; Ctrl+S — save file
; ============================================================
.do_save:
    MOV     WORD [last_ins_pi], 0FFFFh
    MOV     BYTE [undo_group_active], 0
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
    MOV     BYTE [undo_group_active], 0
    CALL    undo_execute
    CLC
    RET

.do_redo:
    MOV     BYTE [undo_group_active], 0
    CALL    redo_execute
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
    ; Pre-capture anchor for Shift+nav (must be before cursor moves)
    TEST    BYTE [_key_modifiers], 03h
    JZ      .ext_dispatch
    CMP     BYTE [sel_active], 0
    JNE     .ext_dispatch
    CALL    sel_start_or_extend
.ext_dispatch:
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
    CALL    sel_delete
    JNC     .del_rec_done           ; selection deleted — skip normal delete
    MOV     WORD [last_ins_pi], 0FFFFh
    MOV     BYTE [undo_group_active], 0

    ; ---- Save pre-cursor for undo ----
    MOV     AX, [cur_line]
    MOV     [undo_pre_line], AX
    MOV     AX, [cur_col]
    MOV     [undo_pre_col], AX

    MOV     BYTE [undo_del_crlf], 0FFh     ; sentinel: no delete yet
    CALL    delete_at
    CALL    scroll_to_cursor
    MOV     BYTE [fw_state + FW_DIRTY], 1

    ; ---- Record undo entry (only if delete succeeded) ----
    CMP     BYTE [undo_del_crlf], 0FFh
    JE      .del_rec_done                   ; delete_at failed — skip

    PUSH    SI
    CMP     BYTE [undo_del_crlf], 0
    JNE     .del_rec_crlf

    ; Single character deleted
    MOV     AL, UR_DEL_CHAR
    MOV     AH, [undo_del_char]
    MOV     BX, [undo_pre_line]
    MOV     CX, [undo_pre_col]
    MOV     DX, [undo_pre_line]
    MOV     SI, [undo_pre_col]
    CALL    undo_push
    JMP     .del_rec_done2

.del_rec_crlf:
    MOV     AL, UR_DEL_CRLF
    MOV     AH, 0
    MOV     BX, [undo_pre_line]
    MOV     CX, [undo_pre_col]
    MOV     DX, [undo_pre_line]
    MOV     SI, [undo_pre_col]
    CALL    undo_push

.del_rec_done2:
    POP     SI

.del_rec_done:
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
    ; ---- Selection: Shift held = extend, no Shift = clear ----
    TEST    BYTE [_key_modifiers], 03h      ; either Shift bit?
    JNZ     .cm_shift
    CALL    sel_clear
    JMP     .cm_sel_done
.cm_shift:
    CALL    sel_start_or_extend
.cm_sel_done:
    MOV     WORD [last_ins_pi], 0FFFFh
    MOV     BYTE [undo_group_active], 0
    CALL    scroll_to_cursor
    MOV     BYTE [fw_state + FW_DIRTY], 1

.handled:
    CLC
    RET
tui_ed_handle_key ENDP
