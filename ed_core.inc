; ============================================================
; ed_core.inc — Piece-Table Engine
;
; All piece-table infrastructure procedures ported from
; TEXTEDIT.ASM. Pure data-manipulation routines — no video output.
; ============================================================

; ============================================================
; SHRINK_MEM
; ============================================================

shrink_mem:
    PUSH    ES
    MOV     AX, CS
    MOV     ES, AX
    MOV     AH, 4Ah
    MOV     BX, SHRINK_PARA
    INT     21h
    POP     ES
    RET

; ============================================================
; PARSE_ARGS
; ============================================================

parse_args:
    MOV     BYTE [wrap_mode], 0
    MOV     SI, 81h
    XOR     CH, CH
    MOV     CL, [80h]
    CMP     CX, 0
    JE      .no_fn
.skip1:
    TEST    CX, CX
    JZ      .no_fn
    MOV     AL, [SI]
    CMP     AL, ' '
    JNE     .got_tok
    INC     SI
    DEC     CX
    JMP     .skip1
.got_tok:
    CMP     CX, 6
    JB      .copy_fn
    CMP     BYTE [SI], '-'
    JNE     .copy_fn
    CMP     BYTE [SI+1], '-'
    JNE     .copy_fn
    MOV     AL, [SI+2]
    OR      AL, 20h
    CMP     AL, 'w'
    JNE     .copy_fn
    MOV     BYTE [wrap_mode], 1
    ADD     SI, 6
    SUB     CX, 6
    JBE     .no_fn
.skip2:
    JCXZ    .no_fn
    MOV     AL, [SI]
    CMP     AL, ' '
    JNE     .copy_fn
    INC     SI
    DEC     CX
    JMP     .skip2
.copy_fn:
    JCXZ    .no_fn
    MOV     DI, filename
.cfloop:
    JCXZ    .fn_done
    MOV     AL, [SI]
    CMP     AL, ' '
    JE      .fn_done
    CMP     AL, 0Dh
    JE      .fn_done
    CMP     AL, 0Ah
    JE      .fn_done
    MOV     [DI], AL
    INC     SI
    INC     DI
    DEC     CX
    JMP     .cfloop
.fn_done:
    MOV     BYTE [DI], 0
    CMP     DI, filename
    JE      .no_fn
    CLC
    RET
.no_fn:
    STC
    RET

; ============================================================
; OPEN_FILE
; ============================================================

open_file:
    MOV     DX, filename
    XOR     AL, AL
    MOV     AH, 3Dh
    INT     21h
    JC      .err
    MOV     [file_hnd], AX
    CLC
    RET
.err:
    STC
    RET

; ============================================================
; LOAD_FILE
; ============================================================

load_file PROC
    PUSH    BX
    PUSH    CX
    PUSH    DX
    PUSH    SI
    PUSH    DI
    PUSH    BP
    XOR     BP, BP
.next_chunk:
    CMP     BP, MAX_ORIG
    JAE     .done
    MOV     BX, CHUNK_PARA
    MOV     AH, 48h
    INT     21h
    JC      .alloc_fail
    MOV     DI, BP
    SHL     DI, 1
    MOV     [DI + orig_seg], AX
    PUSH    DS
    MOV     DS, AX
    XOR     DX, DX
    MOV     BX, CS:[file_hnd]
    MOV     CX, CHUNK_BYTES
    MOV     AH, 3Fh
    INT     21h
    POP     DS
    JC      .read_err
    TEST    AX, AX
    JZ      .last_empty
    MOV     [DI + orig_len], AX
    INC     BP
    CMP     AX, CHUNK_BYTES
    JB      .done
    JMP     .next_chunk
.last_empty:
    MOV     AX, [DI + orig_seg]
    PUSH    ES
    MOV     ES, AX
    MOV     AH, 49h
    INT     21h
    POP     ES
.done:
    MOV     [orig_count], BP
    POP     BP
    POP     DI
    POP     SI
    POP     DX
    POP     CX
    POP     BX
    CLC
    RET
.read_err:
    MOV     AX, [DI + orig_seg]
    PUSH    ES
    MOV     ES, AX
    MOV     AH, 49h
    INT     21h
    POP     ES
.alloc_fail:
    MOV     [orig_count], BP
    POP     BP
    POP     DI
    POP     SI
    POP     DX
    POP     CX
    POP     BX
    STC
    RET
load_file ENDP

; ============================================================
; PT_INIT
; ============================================================

pt_init PROC
    PUSH    BX
    PUSH    CX
    PUSH    DX
    PUSH    SI
    PUSH    DI
    PUSH    ES
    MOV     BX, PT_SEG_PARA
    MOV     AH, 48h
    INT     21h
    JC      .fail
    MOV     [pt_seg], AX
    MOV     ES, AX
    MOV     CX, [orig_count]
    MOV     [pt_count], CX
    XOR     DI, DI
    XOR     BX, BX
.loop:
    CMP     BX, [orig_count]
    JAE     .done
    MOV     BYTE ES:[DI + PT_F_SOURCE], 0
    MOV     BYTE ES:[DI + PT_F_SOURCE + 1], 0
    MOV     AX, BX
    AND     AX, 1
    MOV     CL, 15
    SHL     AX, CL
    MOV     ES:[DI + PT_F_START], AX
    MOV     AX, BX
    SHR     AX, 1
    MOV     ES:[DI + PT_F_START + 2], AX
    MOV     SI, BX
    SHL     SI, 1
    MOV     AX, [SI + orig_len]
    MOV     ES:[DI + PT_F_LENGTH], AX
    MOV     WORD ES:[DI + PT_F_LENGTH + 2], 0
    ADD     DI, PT_DESC_SIZE
    INC     BX
    JMP     .loop
.done:
    POP     ES
    POP     DI
    POP     SI
    POP     DX
    POP     CX
    POP     BX
    CLC
    RET
.fail:
    MOV     WORD [pt_seg], 0
    POP     ES
    POP     DI
    POP     SI
    POP     DX
    POP     CX
    POP     BX
    STC
    RET
pt_init ENDP

; ============================================================
; RESOLVE_PIECE
; Input:  BX = piece index (DS = COM segment)
; Output: AX = segment, SI = base offset, DX = piece length
;         CF=0 success, CF=1 error
; Preserved: BX, DI, BP
; ============================================================

resolve_piece PROC
    CMP     BX, [pt_count]
    JAE     .err
    PUSH    ES
    MOV     AX, [pt_seg]
    MOV     ES, AX
    ; SI = BX * 10
    MOV     SI, BX
    SHL     SI, 1
    MOV     AX, SI
    SHL     SI, 1
    SHL     SI, 1
    ADD     SI, AX
    ; Read length
    MOV     DX, ES:[SI + PT_F_LENGTH]
    ; Read start
    MOV     AX, ES:[SI + PT_F_START]
    MOV     CX, ES:[SI + PT_F_START + 2]
    ; Check source
    CMP     BYTE ES:[SI + PT_F_SOURCE], 0
    JNE     .add_source

    ; ---- Source 0: Original buffer ----
    MOV     SI, AX
    AND     SI, 7FFFh          ; SI = chunk offset
    SHL     CX, 1
    TEST    AX, 8000h
    JZ      .no_hi
    INC     CX
.no_hi:
    PUSH    BX
    MOV     BX, CX
    SHL     BX, 1
    MOV     AX, [BX + orig_seg]
    POP     BX
    POP     ES
    CLC
    RET

.add_source:
    ; ---- Source 1: Add buffer ----
    ; start = byte offset within add buffer (low word in AX)
    MOV     SI, AX             ; SI = offset within add buffer
    MOV     AX, [add_seg]      ; AX = add buffer segment
    POP     ES
    CLC
    RET

.err:
    STC
    RET
resolve_piece ENDP

; ============================================================
; REBUILD_META
; ============================================================

rebuild_meta PROC
    PUSH    BX
    PUSH    CX
    PUSH    DX
    PUSH    SI
    PUSH    DI
    PUSH    BP
    PUSH    ES
    MOV     WORD [chkpt_num], 0
    MOV     WORD [total_lines], 0
    MOV     WORD [stl_col], 0
    MOV     WORD [chkpt_pi], 0
    MOV     WORD [chkpt_po], 0
    MOV     WORD [chkpt_num], 1
    MOV     WORD [total_vis_lines], 0
    MOV     WORD [cur_vis_line], 0
    XOR     BP, BP
.next_piece:
    CMP     BP, [pt_count]
    JAE     .done
    MOV     BX, BP
    CALL    resolve_piece
    JC      .done
    MOV     ES, AX
    MOV     [rnd_base], SI
    INC     BP

    CMP     BYTE [wrap_mode], 0
    JNE     .rm_wrap_loop

    ; ---- NON-WRAP: REPNE SCASB for LF scanning ----
    MOV     DI, SI                 ; DI = scan start = piece base
    ADD     DX, SI                 ; DX = piece end (absolute)
    MOV     AL, 0Ah                ; search for LF

.rm_scas_loop:
    MOV     CX, DX
    SUB     CX, DI                 ; CX = bytes remaining in piece
    JCXZ    .rm_exhaust            ; piece exhausted

    REPNE   SCASB                  ; scan for LF; DI advances past match
    JNE     .rm_exhaust            ; no LF found — piece done

    ; Found LF at DI-1. DI now points to byte after LF.
    MOV     WORD [stl_col], 0
    INC     WORD [total_lines]
    MOV     AX, [total_lines]
    TEST    AX, CHKPT_INTERVAL - 1
    MOV     AL, 0Ah                ; restore search byte (AX was clobbered)
    JNZ     .rm_scas_loop          ; not a checkpoint boundary

    ; Record checkpoint — swap SI/DI for .record_chkpt's convention
    PUSH    SI                     ; save piece base
    MOV     SI, DI                 ; SI = current scan position (past LF)
    PUSH    DI                     ; save scan position
    MOV     DI, DX                 ; DI = piece end (what record_chkpt expects)
    CALL    .record_chkpt
    POP     DI                     ; restore scan position
    POP     SI                     ; restore piece base
    MOV     AL, 0Ah                ; restore search byte (clobbered by record_chkpt)
    JMP     .rm_scas_loop

.rm_exhaust:
    JMP     .end_walk

    ; ---- WRAP MODE: byte-by-byte with dual line counting ----
.rm_wrap_loop:
    ADD     DX, SI                 ; DX = piece end (absolute)
    MOV     DI, DX                 ; DI = piece end (for record_chkpt)
.rm_w_byte:
    CMP     SI, DX
    JAE     .end_walk
    MOV     AL, ES:[SI]
    INC     SI
    CMP     AL, 0Dh
    JE      .rm_w_byte             ; skip CR
    CMP     AL, 0Ah
    JNE     .rm_w_not_lf
    ; ---- LF: increment both counters ----
    MOV     WORD [stl_col], 0
    INC     WORD [total_lines]
    INC     WORD [total_vis_lines]
    ; Track cur_vis_line: when logical line count reaches cur_line
    MOV     AX, [total_lines]
    CMP     AX, [cur_line]
    JNE     .rm_w_lf_no_cvl
    MOV     AX, [total_vis_lines]
    MOV     [cur_vis_line], AX
.rm_w_lf_no_cvl:
    MOV     AX, [total_lines]
    TEST    AX, CHKPT_INTERVAL - 1
    JNZ     .rm_w_byte
    CALL    .record_chkpt
    JMP     .rm_w_byte
.rm_w_not_lf:
    ; ---- Visible char: track column for visual wrap ----
    INC     WORD [stl_col]
    MOV     AX, [stl_col]
    CMP     AX, ED_COLS
    JB      .rm_w_byte
    MOV     WORD [stl_col], 0
    INC     WORD [total_vis_lines]
    JMP     .rm_w_byte

.end_walk:
    JMP     .next_piece
.done:
    INC     WORD [total_lines]
    CMP     BYTE [wrap_mode], 0
    JE      .done_no_vis
    INC     WORD [total_vis_lines]
.done_no_vis:
    POP     ES
    POP     BP
    POP     DI
    POP     SI
    POP     DX
    POP     CX
    POP     BX
    RET

.record_chkpt:
    PUSH    BX
    PUSH    AX
    MOV     AX, [chkpt_num]
    CMP     AX, MAX_CHKPT
    JAE     .rc_done
    MOV     BX, AX
    SHL     BX, 1
    MOV     AX, BP
    DEC     AX
    CMP     SI, DI
    JB      .rc_store
    MOV     AX, BP
    MOV     WORD [BX + chkpt_po], 0
    MOV     [BX + chkpt_pi], AX
    INC     WORD [chkpt_num]
    JMP     .rc_done
.rc_store:
    PUSH    CX
    MOV     CX, SI
    SUB     CX, [rnd_base]
    MOV     [BX + chkpt_pi], AX
    MOV     [BX + chkpt_po], CX
    INC     WORD [chkpt_num]
    POP     CX
.rc_done:
    POP     AX
    POP     BX
    RET
rebuild_meta ENDP

; ============================================================
; SEEK_TO_LINE
; ============================================================

seek_to_line:
    PUSH    AX
    CALL    ensure_meta
    POP     AX
    PUSH    AX
    PUSH    BX
    PUSH    CX
    PUSH    DX
    PUSH    SI
    PUSH    DI
    PUSH    BP
    MOV     DI, AX
    MOV     BX, AX
    MOV     CL, 8
    SHR     AX, CL
    MOV     DX, BX
    AND     DX, 0FFh
    MOV     CX, [chkpt_num]
    DEC     CX
    CMP     AX, CX
    JBE     .stl_c_ok
    MOV     AX, CX
    MOV     DX, AX
    MOV     CL, 8
    SHL     DX, CL
    MOV     CX, DI
    SUB     CX, DX
    MOV     DX, CX
.stl_c_ok:
    MOV     SI, AX
    SHL     SI, 1
    MOV     BX, [SI + chkpt_pi]
    MOV     SI, [SI + chkpt_po]
    MOV     [rnd_pi], BX
    MOV     [rnd_off], SI
    TEST    DX, DX
    JZ      .stl_done
    MOV     CX, DX
    MOV     WORD [stl_col], 0
.stl_walk_loop:
    MOV     BX, [rnd_pi]
    PUSH    CX
    CALL    resolve_piece
    POP     CX
    JC      .stl_page_err
    MOV     [rnd_base], SI
    ADD     DX, SI                 ; DX = piece end (absolute)

    ; ---- REPNE SCASB for LF scanning (both wrap and non-wrap) ----
    PUSH    ES
    MOV     ES, AX                 ; ES = text segment
    MOV     DI, SI
    ADD     DI, [rnd_off]          ; ES:DI = start position in text

    MOV     BX, CX                 ; BX = lines remaining (save CX for REPNE)
    MOV     AL, 0Ah                ; search for LF

.stl_scas_loop:
    MOV     CX, DX
    SUB     CX, DI                 ; CX = bytes remaining in piece
    JCXZ    .stl_scas_next         ; piece exhausted
    REPNE   SCASB                  ; scan for 0Ah; DI advances past match
    JNE     .stl_scas_next         ; not found in piece
    ; Found LF. DI = byte after the LF.
    DEC     BX                     ; one less line to skip
    JZ      .stl_scas_found
    JMP     .stl_scas_loop

.stl_scas_next:
    ; Piece exhausted without finding enough LFs
    POP     ES
    MOV     CX, BX                 ; restore line counter
    INC     WORD [rnd_pi]
    MOV     WORD [rnd_off], 0
    MOV     BX, [rnd_pi]
    CMP     BX, [pt_count]
    JAE     .stl_eof
    JMP     .stl_walk_loop

.stl_scas_found:
    ; DI = byte after the last LF = start of target line
    MOV     AX, DI
    SUB     AX, [rnd_base]         ; AX = piece-relative offset
    POP     ES
    ; Normalize: if offset == piece length, advance to next piece
    MOV     CX, DX
    SUB     CX, [rnd_base]         ; CX = piece length
    CMP     AX, CX
    JB      .stl_scas_ok
    INC     WORD [rnd_pi]
    MOV     WORD [rnd_off], 0
    JMP     .stl_done
.stl_scas_ok:
    MOV     [rnd_off], AX
    JMP     .stl_done

.stl_done:
    POP     BP
    POP     DI
    POP     SI
    POP     DX
    POP     CX
    POP     BX
    POP     AX
    CLC
    RET
.stl_eof:
    JMP     .stl_done
.stl_page_err:
    POP     BP
    POP     DI
    POP     SI
    POP     DX
    POP     CX
    POP     BX
    POP     AX
    STC
    RET

; ============================================================
; ENSURE_META — lazy rebuild of checkpoint table
; ============================================================

ensure_meta PROC
    CMP     BYTE [meta_dirty], 0
    JE      .ok
    CALL    rebuild_meta
    CALL    clamp_cursor
    MOV     BYTE [meta_dirty], 0
.ok:
    RET
ensure_meta ENDP

; ============================================================
; CLAMP_CURSOR — clamp cur_line to [0, total_lines - 1]
; ============================================================

clamp_cursor PROC
    MOV     AX, [total_lines]
    TEST    AX, AX
    JZ      .empty
    DEC     AX                    ; AX = max valid cur_line
    CMP     [cur_line], AX
    JBE     .line_ok
    MOV     [cur_line], AX
.line_ok:
    RET
.empty:
    MOV     WORD [cur_line], 0
    MOV     WORD [cur_col], 0
    RET
clamp_cursor ENDP

; ============================================================
; SCROLL_TO_CURSOR — ensure cur_line is visible
; ============================================================

scroll_to_cursor PROC
    ; ---- Vertical scroll ----
    MOV     AX, [cur_line]
    CMP     AX, [top_line]
    JB      .scroll_up
    MOV     BX, [top_line]
    ADD     BX, ED_TEXT_ROWS
    CMP     AX, BX
    JAE     .scroll_down
    JMP     .vert_done

.scroll_up:
    MOV     [top_line], AX
    JMP     .vert_done

.scroll_down:
    SUB     AX, ED_TEXT_ROWS
    INC     AX
    MOV     [top_line], AX

.vert_done:
    ; ---- Horizontal scroll (non-wrap only) ----
    CMP     BYTE [wrap_mode], 0
    JNE     .done

    MOV     AX, [cur_col]

    ; If cur_col < left_col: scroll left so cursor is at left edge
    CMP     AX, [left_col]
    JB      .scroll_left

    ; If cur_col >= left_col + ED_COLS: scroll right so cursor is at right edge
    MOV     BX, [left_col]
    ADD     BX, ED_COLS
    CMP     AX, BX
    JAE     .scroll_right

    RET

.scroll_left:
    MOV     [left_col], AX          ; left_col = cur_col
    RET

.scroll_right:
    SUB     AX, ED_COLS
    INC     AX                      ; left_col = cur_col - ED_COLS + 1
    MOV     [left_col], AX
.done:
    RET
scroll_to_cursor ENDP

; ============================================================
; CURSOR_TO_OFFSET — convert (cur_line, cur_col) to piece position
; Output: [rnd_pi], [rnd_off] = piece-relative cursor position
; ============================================================

cursor_to_offset PROC
    PUSH    AX
    PUSH    BX
    PUSH    CX
    PUSH    DX
    PUSH    SI
    PUSH    DI

    ; Seek to start of cur_line
    MOV     AX, [cur_line]
    CALL    seek_to_line

    ; Walk forward cur_col visible characters
    MOV     CX, [cur_col]
    JCXZ    .done_j

.done_j_skip:
    JMP     .walk_piece
.done_j:
    JMP     .done

.walk_piece:
    MOV     BX, [rnd_pi]
    CMP     BX, [pt_count]
    JAE     .done
    PUSH    CX
    CALL    resolve_piece     ; AX=seg, SI=base, DX=piece_len
    POP     CX
    JC      .done

    PUSH    DS
    MOV     DS, AX
    MOV     DI, SI            ; DI = base
    ADD     DX, SI            ; DX = chunk end
    MOV     SI, DI
    ADD     SI, CS:[rnd_off]  ; SI = chunk pos

.inner:
    CMP     SI, DX
    JAE     .next_piece_pop

    MOV     AL, [SI]
    CMP     AL, 0Dh
    JE      .skip_cr
    CMP     AL, 0Ah
    JE      .at_eol

    ; Visible character — advance past it
    INC     SI
    DEC     CX
    JZ      .found

    JMP     .inner

.skip_cr:
    INC     SI
    JMP     .inner

.at_eol:
    ; End of line reached before cur_col; cursor stays here
    POP     DS
    SUB     SI, DI
    MOV     [rnd_off], SI
    JMP     .done

.found:
    POP     DS
    ; If SI >= DX (past piece end), advance to next piece
    CMP     SI, DX
    JB      .found_ok
    INC     WORD [rnd_pi]
    MOV     WORD [rnd_off], 0
    JMP     .done
.found_ok:
    SUB     SI, DI
    MOV     [rnd_off], SI
    JMP     .done

.next_piece_pop:
    POP     DS
    MOV     BX, [rnd_pi]
    INC     BX
    MOV     [rnd_pi], BX
    MOV     WORD [rnd_off], 0
    CMP     BX, [pt_count]
    JAE     .done
    JMP     .walk_piece

.done:
    POP     DI
    POP     SI
    POP     DX
    POP     CX
    POP     BX
    POP     AX
    RET
cursor_to_offset ENDP

; ============================================================
; LINE_LENGTH — count visible characters on cur_line
; Output: AX = number of visible chars on the line
; ============================================================

line_length PROC
    PUSH    BX
    PUSH    CX
    PUSH    DX
    PUSH    SI
    PUSH    DI

    MOV     AX, [cur_line]
    CALL    seek_to_line
    XOR     CX, CX            ; CX = visible char count

    CMP     BYTE [wrap_mode], 0
    JNE     .ll_piece_wrap

    ; ---- NON-WRAP FAST PATH ----
.ll_piece_fast:
    MOV     BX, [rnd_pi]
    CMP     BX, [pt_count]
    JAE     .ll_done
    PUSH    CX
    CALL    resolve_piece      ; AX=seg, SI=base, DX=piece_len
    POP     CX
    JC      .ll_done

    PUSH    ES
    MOV     ES, AX
    MOV     DI, SI
    ADD     DI, [rnd_off]      ; ES:DI = start of scan within piece
    MOV     BX, DI             ; BX = saved start position

    PUSH    CX
    MOV     CX, SI
    ADD     CX, DX
    SUB     CX, DI             ; CX = bytes from offset to piece end
    MOV     AL, 0Ah
    REPNE   SCASB
    POP     CX                 ; restore visible char counter
    JE      .ll_found_lf

    ; No LF in piece: count bytes in range, subtract trailing CR.
    MOV     AX, DI
    SUB     AX, BX             ; AX = bytes scanned (all of them, no LF)
    MOV     BX, DI
    DEC     BX                 ; BX = last byte scanned
    CMP     BYTE ES:[BX], 0Dh
    JNE     .ll_no_trail_cr
    DEC     AX
.ll_no_trail_cr:
    ADD     CX, AX
    POP     ES
    INC     WORD [rnd_pi]
    MOV     WORD [rnd_off], 0
    JMP     .ll_piece_fast

.ll_found_lf:
    ; DI = byte after LF. BX = start of scan.
    MOV     AX, DI
    SUB     AX, BX
    DEC     AX                 ; AX = bytes between start and LF

    ; Check for CR immediately before LF
    TEST    AX, AX
    JZ      .ll_no_cr
    MOV     BX, DI
    SUB     BX, 2              ; BX = byte before LF
    CMP     BYTE ES:[BX], 0Dh
    JNE     .ll_no_cr
    DEC     AX
.ll_no_cr:
    ADD     CX, AX
    POP     ES
    JMP     .ll_done

    ; ---- WRAP MODE: original byte-by-byte scan ----
.ll_piece_wrap:
    MOV     BX, [rnd_pi]
    CMP     BX, [pt_count]
    JAE     .ll_done
    PUSH    CX
    CALL    resolve_piece
    POP     CX
    JC      .ll_done

    PUSH    DS
    MOV     DS, AX
    MOV     DI, SI
    ADD     DX, SI
    MOV     SI, DI
    ADD     SI, CS:[rnd_off]

.ll_inner_wrap:
    CMP     SI, DX
    JAE     .ll_next_wrap
    MOV     AL, [SI]
    INC     SI
    CMP     AL, 0Dh
    JE      .ll_inner_wrap
    CMP     AL, 0Ah
    JE      .ll_eol_wrap
    INC     CX
    JMP     .ll_inner_wrap

.ll_eol_wrap:
    POP     DS
    JMP     .ll_done

.ll_next_wrap:
    POP     DS
    INC     WORD [rnd_pi]
    MOV     WORD [rnd_off], 0
    JMP     .ll_piece_wrap

.ll_done:
    MOV     AX, CX
    POP     DI
    POP     SI
    POP     DX
    POP     CX
    POP     BX
    RET
line_length ENDP

; ============================================================
; PEEK_AT_CURSOR — check if character at cursor pos is past EOL
; Output: ZF=1 if at or past end of line (EOL/EOF), ZF=0 if not
; Clobbers: AX, BX, CX, DX, SI, DI
; ============================================================

peek_at_cursor PROC
    CALL    cursor_to_offset

    MOV     BX, [rnd_pi]
    CMP     BX, [pt_count]
    JAE     .at_eol

    CALL    resolve_piece
    JC      .at_eol

    MOV     CX, [rnd_off]
    CMP     CX, DX
    JAE     .at_eol

    PUSH    DS
    MOV     DS, AX
    ADD     SI, CX
    MOV     AL, [SI]
    POP     DS

    CMP     AL, 0Ah
    JE      .at_eol

    RET

.at_eol:
    XOR     AX, AX
    RET
peek_at_cursor ENDP

; ============================================================
; LINE_COL_TO_OFFSET — convert (line, col) to byte offset
;
; Input:  BX = target line (0-based)
;         CX = target column (0-based, visible chars)
; Output: DX:AX = byte offset in logical document
;         CF=0 success, CF=1 overflow (DX != 0)
; Preserves: BX, BP, DI
; Clobbers:  AX, CX, DX, SI, rnd_pi, rnd_off
; ============================================================

line_col_to_offset PROC
    PUSH    BP
    PUSH    DI
    PUSH    BX
    PUSH    CX                      ; save target_col for step 4

    ; ---- Step 1: Seek to target line ----
    MOV     AX, BX
    CALL    seek_to_line            ; sets rnd_pi, rnd_off

    ; ---- Step 2: Sum piece lengths for pieces 0..rnd_pi-1 ----
    XOR     AX, AX
    XOR     DX, DX                  ; DX:AX = 0 (32-bit accumulator)
    XOR     BP, BP                  ; BP = piece index

    PUSH    ES
    MOV     SI, [pt_seg]
    MOV     ES, SI

.lco_sum:
    CMP     BP, [rnd_pi]
    JAE     .lco_sum_done

    ; Compute descriptor offset: DI = BP * 10
    MOV     DI, BP
    SHL     DI, 1
    MOV     SI, DI
    SHL     DI, 1
    SHL     DI, 1
    ADD     DI, SI                  ; DI = BP * 10

    ; Read piece length (low word only; high word is always 0)
    ADD     AX, ES:[DI + PT_F_LENGTH]
    ADC     DX, 0                   ; propagate carry

    INC     BP
    JMP     .lco_sum

.lco_sum_done:
    POP     ES

    ; ---- Step 3: Add rnd_off ----
    ADD     AX, [rnd_off]
    ADC     DX, 0

    ; Save line-start offset to BSS scratch
    MOV     [lco_off_lo], AX
    MOV     [lco_off_hi], DX

    ; ---- Step 4: Walk target_col visible characters ----
    POP     CX                      ; restore target_col
    PUSH    CX                      ; re-save (will be popped in epilogue)
    XOR     BP, BP                  ; BP = column raw byte count

    JCXZ    .lco_col_done           ; col == 0: no walk needed

.lco_col_piece:
    MOV     BX, [rnd_pi]
    CMP     BX, [pt_count]
    JAE     .lco_col_done           ; past all pieces: stop

    PUSH    CX                      ; save remaining col count
    CALL    resolve_piece           ; AX=seg, SI=base, DX=piece_len
    POP     CX
    JC      .lco_col_done

    PUSH    DS
    MOV     DS, AX
    MOV     DI, SI                  ; DI = piece base
    ADD     DX, SI                  ; DX = piece end (absolute)
    ADD     SI, CS:[rnd_off]        ; SI = start position in piece

.lco_col_inner:
    CMP     SI, DX
    JAE     .lco_col_next           ; piece exhausted

    MOV     AL, [SI]
    CMP     AL, 0Dh
    JE      .lco_col_eol            ; CR = line ending, stop
    CMP     AL, 0Ah
    JE      .lco_col_eol            ; LF = line ending, stop

    ; Visible character
    INC     SI
    INC     BP                      ; count raw byte
    DEC     CX
    JZ      .lco_col_found          ; walked enough visible chars
    JMP     .lco_col_inner

.lco_col_eol:
    ; Hit line ending before target_col exhausted — stop
    POP     DS
    JMP     .lco_col_done

.lco_col_found:
    ; Walked exactly target_col visible chars
    POP     DS
    JMP     .lco_col_done

.lco_col_next:
    ; Piece exhausted, advance to next
    POP     DS
    INC     WORD [rnd_pi]
    MOV     WORD [rnd_off], 0
    JMP     .lco_col_piece

.lco_col_done:
    ; ---- Step 5: Add column bytes to line-start offset ----
    MOV     AX, [lco_off_lo]
    MOV     DX, [lco_off_hi]
    ADD     AX, BP
    ADC     DX, 0

    ; ---- Overflow check ----
    TEST    DX, DX
    JNZ     .lco_overflow

    POP     CX                      ; discard saved target_col
    POP     BX
    POP     DI
    POP     BP
    CLC
    RET

.lco_overflow:
    POP     CX
    POP     BX
    POP     DI
    POP     BP
    STC
    RET
line_col_to_offset ENDP

; ============================================================
; SPLIT_PIECE_AT — split piece BX at byte offset SI
;
; Input:  BX = piece index to split
;         SI = split offset within piece (0 < SI < piece.length)
;
; Output: CF=0 success: piece[BX] = left (length SI),
;                        piece[BX+1] = new right piece (remainder)
;         CF=1 if pt_insert fails (table full)
;
; Preserves: BP, DI
; Clobbers:  AX, BX, CX, DX, SI, ES
; ============================================================

split_piece_at PROC
    PUSH    BP
    PUSH    DI

    ; Load piece descriptor from pt_seg
    MOV     AX, [pt_seg]
    MOV     ES, AX

    ; DI = BX * 10
    MOV     DI, BX
    SHL     DI, 1
    MOV     AX, DI
    SHL     DI, 1
    SHL     DI, 1
    ADD     DI, AX                  ; DI = BX * 10

    ; Save fields for right piece
    MOV     DX, ES:[DI + PT_F_LENGTH]
    SUB     DX, SI                  ; DX = right_length
    PUSH    DX                      ; [stk+6] right_length
    MOV     AX, ES:[DI + PT_F_START + 2]
    PUSH    AX                      ; [stk+4] start_hi
    MOV     AX, ES:[DI + PT_F_START]
    PUSH    AX                      ; [stk+2] start_lo
    MOV     AL, ES:[DI + PT_F_SOURCE]
    XOR     AH, AH
    PUSH    AX                      ; [stk+0] source

    ; Shrink left piece to length = SI
    MOV     ES:[DI + PT_F_LENGTH], SI
    MOV     WORD ES:[DI + PT_F_LENGTH + 2], 0

    ; Insert new piece at BX+1
    INC     BX
    CALL    pt_insert
    JC      .spa_fail

    ; Write right piece descriptor at slot BX
    MOV     AX, [pt_seg]
    MOV     ES, AX
    MOV     DI, BX
    SHL     DI, 1
    MOV     AX, DI
    SHL     DI, 1
    SHL     DI, 1
    ADD     DI, AX                  ; DI = BX * 10

    POP     AX                      ; source
    MOV     ES:[DI + PT_F_SOURCE], AL
    MOV     BYTE ES:[DI + PT_F_SOURCE + 1], 0

    POP     AX                      ; start_lo
    ADD     AX, SI                  ; right.start = orig_start + SI
    MOV     ES:[DI + PT_F_START], AX
    POP     AX                      ; start_hi
    JNC     .spa_nc
    INC     AX
.spa_nc:
    MOV     ES:[DI + PT_F_START + 2], AX

    POP     AX                      ; right_length
    MOV     ES:[DI + PT_F_LENGTH], AX
    MOV     WORD ES:[DI + PT_F_LENGTH + 2], 0

    POP     DI
    POP     BP
    CLC
    RET

.spa_fail:
    ADD     SP, 8                   ; discard 4 saved words
    POP     DI
    POP     BP
    STC
    RET
split_piece_at ENDP

; ============================================================
; RANGE_DELETE_PIECES — delete byte range [start_off, end_off)
;
; Input:  AX = start_offset (byte offset, inclusive)
;         DX = end_offset (byte offset, exclusive)
;
; Output: CF=0 success, CF=1 error
;         [rd_save_count] = number of piece descriptors saved
;         [rd_save_start_pi] = piece index of first removal
;         rd_save_buf[] = saved descriptors in forward order
;
; Side effects: modifies piece table, sets meta_dirty = 1
; Preserves: BP, DI
; Clobbers:  AX, BX, CX, DX, SI
; ============================================================

range_delete_pieces PROC
    PUSH    BP
    PUSH    DI

    ; ---- Step 1: Save inputs, validate ----
    MOV     [rd_start_off], AX
    MOV     [rd_end_off], DX
    CMP     AX, DX
    JAE     .rdp_error              ; start >= end: invalid

    ; ---- Step 2: Walk pieces to find start_off ----
    PUSH    ES
    MOV     AX, [pt_seg]
    MOV     ES, AX
    XOR     CX, CX                 ; CX = accum (running byte total)
    XOR     BX, BX                 ; BX = piece index
    XOR     SI, SI                 ; SI = descriptor offset

.rdp_find_start:
    CMP     BX, [pt_count]
    JAE     .rdp_pop_error          ; past all pieces: offset out of range

    MOV     AX, ES:[SI + PT_F_LENGTH]
    MOV     DX, CX
    ADD     DX, AX                 ; DX = accum + plen
    CMP     DX, [rd_start_off]
    JA      .rdp_found_start       ; accum + plen > start_off
    MOV     CX, DX                 ; accum += plen
    ADD     SI, PT_DESC_SIZE
    INC     BX
    JMP     .rdp_find_start

.rdp_found_start:
    MOV     [rd_start_pi], BX
    MOV     AX, [rd_start_off]
    SUB     AX, CX                 ; start_split = start_off - accum
    MOV     [rd_start_split], AX

    ; ---- Step 3: Continue walking to find end_off ----
    ; CX = accum, BX = pi, SI = descriptor offset — still valid
.rdp_find_end:
    CMP     BX, [pt_count]
    JAE     .rdp_pop_error

    MOV     AX, ES:[SI + PT_F_LENGTH]
    MOV     DX, CX
    ADD     DX, AX                 ; DX = accum + plen
    CMP     DX, [rd_end_off]
    JAE     .rdp_found_end         ; accum + plen >= end_off
    MOV     CX, DX
    ADD     SI, PT_DESC_SIZE
    INC     BX
    JMP     .rdp_find_end

.rdp_found_end:
    MOV     [rd_end_pi], BX
    MOV     AX, [rd_end_off]
    SUB     AX, CX                 ; end_split = end_off - accum
    MOV     [rd_end_split], AX
    MOV     AX, ES:[SI + PT_F_LENGTH]
    MOV     [rd_end_plen], AX      ; save end piece length
    POP     ES

    ; ---- Step 4: Detect same-piece case ----
    MOV     WORD [rd_same_piece], 0
    MOV     AX, [rd_start_pi]
    CMP     AX, [rd_end_pi]
    JNE     .rdp_not_same
    MOV     WORD [rd_same_piece], 1
.rdp_not_same:

    ; ---- Step 5: Bounds check ----
    MOV     AX, [rd_end_pi]
    SUB     AX, [rd_start_pi]
    INC     AX                     ; count = end_pi - start_pi + 1
    ADD     AX, 2                  ; worst case: both splits add a piece
    CMP     AX, RD_MAX_PIECES
    JA      .rdp_error

    ; ---- Step 6: Split at start_off (if mid-piece) ----
    CMP     WORD [rd_start_split], 0
    JE      .rdp_no_start_split

    MOV     BX, [rd_start_pi]
    MOV     SI, [rd_start_split]
    PUSH    ES
    CALL    split_piece_at
    POP     ES
    JC      .rdp_error

    ; piece[start_pi] = left (survives), piece[start_pi+1] = right (to delete)
    INC     WORD [rd_start_pi]     ; first piece to delete shifts right
    MOV     AX, [rd_start_pi]
    MOV     [rd_save_start_pi], AX
    INC     WORD [rd_end_pi]       ; end piece also shifted by the insert

    ; CRITICAL: Same-piece adjustment
    CMP     WORD [rd_same_piece], 1
    JNE     .rdp_after_start_split
    MOV     AX, [rd_start_split]
    SUB     [rd_end_split], AX     ; end_split -= start_split
    JMP     .rdp_after_start_split

.rdp_no_start_split:
    MOV     AX, [rd_start_pi]
    MOV     [rd_save_start_pi], AX

.rdp_after_start_split:

    ; ---- Step 7: Split at end_off (if mid-piece) ----
    MOV     AX, [rd_end_split]
    TEST    AX, AX
    JZ      .rdp_end_boundary

    ; Check if end_split < end piece length
    ; In same-piece case, re-read end piece length from pt_seg
    CMP     WORD [rd_same_piece], 1
    JNE     .rdp_end_check_len

    ; Re-read end piece length after start split modified it
    PUSH    ES
    MOV     AX, [pt_seg]
    MOV     ES, AX
    MOV     BX, [rd_end_pi]
    MOV     DI, BX
    SHL     DI, 1
    MOV     AX, DI
    SHL     DI, 1
    SHL     DI, 1
    ADD     DI, AX
    MOV     AX, ES:[DI + PT_F_LENGTH]
    MOV     [rd_end_plen], AX
    POP     ES

    ; Re-check: rd_end_split must be < rd_end_plen
    MOV     AX, [rd_end_split]
    CMP     AX, [rd_end_plen]
    JAE     .rdp_skip_end_split    ; whole piece deleted, no split needed

.rdp_end_check_len:
    MOV     AX, [rd_end_split]
    CMP     AX, [rd_end_plen]
    JAE     .rdp_skip_end_split    ; end_split == plen: whole piece deleted

    MOV     BX, [rd_end_pi]
    MOV     SI, [rd_end_split]
    PUSH    ES
    CALL    split_piece_at
    POP     ES
    JC      .rdp_error
    ; piece[end_pi] = left (deleted), piece[end_pi+1] = right (survives)
    ; rd_end_pi stays the same
    JMP     .rdp_do_save

.rdp_end_boundary:
    ; end_off is exactly at a piece boundary — exclusive, so back up
    DEC     WORD [rd_end_pi]
    JMP     .rdp_do_save

.rdp_skip_end_split:

.rdp_do_save:
    ; ---- Step 8: Save descriptors (forward order) ----
    MOV     AX, [rd_end_pi]
    SUB     AX, [rd_start_pi]
    INC     AX                     ; count = end_pi - start_pi + 1
    MOV     [rd_save_count], AX

    ; Guard: if nothing to remove
    TEST    AX, AX
    JZ      .rdp_ok
    CMP     WORD [rd_end_pi], 0FFFFh  ; end_pi wrapped below start_pi
    JE      .rdp_ok

    ; Verify end_pi >= start_pi
    MOV     AX, [rd_end_pi]
    CMP     AX, [rd_start_pi]
    JB      .rdp_ok                ; nothing to remove

    ; Copy descriptors: source = pt_seg:start_pi*10, dest = rd_save_buf
    PUSH    ES
    PUSH    DS
    MOV     CX, [rd_save_count]
    MOV     AX, PT_DESC_SIZE
    MUL     CX                    ; AX = bytes to copy
    SHR     AX, 1                 ; AX = words to copy
    MOV     CX, AX

    ; Source: pt_seg, offset = start_pi * 10
    MOV     AX, [pt_seg]
    MOV     DS, AX
    MOV     SI, CS:[rd_start_pi]
    SHL     SI, 1
    MOV     AX, SI
    SHL     SI, 1
    SHL     SI, 1
    ADD     SI, AX                ; SI = start_pi * 10

    ; Dest: CS:rd_save_buf + rd_save_used * PT_DESC_SIZE (LIFO stack)
    PUSH    CS
    POP     ES
    MOV     DI, CS:[rd_save_used]
    MOV     AX, PT_DESC_SIZE
    MUL     DI
    MOV     DI, AX
    ADD     DI, rd_save_buf

    CLD
    REP     MOVSW

    POP     DS
    POP     ES

    ; ---- Step 9: Remove pieces (reverse order) ----
    MOV     BX, [rd_end_pi]
    MOV     CX, [rd_save_count]

.rdp_remove_loop:
    JCXZ    .rdp_finalize
    CALL    pt_remove
    DEC     BX
    DEC     CX
    JMP     .rdp_remove_loop

.rdp_finalize:
    ; ---- Step 10: Finalize ----
    MOV     BYTE [meta_dirty], 1

.rdp_ok:
    POP     DI
    POP     BP
    CLC
    RET

.rdp_pop_error:
    POP     ES
.rdp_error:
    POP     DI
    POP     BP
    STC
    RET
range_delete_pieces ENDP

; ============================================================
; FREE_ALL
; ============================================================

free_all:
    PUSH    ES
    PUSH    CX
    PUSH    SI
    MOV     CX, [orig_count]
    JCXZ    .no_orig
    XOR     SI, SI
.free_orig:
    MOV     AX, [SI + orig_seg]
    MOV     ES, AX
    MOV     AH, 49h
    INT     21h
    ADD     SI, 2
    LOOP    .free_orig
.no_orig:
    MOV     AX, [pt_seg]
    TEST    AX, AX
    JZ      .no_pt
    MOV     ES, AX
    MOV     AH, 49h
    INT     21h
.no_pt:
    MOV     AX, [add_seg]
    TEST    AX, AX
    JZ      .no_add
    MOV     ES, AX
    MOV     AH, 49h
    INT     21h
.no_add:
.done:
    POP     SI
    POP     CX
    POP     ES
    RET

; ============================================================
; PT_INSERT — make room for a new descriptor at index BX
; Shifts descriptors BX..pt_count-1 down by one slot.
; Increments pt_count. CF=1 if table full.
; ============================================================

pt_insert PROC
    CMP     WORD [pt_count], MAX_PT_PIECES
    JAE     .full

    PUSH    ES
    PUSH    DS
    PUSH    SI
    PUSH    DI
    PUSH    CX
    PUSH    AX
    PUSH    DX

    MOV     AX, [pt_seg]
    MOV     ES, AX
    MOV     DS, AX

    ; Number of descriptors to move = pt_count - BX
    MOV     CX, CS:[pt_count]
    SUB     CX, BX
    JCXZ    .no_move

    ; Word count = CX * PT_DESC_SIZE / 2
    MOV     AX, CX
    MOV     DX, PT_DESC_SIZE
    MUL     DX
    SHR     AX, 1
    MOV     CX, AX            ; CX = words to move

    ; Source: start of last WORD = (pt_count - 1) * 10 + 8
    MOV     AX, CS:[pt_count]
    DEC     AX
    MOV     DX, PT_DESC_SIZE
    MUL     DX
    ADD     AX, PT_DESC_SIZE - 2
    MOV     SI, AX

    ; Dest: SI + 10
    MOV     DI, SI
    ADD     DI, PT_DESC_SIZE

    STD
    REP     MOVSW
    CLD

.no_move:
    INC     WORD CS:[pt_count]
    PUSH    DS
    PUSH    CS
    POP     DS
    CALL    chkpt_adjust_insert
    POP     DS

    POP     DX
    POP     AX
    POP     CX
    POP     DI
    POP     SI
    POP     DS
    POP     ES
    CLC
    RET

.full:
    STC
    RET
pt_insert ENDP

; ============================================================
; PT_REMOVE — remove descriptor at index BX
; Shifts descriptors BX+1..pt_count-1 up by one slot.
; Decrements pt_count.
; ============================================================

pt_remove PROC
    CMP     WORD [pt_count], 0
    JE      .done

    PUSH    ES
    PUSH    DS
    PUSH    SI
    PUSH    DI
    PUSH    CX
    PUSH    AX
    PUSH    DX

    MOV     AX, [pt_seg]
    MOV     ES, AX
    MOV     DS, AX

    ; Number of descriptors to move = pt_count - BX - 1
    MOV     CX, CS:[pt_count]
    DEC     CX
    SUB     CX, BX
    JCXZ    .no_move

    ; Word count = CX * PT_DESC_SIZE / 2
    MOV     AX, CX
    MOV     DX, PT_DESC_SIZE
    MUL     DX
    SHR     AX, 1
    MOV     CX, AX            ; CX = words to move

    ; Dest = BX * 10
    MOV     DI, BX
    SHL     DI, 1
    MOV     AX, DI
    SHL     DI, 1
    SHL     DI, 1
    ADD     DI, AX            ; DI = BX * 10

    ; Source = (BX+1) * 10 = DI + 10
    MOV     SI, DI
    ADD     SI, PT_DESC_SIZE

    CLD
    REP     MOVSW

.no_move:
    DEC     WORD CS:[pt_count]
    PUSH    DS
    PUSH    CS
    POP     DS
    CALL    chkpt_adjust_remove
    POP     DS

    POP     DX
    POP     AX
    POP     CX
    POP     DI
    POP     SI
    POP     DS
    POP     ES
.done:
    RET
pt_remove ENDP

; ============================================================
; CHKPT_ADJUST_INSERT — adjust checkpoint piece indices after
; a pt_insert at index BX.  All chkpt_pi[i] >= BX get +1.
; Preserves all registers.
; ============================================================

chkpt_adjust_insert PROC
    PUSH    AX
    PUSH    CX
    PUSH    SI
    MOV     CX, [chkpt_num]
    XOR     SI, SI
.loop:
    JCXZ    .done
    MOV     AX, [SI + chkpt_pi]
    CMP     AX, BX
    JB      .next
    INC     WORD [SI + chkpt_pi]
.next:
    ADD     SI, 2
    DEC     CX
    JMP     .loop
.done:
    POP     SI
    POP     CX
    POP     AX
    RET
chkpt_adjust_insert ENDP

; ============================================================
; CHKPT_ADJUST_REMOVE — adjust checkpoint piece indices after
; a pt_remove at index BX.  All chkpt_pi[i] > BX get -1.
; Preserves all registers.
; ============================================================

chkpt_adjust_remove PROC
    PUSH    AX
    PUSH    CX
    PUSH    SI
    MOV     CX, [chkpt_num]
    XOR     SI, SI
.loop:
    JCXZ    .done
    MOV     AX, [SI + chkpt_pi]
    CMP     AX, BX
    JBE     .next
    DEC     WORD [SI + chkpt_pi]
.next:
    ADD     SI, 2
    DEC     CX
    JMP     .loop
.done:
    POP     SI
    POP     CX
    POP     AX
    RET
chkpt_adjust_remove ENDP
