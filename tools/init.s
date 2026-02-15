! /etc/init - minimal test program for exec()
! Calls write(1, msg, 16) then exit(0).
! No CRT, no libc - raw syscalls via SC instruction.

.sect .text
    ld R1, $1           ! fd = stdout
    ld R2, $msg         ! buf pointer
    ld R3, $16          ! count
    sc $4               ! write(1, msg, 16)
    ld R1, $0
    sc $1               ! exit(0)
    halt

.sect .data
msg:
    .ascii "hello from exec\12"
