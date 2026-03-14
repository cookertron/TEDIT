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
ED_ATTR_SEL     EQU 71h         ; blue on light gray (selected text)

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
ADD_BUF_BYTES   EQU ADD_BUF_PARA * 16    ; usable bytes in add buffer

; ---- Checkpoint table ----
CHKPT_INTERVAL  EQU 256
MAX_CHKPT       EQU 2048

; ---- Memory shrink size ----
SHRINK_PARA     EQU 1000h       ; keep 64 KB for COM segment

; ---- Undo / Redo ----
UNDO_SEG_PARA   EQU 0800h          ; 32 KB undo segment (2048 paragraphs)

; Undo record size and maximum
UR_SIZE         EQU 10              ; bytes per undo record
UNDO_MAX_RECORDS EQU (UNDO_SEG_PARA * 16) / UR_SIZE   ; = 3276

; Record type codes (bits 0-3; bits 4-7 reserved for flags)
UR_INS_CHAR     EQU 0              ; a character was inserted
UR_INS_CRLF     EQU 1              ; a CR+LF pair was inserted
UR_DEL_CHAR     EQU 2              ; a character was deleted
UR_DEL_CRLF     EQU 3              ; a CR+LF pair was deleted

; Record type mask and group flag (stored in UR_TYPE byte)
UR_TYPE_MASK    EQU 0Fh            ; low nibble = record type
URF_GROUP_CONT  EQU 80h            ; bit 7 of UR_TYPE = group continuation

; Record field offsets within each 10-byte record
UR_TYPE         EQU 0              ; BYTE  - type + flags
UR_CHAR         EQU 1              ; BYTE  - character (full 0-255 range)
UR_DOC_LINE     EQU 2              ; WORD  - document line of edit
UR_DOC_COL      EQU 4              ; WORD  - document column of edit
UR_CUR_LINE     EQU 6              ; WORD  - cursor line to restore on undo
UR_CUR_COL      EQU 8              ; WORD  - cursor col to restore on undo

; Compile-time verification
ASSERT UR_SIZE, 10
ASSERT UNDO_MAX_RECORDS
PRINT "UNDO_MAX_RECORDS = ", UNDO_MAX_RECORDS

