; ============================================================================
; tui_dialog.inc - Standard Dialog Functions (msgbox, confirm, input)
; ============================================================================

; ----------------------------------------------------------------------------
; tui_dlg_strlen - Measure null-terminated string length
; Input:  SI = string ptr
; Output: CX = length (< 256)
; Preserves all other registers
; ----------------------------------------------------------------------------
tui_dlg_strlen: PROC
    PUSH SI
    XOR CX, CX
.loop:
    CMP BYTE [SI], 0
    JZ .done
    INC SI
    INC CX
    JMP .loop
.done:
    POP SI
    RET
ENDP

; ----------------------------------------------------------------------------
; tui_dlg_run_modal - Create dialog window, run inner event loop, return result
; Input:  dlg_tmpl filled, control structs built in dlg_scratch
; Output: AL = FW_DLG_RESULT
; ----------------------------------------------------------------------------
tui_dlg_run_modal: PROC
    PUSH BX
    PUSH CX
    PUSH DX
    PUSH SI
    PUSH DI

    ; Save current framework state
    MOV AL, [fw_state + FW_RUNNING]
    PUSH AX
    MOV AL, [fw_state + FW_TOPWIN]
    PUSH AX

    ; Create the dialog window from template
    MOV SI, dlg_tmpl
    CALL tui_win_create         ; AL = slot index, or FFh if full
    CMP AL, 0FFh
    JZ .fail

    ; Set modal window
    MOV [fw_state + FW_MODAL_WIN], AL

    ; Default result = cancel (in case of unexpected exit)
    MOV BYTE [fw_state + FW_DLG_RESULT], DLG_CANCEL

    ; Start nested event loop
    MOV BYTE [fw_state + FW_RUNNING], 1
    MOV BYTE [fw_state + FW_DIRTY], 1
    CALL tui_run

    ; Nested loop exited - dialog has been closed by handler or Esc
    MOV BYTE [fw_state + FW_MODAL_WIN], 0FFh

    ; Save result before restoring state
    MOV BL, [fw_state + FW_DLG_RESULT]

    ; Restore previous framework state
    POP AX
    MOV [fw_state + FW_TOPWIN], AL
    POP AX
    MOV [fw_state + FW_RUNNING], AL

    ; Trigger redraw of parent windows
    MOV BYTE [fw_state + FW_DIRTY], 1

    MOV AL, BL                  ; AL = dialog result
    JMP .ret

.fail:
    ; Window table full - discard saved state, return cancel
    POP AX
    POP AX
    MOV AL, DLG_CANCEL

.ret:
    POP DI
    POP SI
    POP DX
    POP CX
    POP BX
    RET
ENDP

; ============================================================================
; Dialog button handlers (called via RET-trampoline: SI=ctrl, DI=window)
; ============================================================================

dlg_handler_ok:
    MOV BYTE [fw_state + FW_DLG_RESULT], DLG_OK
    MOV AL, [DI + WIN_ID]
    CALL tui_win_close
    MOV BYTE [fw_state + FW_RUNNING], 0
    RET

dlg_handler_cancel:
    MOV BYTE [fw_state + FW_DLG_RESULT], DLG_CANCEL
    MOV AL, [DI + WIN_ID]
    CALL tui_win_close
    MOV BYTE [fw_state + FW_RUNNING], 0
    RET

dlg_handler_yes:
    MOV BYTE [fw_state + FW_DLG_RESULT], DLG_YES
    MOV AL, [DI + WIN_ID]
    CALL tui_win_close
    MOV BYTE [fw_state + FW_RUNNING], 0
    RET

dlg_handler_no:
    MOV BYTE [fw_state + FW_DLG_RESULT], DLG_NO
    MOV AL, [DI + WIN_ID]
    CALL tui_win_close
    MOV BYTE [fw_state + FW_RUNNING], 0
    RET

dlg_handler_repl_all:
    MOV BYTE [fw_state + FW_DLG_RESULT], DLG_REPL_ALL
    MOV AL, [DI + WIN_ID]
    CALL tui_win_close
    MOV BYTE [fw_state + FW_RUNNING], 0
    RET

; ============================================================================
; tui_dlg_msgbox - Message box with OK button
; Input:  SI = message string ptr, DI = title string ptr
; Output: (none - blocks until OK or Esc)
; ============================================================================
tui_dlg_msgbox: PROC
    PushAll
    PUSH DI                     ; save title ptr

    ; Measure message length
    CALL tui_dlg_strlen         ; CX = msg_len, SI preserved

    ; Compute interior width = max(msg_len+4, 12, 20)
    MOV AL, CL
    ADD AL, DLG_PAD
    CMP AL, DLG_BTN_OK_W + DLG_PAD
    JAE .w1
    MOV AL, DLG_BTN_OK_W + DLG_PAD
.w1:
    CMP AL, DLG_MIN_W
    JAE .w2
    MOV AL, DLG_MIN_W
.w2:
    MOV BL, AL                  ; BL = interior width
    ADD AL, 2
    MOV BH, AL                  ; BH = total width (with borders)

    ; Center dialog on screen
    MOV AL, SCR_W
    SUB AL, BH
    SHR AL, 1
    MOV DL, AL                  ; DL = dialog X
    MOV DH, (SCR_H - 6) / 2    ; DH = dialog Y (centered)

    ; ---- Build message label at dlg_label ----
    ; Center msg: label_x = (interior_w - msg_len) / 2
    MOV AL, BL
    SUB AL, CL
    SHR AL, 1

    MOV DI, dlg_label
    MOV BYTE [DI + CTRL_TYPE], CTYPE_LABEL
    MOV BYTE [DI + CTRL_FLAGS], CTRLF_VISIBLE
    MOV [DI + CTRL_X], AL
    MOV BYTE [DI + CTRL_Y], 1
    MOV [DI + CTRL_W], CL
    MOV BYTE [DI + CTRL_H], 1
    MOV WORD [DI + CTRL_NEXT], dlg_btn1
    MOV [DI + CTRL_TEXT], SI
    MOV BYTE [DI + CTRL_ATTR], CLR_LABEL
    MOV BYTE [DI + CTRL_FATTR], CLR_LABEL
    MOV WORD [DI + CTRL_HANDLER], 0

    ; ---- Build OK button at dlg_btn1 ----
    ; Center: btn_x = (interior_w - 8) / 2
    MOV AL, BL
    SUB AL, DLG_BTN_OK_W
    SHR AL, 1

    MOV DI, dlg_btn1
    MOV BYTE [DI + CTRL_TYPE], CTYPE_BUTTON
    MOV BYTE [DI + CTRL_FLAGS], CTRLF_VISIBLE + CTRLF_ENABLED + CTRLF_FOCUSABLE
    MOV [DI + CTRL_X], AL
    MOV BYTE [DI + CTRL_Y], 3
    MOV BYTE [DI + CTRL_W], DLG_BTN_OK_W
    MOV BYTE [DI + CTRL_H], 1
    MOV WORD [DI + CTRL_NEXT], 0
    MOV WORD [DI + CTRL_TEXT], dlg_str_ok
    MOV BYTE [DI + CTRL_ATTR], CLR_BTN
    MOV BYTE [DI + CTRL_FATTR], CLR_BTN_FOCUS
    MOV WORD [DI + CTRL_HANDLER], dlg_handler_ok

    ; ---- Build window template at dlg_tmpl ----
    POP AX                      ; AX = title ptr
    MOV DI, dlg_tmpl
    MOV BYTE [DI + WIN_FLAGS], WINF_VISIBLE + WINF_BORDER + WINF_TITLE + WINF_SHADOW
    MOV [DI + WIN_X], DL
    MOV [DI + WIN_Y], DH
    MOV [DI + WIN_W], BH
    MOV BYTE [DI + WIN_H], 6
    MOV BYTE [DI + WIN_ATTR], CLR_WIN_BG
    MOV BYTE [DI + WIN_BATTR], CLR_WIN_BDR
    MOV BYTE [DI + WIN_TATTR], CLR_WIN_TTL
    MOV [DI + WIN_TITLE], AX
    MOV WORD [DI + WIN_FIRST], dlg_label
    MOV WORD [DI + WIN_FOCUS], dlg_btn1
    MOV WORD [DI + WIN_HANDLER], 0
    MOV BYTE [DI + WIN_ZORDER], 0
    MOV BYTE [DI + WIN_ID], 0
    MOV BYTE [DI + WIN_SCROLLX], 0
    MOV BYTE [DI + WIN_SCROLLY], 0

    CALL tui_dlg_run_modal

    PopAll
    RET
ENDP

; ============================================================================
; tui_dlg_confirm - Yes/No confirmation dialog
; Input:  SI = message string ptr, DI = title string ptr
; Output: AL = DLG_YES (1) or DLG_NO (0)
; ============================================================================
tui_dlg_confirm: PROC
    PushAll
    PUSH DI                     ; save title ptr

    ; Measure message length
    CALL tui_dlg_strlen         ; CX = msg_len

    ; Interior width = max(msg_len+4, 22, 20)
    ; where 22 = 8+8+2+4 (two buttons + gap + padding)
    MOV AL, CL
    ADD AL, DLG_PAD
    CMP AL, DLG_BTN_YN_W + DLG_BTN_YN_W + DLG_BTN_GAP + DLG_PAD
    JAE .w1
    MOV AL, DLG_BTN_YN_W + DLG_BTN_YN_W + DLG_BTN_GAP + DLG_PAD
.w1:
    CMP AL, DLG_MIN_W
    JAE .w2
    MOV AL, DLG_MIN_W
.w2:
    MOV BL, AL                  ; BL = interior width
    ADD AL, 2
    MOV BH, AL                  ; BH = total width

    ; Center on screen
    MOV AL, SCR_W
    SUB AL, BH
    SHR AL, 1
    MOV DL, AL                  ; DL = dialog X
    MOV DH, (SCR_H - 6) / 2    ; DH = dialog Y (centered)

    ; ---- Build message label ----
    MOV AL, BL
    SUB AL, CL
    SHR AL, 1

    MOV DI, dlg_label
    MOV BYTE [DI + CTRL_TYPE], CTYPE_LABEL
    MOV BYTE [DI + CTRL_FLAGS], CTRLF_VISIBLE
    MOV [DI + CTRL_X], AL
    MOV BYTE [DI + CTRL_Y], 1
    MOV [DI + CTRL_W], CL
    MOV BYTE [DI + CTRL_H], 1
    MOV WORD [DI + CTRL_NEXT], dlg_btn1
    MOV [DI + CTRL_TEXT], SI
    MOV BYTE [DI + CTRL_ATTR], CLR_LABEL
    MOV BYTE [DI + CTRL_FATTR], CLR_LABEL
    MOV WORD [DI + CTRL_HANDLER], 0

    ; ---- Build Yes and No buttons ----
    ; Center both: total_btn_w = 8+2+8 = 18
    ; btn_start_x = (interior_w - 18) / 2
    MOV AL, BL
    SUB AL, DLG_BTN_YN_W + DLG_BTN_GAP + DLG_BTN_YN_W
    SHR AL, 1
    MOV CH, AL                  ; CH = yes_x

    MOV DI, dlg_btn1
    MOV BYTE [DI + CTRL_TYPE], CTYPE_BUTTON
    MOV BYTE [DI + CTRL_FLAGS], CTRLF_VISIBLE + CTRLF_ENABLED + CTRLF_FOCUSABLE
    MOV [DI + CTRL_X], CH
    MOV BYTE [DI + CTRL_Y], 3
    MOV BYTE [DI + CTRL_W], DLG_BTN_YN_W
    MOV BYTE [DI + CTRL_H], 1
    MOV WORD [DI + CTRL_NEXT], dlg_btn2
    MOV WORD [DI + CTRL_TEXT], dlg_str_yes
    MOV BYTE [DI + CTRL_ATTR], CLR_BTN
    MOV BYTE [DI + CTRL_FATTR], CLR_BTN_FOCUS
    MOV WORD [DI + CTRL_HANDLER], dlg_handler_yes

    ; No button: x = yes_x + 8 + 2
    MOV AL, CH
    ADD AL, DLG_BTN_YN_W + DLG_BTN_GAP

    MOV DI, dlg_btn2
    MOV BYTE [DI + CTRL_TYPE], CTYPE_BUTTON
    MOV BYTE [DI + CTRL_FLAGS], CTRLF_VISIBLE + CTRLF_ENABLED + CTRLF_FOCUSABLE
    MOV [DI + CTRL_X], AL
    MOV BYTE [DI + CTRL_Y], 3
    MOV BYTE [DI + CTRL_W], DLG_BTN_YN_W
    MOV BYTE [DI + CTRL_H], 1
    MOV WORD [DI + CTRL_NEXT], 0
    MOV WORD [DI + CTRL_TEXT], dlg_str_no
    MOV BYTE [DI + CTRL_ATTR], CLR_BTN
    MOV BYTE [DI + CTRL_FATTR], CLR_BTN_FOCUS
    MOV WORD [DI + CTRL_HANDLER], dlg_handler_no

    ; ---- Build window template ----
    POP AX                      ; AX = title ptr
    MOV DI, dlg_tmpl
    MOV BYTE [DI + WIN_FLAGS], WINF_VISIBLE + WINF_BORDER + WINF_TITLE + WINF_SHADOW
    MOV [DI + WIN_X], DL
    MOV [DI + WIN_Y], DH
    MOV [DI + WIN_W], BH
    MOV BYTE [DI + WIN_H], 6
    MOV BYTE [DI + WIN_ATTR], CLR_WIN_BG
    MOV BYTE [DI + WIN_BATTR], CLR_WIN_BDR
    MOV BYTE [DI + WIN_TATTR], CLR_WIN_TTL
    MOV [DI + WIN_TITLE], AX
    MOV WORD [DI + WIN_FIRST], dlg_label
    MOV WORD [DI + WIN_FOCUS], dlg_btn1
    MOV WORD [DI + WIN_HANDLER], 0
    MOV BYTE [DI + WIN_ZORDER], 0
    MOV BYTE [DI + WIN_ID], 0
    MOV BYTE [DI + WIN_SCROLLX], 0
    MOV BYTE [DI + WIN_SCROLLY], 0

    CALL tui_dlg_run_modal

    PopAll
    MOV AL, [fw_state + FW_DLG_RESULT]
    RET
ENDP

; ============================================================================
; tui_dlg_confirm4 - 4-button confirm dialog: [Yes] [No] [All] [Cancel]
; Input:  SI = message string ptr, DI = title string ptr
; Output: AL = DLG_YES (1), DLG_NO (0), DLG_REPL_ALL (3), or DLG_CANCEL (2)
; ============================================================================
tui_dlg_confirm4: PROC
    PushAll
    PUSH DI                     ; save title ptr

    ; Measure message length
    CALL tui_dlg_strlen         ; CX = msg_len

    ; Interior width = max(msg_len+4, 44, 20)
    ; 44 = 8+2+8+2+8+2+10+4 (four buttons + gaps + padding)
    MOV AL, CL
    ADD AL, DLG_PAD
    CMP AL, DLG_BTN_YN_W + DLG_BTN_GAP + DLG_BTN_YN_W + DLG_BTN_GAP + DLG_BTN_ALL_W + DLG_BTN_GAP + DLG_BTN_CAN_W + DLG_PAD
    JAE .c4w1
    MOV AL, DLG_BTN_YN_W + DLG_BTN_GAP + DLG_BTN_YN_W + DLG_BTN_GAP + DLG_BTN_ALL_W + DLG_BTN_GAP + DLG_BTN_CAN_W + DLG_PAD
.c4w1:
    CMP AL, DLG_MIN_W
    JAE .c4w2
    MOV AL, DLG_MIN_W
.c4w2:
    MOV BL, AL                  ; BL = interior width
    ADD AL, 2
    MOV BH, AL                  ; BH = total width

    ; Center on screen
    MOV AL, SCR_W
    SUB AL, BH
    SHR AL, 1
    MOV DL, AL                  ; DL = dialog X
    MOV DH, (SCR_H - 6) / 2    ; DH = dialog Y

    ; ---- Build message label ----
    MOV AL, BL
    SUB AL, CL
    SHR AL, 1

    MOV DI, dlg_label
    MOV BYTE [DI + CTRL_TYPE], CTYPE_LABEL
    MOV BYTE [DI + CTRL_FLAGS], CTRLF_VISIBLE
    MOV [DI + CTRL_X], AL
    MOV BYTE [DI + CTRL_Y], 1
    MOV [DI + CTRL_W], CL
    MOV BYTE [DI + CTRL_H], 1
    MOV WORD [DI + CTRL_NEXT], dlg_btn1
    MOV [DI + CTRL_TEXT], SI
    MOV BYTE [DI + CTRL_ATTR], CLR_LABEL
    MOV BYTE [DI + CTRL_FATTR], CLR_LABEL
    MOV WORD [DI + CTRL_HANDLER], 0

    ; ---- Build 4 buttons ----
    ; total_btn_w = 8+2+8+2+8+2+10 = 40
    ; btn_start_x = (interior_w - 40) / 2
    MOV AL, BL
    SUB AL, DLG_BTN_YN_W + DLG_BTN_GAP + DLG_BTN_YN_W + DLG_BTN_GAP + DLG_BTN_ALL_W + DLG_BTN_GAP + DLG_BTN_CAN_W
    SHR AL, 1
    MOV CH, AL                  ; CH = btn1 x

    ; btn1: Yes
    MOV DI, dlg_btn1
    MOV BYTE [DI + CTRL_TYPE], CTYPE_BUTTON
    MOV BYTE [DI + CTRL_FLAGS], CTRLF_VISIBLE + CTRLF_ENABLED + CTRLF_FOCUSABLE
    MOV [DI + CTRL_X], CH
    MOV BYTE [DI + CTRL_Y], 3
    MOV BYTE [DI + CTRL_W], DLG_BTN_YN_W
    MOV BYTE [DI + CTRL_H], 1
    MOV WORD [DI + CTRL_NEXT], dlg_btn2
    MOV WORD [DI + CTRL_TEXT], dlg_str_yes
    MOV BYTE [DI + CTRL_ATTR], CLR_BTN
    MOV BYTE [DI + CTRL_FATTR], CLR_BTN_FOCUS
    MOV WORD [DI + CTRL_HANDLER], dlg_handler_yes

    ; btn2: No — x = CH + 8 + 2
    MOV AL, CH
    ADD AL, DLG_BTN_YN_W + DLG_BTN_GAP
    MOV DI, dlg_btn2
    MOV BYTE [DI + CTRL_TYPE], CTYPE_BUTTON
    MOV BYTE [DI + CTRL_FLAGS], CTRLF_VISIBLE + CTRLF_ENABLED + CTRLF_FOCUSABLE
    MOV [DI + CTRL_X], AL
    MOV BYTE [DI + CTRL_Y], 3
    MOV BYTE [DI + CTRL_W], DLG_BTN_YN_W
    MOV BYTE [DI + CTRL_H], 1
    MOV WORD [DI + CTRL_NEXT], dlg_btn3
    MOV WORD [DI + CTRL_TEXT], dlg_str_no
    MOV BYTE [DI + CTRL_ATTR], CLR_BTN
    MOV BYTE [DI + CTRL_FATTR], CLR_BTN_FOCUS
    MOV WORD [DI + CTRL_HANDLER], dlg_handler_no

    ; btn3: All — x = CH + 8+2 + 8+2
    ADD AL, DLG_BTN_YN_W + DLG_BTN_GAP
    MOV DI, dlg_btn3
    MOV BYTE [DI + CTRL_TYPE], CTYPE_BUTTON
    MOV BYTE [DI + CTRL_FLAGS], CTRLF_VISIBLE + CTRLF_ENABLED + CTRLF_FOCUSABLE
    MOV [DI + CTRL_X], AL
    MOV BYTE [DI + CTRL_Y], 3
    MOV BYTE [DI + CTRL_W], DLG_BTN_ALL_W
    MOV BYTE [DI + CTRL_H], 1
    MOV WORD [DI + CTRL_NEXT], dlg_btn4
    MOV WORD [DI + CTRL_TEXT], dlg_str_all
    MOV BYTE [DI + CTRL_ATTR], CLR_BTN
    MOV BYTE [DI + CTRL_FATTR], CLR_BTN_FOCUS
    MOV WORD [DI + CTRL_HANDLER], dlg_handler_repl_all

    ; btn4: Cancel — x = CH + 8+2 + 8+2 + 8+2
    ADD AL, DLG_BTN_ALL_W + DLG_BTN_GAP
    MOV DI, dlg_btn4
    MOV BYTE [DI + CTRL_TYPE], CTYPE_BUTTON
    MOV BYTE [DI + CTRL_FLAGS], CTRLF_VISIBLE + CTRLF_ENABLED + CTRLF_FOCUSABLE
    MOV [DI + CTRL_X], AL
    MOV BYTE [DI + CTRL_Y], 3
    MOV BYTE [DI + CTRL_W], DLG_BTN_CAN_W
    MOV BYTE [DI + CTRL_H], 1
    MOV WORD [DI + CTRL_NEXT], 0
    MOV WORD [DI + CTRL_TEXT], dlg_str_cancel
    MOV BYTE [DI + CTRL_ATTR], CLR_BTN
    MOV BYTE [DI + CTRL_FATTR], CLR_BTN_FOCUS
    MOV WORD [DI + CTRL_HANDLER], dlg_handler_cancel

    ; ---- Build window template ----
    POP AX                      ; AX = title ptr
    MOV DI, dlg_tmpl
    MOV BYTE [DI + WIN_FLAGS], WINF_VISIBLE + WINF_BORDER + WINF_TITLE + WINF_SHADOW
    MOV [DI + WIN_X], DL
    MOV [DI + WIN_Y], DH
    MOV [DI + WIN_W], BH
    MOV BYTE [DI + WIN_H], 6
    MOV BYTE [DI + WIN_ATTR], CLR_WIN_BG
    MOV BYTE [DI + WIN_BATTR], CLR_WIN_BDR
    MOV BYTE [DI + WIN_TATTR], CLR_WIN_TTL
    MOV [DI + WIN_TITLE], AX
    MOV WORD [DI + WIN_FIRST], dlg_label
    MOV WORD [DI + WIN_FOCUS], dlg_btn1
    MOV WORD [DI + WIN_HANDLER], 0
    MOV BYTE [DI + WIN_ZORDER], 0
    MOV BYTE [DI + WIN_ID], 0
    MOV BYTE [DI + WIN_SCROLLX], 0
    MOV BYTE [DI + WIN_SCROLLY], 0

    CALL tui_dlg_run_modal

    PopAll
    MOV AL, [fw_state + FW_DLG_RESULT]
    RET
ENDP

; ============================================================================
; tui_dlg_input - Text input dialog
; Input:  SI = prompt string ptr, DI = title string ptr,
;         BX = text buffer ptr, CL = max length
; Output: AL = DLG_OK (1) or DLG_CANCEL (0)
;         Buffer at BX contains entered text (valid only if AL=DLG_OK)
; ============================================================================
tui_dlg_input: PROC
    PushAll

    ; Save caller inputs to temp area
    MOV [dlg_save_title], DI
    MOV [dlg_save_buf], BX
    MOV [dlg_save_maxlen], CL

    ; Measure prompt length
    CALL tui_dlg_strlen         ; CX = prompt_len, SI preserved

    ; Interior width = max(prompt_len+4, 24, 20)
    ; where 24 = 8+10+2+4 (OK + Cancel + gap + padding)
    MOV AL, CL
    ADD AL, DLG_PAD
    CMP AL, DLG_BTN_OK_W + DLG_BTN_CAN_W + DLG_BTN_GAP + DLG_PAD
    JAE .w1
    MOV AL, DLG_BTN_OK_W + DLG_BTN_CAN_W + DLG_BTN_GAP + DLG_PAD
.w1:
    CMP AL, DLG_MIN_W
    JAE .w2
    MOV AL, DLG_MIN_W
.w2:
    MOV BL, AL                  ; BL = interior width
    ADD AL, 2
    MOV BH, AL                  ; BH = total width

    ; Center on screen (height = 7)
    MOV AL, SCR_W
    SUB AL, BH
    SHR AL, 1
    MOV DL, AL                  ; DL = dialog X
    MOV DH, (SCR_H - 7) / 2    ; DH = dialog Y (centered)

    ; ---- Build prompt label at row 1, left-aligned (x=1) ----
    MOV DI, dlg_label
    MOV BYTE [DI + CTRL_TYPE], CTYPE_LABEL
    MOV BYTE [DI + CTRL_FLAGS], CTRLF_VISIBLE
    MOV BYTE [DI + CTRL_X], 1
    MOV BYTE [DI + CTRL_Y], 1
    MOV [DI + CTRL_W], CL
    MOV BYTE [DI + CTRL_H], 1
    MOV WORD [DI + CTRL_NEXT], dlg_textbox
    MOV [DI + CTRL_TEXT], SI
    MOV BYTE [DI + CTRL_ATTR], CLR_LABEL
    MOV BYTE [DI + CTRL_FATTR], CLR_LABEL
    MOV WORD [DI + CTRL_HANDLER], 0

    ; ---- Build textbox at row 2, x=1, width = interior_w - 2 ----
    MOV AL, BL
    SUB AL, 2                   ; textbox visible width

    MOV DI, dlg_textbox
    MOV BYTE [DI + CTRL_TYPE], CTYPE_TEXTBOX
    MOV BYTE [DI + CTRL_FLAGS], CTRLF_VISIBLE + CTRLF_ENABLED + CTRLF_FOCUSABLE
    MOV BYTE [DI + CTRL_X], 1
    MOV BYTE [DI + CTRL_Y], 2
    MOV [DI + CTRL_W], AL
    MOV BYTE [DI + CTRL_H], 1
    MOV WORD [DI + CTRL_NEXT], dlg_btn1
    PUSH AX
    MOV AX, [dlg_save_buf]
    MOV [DI + CTRL_TEXT], AX
    POP AX
    MOV BYTE [DI + CTRL_ATTR], CLR_TEXTBOX
    MOV BYTE [DI + CTRL_FATTR], CLR_TB_FOCUS
    MOV WORD [DI + CTRL_HANDLER], 0
    PUSH AX
    MOV AL, [dlg_save_maxlen]
    MOV [DI + CTRL_TB_MAXLEN], AL
    POP AX
    MOV BYTE [DI + CTRL_TB_CURPOS], 0
    MOV BYTE [DI + CTRL_TB_SCROLL], 0
    ; Compute initial text length from buffer
    PUSH SI
    PUSH CX
    MOV SI, [dlg_save_buf]
    XOR CX, CX
.tb_len:
    CMP BYTE [SI], 0
    JZ .tb_len_done
    INC SI
    INC CL
    JMP .tb_len
.tb_len_done:
    MOV [DI + CTRL_TB_LEN], CL
    MOV [DI + CTRL_TB_CURPOS], CL   ; cursor at end of existing text
    POP CX
    POP SI

    ; ---- Build OK button at row 4 ----
    ; Center: total_btn_w = 8+2+10 = 20, start_x = (interior_w - 20) / 2
    MOV AL, BL
    SUB AL, DLG_BTN_OK_W + DLG_BTN_GAP + DLG_BTN_CAN_W
    SHR AL, 1
    MOV CH, AL                  ; CH = ok_x

    MOV DI, dlg_btn1
    MOV BYTE [DI + CTRL_TYPE], CTYPE_BUTTON
    MOV BYTE [DI + CTRL_FLAGS], CTRLF_VISIBLE + CTRLF_ENABLED + CTRLF_FOCUSABLE
    MOV [DI + CTRL_X], CH
    MOV BYTE [DI + CTRL_Y], 4
    MOV BYTE [DI + CTRL_W], DLG_BTN_OK_W
    MOV BYTE [DI + CTRL_H], 1
    MOV WORD [DI + CTRL_NEXT], dlg_btn2
    MOV WORD [DI + CTRL_TEXT], dlg_str_ok
    MOV BYTE [DI + CTRL_ATTR], CLR_BTN
    MOV BYTE [DI + CTRL_FATTR], CLR_BTN_FOCUS
    MOV WORD [DI + CTRL_HANDLER], dlg_handler_ok

    ; ---- Build Cancel button ----
    MOV AL, CH
    ADD AL, DLG_BTN_OK_W + DLG_BTN_GAP

    MOV DI, dlg_btn2
    MOV BYTE [DI + CTRL_TYPE], CTYPE_BUTTON
    MOV BYTE [DI + CTRL_FLAGS], CTRLF_VISIBLE + CTRLF_ENABLED + CTRLF_FOCUSABLE
    MOV [DI + CTRL_X], AL
    MOV BYTE [DI + CTRL_Y], 4
    MOV BYTE [DI + CTRL_W], DLG_BTN_CAN_W
    MOV BYTE [DI + CTRL_H], 1
    MOV WORD [DI + CTRL_NEXT], 0
    MOV WORD [DI + CTRL_TEXT], dlg_str_cancel
    MOV BYTE [DI + CTRL_ATTR], CLR_BTN
    MOV BYTE [DI + CTRL_FATTR], CLR_BTN_FOCUS
    MOV WORD [DI + CTRL_HANDLER], dlg_handler_cancel

    ; ---- Build window template ----
    MOV DI, dlg_tmpl
    MOV BYTE [DI + WIN_FLAGS], WINF_VISIBLE + WINF_BORDER + WINF_TITLE + WINF_SHADOW
    MOV [DI + WIN_X], DL
    MOV [DI + WIN_Y], DH
    MOV [DI + WIN_W], BH
    MOV BYTE [DI + WIN_H], 7
    MOV BYTE [DI + WIN_ATTR], CLR_WIN_BG
    MOV BYTE [DI + WIN_BATTR], CLR_WIN_BDR
    MOV BYTE [DI + WIN_TATTR], CLR_WIN_TTL
    PUSH AX
    MOV AX, [dlg_save_title]
    MOV [DI + WIN_TITLE], AX
    POP AX
    MOV WORD [DI + WIN_FIRST], dlg_label
    MOV WORD [DI + WIN_FOCUS], dlg_textbox
    MOV WORD [DI + WIN_HANDLER], 0
    MOV BYTE [DI + WIN_ZORDER], 0
    MOV BYTE [DI + WIN_ID], 0
    MOV BYTE [DI + WIN_SCROLLX], 0
    MOV BYTE [DI + WIN_SCROLLY], 0

    CALL tui_dlg_run_modal

    PopAll
    MOV AL, [fw_state + FW_DLG_RESULT]
    RET
ENDP


; ============================================================================
; File Selector Dialog
; ============================================================================

; ----------------------------------------------------------------------------
; tui_file_get_path - Get current working directory into dlg_file_path
; Format: "\DIRNAME" or "\" for root
; ----------------------------------------------------------------------------
tui_file_get_path: PROC
    PUSH AX
    PUSH DX
    PUSH SI
    MOV BYTE [dlg_file_path], '\'
    MOV AH, 47h
    MOV DL, 0                       ; current drive
    MOV SI, dlg_file_path + 1       ; buffer (after leading \)
    INT 21h
    POP SI
    POP DX
    POP AX
    RET
ENDP

; ----------------------------------------------------------------------------
; tui_file_enumerate - List files + directories into separate buffers
; Input:  dlg_file_pattern = wildcard for files (e.g., "*.*")
; Output: dlg_file_count/items/names = files matching pattern
;         dlg_dir_count/items/names  = subdirectories (always includes "..")
; ----------------------------------------------------------------------------
tui_file_enumerate: PROC
    PUSH AX
    PUSH BX
    PUSH CX
    PUSH DX
    PUSH SI
    PUSH DI
    PUSH BP

    ; Set DTA
    MOV AH, 1Ah
    MOV DX, dlg_file_dta
    INT 21h

    ; ---- Init directory list (always has ".." first) ----
    MOV BP, dlg_dir_names
    MOV BX, dlg_dir_items
    MOV BYTE [dlg_dir_count], 1
    MOV WORD [BX], BP
    MOV DI, BP
    MOV BYTE [DI], '.'
    MOV BYTE [DI + 1], '.'
    MOV BYTE [DI + 2], 0
    ADD BP, DLG_FILE_NAMELEN
    ADD BX, 2

    ; ---- Enumerate directories first (FindFirst *.* with dir attribute) ----
    MOV AH, 4Eh
    MOV CX, 10h                     ; directory attribute
    PUSH BP
    PUSH BX
    MOV DX, dlg_file_allpat         ; always "*.*" for directories
    INT 21h
    POP BX
    POP BP
    JC .dirs_done

.dir_process:
    ; Only directories (skip files)
    TEST BYTE [dlg_file_dta + 15h], 10h
    JZ .dir_next
    ; Skip "." and ".."
    CMP BYTE [dlg_file_dta + 1Eh], '.'
    JNZ .dir_not_dot
    CMP BYTE [dlg_file_dta + 1Fh], 0
    JZ .dir_next
    CMP BYTE [dlg_file_dta + 1Fh], '.'
    JNZ .dir_not_dot
    CMP BYTE [dlg_file_dta + 20h], 0
    JZ .dir_next
.dir_not_dot:
    MOV WORD [BX], BP
    MOV SI, dlg_file_dta + 1Eh
    MOV DI, BP
.dir_copy:
    MOV AL, [SI]
    MOV [DI], AL
    OR AL, AL
    JZ .dir_stored
    INC SI
    INC DI
    JMP .dir_copy
.dir_stored:
    ADD BP, DLG_FILE_NAMELEN
    ADD BX, 2
    INC BYTE [dlg_dir_count]
    CMP BYTE [dlg_dir_count], DLG_DIR_MAXENT
    JAE .dirs_done
.dir_next:
    MOV AH, 4Fh
    INT 21h
    JNC .dir_process

.dirs_done:
    ; ---- Now enumerate files matching the wildcard pattern ----
    MOV BP, dlg_file_names
    MOV BX, dlg_file_items
    MOV BYTE [dlg_file_count], 0

    ; Re-set DTA (FindFirst for dirs may have changed state)
    MOV AH, 1Ah
    MOV DX, dlg_file_dta
    INT 21h

    ; FindFirst with the user's wildcard pattern
    MOV AH, 4Eh
    XOR CX, CX                     ; files only (no dir attribute)
    MOV DX, dlg_file_pattern
    INT 21h
    JC .files_done

.file_process:
    ; Skip directories (only want files)
    TEST BYTE [dlg_file_dta + 15h], 10h
    JNZ .file_next

    MOV WORD [BX], BP
    MOV SI, dlg_file_dta + 1Eh
    MOV DI, BP
.file_copy:
    MOV AL, [SI]
    MOV [DI], AL
    OR AL, AL
    JZ .file_stored
    INC SI
    INC DI
    JMP .file_copy
.file_stored:
    ADD BP, DLG_FILE_NAMELEN
    ADD BX, 2
    INC BYTE [dlg_file_count]
    CMP BYTE [dlg_file_count], DLG_FILE_MAXENT
    JAE .files_done
.file_next:
    MOV AH, 4Fh
    INT 21h
    JNC .file_process

.files_done:
    POP BP
    POP DI
    POP SI
    POP DX
    POP CX
    POP BX
    POP AX
    RET
ENDP

; ----------------------------------------------------------------------------
; dlg_file_lb_handler - File listbox Enter: copy filename to textbox
; SI = file listbox control, DI = window
dlg_file_lb_handler: PROC
    PUSH AX
    PUSH BX
    PUSH CX
    PUSH DI

    ; Guard: empty list
    CMP BYTE [SI + CTRL_LB_COUNT], 0
    JZ .flb_done

    ; Look up selected item name
    MOV AL, [SI + CTRL_LB_SEL]
    XOR AH, AH
    SHL AX, 1
    MOV BX, [SI + CTRL_LB_ITEMS]
    ADD BX, AX
    MOV BX, [BX]                    ; BX = item string ptr

    ; Copy to textbox buffer
    MOV DI, [dlg_save_buf]
    MOV CL, [dlg_save_maxlen]
    XOR CH, CH
.flb_copy:
    MOV AL, [BX]
    MOV [DI], AL
    OR AL, AL
    JZ .flb_done
    INC BX
    INC DI
    INC CH
    DEC CL
    JNZ .flb_copy
    MOV BYTE [DI], 0
.flb_done:
    MOV DI, dlg_textbox
    MOV [DI + CTRL_TB_LEN], CH
    MOV [DI + CTRL_TB_CURPOS], CH
    MOV BYTE [DI + CTRL_TB_SCROLL], 0

    ; Close dialog — file selected
    MOV BYTE [fw_state + FW_DLG_RESULT], DLG_OK
    MOV AL, [fw_state + FW_MODAL_WIN]
    CALL tui_win_close
    MOV BYTE [fw_state + FW_RUNNING], 0

    POP DI
    POP CX
    POP BX
    POP AX
    RET
ENDP

; dlg_dir_lb_handler - Directory listbox Enter: navigate into dir, re-enumerate
; SI = dir listbox control, DI = window
dlg_dir_lb_handler: PROC
    PUSH AX
    PUSH BX
    PUSH CX
    PUSH DX
    PUSH SI

    ; Look up selected directory name
    MOV AL, [SI + CTRL_LB_SEL]
    XOR AH, AH
    SHL AX, 1
    MOV BX, [SI + CTRL_LB_ITEMS]
    ADD BX, AX
    MOV DX, [BX]                    ; DX = dir name ptr

    ; CHDIR into it
    MOV AH, 3Bh
    INT 21h
    JC .dlb_ret

    ; Re-enumerate files and directories
    CALL tui_file_enumerate

    ; Update file listbox
    MOV SI, dlg_listbox
    MOV AL, [dlg_file_count]
    MOV [SI + CTRL_LB_COUNT], AL
    MOV BYTE [SI + CTRL_LB_SEL], 0
    MOV BYTE [SI + CTRL_LB_SCROLL], 0

    ; Update dir listbox
    MOV SI, dlg_listbox2
    MOV AL, [dlg_dir_count]
    MOV [SI + CTRL_LB_COUNT], AL
    MOV BYTE [SI + CTRL_LB_SEL], 0
    MOV BYTE [SI + CTRL_LB_SCROLL], 0

    ; Update path display
    CALL tui_file_get_path
    MOV BYTE [fw_state + FW_DIRTY], 1

.dlb_ret:
    POP SI
    POP DX
    POP CX
    POP BX
    POP AX
    RET
ENDP

; dlg_file_ok_handler - OK button with priority dispatch
; 1. Plain filename in textbox → open file
; 2. File list has items → open selected file
; 3. Wildcard in textbox → re-filter, focus file list
dlg_file_ok_handler:
    PUSH AX
    PUSH BX
    PUSH CX
    PUSH SI
    PUSH DI                              ; DI = window struct from caller

    ; ---- Drive-prefix pre-check (P1-#1) ----
    ; Recognise "X:", "X:\", "X:pat", "X:\pat" at the start of the textbox.
    ; - Bare drive / drive+wildcard: switch drive (and CHDIR "\" when the
    ;   user wrote a backslash), rewrite pattern + textbox, refresh listing,
    ;   keep dialog open.
    ; - Sub-path ("X:\SUB\...") or drive+plain-filename ("X:FOO.TXT"): fall
    ;   through to Test 1; the caller / DOS resolves the full path.
    MOV BX, [dlg_save_buf]
    MOV AL, [BX]
    CMP BYTE [BX + 1], ':'
    JNE .dfoh_drv_pre_done
    OR  AL, 20h                          ; tolower
    CMP AL, 'a'
    JB  .dfoh_drv_pre_done
    CMP AL, 'z'
    JA  .dfoh_drv_pre_done

    ; First two bytes are "X:". Look past them.
    MOV SI, BX
    ADD SI, 2                            ; SI -> char after ':'
    XOR CL, CL                           ; CL = 1 if leading '\' present
    CMP BYTE [SI], '\'
    JNE .dfoh_drv_scan_init
    INC SI
    MOV CL, 1
.dfoh_drv_scan_init:
    MOV BX, SI                           ; BX = start of remainder
    XOR CH, CH                           ; CH = 1 if remainder has wildcard
.dfoh_drv_scan_loop:
    MOV AL, [SI]
    TEST AL, AL
    JZ  .dfoh_drv_scan_done
    CMP AL, '\'
    JE  .dfoh_drv_pre_done               ; sub-path -> pass through
    CMP AL, '*'
    JE  .dfoh_drv_scan_wild
    CMP AL, '?'
    JE  .dfoh_drv_scan_wild
    INC SI
    JMP .dfoh_drv_scan_loop
.dfoh_drv_scan_wild:
    MOV CH, 1
    INC SI
    JMP .dfoh_drv_scan_loop
.dfoh_drv_scan_done:
    ; BX = remainder start, CL = root_flag, CH = has_wildcard.
    ; Handle here iff remainder is empty OR contains a wildcard.
    CMP BYTE [BX], 0
    JE  .dfoh_drv_do
    TEST CH, CH
    JZ  .dfoh_drv_pre_done               ; plain filename -> pass through

.dfoh_drv_do:
    ; ---- Switch default drive ----
    PUSH BX                              ; preserve remainder pointer
    MOV BX, [dlg_save_buf]
    MOV AL, [BX]
    OR  AL, 20h
    SUB AL, 'a'
    MOV DL, AL
    MOV AH, 0Eh
    INT 21h

    ; ---- CHDIR to root iff user wrote "X:\" ----
    TEST CL, CL
    JZ  .dfoh_drv_no_chdir
    MOV DX, dlg_file_rootdir
    MOV AH, 3Bh
    INT 21h                              ; ignore CF
.dfoh_drv_no_chdir:
    POP BX                               ; BX = remainder

    ; ---- Write dlg_file_pattern = remainder (or "*.*" when empty) ----
    CMP BYTE [BX], 0
    JNE .dfoh_drv_copy_pat
    MOV BYTE [dlg_file_pattern],     '*'
    MOV BYTE [dlg_file_pattern + 1], '.'
    MOV BYTE [dlg_file_pattern + 2], '*'
    MOV BYTE [dlg_file_pattern + 3], 0
    JMP .dfoh_drv_sync_tb
.dfoh_drv_copy_pat:
    PUSH DS
    POP  ES
    MOV  SI, BX
    MOV  DI, dlg_file_pattern
    MOV  CX, 13
.dfoh_drv_cp_loop:
    LODSB
    STOSB
    TEST AL, AL
    JZ   .dfoh_drv_cp_done
    LOOP .dfoh_drv_cp_loop
    MOV  BYTE [DI], 0
.dfoh_drv_cp_done:

.dfoh_drv_sync_tb:
    ; ---- Mirror the new pattern into the textbox buffer ----
    PUSH DS
    POP  ES
    MOV  SI, dlg_file_pattern
    MOV  DI, [dlg_save_buf]
    XOR  CL, CL
.dfoh_drv_tb_loop:
    LODSB
    MOV  [DI], AL
    TEST AL, AL
    JZ   .dfoh_drv_tb_done
    INC  DI
    INC  CL
    JMP  .dfoh_drv_tb_loop
.dfoh_drv_tb_done:
    MOV  DI, dlg_textbox
    MOV  [DI + CTRL_TB_LEN], CL
    MOV  [DI + CTRL_TB_CURPOS], CL
    MOV  BYTE [DI + CTRL_TB_SCROLL], 0

    ; ---- Refresh path + fall into the shared enumerate/focus path ----
    CALL tui_file_get_path
    JMP  .dfoh_no_drive

.dfoh_drv_pre_done:

    ; ---- Test 1: plain filename in textbox? ----
    MOV BX, [dlg_save_buf]
    CMP BYTE [BX], 0
    JZ .dfoh_not_filename                ; empty → not a filename
    MOV SI, BX
.dfoh_scan:
    MOV AL, [SI]
    TEST AL, AL
    JZ .dfoh_open_file                   ; end of string, no wildcard → filename
    CMP AL, '*'
    JE .dfoh_not_filename
    CMP AL, '?'
    JE .dfoh_not_filename
    INC SI
    JMP .dfoh_scan

.dfoh_open_file:
    ; Plain filename → close dialog
    MOV BYTE [fw_state + FW_DLG_RESULT], DLG_OK
    MOV AL, [fw_state + FW_MODAL_WIN]
    CALL tui_win_close
    MOV BYTE [fw_state + FW_RUNNING], 0
    JMP .dfoh_done

.dfoh_not_filename:
    ; ---- Test 2: if textbox is focused (Enter from textbox), re-filter ----
    CMP WORD [DI + WIN_FOCUS], dlg_textbox
    JE .dfoh_no_files                    ; treat as wildcard re-filter

    ; ---- Test 3: file list has a selected item? ----
    MOV SI, dlg_listbox
    CMP BYTE [SI + CTRL_LB_COUNT], 0
    JZ .dfoh_no_files

    ; Copy selected filename to buffer, then open
    MOV AL, [SI + CTRL_LB_SEL]
    XOR AH, AH
    SHL AX, 1
    MOV BX, [SI + CTRL_LB_ITEMS]
    ADD BX, AX
    MOV BX, [BX]                         ; BX = filename string ptr
    MOV DI, [dlg_save_buf]
    MOV CL, [dlg_save_maxlen]
.dfoh_fl_copy:
    MOV AL, [BX]
    MOV [DI], AL
    TEST AL, AL
    JZ .dfoh_fl_open
    INC BX
    INC DI
    DEC CL
    JNZ .dfoh_fl_copy
    MOV BYTE [DI], 0
.dfoh_fl_open:
    MOV BYTE [fw_state + FW_DLG_RESULT], DLG_OK
    MOV AL, [fw_state + FW_MODAL_WIN]
    CALL tui_win_close
    MOV BYTE [fw_state + FW_RUNNING], 0
    JMP .dfoh_done

.dfoh_no_files:
    ; ---- Fallback: re-filter with wildcard ----
    MOV BX, [dlg_save_buf]
    CMP BYTE [BX], 0
    JZ .dfoh_done                        ; empty textbox, no files → ignore
    MOV SI, BX
    MOV DI, dlg_file_pattern
    MOV CX, 13
.dfoh_cpat:
    LODSB
    MOV [DI], AL
    INC DI
    TEST AL, AL
    JZ .dfoh_cpat_done
    LOOP .dfoh_cpat
    MOV BYTE [DI], 0
.dfoh_cpat_done:
.dfoh_no_drive:
    ; Drive handling now happens in the pre-check at the top of this PROC.
    ; This label remains the shared refresh entry point.
    CALL tui_file_enumerate
    MOV SI, dlg_listbox
    MOV AL, [dlg_file_count]
    MOV [SI + CTRL_LB_COUNT], AL
    MOV BYTE [SI + CTRL_LB_SEL], 0
    MOV BYTE [SI + CTRL_LB_SCROLL], 0
    MOV SI, dlg_listbox2
    MOV AL, [dlg_dir_count]
    MOV [SI + CTRL_LB_COUNT], AL
    MOV BYTE [SI + CTRL_LB_SEL], 0
    MOV BYTE [SI + CTRL_LB_SCROLL], 0

    ; Focus file list
    POP DI
    MOV WORD [DI + WIN_FOCUS], dlg_listbox
    MOV BYTE [fw_state + FW_DIRTY], 1
    JMP .dfoh_done2

.dfoh_done:
    POP DI
.dfoh_done2:
    POP SI
    POP CX
    POP BX
    POP AX
    RET


; ----------------------------------------------------------------------------
; tui_dlg_file - File selector dialog
; Input:  DI = title string ptr, BX = result buffer ptr, CL = max filename len
; Output: AL = DLG_OK (1) or DLG_CANCEL (0)
;         Result buffer contains selected filename if DLG_OK
;         CWD left at the directory where the file was selected
; ----------------------------------------------------------------------------
; Layout (Turbo Debugger style, interior 40 wide, 14 rows, WIN_H=16):
;  Row 0:  File name
;  Row 1:  [wildcard/filename textbox_______________]
;  Row 2:  Files              Directories
;  Row 3-10: [files listbox ] [dirs listbox        ]
;  Row 11: C:\DIRNAME\*.ext
;  Row 12: (empty)
;  Row 13:      [  OK  ]    [ Cancel ]

tui_dlg_file: PROC
    PushAll

    ; Save caller params
    MOV [dlg_save_title], DI
    MOV [dlg_save_buf], BX
    MOV [dlg_save_maxlen], CL

    ; Save current pattern for Cancel restore
    PUSH SI
    PUSH CX
    MOV SI, dlg_file_pattern
    MOV DI, dlg_file_pat_save
    MOV CX, 14
    REP MOVSB
    POP CX
    POP SI

    ; Pre-populate textbox with wildcard pattern if buffer is empty
    CMP BYTE [BX], 0
    JNE ._dfp_skip
    PUSH SI
    MOV SI, dlg_file_pattern
._dfp_copy:
    LODSB
    MOV [BX], AL
    INC BX
    TEST AL, AL
    JNZ ._dfp_copy
    POP SI
._dfp_skip:

    ; Populate path display, files and directories
    CALL tui_file_get_path
    CALL tui_file_enumerate

    ; --- "File name" label (dlg_label2) at row 0 ---
    MOV DI, dlg_label2
    MOV BYTE [DI + CTRL_TYPE], CTYPE_LABEL
    MOV BYTE [DI + CTRL_FLAGS], CTRLF_VISIBLE
    MOV BYTE [DI + CTRL_X], 1
    MOV BYTE [DI + CTRL_Y], 0
    MOV BYTE [DI + CTRL_W], 9
    MOV BYTE [DI + CTRL_H], 1
    MOV WORD [DI + CTRL_NEXT], dlg_textbox
    MOV WORD [DI + CTRL_TEXT], dlg_str_filename
    MOV BYTE [DI + CTRL_ATTR], 70h      ; black on light gray
    MOV BYTE [DI + CTRL_FATTR], 70h
    MOV WORD [DI + CTRL_HANDLER], 0

    ; --- Filename/wildcard textbox at row 1 ---
    MOV DI, dlg_textbox
    MOV BYTE [DI + CTRL_TYPE], CTYPE_TEXTBOX
    MOV BYTE [DI + CTRL_FLAGS], CTRLF_VISIBLE + CTRLF_ENABLED + CTRLF_FOCUSABLE
    MOV BYTE [DI + CTRL_X], 1
    MOV BYTE [DI + CTRL_Y], 1
    MOV BYTE [DI + CTRL_W], 38
    MOV BYTE [DI + CTRL_H], 1
    MOV WORD [DI + CTRL_NEXT], dlg_label
    PUSH AX
    MOV AX, [dlg_save_buf]
    MOV [DI + CTRL_TEXT], AX
    POP AX
    MOV BYTE [DI + CTRL_ATTR], 1Fh      ; bright white on dark blue
    MOV BYTE [DI + CTRL_FATTR], 1Fh
    MOV WORD [DI + CTRL_HANDLER], 0
    PUSH AX
    MOV AL, [dlg_save_maxlen]
    MOV [DI + CTRL_TB_MAXLEN], AL
    POP AX
    MOV BYTE [DI + CTRL_TB_CURPOS], 0
    MOV BYTE [DI + CTRL_TB_SCROLL], 0
    PUSH SI
    PUSH CX
    MOV SI, [dlg_save_buf]
    XOR CX, CX
._dft_len:
    CMP BYTE [SI], 0
    JZ ._dft_len_done
    INC SI
    INC CL
    JMP ._dft_len
._dft_len_done:
    MOV [DI + CTRL_TB_LEN], CL
    MOV [DI + CTRL_TB_CURPOS], CL
    POP CX
    POP SI

    ; --- "Files" label (dlg_label) at row 2 ---
    MOV DI, dlg_label
    MOV BYTE [DI + CTRL_TYPE], CTYPE_LABEL
    MOV BYTE [DI + CTRL_FLAGS], CTRLF_VISIBLE
    MOV BYTE [DI + CTRL_X], 1
    MOV BYTE [DI + CTRL_Y], 2
    MOV BYTE [DI + CTRL_W], 5
    MOV BYTE [DI + CTRL_H], 1
    MOV WORD [DI + CTRL_NEXT], dlg_listbox
    MOV WORD [DI + CTRL_TEXT], dlg_str_files
    MOV BYTE [DI + CTRL_ATTR], 70h      ; black on light gray
    MOV BYTE [DI + CTRL_FATTR], 70h
    MOV WORD [DI + CTRL_HANDLER], 0

    ; --- Files listbox (dlg_listbox) at row 3, left half ---
    MOV DI, dlg_listbox
    MOV BYTE [DI + CTRL_TYPE], CTYPE_LISTBOX
    MOV BYTE [DI + CTRL_FLAGS], CTRLF_VISIBLE + CTRLF_ENABLED + CTRLF_FOCUSABLE
    MOV BYTE [DI + CTRL_X], 1
    MOV BYTE [DI + CTRL_Y], 3
    MOV BYTE [DI + CTRL_W], 18
    MOV BYTE [DI + CTRL_H], 8
    MOV WORD [DI + CTRL_NEXT], dlg_chk1
    MOV WORD [DI + CTRL_TEXT], 0
    MOV BYTE [DI + CTRL_ATTR], 30h      ; black on cyan
    MOV BYTE [DI + CTRL_FATTR], 30h
    MOV WORD [DI + CTRL_HANDLER], dlg_file_lb_handler
    MOV WORD [DI + CTRL_LB_ITEMS], dlg_file_items
    MOV AL, [dlg_file_count]
    MOV [DI + CTRL_LB_COUNT], AL
    MOV BYTE [DI + CTRL_LB_SEL], 0
    MOV BYTE [DI + CTRL_LB_SCROLL], 0

    ; --- "Directories" label (reuse dlg_chk1 as 14-byte label) at row 2 ---
    MOV DI, dlg_chk1
    MOV BYTE [DI + CTRL_TYPE], CTYPE_LABEL
    MOV BYTE [DI + CTRL_FLAGS], CTRLF_VISIBLE
    MOV BYTE [DI + CTRL_X], 21
    MOV BYTE [DI + CTRL_Y], 2
    MOV BYTE [DI + CTRL_W], 11
    MOV BYTE [DI + CTRL_H], 1
    MOV WORD [DI + CTRL_NEXT], dlg_listbox2
    MOV WORD [DI + CTRL_TEXT], dlg_str_dirs
    MOV BYTE [DI + CTRL_ATTR], 70h      ; black on light gray
    MOV BYTE [DI + CTRL_FATTR], 70h
    MOV WORD [DI + CTRL_HANDLER], 0

    ; --- Directories listbox (dlg_listbox2) at row 3, right half ---
    MOV DI, dlg_listbox2
    MOV BYTE [DI + CTRL_TYPE], CTYPE_LISTBOX
    MOV BYTE [DI + CTRL_FLAGS], CTRLF_VISIBLE + CTRLF_ENABLED + CTRLF_FOCUSABLE
    MOV BYTE [DI + CTRL_X], 21
    MOV BYTE [DI + CTRL_Y], 3
    MOV BYTE [DI + CTRL_W], 18
    MOV BYTE [DI + CTRL_H], 8
    MOV WORD [DI + CTRL_NEXT], dlg_chk2
    MOV WORD [DI + CTRL_TEXT], 0
    MOV BYTE [DI + CTRL_ATTR], 30h      ; black on cyan
    MOV BYTE [DI + CTRL_FATTR], 30h
    MOV WORD [DI + CTRL_HANDLER], dlg_dir_lb_handler
    MOV WORD [DI + CTRL_LB_ITEMS], dlg_dir_items
    MOV AL, [dlg_dir_count]
    MOV [DI + CTRL_LB_COUNT], AL
    MOV BYTE [DI + CTRL_LB_SEL], 0
    MOV BYTE [DI + CTRL_LB_SCROLL], 0

    ; --- Path display label (reuse dlg_chk2 as 14-byte label) at row 11 ---
    MOV DI, dlg_chk2
    MOV BYTE [DI + CTRL_TYPE], CTYPE_LABEL
    MOV BYTE [DI + CTRL_FLAGS], CTRLF_VISIBLE
    MOV BYTE [DI + CTRL_X], 1
    MOV BYTE [DI + CTRL_Y], 11
    MOV BYTE [DI + CTRL_W], 38
    MOV BYTE [DI + CTRL_H], 1
    MOV WORD [DI + CTRL_NEXT], dlg_btn1
    MOV WORD [DI + CTRL_TEXT], dlg_file_path
    MOV BYTE [DI + CTRL_ATTR], 70h      ; black on light gray
    MOV BYTE [DI + CTRL_FATTR], 70h
    MOV WORD [DI + CTRL_HANDLER], 0

    ; --- OK button at row 13 ---
    MOV DI, dlg_btn1
    MOV BYTE [DI + CTRL_TYPE], CTYPE_BUTTON
    MOV BYTE [DI + CTRL_FLAGS], CTRLF_VISIBLE + CTRLF_ENABLED + CTRLF_FOCUSABLE
    MOV BYTE [DI + CTRL_X], 10
    MOV BYTE [DI + CTRL_Y], 13
    MOV BYTE [DI + CTRL_W], DLG_BTN_OK_W
    MOV BYTE [DI + CTRL_H], 1
    MOV WORD [DI + CTRL_NEXT], dlg_btn2
    MOV WORD [DI + CTRL_TEXT], dlg_str_ok
    MOV BYTE [DI + CTRL_ATTR], CLR_BTN
    MOV BYTE [DI + CTRL_FATTR], CLR_BTN_FOCUS
    MOV WORD [DI + CTRL_HANDLER], dlg_file_ok_handler

    ; --- Cancel button ---
    MOV DI, dlg_btn2
    MOV BYTE [DI + CTRL_TYPE], CTYPE_BUTTON
    MOV BYTE [DI + CTRL_FLAGS], CTRLF_VISIBLE + CTRLF_ENABLED + CTRLF_FOCUSABLE
    MOV BYTE [DI + CTRL_X], 22
    MOV BYTE [DI + CTRL_Y], 13
    MOV BYTE [DI + CTRL_W], DLG_BTN_CAN_W
    MOV BYTE [DI + CTRL_H], 1
    MOV WORD [DI + CTRL_NEXT], 0
    MOV WORD [DI + CTRL_TEXT], dlg_str_cancel
    MOV BYTE [DI + CTRL_ATTR], CLR_BTN
    MOV BYTE [DI + CTRL_FATTR], CLR_BTN_FOCUS
    MOV WORD [DI + CTRL_HANDLER], dlg_handler_cancel

    ; --- Window template (interior 40 wide, 15 rows + border = 42 x 17) ---
    MOV DI, dlg_tmpl
    MOV BYTE [DI + WIN_FLAGS], WINF_VISIBLE + WINF_BORDER + WINF_TITLE + WINF_SHADOW
    MOV BYTE [DI + WIN_X], (SCR_W - 42) / 2
    MOV BYTE [DI + WIN_Y], (SCR_H - 17) / 2
    MOV BYTE [DI + WIN_W], 42
    MOV BYTE [DI + WIN_H], 17
    MOV BYTE [DI + WIN_ATTR], 70h       ; black on light gray
    MOV BYTE [DI + WIN_BATTR], CLR_WIN_BDR
    MOV BYTE [DI + WIN_TATTR], CLR_WIN_TTL
    PUSH AX
    MOV AX, [dlg_save_title]
    MOV [DI + WIN_TITLE], AX
    POP AX
    MOV WORD [DI + WIN_FIRST], dlg_label2
    MOV WORD [DI + WIN_FOCUS], dlg_textbox
    MOV WORD [DI + WIN_HANDLER], 0
    MOV BYTE [DI + WIN_ZORDER], 0
    MOV BYTE [DI + WIN_ID], 0
    MOV BYTE [DI + WIN_SCROLLX], 0
    MOV BYTE [DI + WIN_SCROLLY], 0

    CALL tui_dlg_run_modal

    ; If cancelled, restore the saved wildcard pattern
    CMP BYTE [fw_state + FW_DLG_RESULT], DLG_CANCEL
    JNE ._dfp_no_restore
    PUSH SI
    PUSH CX
    MOV SI, dlg_file_pat_save
    MOV DI, dlg_file_pattern
    MOV CX, 14
    REP MOVSB
    POP CX
    POP SI
._dfp_no_restore:

    PopAll
    MOV AL, [fw_state + FW_DLG_RESULT]
    RET
ENDP
