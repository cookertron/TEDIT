; ============================================================
; ed_const.inc — Editor EQU Constants
;
; Must be included BEFORE TUI\tui.inc because the TUI code
; references CTYPE_EDITOR, CTRL_ED_STATE, ED_SCROLLY, ED_LINECOUNT.
; ============================================================

; ---- Editor control type (TUI dispatch tables reference these) ----
CTYPE_EDITOR    EQU 9
CTRL_ED_STATE   EQU 14          ; WORD at control offset 14: ptr to ed_state
ED_SCROLLY      EQU 0           ; WORD at ed_state offset 0 = top_line
ED_LINECOUNT    EQU 2           ; WORD at ed_state offset 2 = total_lines

; ---- Display dimensions ----
ED_COLS         EQU 80          ; visible columns
ED_TEXT_ROWS    EQU 23          ; text rows (23 text + 1 status = 24 total)

; ---- Text attributes for rendering ----
ED_ATTR_NORM    EQU 1Fh         ; bright white on blue (text content)
ED_ATTR_STAT    EQU 70h         ; black on light gray (status bar)

; ---- Piece table ----
PT_DESC_SIZE    EQU 10
PT_F_SOURCE     EQU 0
PT_F_START      EQU 2
PT_F_LENGTH     EQU 6
MAX_PT_PIECES   EQU 4096
PT_SEG_PARA     EQU 0A00h       ; 40 KB for piece table segment

; ---- Original buffer / demand paging ----
MAX_ORIG        EQU 1024        ; max 1024 chunks
CHUNK_PARA      EQU 0800h       ; 32 KB per chunk (in paragraphs)
CHUNK_BYTES     EQU 8000h       ; 32 KB per chunk (in bytes)

; ---- Add buffer ----
ADD_BUF_PARA    EQU 1000h       ; 64 KB add buffer (in paragraphs)

; ---- Checkpoint table ----
CHKPT_INTERVAL  EQU 256
MAX_CHKPT       EQU 2048

; ---- Memory shrink size ----
SHRINK_PARA     EQU 1000h       ; keep 64 KB for COM segment
