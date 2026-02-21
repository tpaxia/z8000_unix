! crt0.s - C runtime startup for Z8002 user programs.
!
! Entry: exec() sets up user stack with:
!   *SP = argc
!   SP+2, SP+4, ... = argv[0], argv[1], ..., NULL, envp[], NULL
!
! Calls main(argc, argv), then _exit(retval).
!
! Also provides symbols required by ACK's libem.a runtime.

.define EXIT, WRITE, BRK
.define ERANGE, ESET, EHEAP, EILLINS, EODDZ, ECASE, EBADMON
.define hol0, trppc, trpim, reghp
.define LINO_AD, FILN_AD
.define _exit, __exit

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

! --- Entry point (first instruction in text segment) ---
    ld R1, *SP          ! R1 = argc
    ld R2, R15
    add R2, $2          ! R2 = argv = &SP[1]
    sub R15, $4         ! allocate space for 2 args
    ld 2(R15), R2       ! arg 2: argv
    ld 0(R15), R1       ! arg 1: argc
    call __crtinit      ! set environ = &argv[argc+1]
    call _main
    ! main() returned; call _exit(retval)
    ld 0(R15), R0       ! arg 1: return value
    call _exit
    halt

! --- _exit(status) / exit(status) --- syscall #1
! C's _exit() maps to asm __exit, C's exit() maps to asm _exit
__exit:
_exit:
    ld R1, 2(R15)       ! status
    sc $1
    halt                ! should not return

! --- libem.a runtime stubs ---
EXIT:   halt
WRITE:  ret
BRK:    ret

.sect .data
hol0:
    .data2 0, 0         ! line number
    .data2 0, 0         ! filename
trppc:
    .data2 0
trpim:
    .data2 0
reghp:
    .data2 0
