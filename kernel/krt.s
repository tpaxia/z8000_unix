! Kernel runtime stub (replaces ACK boot.s)
! Entry point called from trap stub via call @r2 in NONSEG+SYS mode.

.define EXIT, WRITE, BRK
.define ERANGE, ESET, EHEAP, EILLINS, EODDZ, ECASE, EBADMON
.define hol0, trppc, trpim, reghp
.define LINO_AD, FILN_AD
.define _putc

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
! Called from trap stub with arguments on the stack.
! Stack at entry:
!   SP+0: return address to trap.s
!   SP+2: syscall number (arg1)
!   SP+4: regs pointer (arg2)
    ld      R1, 2(R15)      ! R1 = syscall number
    ld      R2, 4(R15)      ! R2 = pointer to saved registers
    push    *SP, R2          ! push arg2 for C handler
    push    *SP, R1          ! push arg1 for C handler
    calr    _syscall_handler
    add     R15, $4          ! caller cleans up 2 args
    ret

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
