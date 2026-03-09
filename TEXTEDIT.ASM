; ============================================================
; TEXTEDIT.ASM  -  8086 Text Editor for MS-DOS
;
; Build:  agent86 TEXTEDIT.ASM
; Usage:  TEXTEDIT [--wrap] <filename>
;
; ---- Architecture -----------------------------------------------
;  TRUE PIECE TABLE with two text stores (original + add buffer).
;  Piece descriptors: {source, pad, start(dword), length(dword)}.
;  Checkpoint table for O(1)+O(256) line seeking.
;  Direct VRAM writes to B800:0000.
; ============================================================

    ORG     100h
    SCREEN  CGA80

; ------------------------------------------------------------
; CONSTANTS
; ------------------------------------------------------------

SCREEN_COLS    EQU  80
SCREEN_ROWS    EQU  25
TEXT_ROWS      EQU  24
VIDEO_SEG      EQU  0B800h     ; must match SCREEN CGA80 (line 15)
ATTR_NORM      EQU  07h
ATTR_STAT      EQU  70h

MAX_ORIG       EQU  1024
CHUNK_PARA     EQU  0800h
CHUNK_BYTES    EQU  8000h

PT_DESC_SIZE   EQU  10
PT_F_SOURCE    EQU  0
PT_F_START     EQU  2
PT_F_LENGTH    EQU  6
MAX_PT_PIECES  EQU  4096
PT_SEG_PARA    EQU  0A00h

ADD_BUF_PARA   EQU  1000h     ; 4096 paras = 64 KB

CHKPT_INTERVAL EQU  256
MAX_CHKPT      EQU  2048

KEY_ESC        EQU  1Bh
KEY_ENTER      EQU  0Dh
KEY_BKSP       EQU  08h
SCAN_PGUP      EQU  49h
SCAN_PGDN      EQU  51h
SCAN_UP        EQU  48h
SCAN_DOWN      EQU  50h
SCAN_HOME      EQU  47h
SCAN_END       EQU  4Fh
SCAN_LEFT      EQU  4Bh
SCAN_RIGHT     EQU  4Dh
SCAN_DEL       EQU  53h

; ============================================================
;  ENTRY
; ============================================================

    JMP     main

; ------------------------------------------------------------
; BSS
; ------------------------------------------------------------

filename:    RESB 128
wrap_mode:   RESB 1
file_hnd:    RESW 1

; ---- Original Buffer ----
orig_count:  RESW 1
orig_seg:    RESW MAX_ORIG
orig_len:    RESW MAX_ORIG

; ---- Add Buffer ----
add_seg:     RESW 1          ; segment of append-only buffer
add_used:    RESW 1          ; bytes used (append pointer)

; ---- Piece Table ----
pt_seg:      RESW 1
pt_count:    RESW 1

; ---- Checkpoint Table ----
chkpt_num:   RESW 1
total_lines: RESW 1
chkpt_pi:    RESW MAX_CHKPT
chkpt_po:    RESW MAX_CHKPT

; ---- View / cursor state ----
top_line:    RESW 1
sav_curs:    RESW 1
cur_line:    RESW 1          ; cursor line (0-based)
cur_col:     RESW 1          ; cursor column (0-based)
dirty:       RESB 1          ; 1 = document modified
meta_dirty:  RESB 1          ; 1 = checkpoint table needs rebuild

; ---- Render / seek scratch ----
rnd_pi:      RESW 1
rnd_off:     RESW 1          ; piece-relative byte offset
rnd_base:    RESW 1          ; chunk base offset of current piece
rnd_len:     RESW 1          ; chunk-absolute end
rnd_seg:     RESW 1
rnd_row:     RESW 1
stl_col:     RESW 1

; ---- Last insert tracking (consecutive-char optimisation) ----
last_ins_pi: RESW 1          ; piece index of last inserted piece

; ------------------------------------------------------------
; STRING LITERALS
; ------------------------------------------------------------

s_usage:     DB  'Usage: TEXTEDIT [--wrap] <filename>',0Dh,0Ah,'$'
s_nofile:    DB  'Error: cannot open file',0Dh,0Ah,'$'
s_nomem:     DB  'Error: out of memory',0Dh,0Ah,'$'
s_loading:   DB  'Loading...',0Dh,0Ah,'$'
s_stat_hdr:  DB  ' TEXTEDIT  L$'
s_stat_sep:  DB  '/$'
s_stat_col:  DB  ' C$'
s_stat_wrap: DB  ' [WRAP]$'
s_stat_mod:  DB  ' [Mod]$'
s_stat_keys: DB  '  ^S=Save Esc=Quit$'
s_quit_ask:  DB  ' Unsaved changes! Save? (Y/N/Esc) $'

; ============================================================
;                           MAIN
; ============================================================

main:
    MOV     SP, 7FFEh
    CALL    shrink_mem
    CALL    parse_args
    JC      .err_usage
    CALL    open_file
    JC      .err_nofile

    MOV     DX, s_loading
    MOV     AH, 09h
    INT     21h

    CALL    load_file
    JC      .err_nomem

    MOV     BX, [file_hnd]
    MOV     AH, 3Eh
    INT     21h

    CALL    pt_init
    JC      .err_nomem

    ; Allocate add buffer
    MOV     BX, ADD_BUF_PARA
    MOV     AH, 48h
    INT     21h
    JC      .err_nomem
    MOV     [add_seg], AX
    MOV     WORD [add_used], 0

    CALL    rebuild_meta
    CALL    clamp_cursor
    MOV     BYTE [meta_dirty], 0

    ; Save cursor shape, set to underline (visible)
    MOV     AH, 03h
    XOR     BH, BH
    INT     10h
    MOV     [sav_curs], CX
    MOV     AH, 01h
    MOV     CX, 0607h        ; underline cursor
    INT     10h

    CALL    clear_scr
    MOV     WORD [top_line], 0
    MOV     WORD [cur_line], 0
    MOV     WORD [cur_col], 0
    MOV     BYTE [dirty], 0
    MOV     WORD [last_ins_pi], 0FFFFh

    CALL    render
    CALL    update_cursor

    ; ================================================================
    ;  MAIN EVENT LOOP
    ; ================================================================
.evloop:
    XOR     AH, AH
    INT     16h              ; AH = scan, AL = ASCII

    ; ---- Esc: quit ----
    CMP     AL, KEY_ESC
    JE      .quit

    ; ---- Enter: insert newline ----
    CMP     AL, KEY_ENTER
    JE      .do_enter

    ; ---- Backspace ----
    CMP     AL, KEY_BKSP
    JE      .do_bksp

    ; ---- Ctrl-S: save ----
    CMP     AL, 13h           ; Ctrl+S = ASCII 19
    JE      .do_save

    ; ---- Printable ASCII (20h-7Eh): insert ----
    CMP     AL, 20h
    JB      .check_scan
    CMP     AL, 7Eh
    JA      .check_scan
    CALL    insert_char
    JMP     .evloop

.check_scan:
    CMP     AH, SCAN_UP
    JNE     .ck_dn
    ; Cursor up
    CMP     WORD [cur_line], 0
    JE      .evloop
    DEC     WORD [cur_line]
    JMP     .cursor_moved

.ck_dn:
    CMP     AH, SCAN_DOWN
    JNE     .ck_left
    MOV     AX, [cur_line]
    INC     AX
    CMP     AX, [total_lines]
    JAE     .evloop
    MOV     [cur_line], AX
    JMP     .cursor_moved

.ck_left:
    CMP     AH, SCAN_LEFT
    JNE     .ck_right
    CMP     WORD [cur_col], 0
    JE      .evloop
    DEC     WORD [cur_col]
    JMP     .cursor_moved

.ck_right:
    CMP     AH, SCAN_RIGHT
    JNE     .ck_pgdn
    CALL    peek_at_cursor
    JZ      .evloop
    INC     WORD [cur_col]
    JMP     .cursor_moved

.ck_pgdn:
    CMP     AH, SCAN_PGDN
    JNE     .ck_pgup
    MOV     AX, [cur_line]
    ADD     AX, TEXT_ROWS
    CMP     AX, [total_lines]
    JB      .pgdn_ok
    MOV     AX, [total_lines]
    DEC     AX
.pgdn_ok:
    MOV     [cur_line], AX
    JMP     .cursor_moved

.ck_pgup:
    CMP     AH, SCAN_PGUP
    JNE     .ck_home
    MOV     AX, [cur_line]
    SUB     AX, TEXT_ROWS
    JNC     .pgup_ok
    XOR     AX, AX
.pgup_ok:
    MOV     [cur_line], AX
    JMP     .cursor_moved

.ck_home:
    CMP     AH, SCAN_HOME
    JNE     .ck_end
    MOV     WORD [cur_col], 0
    JMP     .cursor_moved

.ck_end:
    CMP     AH, SCAN_END
    JNE     .ck_del
    CALL    line_length          ; AX = visible chars on cur_line
    MOV     [cur_col], AX
    JMP     .cursor_moved

.ck_del:
    CMP     AH, SCAN_DEL
    JNE     .evloop
    ; Delete key: delete char at cursor without moving cursor first
    MOV     WORD [last_ins_pi], 0FFFFh
    CALL    delete_at
    CALL    scroll_to_cursor
    CALL    render
    CALL    update_cursor
    JMP     .evloop

.cursor_moved:
    MOV     WORD [last_ins_pi], 0FFFFh
    MOV     AX, [top_line]
    PUSH    AX
    CALL    scroll_to_cursor
    POP     AX
    CMP     AX, [top_line]
    JNE     .cursor_scrolled
    ; No scroll — only update status bar and hardware cursor
    CALL    repaint_status
    CALL    update_cursor
    JMP     .evloop
.cursor_scrolled:
    CALL    render
    CALL    update_cursor
    JMP     .evloop

.do_enter:
    MOV     WORD [last_ins_pi], 0FFFFh
    CALL    insert_crlf
    ; Move cursor to start of next line
    INC     WORD [cur_line]
    MOV     WORD [cur_col], 0
    CALL    scroll_to_cursor
    MOV     BYTE [meta_dirty], 1
    CALL    render
    CALL    update_cursor
    JMP     .evloop

.do_bksp:
    MOV     WORD [last_ins_pi], 0FFFFh
    ; Backspace: move cursor left, then delete char at new position
    ; If at col 0, move to end of previous line
    MOV     AX, [cur_col]
    TEST    AX, AX
    JNZ     .bksp_col
    ; At column 0: merge with previous line
    CMP     WORD [cur_line], 0
    JE      .evloop              ; already at start of document
    DEC     WORD [cur_line]
    CALL    line_length          ; AX = visible chars on cur_line
    MOV     [cur_col], AX
    JMP     .bksp_do
.bksp_col:
    DEC     WORD [cur_col]
.bksp_do:
    CALL    delete_at
    CALL    scroll_to_cursor
    CALL    render
    CALL    update_cursor
    JMP     .evloop

.do_save:
    MOV     WORD [last_ins_pi], 0FFFFh
    CALL    save_file
    CALL    render
    CALL    update_cursor
    JMP     .evloop

.quit:
    CMP     BYTE [dirty], 0
    JE      .quit_now
    ; Prompt: "Unsaved! Save? Y/N/Esc"
    CALL    .quit_prompt
    XOR     AH, AH
    INT     16h
    CMP     AL, 'Y'
    JE      .quit_save
    CMP     AL, 'y'
    JE      .quit_save
    CMP     AL, 'N'
    JE      .quit_now
    CMP     AL, 'n'
    JE      .quit_now
    ; Anything else (including Esc): cancel quit
    CALL    render
    CALL    update_cursor
    JMP     .evloop

.quit_save:
    CALL    save_file
.quit_now:
    MOV     AH, 01h
    MOV     CX, [sav_curs]
    INT     10h
    MOV     AH, 02h
    XOR     BX, BX
    XOR     DX, DX
    INT     10h
    CALL    clear_scr
    CALL    free_all
    INT     20h

.quit_prompt:
    ; Display prompt on status bar
    PUSH    ES
    PUSH    DI
    PUSH    SI
    MOV     AX, VIDEO_SEG
    MOV     ES, AX
    MOV     DI, TEXT_ROWS * SCREEN_COLS * 2
    MOV     SI, s_quit_ask
.qp_loop:
    MOV     AL, [SI]
    CMP     AL, '$'
    JE      .qp_pad
    MOV     ES:[DI], AL
    MOV     BYTE ES:[DI+1], ATTR_STAT
    ADD     DI, 2
    INC     SI
    JMP     .qp_loop
.qp_pad:
    MOV     CX, SCREEN_ROWS * SCREEN_COLS * 2
    SUB     CX, DI
    JBE     .qp_done
    SHR     CX, 1
    MOV     AX, 7020h
    REP     STOSW
.qp_done:
    POP     SI
    POP     DI
    POP     ES
    RET

.err_usage:
    MOV     DX, s_usage
    MOV     AH, 09h
    INT     21h
    INT     20h

.err_nofile:
    MOV     DX, s_nofile
    MOV     AH, 09h
    INT     21h
    INT     20h

.err_nomem:
    MOV     DX, s_nomem
    MOV     AH, 09h
    INT     21h
    CALL    free_all
    INT     20h

; ============================================================
; SCROLL_TO_CURSOR — ensure cur_line is visible
; ============================================================

scroll_to_cursor PROC
    MOV     AX, [cur_line]
    CMP     AX, [top_line]
    JB      .scroll_up
    MOV     BX, [top_line]
    ADD     BX, TEXT_ROWS
    CMP     AX, BX
    JAE     .scroll_down
    RET

.scroll_up:
    MOV     [top_line], AX
    RET

.scroll_down:
    SUB     AX, TEXT_ROWS
    INC     AX
    MOV     [top_line], AX
    RET
scroll_to_cursor ENDP

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
; UPDATE_CURSOR — position hardware cursor on screen
; ============================================================

update_cursor PROC
    PUSH    AX
    PUSH    BX
    PUSH    DX
    MOV     AX, [cur_line]
    SUB     AX, [top_line]
    MOV     DH, AL           ; row
    MOV     DL, BYTE [cur_col]  ; col
    MOV     AH, 02h
    XOR     BH, BH
    INT     10h
    POP     DX
    POP     BX
    POP     AX
    RET
update_cursor ENDP

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
; INSERT_CHAR — insert AL at cursor position
; Input: AL = character to insert
; ============================================================

insert_char PROC
    PUSH    AX
    PUSH    BX
    PUSH    CX
    PUSH    DX
    PUSH    SI
    PUSH    DI
    PUSH    ES

    MOV     CL, AL            ; save char in CL

    ; ---- Append char to add buffer ----
    MOV     AX, [add_seg]
    MOV     ES, AX
    MOV     DI, [add_used]
    CMP     DI, 0FFFFh        ; overflow check
    JAE     .ic_done
    MOV     ES:[DI], CL
    INC     WORD [add_used]

    ; ---- FAST PATH: try to extend tracked piece WITHOUT cursor resolve ----
    CMP     WORD [last_ins_pi], 0FFFFh
    JE      .ic_need_resolve       ; no tracked piece — need full resolve

    ; Read tracked piece descriptor
    MOV     BX, [last_ins_pi]
    PUSH    DS
    MOV     AX, [pt_seg]
    MOV     DS, AX

    ; Compute descriptor offset = BX * 10
    MOV     DI, BX
    SHL     DI, 1
    MOV     AX, DI
    SHL     DI, 1
    SHL     DI, 1
    ADD     DI, AX                 ; DI = BX * 10

    ; Check source == 1 (add buffer)
    CMP     BYTE [DI + PT_F_SOURCE], 1
    JNE     .ic_fast_bail

    ; Check contiguity: piece.start + piece.length + 1 == add_used
    MOV     AX, [DI + PT_F_START]
    ADD     AX, [DI + PT_F_LENGTH]
    INC     AX
    CMP     AX, CS:[add_used]
    JNE     .ic_fast_bail          ; not contiguous

    ; ---- All conditions met: extend in place ----
    INC     WORD [DI + PT_F_LENGTH]
    POP     DS                     ; restore COM segment

    MOV     BYTE [dirty], 1
    INC     WORD [cur_col]
    CALL    scroll_to_cursor
    CALL    render
    CALL    update_cursor
    JMP     .ic_done

.ic_fast_bail:
    POP     DS                     ; restore COM segment

.ic_need_resolve:
    ; ---- SLOW PATH: full cursor resolution ----
    CALL    cursor_to_offset   ; sets rnd_pi, rnd_off
    MOV     BX, [rnd_pi]
    MOV     SI, [rnd_off]      ; SI = piece-relative offset

    ; Guard: if cursor is past all pieces, treat as insert-before
    CMP     BX, [pt_count]
    JAE     .ic_insert_before

    ; ---- Check if we're at piece boundary (offset 0 or end) ----
    ; Read piece length from descriptor
    PUSH    DS
    MOV     AX, [pt_seg]
    MOV     DS, AX

    ; Compute descriptor offset = BX * 10
    MOV     DI, BX
    SHL     DI, 1
    MOV     AX, DI
    SHL     DI, 1
    SHL     DI, 1
    ADD     DI, AX            ; DI = BX * 10

    MOV     DX, [DI + PT_F_LENGTH]  ; DX = piece length (low word)
    POP     DS                 ; restore COM segment

    ; ---- Insert at offset SI within piece BX (length DX) ----

    ; If SI == 0: insert new piece before BX
    TEST    SI, SI
    JZ      .ic_insert_before

    ; If SI == DX: insert new piece after BX
    CMP     SI, DX
    JAE     .ic_insert_after

    ; Otherwise: split piece BX at offset SI
    ; We need: [left(len=SI)] [new_char(len=1)] [right(len=DX-SI)]
    ; Read original piece info with ES = pt_seg
    MOV     AX, [pt_seg]
    MOV     ES, AX

    ; Save fields needed for the right piece
    MOV     AX, DX
    SUB     AX, SI
    PUSH    AX                          ; [stk+6] right_length
    MOV     AX, ES:[DI + PT_F_START + 2]
    PUSH    AX                          ; [stk+4] start_hi
    MOV     AX, ES:[DI + PT_F_START]
    PUSH    AX                          ; [stk+2] start_lo
    MOV     AL, ES:[DI + PT_F_SOURCE]
    XOR     AH, AH
    PUSH    AX                          ; [stk+0] source

    ; Shrink piece BX to length = SI (left part)
    MOV     ES:[DI + PT_F_LENGTH], SI
    MOV     WORD ES:[DI + PT_F_LENGTH + 2], 0

    ; Insert right piece at BX+1
    INC     BX
    CALL    pt_insert
    JC      .ic_split_fail

    ; Write right piece descriptor at slot BX
    MOV     AX, [pt_seg]
    MOV     ES, AX
    MOV     DI, BX
    SHL     DI, 1
    MOV     AX, DI
    SHL     DI, 1
    SHL     DI, 1
    ADD     DI, AX                      ; DI = BX * 10

    POP     AX                          ; source
    MOV     ES:[DI + PT_F_SOURCE], AL
    MOV     BYTE ES:[DI + PT_F_SOURCE + 1], 0

    POP     AX                          ; start_lo
    ADD     AX, SI                      ; right.start = orig_start + SI
    MOV     ES:[DI + PT_F_START], AX
    POP     AX                          ; start_hi
    JNC     .no_carry
    INC     AX
.no_carry:
    MOV     ES:[DI + PT_F_START + 2], AX

    POP     AX                          ; right_length
    MOV     ES:[DI + PT_F_LENGTH], AX
    MOV     WORD ES:[DI + PT_F_LENGTH + 2], 0

    ; Insert new char piece at BX (pushes right piece to BX+1)
    CALL    pt_insert
    JC      .ic_done

    ; Write new char piece at slot BX
    MOV     AX, [pt_seg]
    MOV     ES, AX
    MOV     DI, BX
    SHL     DI, 1
    MOV     AX, DI
    SHL     DI, 1
    SHL     DI, 1
    ADD     DI, AX
    MOV     BYTE ES:[DI + PT_F_SOURCE], 1
    MOV     BYTE ES:[DI + PT_F_SOURCE + 1], 0
    MOV     AX, [add_used]
    DEC     AX
    MOV     ES:[DI + PT_F_START], AX
    MOV     WORD ES:[DI + PT_F_START + 2], 0
    MOV     WORD ES:[DI + PT_F_LENGTH], 1
    MOV     WORD ES:[DI + PT_F_LENGTH + 2], 0

    MOV     [last_ins_pi], BX
    MOV     BYTE [dirty], 1
    CMP     CL, 0Dh
    JE      .ic_done
    CMP     CL, 0Ah
    JE      .ic_done
    INC     WORD [cur_col]
    CALL    scroll_to_cursor
    MOV     BYTE [meta_dirty], 1
    CALL    render
    CALL    update_cursor
    JMP     .ic_done

.ic_split_fail:
    ADD     SP, 8                       ; discard 4 saved words
    JMP     .ic_done

.ic_insert_before:
    ; Insert new piece at index BX (before current piece)
    CALL    pt_insert
    JC      .ic_done

    ; Write new piece: source=1, start=add_used-1, length=1
    PUSH    ES
    MOV     AX, [pt_seg]
    MOV     ES, AX
    ; Recompute DI for piece BX
    MOV     DI, BX
    SHL     DI, 1
    MOV     AX, DI
    SHL     DI, 1
    SHL     DI, 1
    ADD     DI, AX
    MOV     BYTE ES:[DI + PT_F_SOURCE], 1
    MOV     BYTE ES:[DI + PT_F_SOURCE + 1], 0
    MOV     AX, [add_used]
    DEC     AX
    MOV     ES:[DI + PT_F_START], AX
    MOV     WORD ES:[DI + PT_F_START + 2], 0
    MOV     WORD ES:[DI + PT_F_LENGTH], 1
    MOV     WORD ES:[DI + PT_F_LENGTH + 2], 0
    POP     ES

    MOV     [last_ins_pi], BX
    MOV     BYTE [dirty], 1
    ; Advance cursor column (if not CR/LF)
    CMP     CL, 0Dh
    JE      .ic_done
    CMP     CL, 0Ah
    JE      .ic_done
    INC     WORD [cur_col]
    CALL    scroll_to_cursor
    CALL    render
    CALL    update_cursor
    JMP     .ic_done

.ic_insert_after:
    ; Insert new piece at index BX+1 (after current piece)
    INC     BX
    CALL    pt_insert
    JC      .ic_done

    PUSH    ES
    MOV     AX, [pt_seg]
    MOV     ES, AX
    MOV     DI, BX
    SHL     DI, 1
    MOV     AX, DI
    SHL     DI, 1
    SHL     DI, 1
    ADD     DI, AX
    MOV     BYTE ES:[DI + PT_F_SOURCE], 1
    MOV     BYTE ES:[DI + PT_F_SOURCE + 1], 0
    MOV     AX, [add_used]
    DEC     AX
    MOV     ES:[DI + PT_F_START], AX
    MOV     WORD ES:[DI + PT_F_START + 2], 0
    MOV     WORD ES:[DI + PT_F_LENGTH], 1
    MOV     WORD ES:[DI + PT_F_LENGTH + 2], 0
    POP     ES

    MOV     [last_ins_pi], BX
    MOV     BYTE [dirty], 1
    CMP     CL, 0Dh
    JE      .ic_done
    CMP     CL, 0Ah
    JE      .ic_done
    INC     WORD [cur_col]
    CALL    scroll_to_cursor
    CALL    render
    CALL    update_cursor

.ic_done:
    POP     ES
    POP     DI
    POP     SI
    POP     DX
    POP     CX
    POP     BX
    POP     AX
    RET
insert_char ENDP

; ============================================================
; INSERT_CRLF — insert CR+LF (2-byte piece) at cursor position
; ============================================================

insert_crlf PROC
    PUSH    AX
    PUSH    BX
    PUSH    CX
    PUSH    DX
    PUSH    SI
    PUSH    DI
    PUSH    ES

    ; Append CR+LF to add buffer
    MOV     AX, [add_seg]
    MOV     ES, AX
    MOV     DI, [add_used]
    CMP     DI, 0FFFEh
    JAE     .icr_done
    MOV     BYTE ES:[DI], 0Dh
    MOV     BYTE ES:[DI+1], 0Ah
    ADD     WORD [add_used], 2

    ; Find cursor position
    CALL    cursor_to_offset
    MOV     BX, [rnd_pi]
    MOV     SI, [rnd_off]

    ; Guard: if cursor is past all pieces, treat as insert-before
    CMP     BX, [pt_count]
    JAE     .icr_insert_before

    ; Read piece length
    PUSH    DS
    MOV     AX, [pt_seg]
    MOV     DS, AX
    MOV     DI, BX
    SHL     DI, 1
    MOV     AX, DI
    SHL     DI, 1
    SHL     DI, 1
    ADD     DI, AX
    MOV     DX, [DI + PT_F_LENGTH]
    POP     DS

    ; Same insert logic as insert_char: before, after, or split
    TEST    SI, SI
    JZ      .icr_insert_before
    CMP     SI, DX
    JAE     .icr_insert_after

    ; ---- Split piece: [left] [CRLF] [right] ----
    MOV     AX, [pt_seg]
    MOV     ES, AX

    MOV     AX, DX
    SUB     AX, SI
    PUSH    AX                          ; right_length
    MOV     AX, ES:[DI + PT_F_START + 2]
    PUSH    AX                          ; start_hi
    MOV     AX, ES:[DI + PT_F_START]
    PUSH    AX                          ; start_lo
    MOV     AL, ES:[DI + PT_F_SOURCE]
    XOR     AH, AH
    PUSH    AX                          ; source

    MOV     ES:[DI + PT_F_LENGTH], SI
    MOV     WORD ES:[DI + PT_F_LENGTH + 2], 0

    INC     BX
    CALL    pt_insert
    JC      .icr_split_fail

    ; Write right piece at BX
    MOV     AX, [pt_seg]
    MOV     ES, AX
    MOV     DI, BX
    SHL     DI, 1
    MOV     AX, DI
    SHL     DI, 1
    SHL     DI, 1
    ADD     DI, AX
    POP     AX                          ; source
    MOV     ES:[DI + PT_F_SOURCE], AL
    MOV     BYTE ES:[DI + PT_F_SOURCE + 1], 0
    POP     AX                          ; start_lo
    ADD     AX, SI
    MOV     ES:[DI + PT_F_START], AX
    POP     AX                          ; start_hi
    JNC     .icr_nc
    INC     AX
.icr_nc:
    MOV     ES:[DI + PT_F_START + 2], AX
    POP     AX                          ; right_length
    MOV     ES:[DI + PT_F_LENGTH], AX
    MOV     WORD ES:[DI + PT_F_LENGTH + 2], 0

    ; Insert CRLF piece at BX (pushes right to BX+1)
    CALL    pt_insert
    JC      .icr_done
    JMP     .icr_write_piece

.icr_split_fail:
    ADD     SP, 8
    JMP     .icr_done

.icr_insert_before:
    CALL    pt_insert
    JC      .icr_done
    JMP     .icr_write_piece

.icr_insert_after:
    INC     BX
    CALL    pt_insert
    JC      .icr_done
    JMP     .icr_write_piece

.icr_write_piece:
    ; Write CRLF piece at index BX: source=1, start=add_used-2, length=2
    PUSH    ES
    MOV     AX, [pt_seg]
    MOV     ES, AX
    MOV     DI, BX
    SHL     DI, 1
    MOV     AX, DI
    SHL     DI, 1
    SHL     DI, 1
    ADD     DI, AX
    MOV     BYTE ES:[DI + PT_F_SOURCE], 1
    MOV     BYTE ES:[DI + PT_F_SOURCE + 1], 0
    MOV     AX, [add_used]
    SUB     AX, 2
    MOV     ES:[DI + PT_F_START], AX
    MOV     WORD ES:[DI + PT_F_START + 2], 0
    MOV     WORD ES:[DI + PT_F_LENGTH], 2
    MOV     WORD ES:[DI + PT_F_LENGTH + 2], 0
    POP     ES

    MOV     BYTE [dirty], 1
    MOV     WORD [last_ins_pi], 0FFFFh

.icr_done:
    POP     ES
    POP     DI
    POP     SI
    POP     DX
    POP     CX
    POP     BX
    POP     AX
    RET
insert_crlf ENDP

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

; ============================================================
; DELETE_BYTE — delete 1 byte at {rnd_pi, rnd_off}
; Returns: CL = deleted byte, CF=0 success, CF=1 error
; Clobbers: AX, BX, DX, SI, DI, ES (caller must save)
; ============================================================

delete_byte PROC
    MOV     BX, [rnd_pi]
    CMP     BX, [pt_count]
    JAE     .db_fail

    ; Read the byte at rnd_off
    PUSH    CX
    CALL    resolve_piece      ; AX=seg, SI=base, DX=piece_len
    POP     CX
    JC      .db_fail
    MOV     ES, AX
    MOV     DI, SI
    ADD     DI, [rnd_off]
    MOV     CL, ES:[DI]       ; CL = byte being deleted

    ; Get piece length from descriptor
    MOV     BX, [rnd_pi]
    MOV     SI, [rnd_off]     ; SI = offset within piece
    PUSH    DS
    MOV     AX, [pt_seg]
    MOV     DS, AX
    MOV     DI, BX
    SHL     DI, 1
    MOV     AX, DI
    SHL     DI, 1
    SHL     DI, 1
    ADD     DI, AX            ; DI = BX * 10
    MOV     DX, [DI + PT_F_LENGTH]
    POP     DS

    ; Case 1: piece length == 1 → remove entire piece
    CMP     DX, 1
    JE      .db_remove

    ; Case 2: offset == 0 → shrink from start
    TEST    SI, SI
    JZ      .db_shrink_start

    ; Case 3: offset == length-1 → shrink from end
    MOV     AX, DX
    DEC     AX
    CMP     SI, AX
    JAE     .db_shrink_end

    ; Case 4: middle → split piece, skip 1 byte
    JMP     .db_split

.db_remove:
    MOV     BYTE [meta_dirty], 1
    CALL    pt_remove
    CLC
    RET

.db_shrink_start:
    PUSH    DS
    MOV     AX, [pt_seg]
    MOV     DS, AX
    MOV     DI, BX
    SHL     DI, 1
    MOV     AX, DI
    SHL     DI, 1
    SHL     DI, 1
    ADD     DI, AX
    MOV     AX, [DI + PT_F_START]
    ADD     AX, 1
    MOV     [DI + PT_F_START], AX
    JNC     .ss_nc
    INC     WORD [DI + PT_F_START + 2]
.ss_nc:
    DEC     WORD [DI + PT_F_LENGTH]
    POP     DS
    CLC
    RET

.db_shrink_end:
    PUSH    DS
    MOV     AX, [pt_seg]
    MOV     DS, AX
    MOV     DI, BX
    SHL     DI, 1
    MOV     AX, DI
    SHL     DI, 1
    SHL     DI, 1
    ADD     DI, AX
    DEC     WORD [DI + PT_F_LENGTH]
    POP     DS
    CLC
    RET

.db_split:
    ; Left: length = SI.  Right: start += SI+1, length = DX - SI - 1
    PUSH    DS
    MOV     AX, [pt_seg]
    MOV     ES, AX
    MOV     DI, BX
    SHL     DI, 1
    MOV     AX, DI
    SHL     DI, 1
    SHL     DI, 1
    ADD     DI, AX

    ; Save right piece fields
    MOV     AX, DX
    SUB     AX, SI
    DEC     AX
    PUSH    AX                          ; right_length
    MOV     AX, ES:[DI + PT_F_START + 2]
    PUSH    AX                          ; start_hi
    MOV     AX, ES:[DI + PT_F_START]
    PUSH    AX                          ; start_lo
    MOV     AL, ES:[DI + PT_F_SOURCE]
    XOR     AH, AH
    PUSH    AX                          ; source

    ; Shrink left to length = SI
    MOV     ES:[DI + PT_F_LENGTH], SI
    MOV     WORD ES:[DI + PT_F_LENGTH + 2], 0

    ; Insert right piece at BX+1
    INC     BX
    POP     DS
    PUSH    DS                          ; save source, restore later

    PUSH    CS
    POP     DS
    CALL    pt_insert
    JC      .db_split_fail
    MOV     BYTE CS:[meta_dirty], 1

    ; Write right piece descriptor
    MOV     AX, [pt_seg]
    MOV     ES, AX
    MOV     DI, BX
    SHL     DI, 1
    MOV     AX, DI
    SHL     DI, 1
    SHL     DI, 1
    ADD     DI, AX

    POP     AX                          ; source
    MOV     ES:[DI + PT_F_SOURCE], AL
    MOV     BYTE ES:[DI + PT_F_SOURCE + 1], 0
    POP     AX                          ; start_lo
    ADD     AX, SI
    INC     AX
    MOV     ES:[DI + PT_F_START], AX
    POP     AX                          ; start_hi
    JNC     .ds_nc
    INC     AX
.ds_nc:
    MOV     ES:[DI + PT_F_START + 2], AX
    POP     AX                          ; right_length
    MOV     ES:[DI + PT_F_LENGTH], AX
    MOV     WORD ES:[DI + PT_F_LENGTH + 2], 0

    POP     DS                          ; restore original DS

    ; Remove zero-length left piece if needed
    DEC     BX
    TEST    SI, SI
    JNZ     .db_split_ck_right
    CALL    pt_remove
    CLC
    RET

.db_split_ck_right:
    INC     BX
    MOV     AX, DX
    SUB     AX, SI
    DEC     AX
    TEST    AX, AX
    JNZ     .db_split_ok
    CALL    pt_remove
.db_split_ok:
    CLC
    RET

.db_split_fail:
    ADD     SP, 8                       ; discard 4 saved words
    POP     DS
.db_fail:
    STC
    RET
delete_byte ENDP

; ============================================================
; DELETE_AT — delete 1 byte (+ CRLF companion) at cursor pos
; Uses cursor_to_offset, then delete_byte, then CRLF handling.
; ============================================================

delete_at PROC
    PUSH    AX
    PUSH    BX
    PUSH    CX
    PUSH    DX
    PUSH    SI
    PUSH    DI
    PUSH    ES

    CALL    cursor_to_offset   ; sets rnd_pi, rnd_off
    CALL    delete_byte        ; CL = deleted byte
    JC      .da_done

    MOV     BYTE [dirty], 1

    ; ---- CRLF handling ----
    CMP     CL, 0Dh
    JE      .da_deleted_cr
    CMP     CL, 0Ah
    JE      .da_deleted_lf
    JMP     .da_done

.da_deleted_cr:
    MOV     BYTE [meta_dirty], 1
    ; Deleted CR — check if byte now at cursor position is LF
    CALL    cursor_to_offset
    MOV     BX, [rnd_pi]
    CMP     BX, [pt_count]
    JAE     .da_done
    PUSH    CX
    CALL    resolve_piece      ; AX=seg, SI=base, DX=piece_len
    POP     CX
    JC      .da_done
    MOV     ES, AX
    MOV     DI, SI
    ADD     DI, [rnd_off]
    ; Check bounds
    ADD     SI, DX             ; SI = piece end
    CMP     DI, SI
    JAE     .da_done
    CMP     BYTE ES:[DI], 0Ah
    JNE     .da_done
    ; Delete the LF (rnd_pi/rnd_off already correct)
    CALL    delete_byte
    JMP     .da_done

.da_deleted_lf:
    MOV     BYTE [meta_dirty], 1
    ; Deleted LF — check if preceding byte is CR
    CALL    cursor_to_offset
    MOV     BX, [rnd_pi]
    MOV     SI, [rnd_off]
    TEST    SI, SI
    JNZ     .da_lf_same_piece
    ; Offset 0 — look at last byte of previous piece
    TEST    BX, BX
    JZ      .da_done
    DEC     BX
    PUSH    CX
    CALL    resolve_piece      ; AX=seg, SI=base, DX=piece_len
    POP     CX
    JC      .da_done
    MOV     ES, AX
    MOV     DI, SI
    ADD     DI, DX
    DEC     DI
    CMP     BYTE ES:[DI], 0Dh
    JNE     .da_done
    ; Set rnd_pi/rnd_off to point to this CR
    MOV     [rnd_pi], BX
    DEC     DX
    MOV     [rnd_off], DX
    CALL    delete_byte
    JMP     .da_done

.da_lf_same_piece:
    ; Offset > 0 — check byte at rnd_off-1 in same piece
    PUSH    CX
    CALL    resolve_piece
    POP     CX
    JC      .da_done
    MOV     ES, AX
    MOV     DI, SI
    ADD     DI, [rnd_off]
    DEC     DI
    CMP     BYTE ES:[DI], 0Dh
    JNE     .da_done
    DEC     WORD [rnd_off]
    CALL    delete_byte
    JMP     .da_done

.da_done:
    MOV     WORD [last_ins_pi], 0FFFFh
    POP     ES
    POP     DI
    POP     SI
    POP     DX
    POP     CX
    POP     BX
    POP     AX
    RET
delete_at ENDP

; ============================================================
; SAVE_FILE — write piece table contents to filename
; Clears dirty flag on success.
; ============================================================

save_file PROC
    PUSH    AX
    PUSH    BX
    PUSH    CX
    PUSH    DX
    PUSH    SI
    PUSH    DI
    PUSH    BP
    PUSH    DS
    PUSH    ES

    ; Create/truncate output file
    MOV     DX, filename
    XOR     CX, CX            ; normal attributes
    MOV     AH, 3Ch
    INT     21h
    JC      .sf_done           ; create failed

    MOV     BP, AX             ; BP = file handle

    ; Walk pieces, coalescing contiguous same-segment runs.
    XOR     DI, DI             ; DI = piece index

    ; Resolve first piece to seed the pending write
    CMP     DI, [pt_count]
    JAE     .sf_close_ok
    MOV     BX, DI
    CALL    resolve_piece      ; AX=seg, SI=base, DX=piece_len
    JC      .sf_close_ok

    ; Seed pending write: segment AX, offset SI, length DX
    PUSH    AX                 ; [stk] = pending segment
    MOV     [rnd_base], SI     ; pending start offset
    MOV     [rnd_len], DX      ; pending length
    INC     DI

.sf_coalesce:
    CMP     DI, [pt_count]
    JAE     .sf_flush           ; no more pieces — flush pending

    MOV     BX, DI
    CALL    resolve_piece      ; AX=seg, SI=base, DX=piece_len
    JC      .sf_flush

    ; Check if this piece is contiguous with pending write
    POP     BX                 ; BX = pending segment
    PUSH    BX
    CMP     AX, BX
    JNE     .sf_break          ; different segment

    MOV     BX, [rnd_base]
    ADD     BX, [rnd_len]      ; BX = pending end offset
    CMP     SI, BX
    JNE     .sf_break          ; not contiguous

    ; Contiguous — extend pending write
    ADD     [rnd_len], DX
    INC     DI
    JMP     .sf_coalesce

.sf_break:
    ; Flush pending write, then seed new one from current piece
    POP     BX                 ; BX = pending segment (consumed)

    PUSH    AX                 ; save current piece segment
    PUSH    SI                 ; save current piece base
    PUSH    DX                 ; save current piece length

    MOV     DS, BX
    MOV     DX, CS:[rnd_base]
    MOV     CX, CS:[rnd_len]
    MOV     BX, BP
    MOV     AH, 40h
    INT     21h

    PUSH    CS
    POP     DS
    JC      .sf_break_err

    POP     DX                 ; current piece length
    POP     SI                 ; current piece base
    POP     AX                 ; current piece segment

    PUSH    AX                 ; push new pending segment
    MOV     [rnd_base], SI
    MOV     [rnd_len], DX
    INC     DI
    JMP     .sf_coalesce

.sf_flush:
    ; Write final pending block
    POP     BX                 ; BX = pending segment
    MOV     DS, BX
    MOV     DX, CS:[rnd_base]
    MOV     CX, CS:[rnd_len]
    MOV     BX, BP
    MOV     AH, 40h
    INT     21h

    PUSH    CS
    POP     DS
    JC      .sf_err

.sf_close_ok:
    PUSH    CS
    POP     DS
    MOV     BX, BP
    MOV     AH, 3Eh
    INT     21h
    MOV     BYTE [dirty], 0
    JMP     .sf_done

.sf_break_err:
    ADD     SP, 6              ; discard 3 saved words
    ; fall through to sf_err

.sf_err:
    PUSH    CS
    POP     DS
    MOV     BX, BP
    MOV     AH, 3Eh
    INT     21h
    ; dirty stays set

.sf_done:
    POP     ES
    POP     DS
    POP     BP
    POP     DI
    POP     SI
    POP     DX
    POP     CX
    POP     BX
    POP     AX
    RET
save_file ENDP


; ============================================================
; SHRINK_MEM
; ============================================================

shrink_mem:
    PUSH    ES
    MOV     AX, CS
    MOV     ES, AX
    MOV     AH, 4Ah
    MOV     BX, 0800h
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
    XOR     BP, BP
.next_piece:
    CMP     BP, [pt_count]
    JAE     .done
    MOV     BX, BP
    CALL    resolve_piece
    JC      .done
    MOV     ES, AX
    MOV     [rnd_base], SI
    MOV     DI, SI
    ADD     DI, DX
    INC     BP

    CMP     BYTE [wrap_mode], 0
    JNE     .byte_loop_wrap        ; wrap mode: use original byte loop

    ; ---- NON-WRAP FAST PATH: REPNE SCASB ----
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

    ; ---- WRAP MODE: original byte-by-byte loop ----
.byte_loop_wrap:
    CMP     SI, DI
    JAE     .end_walk
    MOV     AL, ES:[SI]
    INC     SI
    CMP     AL, 0Dh
    JE      .byte_loop_wrap
    CMP     AL, 0Ah
    JNE     .not_lf_wrap
    MOV     WORD [stl_col], 0
    INC     WORD [total_lines]
    MOV     AX, [total_lines]
    TEST    AX, CHKPT_INTERVAL - 1
    JNZ     .byte_loop_wrap
    CALL    .record_chkpt
    JMP     .byte_loop_wrap
.not_lf_wrap:
    INC     WORD [stl_col]
    MOV     AX, [stl_col]
    CMP     AX, SCREEN_COLS
    JB      .byte_loop_wrap
    MOV     WORD [stl_col], 0
    INC     WORD [total_lines]
    MOV     AX, [total_lines]
    TEST    AX, CHKPT_INTERVAL - 1
    JNZ     .byte_loop_wrap
    CALL    .record_chkpt
    JMP     .byte_loop_wrap

.end_walk:
    JMP     .next_piece
.done:
    CMP     WORD [pt_count], 0
    JE      .no_plus
    INC     WORD [total_lines]
.no_plus:
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

    CMP     BYTE [wrap_mode], 0
    JNE     .stl_inner_wrap        ; wrap mode: use byte-by-byte loop

    ; ---- NON-WRAP FAST PATH: REPNE SCASB for LF scanning ----
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

    ; ---- WRAP MODE: byte-by-byte walk ----
.stl_inner_wrap:
    PUSH    DS
    MOV     DS, AX
    MOV     SI, CS:[rnd_base]
    ADD     SI, CS:[rnd_off]
.stl_iw_byte:
    CMP     SI, DX
    JAE     .stl_iw_next_piece
    MOV     AL, [SI]
    INC     SI
    CMP     AL, 0Dh
    JE      .stl_iw_byte
    CMP     AL, 0Ah
    JNE     .stl_iw_not_lf
    MOV     WORD CS:[stl_col], 0
    DEC     CX
    JZ      .stl_iw_found
    JMP     .stl_iw_byte
.stl_iw_not_lf:
    INC     WORD CS:[stl_col]
    MOV     AX, CS:[stl_col]
    CMP     AX, SCREEN_COLS
    JB      .stl_iw_byte
    MOV     WORD CS:[stl_col], 0
    DEC     CX
    JZ      .stl_iw_found
    JMP     .stl_iw_byte
.stl_iw_next_piece:
    POP     DS
    INC     WORD [rnd_pi]
    MOV     WORD [rnd_off], 0
    MOV     BX, [rnd_pi]
    CMP     BX, [pt_count]
    JAE     .stl_eof
    JMP     .stl_walk_loop
.stl_iw_found:
    POP     DS
    SUB     SI, [rnd_base]
    MOV     AX, DX
    SUB     AX, [rnd_base]         ; AX = piece length
    CMP     SI, AX
    JB      .stl_iw_found_ok
    INC     WORD [rnd_pi]
    MOV     WORD [rnd_off], 0
    JMP     .stl_done
.stl_iw_found_ok:
    MOV     [rnd_off], SI
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
; RENDER
; ============================================================

render:
    PUSH    DS
    PUSH    ES
    PUSH    BP
    PUSH    BX
    PUSH    SI
    PUSH    DI
    MOV     AX, VIDEO_SEG
    MOV     ES, AX
    MOV     BP, [top_line]
    MOV     AX, [top_line]
    CALL    seek_to_line
    MOV     WORD [rnd_row], 0
    XOR     DI, DI

.row_loop:
    MOV     AX, [rnd_row]
    CMP     AX, TEXT_ROWS
    JAE     .do_status
    MOV     AX, BP
    CMP     AX, [total_lines]
    JAE     .blank_row
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
    MOV     AH, ATTR_NORM

.char_loop:
    CMP     CX, SCREEN_COLS
    JAE     .row_done
    CMP     SI, CS:[rnd_len]
    JAE     .adv_piece
    MOV     AL, [SI]
    INC     SI
    CMP     AL, 0Dh
    JE      .char_loop
    CMP     AL, 0Ah
    JE      .row_done_lf
    MOV     ES:[DI], AX
    ADD     DI, 2
    INC     CX
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
    MOV     AH, ATTR_NORM
    JMP     .char_loop

.row_done_lf:
    PUSH    AX
    MOV     AX, SI
    SUB     AX, CS:[rnd_base]
    MOV     CS:[rnd_off], AX
    POP     AX
    JMP     .row_pad

.row_done:
    ; In non-wrap mode: skip remaining bytes until LF (truncate long line)
    CMP     BYTE CS:[wrap_mode], 0
    JNE     .row_done_store
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
    MOV     AX, SCREEN_COLS
    SUB     AX, CX
    MOV     CX, AX
    JCXZ    .row_next
    MOV     AX, 0720h
    REP     STOSW
    JMP     .row_next

.blank_row:
    MOV     CX, SCREEN_COLS
    MOV     AX, 0720h
    REP     STOSW

.row_next:
    INC     WORD [rnd_row]
    INC     BP
    JMP     .row_loop

; ---- STATUS BAR ----
.do_status:
    PUSH    CS
    POP     DS
    CALL    repaint_status
    POP     DI
    POP     SI
    POP     BX
    POP     BP
    POP     ES
    POP     DS
    RET

; ============================================================
; WSTR_STAT — write '$'-terminated string to video with ATTR_STAT
; Input:  DS:SI = string, ES:DI = video position
; Output: DI advanced past written characters
; ============================================================

wstr_stat PROC
.lp:
    MOV     AL, [SI]
    CMP     AL, '$'
    JE      .done
    MOV     ES:[DI], AL
    MOV     BYTE ES:[DI+1], ATTR_STAT
    ADD     DI, 2
    INC     SI
    JMP     .lp
.done:
    RET
wstr_stat ENDP

; ============================================================
; WDEC_STAT — write AX as decimal digits to video with ATTR_STAT
; Input:  AX = value, ES:DI = video position
; Output: DI advanced past written digits
; ============================================================

wdec_stat PROC
    PUSH    AX
    PUSH    BX
    PUSH    CX
    PUSH    DX
    MOV     BX, 10
    XOR     CX, CX
    TEST    AX, AX
    JNZ     .split
    MOV     BYTE ES:[DI], '0'
    MOV     BYTE ES:[DI+1], ATTR_STAT
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
    MOV     BYTE ES:[DI+1], ATTR_STAT
    ADD     DI, 2
    LOOP    .emit
.done:
    POP     DX
    POP     CX
    POP     BX
    POP     AX
    RET
wdec_stat ENDP

; ============================================================
; REPAINT_STATUS — paint the status bar (row 24)
; Assumes DS = COM segment. Sets ES = VIDEO_SEG internally.
; ============================================================

repaint_status PROC
    PUSH    ES
    PUSH    DI
    PUSH    SI
    PUSH    AX
    PUSH    BX
    PUSH    CX
    PUSH    DX

    MOV     AX, VIDEO_SEG
    MOV     ES, AX
    MOV     DI, TEXT_ROWS * SCREEN_COLS * 2

    MOV     SI, s_stat_hdr
    CALL    wstr_stat

    MOV     AX, [cur_line]
    INC     AX
    CALL    wdec_stat

    MOV     SI, s_stat_sep
    CALL    wstr_stat

    MOV     AX, [total_lines]
    CALL    wdec_stat

    MOV     SI, s_stat_col
    CALL    wstr_stat

    MOV     AX, [cur_col]
    INC     AX
    CALL    wdec_stat

    CMP     BYTE [wrap_mode], 0
    JE      .no_wrap
    MOV     SI, s_stat_wrap
    CALL    wstr_stat
.no_wrap:

    CMP     BYTE [dirty], 0
    JE      .no_mod
    MOV     SI, s_stat_mod
    CALL    wstr_stat
.no_mod:

    MOV     SI, s_stat_keys
    CALL    wstr_stat

.pad:
    MOV     CX, SCREEN_ROWS * SCREEN_COLS * 2
    SUB     CX, DI
    JBE     .done
    SHR     CX, 1
    MOV     AX, 7020h
    REP     STOSW

.done:
    POP     DX
    POP     CX
    POP     BX
    POP     AX
    POP     SI
    POP     DI
    POP     ES
    RET
repaint_status ENDP

; ============================================================
; ============================================================
; CLEAR_SCR
; ============================================================

clear_scr:
    PUSH    ES
    PUSH    DI
    PUSH    CX
    PUSH    AX
    MOV     AX, VIDEO_SEG
    MOV     ES, AX
    XOR     DI, DI
    MOV     CX, SCREEN_ROWS * SCREEN_COLS
    MOV     AX, 0720h
    REP     STOSW
    POP     AX
    POP     CX
    POP     DI
    POP     ES
    RET

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
    JZ      .done
    MOV     ES, AX
    MOV     AH, 49h
    INT     21h
.done:
    POP     SI
    POP     CX
    POP     ES
    RET

; ============================================================
; End of TEXTEDIT.ASM
; ============================================================
