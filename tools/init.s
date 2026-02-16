! /etc/init - test program for read() + write()
! Reads from stdin, writes back to stdout, then exits.
! No CRT, no libc - raw syscalls via SC instruction.

.sect .text
    ld R1, $0           ! fd = stdin
    ld R2, $buf         ! buffer
    ld R3, $80          ! max count
    sc $3               ! read(0, buf, 80)
    ! R0 = bytes read
    ld R3, R0           ! count = bytes read
    ld R1, $1           ! fd = stdout
    ld R2, $buf         ! buffer
    sc $4               ! write(1, buf, n)
    ld R1, $0
    sc $1               ! exit(0)
    halt

.sect .bss
buf: .space 80
