; ============================================================================
; tui_control.inc - Control Framework (Labels + Buttons)
; ============================================================================

; ----------------------------------------------------------------------------
; tui_ctrl_draw_all - Draw all controls for a window
; Input: SI = window struct pointer
; ----------------------------------------------------------------------------
tui_ctrl_draw_all: PROC
    PushAll

    ; Get first control pointer
    MOV BX, [SI + WIN_FIRST]
    OR BX, BX
    JNZ .have_ctrls
    JMP .done
.have_ctrls:

    ; Calculate interior origin
    MOV DL, [SI + WIN_X]
    MOV DH, [SI + WIN_Y]
    TEST BYTE [SI + WIN_FLAGS], WINF_BORDER
    JZ .origin_set
    INC DL                      ; interior starts at x+1
    INC DH                      ; interior starts at y+1
.origin_set:
    ; Save interior origin on stack
    PUSH DX

    ; Save WIN_FOCUS for comparison
    MOV DI, [SI + WIN_FOCUS]

.ctrl_loop:
    OR BX, BX
    JNZ .ctrl_not_null
    JMP .loop_done
.ctrl_not_null:

    ; Check CTRLF_VISIBLE
    TEST BYTE [BX + CTRL_FLAGS], CTRLF_VISIBLE
    JZ .next_ctrl

    ; Calculate absolute position: origin + ctrl relative pos
    POP DX                      ; restore origin
    PUSH DX                     ; keep it on stack
    ADD DL, [BX + CTRL_X]
    ADD DH, [BX + CTRL_Y]

    ; Determine if focused: CH = 1 if focused, 0 if not
    XOR CH, CH
    CMP BX, DI
    JNZ .not_focused
    MOV CH, 1
.not_focused:

    ; Dispatch by type
    CMP BYTE [BX + CTRL_TYPE], CTYPE_LABEL
    JZ .draw_label
    CMP BYTE [BX + CTRL_TYPE], CTYPE_BUTTON
    JZ .draw_button
    CMP BYTE [BX + CTRL_TYPE], CTYPE_TEXTBOX
    JZ .draw_textbox
    CMP BYTE [BX + CTRL_TYPE], CTYPE_CHECKBOX
    JZ .draw_checkbox
    CMP BYTE [BX + CTRL_TYPE], CTYPE_RADIO
    JZ .draw_radio
    CMP BYTE [BX + CTRL_TYPE], CTYPE_LISTBOX
    JZ .draw_listbox
    CMP BYTE [BX + CTRL_TYPE], CTYPE_EDITOR
    JZ .draw_editor
    JMP .next_ctrl

.draw_label:
    CALL tui_ctrl_draw_label
    JMP .next_ctrl

.draw_button:
    CALL tui_ctrl_draw_button
    JMP .next_ctrl

.draw_textbox:
    CALL tui_ctrl_draw_textbox
    JMP .next_ctrl

.draw_checkbox:
    CALL tui_ctrl_draw_checkbox
    JMP .next_ctrl

.draw_radio:
    CALL tui_ctrl_draw_radio
    JMP .next_ctrl

.draw_listbox:
    CALL tui_ctrl_draw_listbox
    JMP .next_ctrl

.draw_editor:
    CALL tui_ctrl_draw_editor
    JMP .next_ctrl

.next_ctrl:
    MOV BX, [BX + CTRL_NEXT]
    JMP .ctrl_loop

.loop_done:
    POP DX                      ; clean up origin from stack

.done:
    PopAll
    RET
ENDP

; ----------------------------------------------------------------------------
; tui_ctrl_draw_label - Draw a label control
; Input: BX = control pointer, DH = abs row, DL = abs col
; Preserves: BX, DI, DH, DL
; ----------------------------------------------------------------------------
tui_ctrl_draw_label: PROC
    PUSH SI
    PUSH AX
    MOV AH, [BX + CTRL_ATTR]
    MOV SI, [BX + CTRL_TEXT]
    CALL tui_putstr
    POP AX
    POP SI
    RET
ENDP

; ----------------------------------------------------------------------------
; tui_ctrl_draw_button - Draw a button control
; Input: BX = control pointer, DH = abs row, DL = abs col, CH = focused flag
; Preserves: BX, DI, DH, DL
; ----------------------------------------------------------------------------
tui_ctrl_draw_button: PROC
    PUSH SI
    PUSH AX
    PUSH CX
    PUSH DX

    ; Select attribute: pressed > focused > normal
    CMP [mouse_state + MS_PRESSED], BX
    JNZ .not_pressed
    MOV AH, CLR_BTN_PRESSED
    JMP .attr_set
.not_pressed:
    OR CH, CH
    JZ .normal_attr
    MOV AH, [BX + CTRL_FATTR]
    JMP .attr_set
.normal_attr:
    MOV AH, [BX + CTRL_ATTR]
.attr_set:

    ; Fill button width with spaces (background)
    MOV AL, 20h
    MOV CL, [BX + CTRL_W]
    CALL tui_hline

    ; Draw '[' at left edge
    MOV AL, '['
    CALL tui_putchar

    ; Draw ']' at right edge
    PUSH DX
    ADD DL, [BX + CTRL_W]
    DEC DL
    MOV AL, ']'
    CALL tui_putchar
    POP DX

    ; Measure text length
    PUSH DI
    MOV DI, [BX + CTRL_TEXT]
    XOR CX, CX
.measure:
    CMP BYTE [DI], 0
    JZ .measured
    INC DI
    INC CX
    JMP .measure
.measured:
    POP DI

    ; Center text between brackets
    ; inner_width = w - 2, offset = (inner_width - len) / 2
    ; start_col = col + 1 + offset
    MOV AL, [BX + CTRL_W]
    SUB AL, 2               ; inner width
    SUB AL, CL              ; inner_width - text_len
    SHR AL, 1               ; / 2
    ADD AL, DL              ; + original col
    INC AL                  ; + 1 (past '[')
    MOV DL, AL

    ; Draw text (AH still has correct attribute)
    MOV SI, [BX + CTRL_TEXT]
    CALL tui_putstr

    POP DX
    POP CX
    POP AX
    POP SI
    RET
ENDP

; ----------------------------------------------------------------------------
; tui_ctrl_focus_next - Cycle focus to next focusable control
; Input:  SI = window struct pointer
; Output: CF=0 if focus changed, CF=1 if no focusable control
; ----------------------------------------------------------------------------
tui_ctrl_focus_next: PROC
    PUSH AX
    PUSH BX
    PUSH CX
    PUSH DX

    ; Get current focus
    MOV BX, [SI + WIN_FOCUS]
    OR BX, BX
    JZ .start_from_head

    ; Start searching from next control after current focus
    MOV BX, [BX + CTRL_NEXT]
    OR BX, BX
    JNZ .search

    ; Reached end of list - wrap to head
    JMP .start_from_head

.search:
    ; Search forward for a focusable+enabled control
.search_loop:
    OR BX, BX
    JZ .wrap

    TEST BYTE [BX + CTRL_FLAGS], CTRLF_FOCUSABLE
    JZ .search_next
    TEST BYTE [BX + CTRL_FLAGS], CTRLF_ENABLED
    JZ .search_next

    ; Found one - is it different from current?
    CMP BX, [SI + WIN_FOCUS]
    JZ .no_change
    MOV [SI + WIN_FOCUS], BX
    MOV BYTE [fw_state + FW_DIRTY], 1
    CLC
    JMP .ret

.search_next:
    MOV BX, [BX + CTRL_NEXT]
    JMP .search_loop

.wrap:
    ; Wrapped to end of list, restart from head
    MOV BX, [SI + WIN_FIRST]

.wrap_loop:
    OR BX, BX
    JZ .no_change
    TEST BYTE [BX + CTRL_FLAGS], CTRLF_FOCUSABLE
    JZ .wrap_next
    TEST BYTE [BX + CTRL_FLAGS], CTRLF_ENABLED
    JZ .wrap_next

    ; Found one
    CMP BX, [SI + WIN_FOCUS]
    JZ .no_change
    MOV [SI + WIN_FOCUS], BX
    MOV BYTE [fw_state + FW_DIRTY], 1
    CLC
    JMP .ret

.wrap_next:
    MOV BX, [BX + CTRL_NEXT]
    JMP .wrap_loop

.start_from_head:
    ; No current focus - find first focusable from head
    MOV BX, [SI + WIN_FIRST]

.head_loop:
    OR BX, BX
    JZ .no_change
    TEST BYTE [BX + CTRL_FLAGS], CTRLF_FOCUSABLE
    JZ .head_next
    TEST BYTE [BX + CTRL_FLAGS], CTRLF_ENABLED
    JZ .head_next

    ; Found one
    CMP BX, [SI + WIN_FOCUS]
    JZ .no_change
    MOV [SI + WIN_FOCUS], BX
    MOV BYTE [fw_state + FW_DIRTY], 1
    CLC
    JMP .ret

.head_next:
    MOV BX, [BX + CTRL_NEXT]
    JMP .head_loop

.no_change:
    STC

.ret:
    POP DX
    POP CX
    POP BX
    POP AX
    RET
ENDP

; ----------------------------------------------------------------------------
; tui_ctrl_focus_prev - Cycle focus to previous focusable control
; Input:  SI = window struct pointer
; Output: CF=0 if focus changed, CF=1 if no focusable control
; Walks the singly-linked list from HEAD, tracking the last focusable
; control seen before the current focus. If current is first, wraps to last.
; ----------------------------------------------------------------------------
tui_ctrl_focus_prev: PROC
    PUSH AX
    PUSH BX
    PUSH CX
    PUSH DX

    MOV DX, [SI + WIN_FOCUS]   ; DX = current focus
    OR DX, DX
    JZ .fp_start_from_head

    ; Walk list from head, find last focusable before current
    XOR CX, CX                 ; CX = last focusable seen (0 = none yet)
    MOV BX, [SI + WIN_FIRST]

.fp_scan:
    OR BX, BX
    JZ .fp_wrap_to_last         ; reached end without finding current
    CMP BX, DX
    JZ .fp_found_current        ; reached current focus

    ; Is this control focusable+enabled?
    TEST BYTE [BX + CTRL_FLAGS], CTRLF_FOCUSABLE
    JZ .fp_scan_next
    TEST BYTE [BX + CTRL_FLAGS], CTRLF_ENABLED
    JZ .fp_scan_next
    MOV CX, BX                 ; remember as candidate

.fp_scan_next:
    MOV BX, [BX + CTRL_NEXT]
    JMP .fp_scan

.fp_found_current:
    ; CX = last focusable before current (0 if current was first)
    OR CX, CX
    JNZ .fp_set_focus

    ; Current was first focusable — wrap to last focusable in list
.fp_wrap_to_last:
    ; Continue from current position (or head) to find last focusable
    CMP BX, 0
    JZ .fp_from_head
    MOV BX, [BX + CTRL_NEXT]   ; skip past current
    JMP .fp_tail_scan
.fp_from_head:
    MOV BX, [SI + WIN_FIRST]
.fp_tail_scan:
    OR BX, BX
    JZ .fp_check_last
    TEST BYTE [BX + CTRL_FLAGS], CTRLF_FOCUSABLE
    JZ .fp_tail_next
    TEST BYTE [BX + CTRL_FLAGS], CTRLF_ENABLED
    JZ .fp_tail_next
    MOV CX, BX
.fp_tail_next:
    MOV BX, [BX + CTRL_NEXT]
    JMP .fp_tail_scan

.fp_check_last:
    OR CX, CX
    JZ .fp_no_change

.fp_set_focus:
    CMP CX, DX
    JZ .fp_no_change            ; same as current
    MOV [SI + WIN_FOCUS], CX
    MOV BYTE [fw_state + FW_DIRTY], 1
    CLC
    JMP .fp_ret

.fp_start_from_head:
    ; No current focus — find last focusable
    MOV BX, [SI + WIN_FIRST]
    XOR CX, CX
.fp_head_loop:
    OR BX, BX
    JZ .fp_head_done
    TEST BYTE [BX + CTRL_FLAGS], CTRLF_FOCUSABLE
    JZ .fp_head_next
    TEST BYTE [BX + CTRL_FLAGS], CTRLF_ENABLED
    JZ .fp_head_next
    MOV CX, BX
.fp_head_next:
    MOV BX, [BX + CTRL_NEXT]
    JMP .fp_head_loop
.fp_head_done:
    OR CX, CX
    JZ .fp_no_change
    MOV [SI + WIN_FOCUS], CX
    MOV BYTE [fw_state + FW_DIRTY], 1
    CLC
    JMP .fp_ret

.fp_no_change:
    STC

.fp_ret:
    POP DX
    POP CX
    POP BX
    POP AX
    RET
ENDP

; ----------------------------------------------------------------------------
; tui_ctrl_activate - Activate the focused control (button press)
; Input: SI = window struct pointer
; ----------------------------------------------------------------------------
tui_ctrl_activate: PROC
    PUSH AX
    PUSH BX
    PUSH DI

    MOV BX, [SI + WIN_FOCUS]
    OR BX, BX
    JZ .done

    CMP BYTE [BX + CTRL_TYPE], CTYPE_BUTTON
    JZ .do_button
    CMP BYTE [BX + CTRL_TYPE], CTYPE_CHECKBOX
    JZ .do_checkbox
    CMP BYTE [BX + CTRL_TYPE], CTYPE_RADIO
    JZ .do_radio
    CMP BYTE [BX + CTRL_TYPE], CTYPE_LISTBOX
    JZ .call_handler
    CMP BYTE [BX + CTRL_TYPE], CTYPE_TEXTBOX
    JZ .do_textbox
    JMP .done

.do_textbox:
    ; Enter on a textbox: find and activate the first button in the window
    MOV BX, [SI + WIN_FIRST]
.tb_find_btn:
    OR BX, BX
    JZ .done                    ; no button found
    CMP BYTE [BX + CTRL_TYPE], CTYPE_BUTTON
    JZ .call_handler            ; found a button — activate it
    MOV BX, [BX + CTRL_NEXT]
    JMP .tb_find_btn

.do_button:
    JMP .call_handler

.do_checkbox:
    ; Toggle: 0->1, 1->0
    MOV AL, [BX + CTRL_CB_STATE]
    XOR AL, 1
    MOV [BX + CTRL_CB_STATE], AL
    MOV BYTE [fw_state + FW_DIRTY], 1
    JMP .call_handler

.do_radio:
    ; Mutual exclusion: clear all radios in window, set this one
    PUSH BX                     ; save activated radio
    MOV BX, [SI + WIN_FIRST]
.radio_clear:
    OR BX, BX
    JZ .radio_clear_done
    CMP BYTE [BX + CTRL_TYPE], CTYPE_RADIO
    JNE .radio_clear_next
    MOV BYTE [BX + CTRL_CB_STATE], 0
.radio_clear_next:
    MOV BX, [BX + CTRL_NEXT]
    JMP .radio_clear
.radio_clear_done:
    POP BX                      ; restore activated radio
    MOV BYTE [BX + CTRL_CB_STATE], 1
    MOV BYTE [fw_state + FW_DIRTY], 1
    JMP .call_handler

.call_handler:
    MOV AX, [BX + CTRL_HANDLER]
    OR AX, AX
    JZ .done

    ; Call handler: SI = control ptr, DI = window ptr
    ; Use RET-trampoline since CALL reg is not supported
    MOV WORD [ctrl_dispatch_addr], AX
    MOV DI, SI              ; DI = window struct
    MOV SI, BX              ; SI = control struct
    MOV AX, .after_handler
    PUSH AX                 ; push return address
    MOV AX, [ctrl_dispatch_addr]
    PUSH AX                 ; push handler address
    RET                     ; pops handler addr -> IP
.after_handler:
    MOV SI, DI              ; restore SI to window struct

.done:
    POP DI
    POP BX
    POP AX
    RET
ENDP

; ----------------------------------------------------------------------------
; tui_ctrl_draw_textbox - Draw a TextBox control
; Input: BX = control pointer, DH = abs row, DL = abs col, CH = focused flag
; Preserves: BX, DI, DH, DL
; ----------------------------------------------------------------------------
tui_ctrl_draw_textbox: PROC
    PUSH SI
    PUSH AX
    PUSH CX
    PUSH DX

    ; Select attribute based on focus
    OR CH, CH
    JZ .tb_normal_attr
    MOV AH, [BX + CTRL_FATTR]
    JMP .tb_attr_set
.tb_normal_attr:
    MOV AH, [BX + CTRL_ATTR]
.tb_attr_set:
    PUSH AX                     ; save attr in stack

    ; Fill entire width with spaces (background)
    MOV AL, 20h
    MOV CL, [BX + CTRL_W]
    CALL tui_hline

    ; Draw visible text char-by-char from text[scroll]
    MOV SI, [BX + CTRL_TEXT]
    MOV AL, [BX + CTRL_TB_SCROLL]
    XOR AH, AH
    ADD SI, AX                  ; SI = text + scroll

    POP AX                      ; restore attr (AH = attr)
    PUSH DX                     ; save original DL position
    MOV CL, [BX + CTRL_W]      ; max chars to draw

.tb_draw_loop:
    OR CL, CL
    JZ .tb_draw_done
    CMP BYTE [SI], 0
    JZ .tb_draw_done
    MOV AL, [SI]
    CALL tui_putchar
    INC DL
    INC SI
    DEC CL
    JMP .tb_draw_loop

.tb_draw_done:
    POP DX                      ; restore original col

    ; If focused: draw block cursor
    OR CH, CH
    JZ .tb_no_cursor

    ; Cursor position on screen = DL + (curpos - scroll)
    PUSH DX
    MOV AL, [BX + CTRL_TB_CURPOS]
    SUB AL, [BX + CTRL_TB_SCROLL]
    ADD DL, AL

    ; Character under cursor = text[curpos] or space if at end
    MOV SI, [BX + CTRL_TEXT]
    MOV AL, [BX + CTRL_TB_CURPOS]
    XOR AH, AH
    ADD SI, AX
    MOV AL, [SI]
    OR AL, AL
    JNZ .tb_cursor_char
    MOV AL, 20h                 ; space if at end of text
.tb_cursor_char:
    MOV AH, CLR_TB_CURSOR
    CALL tui_putchar
    POP DX

.tb_no_cursor:
    POP DX
    POP CX
    POP AX
    POP SI
    RET
ENDP

; ----------------------------------------------------------------------------
; tui_ctrl_draw_checkbox - Draw a Checkbox control
; Input: BX = control pointer, DH = abs row, DL = abs col, CH = focused flag
; Renders: [X] Label  or  [ ] Label
; Preserves: BX, DI, DH, DL
; ----------------------------------------------------------------------------
tui_ctrl_draw_checkbox: PROC
    PUSH SI
    PUSH AX
    PUSH CX
    PUSH DX

    ; Select attribute based on focus
    OR CH, CH
    JZ .cb_normal_attr
    MOV AH, [BX + CTRL_FATTR]
    JMP .cb_attr_set
.cb_normal_attr:
    MOV AH, [BX + CTRL_ATTR]
.cb_attr_set:

    ; Fill CTRL_W cells with spaces (background)
    MOV AL, 20h
    MOV CL, [BX + CTRL_W]
    CALL tui_hline

    ; Draw '[' at col
    MOV AL, '['
    CALL tui_putchar

    ; Draw X or space at col+1
    INC DL
    CMP BYTE [BX + CTRL_CB_STATE], 0
    JZ .cb_unchecked
    MOV AL, 'X'
    JMP .cb_draw_mark
.cb_unchecked:
    MOV AL, 20h
.cb_draw_mark:
    CALL tui_putchar

    ; Draw ']' at col+2
    INC DL
    MOV AL, ']'
    CALL tui_putchar

    ; Draw label at col+4 (skip 1 space)
    INC DL
    INC DL
    MOV SI, [BX + CTRL_TEXT]
    CALL tui_putstr

    POP DX
    POP CX
    POP AX
    POP SI
    RET
ENDP

; ----------------------------------------------------------------------------
; tui_ctrl_draw_radio - Draw a Radio button control
; Input: BX = control pointer, DH = abs row, DL = abs col, CH = focused flag
; Renders: (*) Label  or  ( ) Label
; Uses CTRL_CB_STATE for selected state (same layout as checkbox).
; Preserves: BX, DI, DH, DL
; ----------------------------------------------------------------------------
tui_ctrl_draw_radio: PROC
    PUSH SI
    PUSH AX
    PUSH CX
    PUSH DX

    ; Select attribute based on focus
    OR CH, CH
    JZ .rb_normal_attr
    MOV AH, [BX + CTRL_FATTR]
    JMP .rb_attr_set
.rb_normal_attr:
    MOV AH, [BX + CTRL_ATTR]
.rb_attr_set:

    ; Fill CTRL_W cells with spaces (background)
    MOV AL, 20h
    MOV CL, [BX + CTRL_W]
    CALL tui_hline

    ; Draw '(' at col
    MOV AL, '('
    CALL tui_putchar

    ; Draw * or space at col+1
    INC DL
    CMP BYTE [BX + CTRL_CB_STATE], 0
    JZ .rb_unselected
    MOV AL, '*'
    JMP .rb_draw_mark
.rb_unselected:
    MOV AL, 20h
.rb_draw_mark:
    CALL tui_putchar

    ; Draw ')' at col+2
    INC DL
    MOV AL, ')'
    CALL tui_putchar

    ; Draw label at col+4 (skip 1 space)
    INC DL
    INC DL
    MOV SI, [BX + CTRL_TEXT]
    CALL tui_putstr

    POP DX
    POP CX
    POP AX
    POP SI
    RET
ENDP

; ----------------------------------------------------------------------------
; tui_ctrl_handle_key - Route key to focused TextBox (if any)
; Input:  AH = key type, AL = char/scan
; Output: CF=0 handled, CF=1 not handled
; Clobbers: nothing (all regs saved/restored)
; ----------------------------------------------------------------------------
tui_ctrl_handle_key: PROC
    PUSH BX
    PUSH CX
    PUSH DX
    PUSH SI
    PUSH DI

    ; Get active window
    MOV DL, [fw_state + FW_TOPWIN]
    CMP DL, 0FFh
    JNZ .hk_have_win
    JMP .not_handled
.hk_have_win:

    ; Get window struct: SI = win_table + topwin * WIN_SIZE
    PUSH AX
    MOV AL, DL
    XOR AH, AH
    MOV BL, WIN_SIZE
    MUL BL
    MOV SI, AX
    ADD SI, win_table
    POP AX

    ; Get focused control
    MOV BX, [SI + WIN_FOCUS]
    OR BX, BX
    JNZ .hk_have_focus
    JMP .not_handled
.hk_have_focus:

    ; Check for listbox
    CMP BYTE [BX + CTRL_TYPE], CTYPE_LISTBOX
    JNZ .hk_not_listbox
    CALL tui_lb_handle_key
    JMP .ret
.hk_not_listbox:

    ; Check for editor
    CMP BYTE [BX + CTRL_TYPE], CTYPE_EDITOR
    JNZ .hk_not_editor
    CALL tui_ed_handle_key
    JMP .ret
.hk_not_editor:

    ; Must be a TextBox
    CMP BYTE [BX + CTRL_TYPE], CTYPE_TEXTBOX
    JZ .hk_is_textbox
    JMP .not_handled
.hk_is_textbox:

    ; Dispatch based on key type
    CMP AH, KTYPE_EXTENDED
    JZ .tb_extended

    ; --- Normal keys ---
    CMP AL, KEY_BACKSPACE
    JZ .tb_backspace
    CMP AL, 20h
    JNB .hk_above_space
    JMP .not_handled
.hk_above_space:
    CMP AL, 7Eh
    JBE .hk_printable
    JMP .not_handled
.hk_printable:

    ; Printable char (20h-7Eh) → insert
    CALL tui_tb_insert_char
    CALL tui_tb_ensure_visible
    MOV BYTE [fw_state + FW_DIRTY], 1
    CLC
    JMP .ret

.tb_backspace:
    CALL tui_tb_backspace
    CALL tui_tb_ensure_visible
    MOV BYTE [fw_state + FW_DIRTY], 1
    CLC
    JMP .ret

.tb_extended:
    ; --- Extended keys (AL = scan code) ---
    CMP AL, KEY_LEFT
    JZ .tb_left
    CMP AL, KEY_RIGHT
    JZ .tb_right
    CMP AL, KEY_HOME
    JZ .tb_home
    CMP AL, KEY_END
    JZ .tb_end
    CMP AL, KEY_DELETE
    JZ .tb_delete
    JMP .not_handled            ; Up/Down/F6/etc → fall through

.tb_left:
    CALL tui_tb_cursor_left
    CALL tui_tb_ensure_visible
    MOV BYTE [fw_state + FW_DIRTY], 1
    CLC
    JMP .ret

.tb_right:
    CALL tui_tb_cursor_right
    CALL tui_tb_ensure_visible
    MOV BYTE [fw_state + FW_DIRTY], 1
    CLC
    JMP .ret

.tb_home:
    CALL tui_tb_home
    CALL tui_tb_ensure_visible
    MOV BYTE [fw_state + FW_DIRTY], 1
    CLC
    JMP .ret

.tb_end:
    CALL tui_tb_end
    CALL tui_tb_ensure_visible
    MOV BYTE [fw_state + FW_DIRTY], 1
    CLC
    JMP .ret

.tb_delete:
    CALL tui_tb_delete
    CALL tui_tb_ensure_visible
    MOV BYTE [fw_state + FW_DIRTY], 1
    CLC
    JMP .ret

.not_handled:
    STC

.ret:
    POP DI
    POP SI
    POP DX
    POP CX
    POP BX
    RET
ENDP

; ----------------------------------------------------------------------------
; tui_tb_insert_char - Insert AL at cursor position
; Input: BX = TextBox control pointer, AL = character to insert
; ----------------------------------------------------------------------------
tui_tb_insert_char: PROC
    PUSH CX
    PUSH DI
    PUSH AX

    ; Check if buffer is full
    MOV CL, [BX + CTRL_TB_LEN]
    CMP CL, [BX + CTRL_TB_MAXLEN]
    JAE .ins_done

    ; Shift tail right: from text+len down to text+curpos (includes null)
    ; DI starts at text+len, copies [DI] to [DI+1], dec DI
    MOV DI, [BX + CTRL_TEXT]
    MOV CL, [BX + CTRL_TB_LEN]
    XOR CH, CH                  ; CH must be zero for ADD DI, CX
    ADD DI, CX                  ; DI = text + len

    ; Count = len - curpos + 1 (includes null terminator)
    ; CL still = len from above
    SUB CL, [BX + CTRL_TB_CURPOS] ; CL = len - curpos
    INC CL                      ; +1 for null

.ins_shift:
    OR CL, CL
    JZ .ins_write
    MOV AH, [DI]
    MOV [DI + 1], AH
    DEC DI
    DEC CL
    JMP .ins_shift

.ins_write:
    ; DI now points to text+curpos-1, write char to [DI+1]
    POP AX                      ; restore AL = char
    MOV [DI + 1], AL
    INC BYTE [BX + CTRL_TB_CURPOS]
    INC BYTE [BX + CTRL_TB_LEN]
    JMP .ins_ret

.ins_done:
    POP AX
.ins_ret:
    POP DI
    POP CX
    RET
ENDP

; ----------------------------------------------------------------------------
; tui_tb_backspace - Delete character before cursor
; Input: BX = TextBox control pointer
; ----------------------------------------------------------------------------
tui_tb_backspace: PROC
    PUSH CX
    PUSH SI
    PUSH DI

    CMP BYTE [BX + CTRL_TB_CURPOS], 0
    JZ .bks_done

    ; Shift tail left: source = text+curpos, dest = text+curpos-1
    MOV SI, [BX + CTRL_TEXT]
    MOV DI, SI                  ; DI = text base
    MOV CL, [BX + CTRL_TB_CURPOS]
    XOR CH, CH
    ADD SI, CX                  ; SI = text + curpos
    MOV DI, SI
    DEC DI                      ; DI = text + curpos - 1

    ; Count = len - curpos + 1 (includes null)
    MOV CL, [BX + CTRL_TB_LEN]
    SUB CL, [BX + CTRL_TB_CURPOS]
    INC CL

.bks_shift:
    OR CL, CL
    JZ .bks_update
    MOV AL, [SI]
    MOV [DI], AL
    INC SI
    INC DI
    DEC CL
    JMP .bks_shift

.bks_update:
    DEC BYTE [BX + CTRL_TB_CURPOS]
    DEC BYTE [BX + CTRL_TB_LEN]

.bks_done:
    POP DI
    POP SI
    POP CX
    RET
ENDP

; ----------------------------------------------------------------------------
; tui_tb_delete - Delete character at cursor position
; Input: BX = TextBox control pointer
; ----------------------------------------------------------------------------
tui_tb_delete: PROC
    PUSH CX
    PUSH SI
    PUSH DI

    MOV CL, [BX + CTRL_TB_CURPOS]
    CMP CL, [BX + CTRL_TB_LEN]
    JAE .del_done

    ; Shift tail left: source = text+curpos+1, dest = text+curpos
    MOV DI, [BX + CTRL_TEXT]
    XOR CH, CH
    ADD DI, CX                  ; DI = text + curpos
    MOV SI, DI
    INC SI                      ; SI = text + curpos + 1

    ; Count = len - curpos (includes rest + null)
    MOV CL, [BX + CTRL_TB_LEN]
    SUB CL, [BX + CTRL_TB_CURPOS]

.del_shift:
    OR CL, CL
    JZ .del_update
    MOV AL, [SI]
    MOV [DI], AL
    INC SI
    INC DI
    DEC CL
    JMP .del_shift

.del_update:
    DEC BYTE [BX + CTRL_TB_LEN]

.del_done:
    POP DI
    POP SI
    POP CX
    RET
ENDP

; ----------------------------------------------------------------------------
; tui_tb_cursor_left - Move cursor one position left
; Input: BX = TextBox control pointer
; ----------------------------------------------------------------------------
tui_tb_cursor_left: PROC
    CMP BYTE [BX + CTRL_TB_CURPOS], 0
    JZ .cl_done
    DEC BYTE [BX + CTRL_TB_CURPOS]
.cl_done:
    RET
ENDP

; ----------------------------------------------------------------------------
; tui_tb_cursor_right - Move cursor one position right
; Input: BX = TextBox control pointer
; ----------------------------------------------------------------------------
tui_tb_cursor_right: PROC
    PUSH AX
    MOV AL, [BX + CTRL_TB_CURPOS]
    CMP AL, [BX + CTRL_TB_LEN]
    JAE .cr_done
    INC BYTE [BX + CTRL_TB_CURPOS]
.cr_done:
    POP AX
    RET
ENDP

; ----------------------------------------------------------------------------
; tui_tb_home - Move cursor to beginning
; Input: BX = TextBox control pointer
; ----------------------------------------------------------------------------
tui_tb_home: PROC
    MOV BYTE [BX + CTRL_TB_CURPOS], 0
    RET
ENDP

; ----------------------------------------------------------------------------
; tui_tb_end - Move cursor to end
; Input: BX = TextBox control pointer
; ----------------------------------------------------------------------------
tui_tb_end: PROC
    PUSH AX
    MOV AL, [BX + CTRL_TB_LEN]
    MOV [BX + CTRL_TB_CURPOS], AL
    POP AX
    RET
ENDP

; ----------------------------------------------------------------------------
; tui_tb_ensure_visible - Adjust scroll so cursor is visible
; Input: BX = TextBox control pointer
; If curpos < scroll: scroll = curpos
; If curpos >= scroll + width: scroll = curpos - width + 1
; ----------------------------------------------------------------------------
tui_tb_ensure_visible: PROC
    PUSH AX

    MOV AL, [BX + CTRL_TB_CURPOS]

    ; If curpos < scroll: scroll = curpos
    CMP AL, [BX + CTRL_TB_SCROLL]
    JAE .check_right
    MOV [BX + CTRL_TB_SCROLL], AL
    JMP .ev_done

.check_right:
    ; If curpos >= scroll + width: scroll = curpos - width + 1
    MOV AH, [BX + CTRL_TB_SCROLL]
    ADD AH, [BX + CTRL_W]
    CMP AL, AH
    JB .ev_done
    MOV AH, AL
    SUB AH, [BX + CTRL_W]
    INC AH
    MOV [BX + CTRL_TB_SCROLL], AH

.ev_done:
    POP AX
    RET
ENDP

; ============================================================================
; Dropdown Stubs (TEDIT never creates CTYPE_DROPDOWN; stubs for call sites)
; ============================================================================

tui_dd_close_cancel: PROC
    MOV WORD [fw_state + FW_OPENDD], 0
    MOV BYTE [fw_state + FW_DIRTY], 1
    RET
ENDP

tui_dd_close_commit: PROC
    MOV WORD [fw_state + FW_OPENDD], 0
    MOV BYTE [fw_state + FW_DIRTY], 1
    RET
ENDP
; ============================================================================
; Scroll Bar Drawing Helpers
; ============================================================================

; ----------------------------------------------------------------------------
; tui_draw_vscrollbar - Draw a vertical scroll bar
; Input:  DH/DL = abs top row/col of bar column
;         BL = bar_height, AL = scroll_pos, AH = visible_count, CL = total_count
; Preserves: BX, DI, DH, DL
; sb_scratch usage: [0]=scroll_pos, [1]=visible, [2]=total, [3]=thumb_size
; ----------------------------------------------------------------------------
tui_draw_vscrollbar: PROC
    PUSH SI
    PUSH AX
    PUSH CX
    PUSH DX
    PUSH BP
    PUSH BX

    ; Save params
    MOV [sb_scratch], AL        ; [0] = scroll_pos (never overwritten until thumb calc)
    MOV [sb_scratch + 1], AH   ; [1] = visible
    MOV [sb_scratch + 2], CL   ; [2] = total

    ; --- Draw up arrow ---
    MOV AL, SB_ARROW_UP
    MOV AH, CLR_SB_ARROW
    CALL tui_putchar

    ; --- Draw down arrow ---
    PUSH DX
    MOV AL, BL
    DEC AL
    ADD DH, AL
    MOV AL, SB_ARROW_DN
    MOV AH, CLR_SB_ARROW
    CALL tui_putchar
    POP DX

    ; track_len = bar_height - 2
    SUB BL, 2                   ; BL = track_len
    CMP BL, 1
    JNB .vsb_has_track          ; bar too small for track
    JMP .vsb_done
.vsb_has_track:

    ; --- Fill entire track ---
    PUSH DX
    INC DH
    MOV CL, BL
.vsb_track_loop:
    OR CL, CL
    JZ .vsb_track_done
    MOV AL, SB_TRACK
    MOV AH, CLR_SB_TRACK
    CALL tui_putchar
    INC DH
    DEC CL
    JMP .vsb_track_loop
.vsb_track_done:
    POP DX

    ; Save drawing DX to BP (DIV clobbers DX)
    MOV BP, DX

    ; --- thumb_size = MAX(1, (visible * track_len) / total) ---
    MOV AL, [sb_scratch + 1]   ; visible
    MUL BL                      ; AX = visible * track_len
    MOV CL, [sb_scratch + 2]  ; total
    XOR CH, CH
    XOR DX, DX
    DIV CX                      ; AX = quotient
    OR AL, AL
    JNZ .vsb_ts_ok
    MOV AL, 1
.vsb_ts_ok:
    CMP AL, BL
    JBE .vsb_ts_clamp
    MOV AL, BL
.vsb_ts_clamp:
    MOV [sb_scratch + 3], AL   ; [3] = thumb_size

    ; --- thumb_pos = (scroll * (track_len - thumb_size)) / max_scroll ---
    ; max_scroll = total - visible
    MOV CL, [sb_scratch + 2]  ; total
    SUB CL, [sb_scratch + 1]  ; CL = max_scroll
    XOR CH, CH
    OR CL, CL
    JZ .vsb_pos_zero

    ; range = track_len - thumb_size
    MOV AH, BL
    SUB AH, [sb_scratch + 3]  ; AH = range

    ; scroll_pos * range
    MOV AL, [sb_scratch]       ; scroll_pos
    MUL AH                      ; AX = scroll_pos * range
    XOR DX, DX
    DIV CX                      ; AX = thumb_pos
    ; Clamp thumb_pos so thumb doesn't exceed track
    MOV AH, BL
    SUB AH, [sb_scratch + 3]  ; AH = max thumb_pos
    CMP AL, AH
    JBE .vsb_tp_ok
    MOV AL, AH
.vsb_tp_ok:
    MOV AH, AL                 ; AH = thumb_pos
    JMP .vsb_draw_thumb

.vsb_pos_zero:
    XOR AH, AH

.vsb_draw_thumb:
    ; AH = thumb_pos, [sb_scratch+3] = thumb_size
    MOV DX, BP                  ; restore drawing coords
    INC DH                      ; past up arrow
    ADD DH, AH                  ; + thumb_pos
    MOV CL, [sb_scratch + 3]  ; thumb_size
.vsb_thumb_loop:
    OR CL, CL
    JZ .vsb_done
    MOV AL, SB_THUMB
    MOV AH, CLR_SB_THUMB
    CALL tui_putchar
    INC DH
    DEC CL
    JMP .vsb_thumb_loop

.vsb_done:
    POP BX
    POP BP
    POP DX
    POP CX
    POP AX
    POP SI
    RET
ENDP

; ----------------------------------------------------------------------------
; tui_draw_hscrollbar - Draw a horizontal scroll bar
; Input:  DH/DL = abs row/left col of bar row
;         BL = bar_width, AL = scroll_x, AH = visible_width, CL = max_content_width
; Preserves: BX, DI, DH, DL
; sb_scratch usage: [0]=scroll_x, [1]=visible_w, [2]=total_w, [3]=thumb_size
; ----------------------------------------------------------------------------
tui_draw_hscrollbar: PROC
    PUSH SI
    PUSH AX
    PUSH CX
    PUSH DX
    PUSH BP
    PUSH BX

    ; Save params
    MOV [sb_scratch], AL        ; [0] = scroll_x
    MOV [sb_scratch + 1], AH   ; [1] = visible_w
    MOV [sb_scratch + 2], CL   ; [2] = max_content_w (= total)

    ; --- Draw left arrow ---
    MOV AL, SB_ARROW_LT
    MOV AH, CLR_SB_ARROW
    CALL tui_putchar

    ; --- Draw right arrow ---
    PUSH DX
    MOV AL, BL
    DEC AL
    ADD DL, AL
    MOV AL, SB_ARROW_RT
    MOV AH, CLR_SB_ARROW
    CALL tui_putchar
    POP DX

    ; track_len = bar_width - 2
    SUB BL, 2                   ; BL = track_len
    CMP BL, 1
    JB .hsb_done

    ; --- Fill track ---
    PUSH DX
    INC DL
    MOV AL, SB_TRACK
    MOV AH, CLR_SB_TRACK
    MOV CL, BL
    CALL tui_hline
    POP DX

    ; Save DX to BP
    MOV BP, DX

    ; --- thumb_size = MAX(1, (visible_w * track_len) / total_w) ---
    MOV AL, [sb_scratch + 1]   ; visible_w
    MUL BL                      ; AX = visible_w * track_len
    MOV CL, [sb_scratch + 2]  ; total_w
    XOR CH, CH
    XOR DX, DX
    DIV CX
    OR AL, AL
    JNZ .hsb_ts_ok
    MOV AL, 1
.hsb_ts_ok:
    CMP AL, BL
    JBE .hsb_ts_clamp
    MOV AL, BL
.hsb_ts_clamp:
    MOV [sb_scratch + 3], AL   ; [3] = thumb_size

    ; --- thumb_pos = (scroll_x * (track_len - thumb_size)) / max_scroll ---
    ; max_scroll = total_w - visible_w
    MOV CL, [sb_scratch + 2]
    SUB CL, [sb_scratch + 1]  ; CL = max_scroll
    XOR CH, CH
    OR CL, CL
    JZ .hsb_pos_zero

    MOV AH, BL
    SUB AH, [sb_scratch + 3]  ; AH = range

    MOV AL, [sb_scratch]       ; scroll_x
    MUL AH                      ; AX = scroll_x * range
    XOR DX, DX
    DIV CX                      ; AX = thumb_pos
    MOV AH, BL
    SUB AH, [sb_scratch + 3]
    CMP AL, AH
    JBE .hsb_tp_ok
    MOV AL, AH
.hsb_tp_ok:
    MOV AH, AL
    JMP .hsb_draw_thumb

.hsb_pos_zero:
    XOR AH, AH

.hsb_draw_thumb:
    ; AH = thumb_pos
    MOV DX, BP                  ; restore drawing coords
    INC DL                      ; past left arrow
    ADD DL, AH                  ; + thumb_pos
    MOV CL, [sb_scratch + 3]  ; thumb_size
.hsb_thumb_loop:
    OR CL, CL
    JZ .hsb_done
    MOV AL, SB_THUMB
    MOV AH, CLR_SB_THUMB
    CALL tui_putchar
    INC DL
    DEC CL
    JMP .hsb_thumb_loop

.hsb_done:
    POP BX
    POP BP
    POP DX
    POP CX
    POP AX
    POP SI
    RET
ENDP

; ============================================================================
; Listbox Control
; ============================================================================

; ----------------------------------------------------------------------------
; tui_ctrl_draw_listbox - Draw all visible rows of a listbox
; Input: BX = control, DH = abs row, DL = abs col, CH = focused flag
; Preserves: BX, DI, DH, DL
; Uses sb_scratch[0] for content_w during row loop
; ----------------------------------------------------------------------------
tui_ctrl_draw_listbox: PROC
    PUSH SI
    PUSH AX
    PUSH CX
    PUSH DX
    PUSH BP
    PUSH BX

    MOV BP, BX                  ; BP = control ptr (frees BX for scratch)

    ; --- Determine if vbar needed: COUNT > CTRL_H ---
    XOR BH, BH                  ; BH = needs_vbar (0 or 1)
    MOV AL, [BP + CTRL_LB_COUNT]
    CMP AL, [BP + CTRL_H]
    JBE .lb_no_vbar
    MOV BH, 1
.lb_no_vbar:
    ; content_w = CTRL_W - needs_vbar
    MOV BL, [BP + CTRL_W]
    SUB BL, BH                  ; BL = content_w
    MOV [sb_scratch], BL        ; save content_w for row loop
    PUSH BX                     ; save needs_vbar(BH) on stack

    MOV CL, [BP + CTRL_H]      ; CL = visible rows (loop counter)
    ; CH = focused flag (set by caller)
    MOV AL, [BP + CTRL_LB_SCROLL] ; AL = current item index

.lb_row_loop:
    OR CL, CL
    JZ .lb_draw_rows_done

    CMP AL, [BP + CTRL_LB_COUNT]
    JAE .lb_empty_row

    ; --- Compute attribute (AH) based on selected x focused ---
    MOV AH, [BP + CTRL_ATTR]       ; default: control's normal attribute
    CMP AL, [BP + CTRL_LB_SEL]
    JNZ .lb_not_sel
    ; Selected item: only highlight when focused
    OR CH, CH
    JZ .lb_attr_done                ; unfocused → same as normal (no highlight)
    MOV AH, CLR_LB_FOCSEL          ; focused → highlight
    JMP .lb_attr_done
.lb_not_sel:
    OR CH, CH
    JZ .lb_attr_done
    MOV AH, [BP + CTRL_FATTR]      ; focused non-selected: control's focused attr
.lb_attr_done:

    ; --- Fill row with spaces using content_w ---
    PUSH AX
    PUSH CX
    MOV AL, 20h
    MOV CL, [sb_scratch]       ; content_w
    CALL tui_hline
    POP CX
    POP AX

    ; --- Look up items[idx] and draw text ---
    PUSH AX
    MOV BL, AL
    XOR BH, BH
    SHL BX, 1
    MOV SI, [BP + CTRL_LB_ITEMS]
    ADD SI, BX
    MOV SI, [SI]               ; SI = string pointer
    POP AX
    PUSH AX
    CALL tui_putstr
    POP AX
    JMP .lb_next_row

.lb_empty_row:
    PUSH AX
    PUSH CX
    MOV AH, [BP + CTRL_ATTR]       ; empty rows use control's normal attr
    MOV AL, 20h
    MOV CL, [sb_scratch]       ; content_w
    CALL tui_hline
    POP CX
    POP AX

.lb_next_row:
    INC AL
    INC DH
    DEC CL
    JMP .lb_row_loop

.lb_draw_rows_done:
    POP BX                      ; BH = needs_vbar

    ; --- Draw vertical scroll bar if needed ---
    OR BH, BH
    JZ .lb_draw_done

    ; Peek at original DX from stack
    ; Stack: BX(orig) BP DX CX AX SI
    PUSH BX
    MOV BX, SP
    ADD BX, 6                   ; skip pushed BX(2) + orig BX(2) + BP(2)
    MOV DX, [BX]
    POP BX

    ; vbar col = DL + CTRL_W - 1
    ADD DL, [BP + CTRL_W]
    DEC DL

    MOV BL, [BP + CTRL_H]          ; bar_height
    MOV AL, [BP + CTRL_LB_SCROLL]  ; scroll_pos
    MOV AH, [BP + CTRL_H]          ; visible_count
    MOV CL, [BP + CTRL_LB_COUNT]   ; total_count
    CALL tui_draw_vscrollbar

.lb_draw_done:
    POP BX
    POP BP
    POP DX
    POP CX
    POP AX
    POP SI
    RET
ENDP

; ----------------------------------------------------------------------------
; tui_lb_handle_key - Keyboard navigation for listbox
; Input:  AH = key type, AL = char/scan, BX = control, SI = window
; Output: CF=0 handled, CF=1 not handled
; ----------------------------------------------------------------------------
tui_lb_handle_key: PROC
    ; Only handle extended keys (arrows, home, end, pgup, pgdn)
    CMP AH, KTYPE_EXTENDED
    JZ .lb_hk_ext
    ; Normal keys: not handled (Enter/Space fall through to activate)
    STC
    RET

.lb_hk_ext:
    CMP AL, KEY_UP
    JZ .lb_hk_up
    CMP AL, KEY_DOWN
    JZ .lb_hk_down
    CMP AL, KEY_HOME
    JZ .lb_hk_home
    CMP AL, KEY_END
    JZ .lb_hk_end
    CMP AL, KEY_PGUP
    JZ .lb_hk_pgup
    CMP AL, KEY_PGDN
    JZ .lb_hk_pgdn
    ; Other extended keys: not handled
    STC
    RET

.lb_hk_up:
    MOV AL, [BX + CTRL_LB_SEL]
    OR AL, AL
    JNZ .lb_hk_up_dec
    ; At top: wrap to last item
    MOV AL, [BX + CTRL_LB_COUNT]
    DEC AL
    MOV [BX + CTRL_LB_SEL], AL
    JMP .lb_hk_moved
.lb_hk_up_dec:
    DEC BYTE [BX + CTRL_LB_SEL]
    JMP .lb_hk_moved

.lb_hk_down:
    MOV AL, [BX + CTRL_LB_SEL]
    INC AL
    CMP AL, [BX + CTRL_LB_COUNT]
    JB .lb_hk_down_set
    XOR AL, AL                  ; wrap to 0
.lb_hk_down_set:
    MOV [BX + CTRL_LB_SEL], AL
    JMP .lb_hk_moved

.lb_hk_home:
    MOV BYTE [BX + CTRL_LB_SEL], 0
    JMP .lb_hk_moved

.lb_hk_end:
    MOV AL, [BX + CTRL_LB_COUNT]
    DEC AL
    MOV [BX + CTRL_LB_SEL], AL
    JMP .lb_hk_moved

.lb_hk_pgup:
    MOV AL, [BX + CTRL_LB_SEL]
    CMP AL, [BX + CTRL_H]
    JAE .lb_pgup_sub
    ; Would go negative: clamp to 0
    XOR AL, AL
    MOV [BX + CTRL_LB_SEL], AL
    JMP .lb_hk_moved
.lb_pgup_sub:
    SUB AL, [BX + CTRL_H]
    MOV [BX + CTRL_LB_SEL], AL
    JMP .lb_hk_moved

.lb_hk_pgdn:
    MOV AL, [BX + CTRL_LB_SEL]
    ADD AL, [BX + CTRL_H]
    CMP AL, [BX + CTRL_LB_COUNT]
    JB .lb_pgdn_ok
    ; Clamp to count-1
    MOV AL, [BX + CTRL_LB_COUNT]
    DEC AL
.lb_pgdn_ok:
    MOV [BX + CTRL_LB_SEL], AL
    JMP .lb_hk_moved

.lb_hk_moved:
    CALL tui_lb_ensure_visible
    MOV BYTE [fw_state + FW_DIRTY], 1
    CLC
    RET
ENDP

; ----------------------------------------------------------------------------
; tui_lb_ensure_visible - Scroll adjustment for listbox
; Input: BX = control
; If SEL < SCROLL: SCROLL = SEL
; If SEL >= SCROLL + CTRL_H: SCROLL = SEL - CTRL_H + 1
; ----------------------------------------------------------------------------
tui_lb_ensure_visible: PROC
    PUSH AX

    MOV AL, [BX + CTRL_LB_SEL]

    ; If SEL < SCROLL: SCROLL = SEL
    CMP AL, [BX + CTRL_LB_SCROLL]
    JAE .lb_ev_check_below
    MOV [BX + CTRL_LB_SCROLL], AL
    JMP .lb_ev_done

.lb_ev_check_below:
    ; If SEL >= SCROLL + CTRL_H: SCROLL = SEL - CTRL_H + 1
    MOV AH, [BX + CTRL_LB_SCROLL]
    ADD AH, [BX + CTRL_H]
    CMP AL, AH
    JB .lb_ev_done
    MOV AH, AL
    SUB AH, [BX + CTRL_H]
    INC AH
    MOV [BX + CTRL_LB_SCROLL], AH

.lb_ev_done:
    POP AX
    RET
ENDP
