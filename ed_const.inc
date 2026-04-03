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

; Side panel constants
PANEL_WIDTH     EQU 18          ; total columns (│»N FILENAME.EXT*│)
PANEL_ATTR_NORM EQU 3Fh         ; bright white on cyan (normal entry)
PANEL_ATTR_HI   EQU 3Eh         ; yellow on cyan (highlighted entry)
PANEL_ATTR_HDR  EQU 30h         ; black on cyan (section header)
KEY_F1          EQU 3Bh         ; F1 scan code (extended key)

; ---- Piece table ----
PT_DESC_SIZE    EQU 10
PT_F_SOURCE     EQU 0
PT_F_START      EQU 2
PT_F_LENGTH     EQU 6
MAX_PT_PIECES   EQU 4096
PT_SEG_PARA     EQU 0A00h       ; 40 KB for piece table segment

; ---- Original buffer / file-backed cache ----
CHUNK_PARA      EQU 0800h       ; 32 KB per chunk (in paragraphs)
CHUNK_BYTES     EQU 8000h       ; 32 KB per chunk (in bytes)
CACHE_SLOTS      EQU 2           ; number of LRU cache slots
CACHE_CHUNK_PARA EQU CHUNK_PARA  ; cache slot = one chunk
CACHE_CHUNK_BYTES EQU CHUNK_BYTES
CACHE_EMPTY      EQU 0FFFFh      ; sentinel: cache slot is empty

; ---- Add buffer ----
ADD_BUF_PARA    EQU 1000h       ; 64 KB add buffer (in paragraphs)
ADD_BUF_BYTES   EQU ADD_BUF_PARA * 16    ; usable bytes in add buffer

; ---- Checkpoint table ----
CHKPT_INTERVAL  EQU 256
MAX_CHKPT       EQU 2048

; ---- Memory shrink size ----
SHRINK_PARA     EQU 1000h       ; keep 64 KB for COM segment

; ---- Shadow buffer segment ----
SHADOW_SEG_PARA EQU 0100h       ; 4 KB (256 paragraphs) for shadow buffer

; ---- Document table segment ----
DOC_TBL_SEG_PARA EQU 00C0h     ; 3 KB (192 paragraphs) for doc_table

; ---- Range delete ----
RD_MAX_PIECES   EQU 102            ; max pieces saveable in one range delete

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
UR_DEL_RANGE    EQU 4              ; range deletion (pieces saved in rd_save_buf)

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

; ---- Clipboard ----
CLIP_SEG_PARA   EQU 0400h          ; 16 KB clipboard segment (1024 paragraphs)
CLIP_BUF_BYTES  EQU CLIP_SEG_PARA * 16   ; = 16384 usable bytes

; ---- Paste undo ----
UR_INS_RANGE    EQU 5              ; range insertion (paste)

; ---- Selection grace period ----
SEL_GRACE_TICKS EQU 9              ; ~500ms at 18.2 ticks/sec

; ---- Tab ----
ED_TAB_DEFAULT  EQU 8               ; default tab width (2, 4, or 8)

; ---- Insert key (extended scan code) ----
KEY_INSERT      EQU 52h

; ---- Replace All threshold ----
REPL_PERM_THRESHOLD EQU 50         ; matches above this → permanent (no undo)

; ---- Find / Replace ----
FIND_MAX_LEN    EQU 64              ; max search/replace term length (bytes)
FIND_FLAG_CI    EQU 01h             ; bit 0: case-insensitive search
FIND_FLAG_SEL   EQU 02h             ; bit 1: find in selection only

; ---- Multi-document (Hot Seat architecture) ----
MAX_DOCS         EQU 20             ; maximum simultaneous document slots

; Document slot flags (DOC_FLAGS byte, bit field)
DOCF_OCCUPIED    EQU 01h            ; bit 0: slot is in use
DOCF_SWAPPED     EQU 02h            ; bit 1: document state is on disk
DOCF_DIRTY       EQU 04h            ; bit 2: cached dirty flag (for tab display)
DOCF_UNTITLED    EQU 08h            ; bit 3: no filename (new unsaved document)

; Document table slot layout (152 bytes per slot)
DOC_FLAGS        EQU 0              ; BYTE  - DOCF_* flags
DOC_SLOT_ID      EQU 1              ; BYTE  - slot index (0-19)
DOC_FILENAME     EQU 2              ; 128 bytes - full file path (null-terminated)
DOC_SWAP_PATH    EQU 130            ; 14 bytes  - swap temp filename (null-terminated)
DOC_CACHED_LINES EQU 144            ; WORD  - cached total_lines (for tab display)
DOC_CACHED_LINE  EQU 146            ; WORD  - cached cur_line
DOC_CACHED_COL   EQU 148            ; WORD  - cached cur_col
DOC_RESERVED     EQU 150            ; WORD  - reserved (zero)
DOC_SLOT_SIZE    EQU 152            ; total bytes per slot

; Swap file constants
SWAP_MAGIC       EQU 4554h          ; 'TE' (little-endian: 54h='T', 45h='E')
SWAP_VERSION     EQU 2
SWAP_HEADER_SIZE EQU 128            ; bytes in swap file header

; Swap header field offsets (128-byte header)
SWH_MAGIC        EQU 0              ; WORD  - 'TE' (4554h)
SWH_VERSION      EQU 2              ; WORD  - version (1)
SWH_PT_COUNT     EQU 4              ; WORD  - piece table entry count
SWH_ADD_USED     EQU 6              ; WORD  - add buffer bytes used
SWH_UNDO_POS     EQU 8              ; WORD  - undo write cursor
SWH_UNDO_COUNT   EQU 10             ; WORD  - undo record count
SWH_UNDO_SPOS    EQU 12             ; WORD  - undo_save_pos
SWH_RD_SAVE_USED EQU 14             ; WORD  - rd_save_used count
SWH_CUR_LINE     EQU 16             ; WORD  - cursor line
SWH_CUR_COL      EQU 18             ; WORD  - cursor column
SWH_TOP_LINE     EQU 20             ; WORD  - scroll top line
SWH_LEFT_COL     EQU 22             ; WORD  - horizontal scroll offset
SWH_DIRTY        EQU 24             ; BYTE  - dirty flag
SWH_META_DIRTY   EQU 25             ; BYTE  - always 1 (rebuild on load)
SWH_OVERWRITE    EQU 26             ; BYTE  - overwrite mode
SWH_FIND_FLAGS   EQU 27             ; BYTE  - find flags
SWH_TAB_WIDTH    EQU 28             ; WORD  - tab width
SWH_TAB_MASK     EQU 30             ; WORD  - tab mask
SWH_TOTAL_LINES  EQU 32             ; WORD  - total lines (informational)
SWH_ORIG_COUNT   EQU 34             ; WORD  - reserved (was orig_count, now unused)
SWH_DOC_CWD      EQU 36             ; 68 bytes - document's CWD "D:\path\0"
SWH_DOC_CWD_SIZE EQU 68
SWH_RESERVED     EQU 104            ; 24 bytes reserved (zero-filled)

; ---- Multi-doc constant verification ----
ASSERT DOC_SLOT_SIZE                         ; must be nonzero
ASSERT DOC_SLOT_SIZE, 152                    ; exact size match
ASSERT MAX_DOCS * DOC_SLOT_SIZE, 3040        ; total table size
ASSERT DOC_SWAP_PATH, 130                    ; offset sanity
ASSERT DOC_CACHED_LINES, 144                 ; offset sanity
ASSERT SWAP_HEADER_SIZE, 128
ASSERT SWH_DOC_CWD + SWH_DOC_CWD_SIZE, 104  ; CWD ends at reserved
ASSERT SWH_RESERVED + 24, SWAP_HEADER_SIZE   ; reserved fills to 128
ASSERT SWH_MAGIC, 0
ASSERT SWH_PT_COUNT, 4
ASSERT SWH_ORIG_COUNT, 34
PRINT "DOC_SLOT_SIZE = ", DOC_SLOT_SIZE
PRINT "DOC_TABLE_TOTAL = ", MAX_DOCS * DOC_SLOT_SIZE
PRINT "SWAP_HEADER_SIZE = ", SWAP_HEADER_SIZE

; ---- Shell swap (memory dump to disk for Shell to DOS) ----
SHELL_SWAP_MAGIC   EQU 4853h       ; 'SH' (little-endian: 53h='S', 48h='H')
SHELL_SWAP_VERSION EQU 1

; Shell swap global header (128 bytes)
SHELL_HDR_SIZE     EQU 128
SHL_MAGIC          EQU 0            ; WORD  - magic (4853h)
SHL_VERSION        EQU 2            ; WORD  - version (1)
SHL_DOC_COUNT      EQU 4            ; BYTE  - number of occupied doc slots
SHL_ACTIVE_DOC     EQU 5            ; BYTE  - active document index
SHL_CLIP_USED      EQU 6            ; WORD  - clipboard bytes used
SHL_CLIP_LINES     EQU 8            ; WORD  - clipboard line count
SHL_CLIP_LAST_COL  EQU 10           ; WORD  - clipboard last line column width
SHL_RESERVED       EQU 12           ; 116 bytes reserved (zero-filled)

; Shell swap verification
ASSERT SHELL_HDR_SIZE, 128
ASSERT SHL_RESERVED + 116, SHELL_HDR_SIZE

