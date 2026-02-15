! Kernel runtime stub (replaces ACK boot.s)
! Entry point called from trap.s boot entry in NONSEG+SYS mode.

.define EXIT, WRITE, BRK
.define ERANGE, ESET, EHEAP, EILLINS, EODDZ, ECASE, EBADMON
.define hol0, trppc, trpim, reghp
.define LINO_AD, FILN_AD
.define _putc, _putchar, _inb, _outb, _idle

.sect .text
.sect .rom
.sect .data
.sect .bss
.sect .text

LINO_AD = 0
FILN_AD = 4
ERANGE  = 1
ESET    = 2
EHEAP   = 17
EILLINS = 18
EODDZ   = 19
ECASE   = 20
EBADMON = 25

! --- Entry point (offset 0x0000 in this binary, loaded at 0x0200) ---
! Called from trap.s boot entry.
! First, zero BSS (C requires globals to be zero-initialized).
    ld      R2, $begbss
    ld      R3, $endbss
1:  cp      R2, R3
    jr      GE, 2f
    clr     *RR2            ! z8002: *RR2 dereferences R2
    inc     R2, $2
    jr      1b
2:  calr    _main
    halt

EXIT:   halt
WRITE:  ret
BRK:    ret

! --- void putc(int ch) ---
! Outputs a character to console port 0x00F0.
_putc:
    push    *SP, R13
    ld      R13, R15
    ld      R1, 4(R13)      ! arg: character (EM_BSIZE=4 for z8002)
    outb    0x00F0, RL1     ! output low byte to console port
    ld      R15, R13
    pop     R13, *SP
    ret

! --- void putchar(int ch) ---
! Alias for putc, used by prf.c printf.
_putchar:
    push    *SP, R13
    ld      R13, R15
    ld      R1, 4(R13)      ! arg: character
    outb    0x00F0, RL1     ! output low byte to console port
    ld      R15, R13
    pop     R13, *SP
    ret

! --- int inb(int port) ---
! Reads a byte from the given I/O port.
! Returns the byte value in R0 (ACK return register).
_inb:
    push    *SP, R13
    ld      R13, R15
    ld      R2, 4(R13)      ! arg: port address into R2
    inb     RL7, *RR2       ! read byte from port (RR2 = R2 in z8002)
    and     R7, $0x00FF     ! zero-extend to 16-bit
    ld      R0, R7          ! return value in R0
    ld      R15, R13
    pop     R13, *SP
    ret

! --- void outb(int port, int byte) ---
! Writes a byte to the given I/O port.
_outb:
    push    *SP, R13
    ld      R13, R15
    ld      R2, 4(R13)      ! arg1: port address into R2
    ld      R4, 6(R13)      ! arg2: byte value into R4
    outb    *RR2, RL4       ! write low byte to port (RR2 = R2 in z8002)
    ld      R15, R13
    pop     R13, *SP
    ret

! --- void idle(void) ---
! Halts the CPU.
_idle:
    halt


.sect .bss
begbss:

.sect .data
hol0:
    .data2 0, 0             ! line number
    .data2 0, 0             ! filename
trppc:
    .data2 0
trpim:
    .data2 0
reghp:
    .data2 endbss
